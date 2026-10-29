#include "feedback.h"
#include "resource.h"
#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

void PlayShutter() {
    PlaySoundW(MAKEINTRESOURCEW(IDR_SHUTTER), GetModuleHandleW(NULL),
               SND_RESOURCE | SND_ASYNC | SND_NODEFAULT);
}

void PlayWarning() { MessageBeep(MB_ICONEXCLAMATION); }
void PlayError()   { MessageBeep(MB_ICONHAND); }

// 只有使用者正常在桌面時才允許跳視窗（例如存檔失敗的說明）
bool CanShowPopup() {
    QUERY_USER_NOTIFICATION_STATE s;
    return SUCCEEDED(SHQueryUserNotificationState(&s))
        && s == QUNS_ACCEPTS_NOTIFICATIONS;
}
