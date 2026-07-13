@echo off
REM ============================================================
REM  用 MSVC 編譯 SnapShot（合併版）
REM  請在「Developer Command Prompt for VS」中切到本資料夾執行。
REM  /MT 靜態連結 C runtime → 單一免安裝 exe，不需安裝任何 runtime。
REM  相依函式庫由原始碼的 #pragma comment 自動帶入。
REM ============================================================
cl /nologo /W3 /O2 /EHsc /MT /DUNICODE /D_UNICODE snapshot.cpp /Fe:Snap.exe ^
   /link /SUBSYSTEM:WINDOWS

if %errorlevel%==0 (
  echo.
  echo Build OK  ^-^>  Snap.exe
) else (
  echo.
  echo Build FAILED  ^(error %errorlevel%^)
)
