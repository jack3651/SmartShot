@echo off
REM ============================================================
REM  用 MinGW-w64 (g++) 編譯 SmartShot
REM  需先安裝 MinGW-w64（或 MSYS2 的 mingw-w64-x86_64-gcc），
REM  並把 g++ / windres 所在 bin 資料夾加入 PATH。
REM  -s 去除除錯符號讓 exe 更小；-static 靜態連結成單一免安裝檔。
REM ============================================================
chcp 65001 >nul
echo 編譯資源檔（快門音效）...
windres smartshot.rc -O coff -o smartshot.res
if errorlevel 1 goto fail

echo 編譯中...
g++ -municode -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -finput-charset=UTF-8 -O2 -s -o SmartShot.exe ^
    smartshot.cpp capture_common.cpp capture_gdi.cpp saver.cpp feedback.cpp smartshot.res ^
    -mwindows -static -static-libgcc -static-libstdc++ ^
    -lgdiplus -lgdi32 -lshell32 -lole32 -luuid -lcomdlg32 -lwinmm
if errorlevel 1 goto fail

echo.
echo 完成！產生 SmartShot.exe，雙擊即可執行（常駐系統列右下角）。
pause
exit /b 0

:fail
echo.
echo 編譯失敗，請確認 g++ 與 windres 已安裝並在 PATH 中。
pause
exit /b 1
