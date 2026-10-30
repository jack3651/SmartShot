@echo off
REM ============================================================
REM  Build the download zip for itch.io / GitHub Releases (free edition).
REM  Run build_mingw.bat first.
REM  Output: dist\SmartShot-<version>.zip, containing a SmartShot-<version> folder with
REM          SmartShot.exe, the readme files in package\ (*.txt) and LICENSE.txt.
REM  The version comes from resource.h; the exe must have the same version.
REM
REM  Keep this file ASCII-only: cmd mis-tracks its read position in batch files
REM  that contain non-ASCII text when the code page changes, and runs fragments
REM  of earlier lines as commands. (Notes in Chinese: see make_zip.ps1.)
REM ============================================================
setlocal
cd /d "%~dp0.."

set VER=
for /f "tokens=3" %%v in ('findstr /c:"SMARTSHOT_VERSION_STR" resource.h') do set VER=%%~v
if "%VER%"=="" (
    echo Cannot read the version from resource.h.
    goto fail
)

if not exist SmartShot.exe (
    echo SmartShot.exe not found. Run build_mingw.bat first.
    goto fail
)
set EXEVER=
for /f "delims=" %%v in ('powershell -NoProfile -Command "(Get-Item 'SmartShot.exe').VersionInfo.ProductVersion"') do set EXEVER=%%v
if not "%EXEVER%"=="%VER%" (
    echo SmartShot.exe is version %EXEVER% but resource.h says %VER%. Rebuild first.
    goto fail
)

set NAME=SmartShot-%VER%
set OUT=dist\%NAME%
if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%" || goto fail
copy /y SmartShot.exe "%OUT%\" >nul || goto fail
copy /y package\*.txt "%OUT%\" >nul || goto fail
copy /y LICENSE "%OUT%\LICENSE.txt" >nul || goto fail

powershell -NoProfile -ExecutionPolicy Bypass -File package\make_zip.ps1 -Dir "%OUT%" -Zip "dist\%NAME%.zip" || goto fail

echo.
echo Done: dist\%NAME%.zip
powershell -NoProfile -Command "Get-FileHash 'dist\%NAME%.zip' -Algorithm SHA256 | ForEach-Object { 'SHA-256: ' + $_.Hash.ToLower() }"
pause
exit /b 0

:fail
echo.
echo Packaging failed.
pause
exit /b 1
