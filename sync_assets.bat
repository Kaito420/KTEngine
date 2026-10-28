@echo off
setlocal enabledelayedexpansion

:: -------------------------------------------------------------
:: sync_assets.bat - Sync assets, shaders, and configs to output directory
:: Usage: sync_assets.bat <output_dir>
:: -------------------------------------------------------------

set "DEST_DIR=%~1"
if "%DEST_DIR%"=="" (
    echo [ERROR] Destination directory not specified.
    exit /b 1
)

:: Remove trailing backslash if present
if "%DEST_DIR:~-1%"=="\" set "DEST_DIR=%DEST_DIR:~0,-1%"

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"

if not exist "%DEST_DIR%" mkdir "%DEST_DIR%"

echo [INFO] Syncing assets, shaders, and configs to:
echo        "%DEST_DIR%"

:: 1. Sync asset folder
if exist "%ROOT_DIR%\asset" (
    robocopy "%ROOT_DIR%\asset" "%DEST_DIR%\asset" /E /XO /NP /NFL /NDL /NJH /NJS >nul
    if errorlevel 8 (
        echo [ERROR] Failed to sync asset folder.
        exit /b 1
    )
)

:: 2. Sync shader folder
if exist "%ROOT_DIR%\shader" (
    robocopy "%ROOT_DIR%\shader" "%DEST_DIR%\shader" /E /XO /NP /NFL /NDL /NJH /NJS >nul
    if errorlevel 8 (
        echo [ERROR] Failed to sync shader folder.
        exit /b 1
    )
)

:: 3. Copy config files directly into the exe directory (no config subfolder)
if exist "%ROOT_DIR%\engine_config.json" (
    copy /Y "%ROOT_DIR%\engine_config.json" "%DEST_DIR%\" >nul 2>nul
)
if exist "%ROOT_DIR%\engine.json" (
    copy /Y "%ROOT_DIR%\engine.json" "%DEST_DIR%\" >nul 2>nul
)
if exist "%ROOT_DIR%\imgui.ini" (
    copy /Y "%ROOT_DIR%\imgui.ini" "%DEST_DIR%\" >nul 2>nul
)
if exist "%ROOT_DIR%\Setting.json" (
    copy /Y "%ROOT_DIR%\Setting.json" "%DEST_DIR%\" >nul 2>nul
)
    copy /Y "%ROOT_DIR%\imgui.ini" "%DEST_DIR%\" >nul 2>nul
)

echo [INFO] Synchronization complete.
exit /b 0

