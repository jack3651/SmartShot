// ============================================================
//  背景存檔：Frame 排入佇列，由背景執行緒依序編碼成 PNG
//  主執行緒永遠不做 PNG 編碼；連按也只是排隊，不會卡住。
// ============================================================
#pragma once
#include "capture_common.h"
#include <string>

// 需在 GdiplusStartup 之後呼叫。存檔失敗時 PostMessage(notifyHwnd, failMsg, 0, 0)。
bool SaverStart(HWND notifyHwnd, UINT failMsg);
void EnqueueSave(Frame&& f, const std::wstring& dir);
// 等佇列中的截圖全部存完才返回；需在 GdiplusShutdown 之前呼叫。
void SaverStop();
