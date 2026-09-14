@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set CONFIG=Debug
if /i "%~1"=="release"     set CONFIG=Release
if /i "%~1"=="dev"         set CONFIG=Development
if /i "%~1"=="development" set CONFIG=Development

echo ========================================================
echo   KTEngine Build ^& Run Script [%CONFIG% / x64]
echo ========================================================
echo.

:: 1. ビルド実行
call "%~dp0build.bat" %* /nopause
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ABORT] Build failed. Launch cancelled.
    echo ========================================================
    pause
    exit /b %ERRORLEVEL%
)

:: 2. 実行ファイルの確認
set EXE_PATH=%~dp0x64\%CONFIG%\KTEngine.exe

if not exist "%EXE_PATH%" (
    echo.
    echo [ERROR] Executable not found:
    echo         "%EXE_PATH%"
    echo ========================================================
    pause
    exit /b 1
)

:: 3. アプリケーション起動
echo.
echo [INFO] Launching KTEngine...
echo        Working Directory: %~dp0
echo        Executable:        %EXE_PATH%
echo ========================================================
echo.

start "" "%EXE_PATH%"
exit /b 0
