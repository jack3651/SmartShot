#include "saver.h"
#include <objidl.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <stdlib.h>
#include <deque>
#include <utility>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")

using namespace Gdiplus;

namespace {
struct Job {
    Frame        frame;
    std::wstring dir;
};

CRITICAL_SECTION   g_lock;
CONDITION_VARIABLE g_cv;
std::deque<Job>    g_queue;
bool               g_stop     = false;
HANDLE             g_thread   = NULL;
HWND               g_notify   = NULL;
UINT               g_failMsg  = 0;
CLSID              g_pngClsid = {};
bool               g_hasPng   = false;
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

// ---- 產生不重複的檔名（同一秒自動加 _1、_2…） -------------
static std::wstring MakeFilePath(const std::wstring& dir) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t base[64];
    swprintf(base, 64, L"SmartShot_%04d%02d%02d_%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    for (int i = 0; ; ++i) {
        std::wstring path = dir + L"\\" + base;
        if (i > 0) path += L"_" + std::to_wstring(i);
        path += L".png";
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
            return path;
    }
}

static bool SaveJob(Job& j) {
    if (!g_hasPng || j.frame.empty()) return false;
    SHCreateDirectoryExW(NULL, j.dir.c_str(), NULL); // 資料夾不存在就建立
    std::wstring path = MakeFilePath(j.dir);
    // 用 32bppRGB 忽略 alpha：GDI/DXGI 拿到的 alpha 可能是 0，用 ARGB 存會變透明
    Bitmap bmp(j.frame.width, j.frame.height, j.frame.width * 4,
               PixelFormat32bppRGB, j.frame.pixels.data());
    return bmp.Save(path.c_str(), &g_pngClsid, NULL) == Ok;
}

static DWORD WINAPI SaverThread(LPVOID) {
    for (;;) {
        EnterCriticalSection(&g_lock);
        while (g_queue.empty() && !g_stop)
            SleepConditionVariableCS(&g_cv, &g_lock, INFINITE);
        if (g_queue.empty()) {          // 已要求停止，且佇列已存完
            LeaveCriticalSection(&g_lock);
            return 0;
        }
        Job j = std::move(g_queue.front());
        g_queue.pop_front();
        LeaveCriticalSection(&g_lock);

        if (!SaveJob(j) && g_notify)
            PostMessageW(g_notify, g_failMsg, 0, 0);
    }
}

bool SaverStart(HWND notifyHwnd, UINT failMsg) {
    g_notify  = notifyHwnd;
    g_failMsg = failMsg;
    g_hasPng  = GetEncoderClsid(L"image/png", &g_pngClsid) >= 0;
    InitializeCriticalSection(&g_lock);
    InitializeConditionVariable(&g_cv);

    g_thread = CreateThread(NULL, 0, SaverThread, NULL, 0, NULL);
    if (!g_thread) return false;
    SetThreadPriority(g_thread, THREAD_PRIORITY_BELOW_NORMAL); // 不跟遊戲搶 CPU
    return true;
}

void EnqueueSave(Frame&& f, const std::wstring& dir) {
    EnterCriticalSection(&g_lock);
    g_queue.push_back(Job{ std::move(f), dir });
    LeaveCriticalSection(&g_lock);
    WakeConditionVariable(&g_cv);
}

void SaverStop() {
    if (!g_thread) return;
    EnterCriticalSection(&g_lock);
    g_stop = true;
    LeaveCriticalSection(&g_lock);
    WakeConditionVariable(&g_cv);
    WaitForSingleObject(g_thread, INFINITE);
    CloseHandle(g_thread);
    g_thread = NULL;
    DeleteCriticalSection(&g_lock);
}
