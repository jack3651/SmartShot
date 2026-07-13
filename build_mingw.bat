@echo off
REM ============================================================
REM  用 MinGW-w64 (g++) 編譯 SnapShot（合併版）
REM  需先安裝 MinGW-w64（或 MSYS2 的 mingw-w64-x86_64-gcc），
REM  並把 g++ 所在 bin 資料夾加入 PATH。
REM  -s 去除除錯符號讓 exe 更小；-static 靜態連結成單一免安裝檔。
REM ============================================================
chcp 65001 >nul
echo 編譯中...
g++ -municode -finput-charset=UTF-8 -O2 -s -o Snap.exe snapshot.cpp ^
    -mwindows -static -static-libgcc -static-libstdc++ ^
    -lgdiplus -lgdi32 -lshell32 -lole32 -luuid -lcomdlg32

if %errorlevel%==0 (
    echo.
    echo 完成！產生 Snap.exe，雙擊即可執行（常駐系統列右下角）。
) else (
    echo.
    echo 編譯失敗，請確認 g++ 已安裝並在 PATH 中。
)
pause
