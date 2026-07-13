// ============================================================
//  一鍵截圖 SnapShot (合併版, C++ / Win32 / GDI+)
//  功能：
//    - 系統列常駐（右鍵選單）
//    - 全域快捷鍵 PrintScreen -> 全螢幕截圖（僅此一組）
//    - 系統列選單內建「選擇資料夾」對話框
//    - 「開機時自動啟動」可勾選切換（寫入 HKCU\...\Run）
//    - 存檔資料夾記憶於登錄檔
//    - 單一免安裝 exe，無第三方相依
//  相較原始 SnapShot 的三項強化：
//    (1) 檔名防覆蓋：同一秒多張自動加 _1、_2…（原版會覆蓋）
//    (2) per-monitor DPI v2：混合 DPI 多螢幕也抓正確像素
//    (3) PrtSc 註冊失敗會跳提示並說明修法（原版靜默無反應）
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

// MSVC 用；MinGW 會忽略（改在命令列 -l 指定）
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

using namespace Gdiplus;

// ---- 訊息與選單 ID ----------------------------------------
#define WM_TRAYICON     (WM_APP + 1)
#define ID_HOTKEY_PRTSC 1
#define IDM_CAPTURE     1001
#define IDM_OPENDIR     1002
#define IDM_CHOOSEDIR   1003
#define IDM_AUTOSTART   1004
#define IDM_EXIT        1005

static const wchar_t* APP_NAME = L"SnapShot";
static const wchar_t* REG_APP  = L"Software\\SnapShot";
static const wchar_t* REG_RUN  = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

static NOTIFYICONDATAW g_nid = {};
static HWND         g_hwnd   = NULL;
static ULONG_PTR    g_gdiTok = 0;
static std::wstring g_saveDir;

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

// ---- PNG 編碼器 CLSID -------------------------------------
static int GetEncoderClsid(const WCHAR* mime, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    ImageCodecInfo* info = (ImageCodecInfo*)malloc(size);
    if (!info) return -1;
    GetImageEncoders(num, size, info);
    int ret = -1;
    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(info[i].MimeType, mime) == 0) {
            *pClsid = info[i].Clsid;
            ret = (int)i;
            break;
        }
    }
    free(info);
    return ret;
}

// ---- 產生不重複的檔名（強化：同一秒不覆蓋） ---------------
static std::wstring MakeFilePath(const std::wstring& dir) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t base[64];
    swprintf(base, 64, L"Snap_%04d%02d%02d_%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    for (int i = 0; ; ++i) {
        std::wstring path = dir + L"\\" + base;
        if (i > 0) path += L"_" + std::to_wstring(i);
        path += L".png";
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
            return path;
    }
}

// ---- 系統列氣泡通知 ---------------------------------------
static void Notify(const wchar_t* title, const wchar_t* msg) {
    g_nid.uFlags = NIF_INFO;
    lstrcpynW(g_nid.szInfoTitle, title, ARRAYSIZE(g_nid.szInfoTitle));
    lstrcpynW(g_nid.szInfo, msg, ARRAYSIZE(g_nid.szInfo));
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; // 還原
}

// ---- 全螢幕截圖 -------------------------------------------
static void CaptureFullScreen() {
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC hScreen = GetDC(NULL);
    HDC hDC     = CreateCompatibleDC(hScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hScreen, w, h);
    HGDIOBJ old  = SelectObject(hDC, hBmp);
    // CAPTUREBLT 連圖層/半透明視窗也一起擷取
    BOOL blt = BitBlt(hDC, 0, 0, w, h, hScreen, x, y, SRCCOPY | CAPTUREBLT);
    SelectObject(hDC, old);

    bool ok = false;
    std::wstring path;
    if (blt) {
        Bitmap bmp(hBmp, (HPALETTE)NULL);
        CLSID clsid;
        if (GetEncoderClsid(L"image/png", &clsid) >= 0) {
            SHCreateDirectoryExW(NULL, g_saveDir.c_str(), NULL); // 資料夾不存在就建立
            path = MakeFilePath(g_saveDir);
            ok = (bmp.Save(path.c_str(), &clsid, NULL) == Ok);
        }
    }
    DeleteObject(hBmp);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);

    if (ok) { MessageBeep(MB_OK);        Notify(L"截圖完成", path.c_str()); }
    else    { MessageBeep(MB_ICONERROR); Notify(L"截圖失敗", L"無法儲存檔案"); }
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
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, IDM_CAPTURE,  L"立即截圖  (PrtSc)");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
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
    switch (msg) {
    case WM_TRAYICON:
        if (LOWORD(lp) == WM_RBUTTONUP)
            ShowMenu();
        else if (LOWORD(lp) == WM_LBUTTONDBLCLK)
            CaptureFullScreen();
        return 0;

    case WM_HOTKEY:
        if (wp == ID_HOTKEY_PRTSC)
            CaptureFullScreen();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDM_CAPTURE:   CaptureFullScreen(); break;
        case IDM_OPENDIR:
            ShellExecuteW(NULL, L"open", g_saveDir.c_str(), NULL, NULL, SW_SHOWNORMAL);
            break;
        case IDM_CHOOSEDIR: ChooseDir(); break;
        case IDM_AUTOSTART: SetAutoStart(!IsAutoStart()); break;
        case IDM_EXIT:      DestroyWindow(hwnd); break;
        }
        return 0;

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        UnregisterHotKey(hwnd, ID_HOTKEY_PRTSC);
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

    // 隱形視窗（承載熱鍵與系統列訊息）
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"SnapShotWndClass";
    RegisterClassW(&wc);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"SnapShot",
                             0, 0, 0, 0, 0,
                             HWND_MESSAGE, NULL, hInst, NULL);

    // 系統列圖示
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = g_hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = LoadIcon(NULL, IDI_APPLICATION);
    lstrcpynW(g_nid.szTip, L"一鍵截圖 SnapShot", ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    // 全域快捷鍵：只註冊 PrtSc，失敗要讓使用者知道（強化點 3）
    if (!RegisterHotKey(g_hwnd, ID_HOTKEY_PRTSC, 0, VK_SNAPSHOT)) {
        MessageBoxW(g_hwnd,
            L"無法註冊 Print Screen 熱鍵。\n\n"
            L"可能被其他截圖軟體佔用，或 Windows 11 的\n"
            L"「使用 Print Screen 鍵開啟螢幕擷取畫面」設定攔截了它。\n\n"
            L"你仍可用系統列圖示的「立即截圖」或雙擊圖示來截圖。\n"
            L"若要修好 PrtSc：設定 → 協助工具 → 鍵盤 → 關閉該選項，再重開本程式。",
            L"熱鍵註冊失敗", MB_OK | MB_ICONWARNING);
    }

    Notify(L"一鍵截圖已啟動",
           L"按 Print Screen 截圖；右鍵圖示可選資料夾與設定開機自啟");

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    GdiplusShutdown(g_gdiTok);
    OleUninitialize();
    if (mtx) CloseHandle(mtx);
    return 0;
}
