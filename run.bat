@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set CONFIG=Debug
if /i "%~1"=="release" set CONFIG=Release

echo ========================================================
echo   KTEngine Build ^& Run Script [%CONFIG% / x64]
echo ========================================================
echo.

:: 1. ?r???h???s
call "%~dp0build.bat" %* /nopause
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ABORT] Build failed. Launch cancelled.
    echo ========================================================
    pause
    exit /b %ERRORLEVEL%
)

:: 2. ???s?t?@?C????m?F
set EXE_PATH=%~dp0x64\%CONFIG%\KTEngine.exe

if not exist "%EXE_PATH%" (
    echo.
    echo [ERROR] Executable not found:
    echo         "%EXE_PATH%"
    echo ========================================================
    pause
    exit /b 1
)

:: 3. ?A?v???P?[?V?????N??
echo.
echo [INFO] Launching KTEngine...
echo        Working Directory: %~dp0
echo        Executable:        %EXE_PATH%
echo ========================================================
echo.

start "" "%EXE_PATH%"
exit /b 0
