@echo off
REM ============================================================
REM  Build SmartShot with MinGW-w64 (g++).
REM  Requires MinGW-w64 (or mingw-w64-x86_64-gcc from MSYS2) with its bin
REM  folder (g++, windres) on PATH.
REM  -s strips symbols for a smaller exe; -static links the runtime in,
REM  so the result is a single portable exe.
REM
REM  Keep this file ASCII-only: cmd mis-tracks its read position in batch files
REM  that contain non-ASCII text when the code page changes, and runs fragments
REM  of earlier lines as commands.
REM ============================================================
setlocal
REM Run from this folder (.rc and sources live here)
cd /d "%~dp0"
echo Compiling resources (icon, sound)...
windres smartshot.rc -O coff -o smartshot.res
if errorlevel 1 goto fail

echo Compiling...
g++ -municode -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -finput-charset=UTF-8 -O2 -s -o SmartShot.exe ^
    smartshot.cpp capture_common.cpp capture_gdi.cpp saver.cpp feedback.cpp smartshot.res ^
    -mwindows -static -static-libgcc -static-libstdc++ ^
    -lgdiplus -lgdi32 -lshell32 -lole32 -luuid -lcomdlg32 -lwinmm
if errorlevel 1 goto fail

echo.
echo Build OK  ^-^>  SmartShot.exe (double-click to run; it lives in the tray, bottom right).
pause
exit /b 0

:fail
echo.
echo Build FAILED. Make sure g++ and windres are installed and on PATH.
pause
exit /b 1
