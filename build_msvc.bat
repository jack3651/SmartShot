@echo off
REM ============================================================
REM  Build SmartShot with MSVC.
REM  Run from a "Developer Command Prompt for VS".
REM  /MT links the C runtime statically: a single portable exe, no runtime
REM  to install.
REM  /utf-8: the sources contain Chinese strings; without it cl reads them
REM  in the system code page and they come out garbled.
REM  Libraries are pulled in by #pragma comment in the sources.
REM
REM  Keep this file ASCII-only (see build_mingw.bat).
REM ============================================================
setlocal
REM Run from this folder, wherever it is called from
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
