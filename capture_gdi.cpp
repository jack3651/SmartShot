#include "capture_gdi.h"
#include <string.h>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

// 只 BitBlt 目標螢幕的矩形。程式為 per-monitor DPI v2，rcMonitor 即實際像素。
// 不使用 CAPTUREBLT，避免游標閃爍與變慢。
bool CaptureGDI(HMONITOR mon, Frame& out) {
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(mon, &mi)) return false;
    const int x = mi.rcMonitor.left;
    const int y = mi.rcMonitor.top;
    const int w = mi.rcMonitor.right  - mi.rcMonitor.left;
    const int h = mi.rcMonitor.bottom - mi.rcMonitor.top;
    if (w <= 0 || h <= 0) return false;

    // 32bpp top-down DIB，像素可直接讀出做全黑掃描
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC hScreen = GetDC(NULL);
    if (!hScreen) return false;
    HDC hDC = CreateCompatibleDC(hScreen);
    void* bits = NULL;
    HBITMAP hBmp = hDC ? CreateDIBSection(hScreen, &bi, DIB_RGB_COLORS, &bits, NULL, 0) : NULL;

    bool ok = false;
    if (hBmp && bits) {
        HGDIOBJ old = SelectObject(hDC, hBmp);
        if (BitBlt(hDC, 0, 0, w, h, hScreen, x, y, SRCCOPY)) {
            GdiFlush();
            out.width  = w;
            out.height = h;
            out.pixels.resize((size_t)w * h * 4);
            memcpy(out.pixels.data(), bits, out.pixels.size());
            ok = true;
        }
        SelectObject(hDC, old);
    }
    if (hBmp) DeleteObject(hBmp);
    if (hDC)  DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);
    return ok;
}
