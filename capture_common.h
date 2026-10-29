// ============================================================
//  擷取共用：Frame、全黑掃描、獨佔全螢幕偵測、螢幕選擇
// ============================================================
#pragma once
#include <windows.h>
#include <vector>

struct Frame {
    std::vector<BYTE> pixels;   // 32bpp BGRA，每列 stride = width * 4
    int width  = 0;
    int height = 0;
    bool empty() const { return pixels.empty(); }
};

bool     IsAllBlack(const Frame& f);
bool     IsExclusiveFullscreen();
HMONITOR GetTargetMonitor();
