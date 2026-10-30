@echo off
REM ============================================================
REM  用 MSVC 編譯 SmartShot
REM  請在「Developer Command Prompt for VS」中切到本資料夾執行。
REM  /MT 靜態連結 C runtime → 單一免安裝 exe，不需安裝任何 runtime。
REM  /utf-8：原始碼含中文字串，沒有這個旗標會以系統字碼頁讀取而變亂碼。
REM  相依函式庫由原始碼的 #pragma comment 自動帶入。
REM ============================================================
REM 從任何資料夾執行都切到本資料夾
setlocal
cd /d "%~dp0"
rc /nologo smartshot.rc
if errorlevel 1 goto fail

cl /nologo /W3 /O2 /EHsc /MT /utf-8 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0601 ^
   smartshot.cpp capture_common.cpp capture_gdi.cpp saver.cpp feedback.cpp smartshot.res ^
   /Fe:SmartShot.exe /link /SUBSYSTEM:WINDOWS
if errorlevel 1 goto fail

echo.
echo Build OK  ^-^>  SmartShot.exe
exit /b 0

:fail
echo.
echo Build FAILED  ^(error %errorlevel%^)
exit /b 1
