#include "capture_common.h"
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

// ---- 全黑掃描：每 16 像素取樣，只判斷 RGB 全為 0 ------------
bool IsAllBlack(const Frame& f) {
    const int stride = f.width * 4;
    for (int y = 0; y < f.height; y += 16) {
        const BYTE* row = f.pixels.data() + (size_t)y * stride;
        for (int x = 0; x < f.width; x += 16) {
            const BYTE* p = row + x * 4;
            if (p[0] | p[1] | p[2]) return false;   // 忽略 alpha
        }
    }
    return true;
}

// ---- 獨佔全螢幕偵測（全螢幕最佳化下的準確度需實測） --------
bool IsExclusiveFullscreen() {
    QUERY_USER_NOTIFICATION_STATE s;
    return SUCCEEDED(SHQueryUserNotificationState(&s))
        && s == QUNS_RUNNING_D3D_FULL_SCREEN;
}

// ---- 目標螢幕：前景視窗所在的螢幕 -------------------------
HMONITOR GetTargetMonitor() {
    // 前景是本程式（例如剛點過系統列選單）或沒有前景視窗時，改用游標所在的螢幕
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    if (fg) GetWindowThreadProcessId(fg, &pid);
    if (fg && pid != GetCurrentProcessId())
        return MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST);

    POINT pt;
    GetCursorPos(&pt);
    return MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
}
