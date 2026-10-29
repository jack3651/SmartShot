// ============================================================
//  一鍵截圖 SnapShot (C++ / Win32 / GDI+)
//  功能：
//    - 系統列常駐（右鍵選單）
//    - 全域快捷鍵 PrintScreen -> 截取前景視窗所在的螢幕
//    - 系統列選單內建「選擇資料夾」對話框
//    - 「開機時自動啟動」可勾選切換（寫入 HKCU\...\Run）
//    - 存檔資料夾記憶於登錄檔
//    - 單一免安裝 exe，無第三方相依
//    - 備援熱鍵 Ctrl+Shift+P
//  v2 擷取流程（見《SmartShot 擷取流程規格》）：
//    - 免費版只用 GDI；Pro 版（SMARTSHOT_PRO）全黑或獨佔全螢幕時改用 DXGI
//    - 截圖成功只播快門聲，不跳任何視窗；全黑／失敗播錯誤音
//    - 主執行緒只做擷取與全黑掃描，PNG 由背景執行緒編碼
//  v2 首次使用（見《SmartShot 首次使用流程》）：
//    - PrtSc 被佔用時引導並每秒自動重試，成功時播快門聲
//    - 第一次執行顯示一次性提示氣泡（含 OneDrive 同步提醒）
// ============================================================
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <gdiplus.h>
#include <string>
#include <utility>

#include "capture_common.h"
#include "capture_gdi.h"
#include "saver.h"
#include "feedback.h"
#ifdef SMARTSHOT_PRO
#include "capture_dxgi.h"   // 私有 repo
#endif

// MSVC 用；MinGW 會忽略（改在命令列 -l 指定）
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

using namespace Gdiplus;

// ---- 訊息與選單 ID ----------------------------------------
#define WM_TRAYICON     (WM_APP + 1)
#define WM_SAVEFAILED   (WM_APP + 2)
#define ID_HOTKEY_PRTSC  1
#define ID_TIMER_RETRY   2
#define ID_HOTKEY_BACKUP 3
#define IDM_OPENDIR     1002
#define IDM_CHOOSEDIR   1003
#define IDM_AUTOSTART   1004
#define IDM_EXIT        1005

// ---- 固定備援熱鍵（PrtSc 被佔用或被遊戲攔走時使用） -------
#define BACKUP_HOTKEY_MOD  (MOD_CONTROL | MOD_SHIFT)
#define BACKUP_HOTKEY_VK   'P'
#define BACKUP_HOTKEY_TEXT L"Ctrl+Shift+P"

static const wchar_t* APP_NAME = L"SnapShot";
static const wchar_t* REG_APP  = L"Software\\SnapShot";
static const wchar_t* REG_RUN  = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

static NOTIFYICONDATAW g_nid = {};
static HWND         g_hwnd   = NULL;
static ULONG_PTR    g_gdiTok = 0;
static std::wstring g_saveDir;
static UINT         g_wmTaskbarCreated = 0;   // Explorer 重啟後要重新加入系統列圖示
static bool         g_prtscOk  = false;       // PrtSc 熱鍵已註冊
static bool         g_backupOk = false;       // 備援熱鍵已註冊

// ---- 高 DPI：per-monitor v2（動態載入，舊系統自動退回） ----
static void EnableDpiAwareness() {
    HMODULE u = LoadLibraryW(L"user32.dll");
    if (u) {
        typedef BOOL (WINAPI *SetCtxFn)(HANDLE);
        SetCtxFn f = (SetCtxFn)GetProcAddress(u, "SetProcessDpiAwarenessContext");
        // (HANDLE)-4 == DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
        if (f && f((HANDLE)-4)) { FreeLibrary(u); return; }
        FreeLibrary(u);
    }
    SetProcessDPIAware(); // Vista+ 退路
}

// ---- 儲存資料夾（登錄檔記憶） -----------------------------
static std::wstring PicturesDir() {
    wchar_t p[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_MYPICTURES, NULL, 0, p)))
        return std::wstring(p) + L"\\SnapShot";
    return L"C:\\SnapShot";
}

static std::wstring LoadSaveDir() {
    HKEY hKey;
    wchar_t buf[MAX_PATH];
    DWORD sz = sizeof(buf), type = 0;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_APP, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        LONG r = RegQueryValueExW(hKey, L"SaveDir", NULL, &type, (LPBYTE)buf, &sz);
        RegCloseKey(hKey);
        if (r == ERROR_SUCCESS && type == REG_SZ && buf[0])
            return buf;
    }
    return PicturesDir();
}

static void SaveSaveDir(const std::wstring& dir) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_APP, 0, NULL, 0,
                        KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"SaveDir", 0, REG_SZ,
                       (const BYTE*)dir.c_str(),
                       (DWORD)((dir.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
}

// ---- 首次執行標記（登錄檔 FirstRunDone = 1） --------------
static bool IsFirstRunDone() {
    HKEY hKey;
    DWORD v = 0, sz = sizeof(v), type = 0;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_APP, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        LONG r = RegQueryValueExW(hKey, L"FirstRunDone", NULL, &type, (LPBYTE)&v, &sz);
        RegCloseKey(hKey);
        return r == ERROR_SUCCESS && type == REG_DWORD && v == 1;
    }
    return false;
}

static void SetFirstRunDone() {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_APP, 0, NULL, 0,
                        KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD v = 1;
        RegSetValueExW(hKey, L"FirstRunDone", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
}

// ---- 開機自啟 ---------------------------------------------
static bool IsAutoStart() {
    HKEY hKey;
    bool exist = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, APP_NAME, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
            exist = true;
        RegCloseKey(hKey);
    }
    return exist;
}

static void SetAutoStart(bool on) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (on) {
            wchar_t exe[MAX_PATH];
            GetModuleFileNameW(NULL, exe, MAX_PATH);
            std::wstring q = L"\"" + std::wstring(exe) + L"\"";
            RegSetValueExW(hKey, APP_NAME, 0, REG_SZ,
                           (const BYTE*)q.c_str(),
                           (DWORD)((q.size() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, APP_NAME);
        }
        RegCloseKey(hKey);
    }
}

// ---- 系統列氣泡通知（截圖成功時不使用） -------------------
static void Notify(const wchar_t* title, const wchar_t* msg) {
    g_nid.uFlags = NIF_INFO;
    lstrcpynW(g_nid.szInfoTitle, title, ARRAYSIZE(g_nid.szInfoTitle));
    lstrcpynW(g_nid.szInfo, msg, ARRAYSIZE(g_nid.szInfo));
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; // 還原
}

// ---- 熱鍵：擷取 → 全黑掃描 → 播放聲音 → 排入存檔 ---------
#ifdef SMARTSHOT_PRO
enum class CaptureMode { GDI, DXGI };
static CaptureMode g_mode = CaptureMode::GDI;   // 每次啟動都從 GDI 開始，不存檔

static void Finish(Frame& f, bool ok) {
    if (ok) PlayShutter(); else PlayError();
    // 全黑也照樣存，避免漏掉真的黑幕畫面
    if (!f.empty()) EnqueueSave(std::move(f), g_saveDir);
}

static void OnHotkey() {
    HMONITOR mon = GetTargetMonitor();
    Frame f;

    if (g_mode == CaptureMode::GDI && !IsExclusiveFullscreen()) {
        if (CaptureGDI(mon, f) && !IsAllBlack(f)) {
            Finish(f, true);
            return;
        }
        // 全黑（或 GDI 失敗）→ 同一次按鍵立刻改用 DXGI
        f = Frame{};
    }
    // 獨佔全螢幕也直接走到這裡

    if (CaptureDXGI(mon, f)) {
        g_mode = CaptureMode::DXGI;     // 本次執行期間之後都用 DXGI
    } else {
        f = Frame{};
        CaptureGDI(mon, f);              // DXGI 出錯 → 這一張用 GDI，模式不變
    }
    Finish(f, !f.empty() && !IsAllBlack(f));
}
#else
static void OnHotkey() {
    HMONITOR mon = GetTargetMonitor();
    Frame f;
    bool captured = CaptureGDI(mon, f);

    if (!captured || IsAllBlack(f)) {
        PlayError();
    } else if (IsExclusiveFullscreen()) {
        PlayWarning();   // 可能截到桌面而非遊戲；可作為升級 Pro 的提示點
    } else {
        PlayShutter();
    }
    // 全黑也照樣存，避免漏掉真的黑幕畫面
    if (!f.empty()) EnqueueSave(std::move(f), g_saveDir);
}
#endif

// ---- 存檔位置是否在 OneDrive 同步資料夾內 -----------------
static bool IsUnderOneDrive(const std::wstring& path) {
    const wchar_t* vars[] = { L"OneDrive", L"OneDriveConsumer", L"OneDriveCommercial" };
    for (const wchar_t* v : vars) {
        wchar_t root[MAX_PATH];
        DWORD n = GetEnvironmentVariableW(v, root, MAX_PATH);
        if (n == 0 || n >= MAX_PATH || path.size() < n) continue;
        if (_wcsnicmp(path.c_str(), root, n) == 0 &&
            (path.size() == n || path[n] == L'\\'))
            return true;
    }
    return false;
}

// ---- 首次提示氣泡：確認熱鍵有效、聽得到快門聲、告知存檔位置 --
static void MaybeShowFirstRunTip() {
    if (IsFirstRunDone() || !CanShowPopup()) return;   // 無法顯示就等下次啟動

    std::wstring msg =
        L"按一下 PrtSc 試試看，聽到快門聲就完成設定。\n"
        L"備援熱鍵：" BACKUP_HOTKEY_TEXT L"\n存檔位置：" + g_saveDir;
    if (IsUnderOneDrive(g_saveDir))
        msg += L"\n此資料夾會同步到 OneDrive，可從右鍵選單更改。";
    Notify(L"SnapShot 已在背景執行", msg.c_str());

    SetFirstRunDone();
}

// ---- 熱鍵：註冊失敗時引導，並每秒自動重試 -----------------
// 回傳兩組熱鍵是否都已註冊。MOD_NOREPEAT：按住不會連拍。
static bool TryRegisterHotkeys() {
    if (!g_prtscOk)
        g_prtscOk = RegisterHotKey(g_hwnd, ID_HOTKEY_PRTSC, MOD_NOREPEAT, VK_SNAPSHOT) != FALSE;
    if (!g_backupOk)
        g_backupOk = RegisterHotKey(g_hwnd, ID_HOTKEY_BACKUP,
                                    BACKUP_HOTKEY_MOD | MOD_NOREPEAT, BACKUP_HOTKEY_VK) != FALSE;
    return g_prtscOk && g_backupOk;
}

static void ShowPrtScGuide() {
    int r = MessageBoxW(g_hwnd,
        L"Print Screen 鍵被其他程式佔用了。\n\n"
        L"最常見的是 Windows 11 剪取工具。要現在打開設定，"
        L"把「使用 Print Screen 鍵開啟螢幕擷取」關掉嗎？\n\n"
        L"如果不是剪取工具，可能是 OneDrive、Dropbox 或其他截圖軟體的截圖功能，"
        L"請先關閉該功能。\n\n"
        L"關掉後立即生效，不用重開本程式。在那之前，可以先用 "
        BACKUP_HOTKEY_TEXT L" 截圖。",
        L"SnapShot", MB_YESNO | MB_ICONINFORMATION);
    if (r == IDYES) {
        ShellExecuteW(NULL, L"open", L"ms-settings:easeofaccess-keyboard",
                      NULL, NULL, SW_SHOWNORMAL);
    }
}

// ---- 選擇資料夾（內建對話框） -----------------------------
static void ChooseDir() {
    BROWSEINFOW bi = {};
    bi.hwndOwner = g_hwnd;
    bi.lpszTitle = L"選擇截圖儲存資料夾";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            g_saveDir = path;
            SaveSaveDir(g_saveDir);
            Notify(L"已設定儲存資料夾", g_saveDir.c_str());
        }
        CoTaskMemFree(pidl);
    }
}

// ---- 系統列選單 -------------------------------------------
static void ShowMenu() {
    POINT pt;
    GetCursorPos(&pt);
    // 極簡選單：截圖只靠熱鍵（全螢幕遊戲裡點不到選單）
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, IDM_OPENDIR,  L"開啟儲存資料夾");
    AppendMenuW(m, MF_STRING, IDM_CHOOSEDIR, L"選擇儲存資料夾…");
    AppendMenuW(m, MF_STRING | (IsAutoStart() ? MF_CHECKED : 0),
                IDM_AUTOSTART, L"開機時自動啟動");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_EXIT, L"結束");

    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, NULL);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);  // MSDN 建議：確保選單正常關閉
    DestroyMenu(m);
}

// ---- 視窗訊息 ---------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_wmTaskbarCreated && msg == g_wmTaskbarCreated) {
        Shell_NotifyIconW(NIM_ADD, &g_nid);   // Explorer 重啟，重新加入圖示
        return 0;
    }

    switch (msg) {
    case WM_TRAYICON:
        if (LOWORD(lp) == WM_RBUTTONUP)
            ShowMenu();
        return 0;

    case WM_HOTKEY:
        if (wp == ID_HOTKEY_PRTSC || wp == ID_HOTKEY_BACKUP)
            OnHotkey();
        return 0;

    case WM_TIMER:
        if (wp == ID_TIMER_RETRY) {
            bool hadPrtSc = g_prtscOk;
            if (TryRegisterHotkeys())
                KillTimer(hwnd, ID_TIMER_RETRY);
            if (!hadPrtSc && g_prtscOk) {
                PlayShutter();        // 用快門聲告訴使用者「可以用了」
                MaybeShowFirstRunTip();
            }
        }
        return 0;

    case WM_SAVEFAILED:
        PlayError();
        if (CanShowPopup()) {
            std::wstring msg = L"無法儲存到：" + g_saveDir;
            Notify(L"截圖存檔失敗", msg.c_str());
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDM_OPENDIR:
            ShellExecuteW(NULL, L"open", g_saveDir.c_str(), NULL, NULL, SW_SHOWNORMAL);
            break;
        case IDM_CHOOSEDIR: ChooseDir(); break;
        case IDM_AUTOSTART: SetAutoStart(!IsAutoStart()); break;
        case IDM_EXIT:      DestroyWindow(hwnd); break;
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER_RETRY);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        UnregisterHotKey(hwnd, ID_HOTKEY_PRTSC);
        UnregisterHotKey(hwnd, ID_HOTKEY_BACKUP);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---- 進入點 -----------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    // 單一執行個體
    HANDLE mtx = CreateMutexW(NULL, TRUE, L"SnapShot_SingleInstance");
    if (mtx && GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }

    OleInitialize(NULL);    // 資料夾對話框新樣式需要
    EnableDpiAwareness();   // 混合 DPI 多螢幕也抓正確像素

    GdiplusStartupInput gsi;
    GdiplusStartup(&g_gdiTok, &gsi, NULL);

    g_saveDir = LoadSaveDir();

    // 隱形視窗（承載熱鍵與系統列訊息）。
    // 不用 HWND_MESSAGE：訊息專用視窗收不到 TaskbarCreated 廣播。
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"SnapShotWndClass";
    RegisterClassW(&wc);
    g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"SnapShot",
                             WS_POPUP, 0, 0, 0, 0,
                             NULL, NULL, hInst, NULL);

    g_wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    // 以系統管理員身分執行時，仍要收得到 Explorer 送來的 TaskbarCreated
    ChangeWindowMessageFilterEx(g_hwnd, g_wmTaskbarCreated, MSGFLT_ALLOW, NULL);

    SaverStart(g_hwnd, WM_SAVEFAILED);

    // 系統列圖示
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = g_hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = LoadIcon(NULL, IDI_APPLICATION);
    lstrcpynW(g_nid.szTip, L"一鍵截圖 SnapShot（PrtSc／" BACKUP_HOTKEY_TEXT L"）",
              ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    // 全域快捷鍵：PrtSc＋備援熱鍵。失敗就每秒自動重試；PrtSc 失敗另外引導。
    // 先啟動計時器：引導視窗開著時（MessageBox 仍會處理訊息）就能偵測到使用者關掉設定。
    if (!TryRegisterHotkeys())
        SetTimer(g_hwnd, ID_TIMER_RETRY, 1000, NULL);
    if (g_prtscOk)
        MaybeShowFirstRunTip();
    else if (CanShowPopup())
        ShowPrtScGuide();

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    SaverStop();            // 等排隊中的截圖存完
    GdiplusShutdown(g_gdiTok);
    OleUninitialize();
    if (mtx) CloseHandle(mtx);
    return 0;
}
