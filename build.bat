@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

:: -------------------------------------------------------------
:: 引数の解析
:: -------------------------------------------------------------
set CONFIG=Debug
set TARGET=Build
set NO_PAUSE=0

:parse_args
if "%~1"=="" goto done_args
if /i "%~1"=="debug"    (set CONFIG=Debug& shift & goto parse_args)
if /i "%~1"=="release"  (set CONFIG=Release& shift & goto parse_args)
if /i "%~1"=="rebuild"  (set TARGET=Rebuild& shift & goto parse_args)
if /i "%~1"=="clean"    (set TARGET=Clean& shift & goto parse_args)
if /i "%~1"=="/nopause" (set NO_PAUSE=1& shift & goto parse_args)
shift
goto parse_args
:done_args

echo ========================================================
echo   KTEngine Build Script [%CONFIG% / x64]
echo ========================================================

:: -------------------------------------------------------------
:: MSBuild の検出
:: -------------------------------------------------------------
set MSBUILD_PATH=

:: 1. PATH 上にあるか確認 (Developer Command Prompt など)
where msbuild >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    for /f "tokens=*" %%i in ('where msbuild') do (
        set "MSBUILD_PATH=%%i"
        goto msbuild_found
    )
)

:: 2. vswhere.exe による最新 Visual Studio / Build Tools 探索
set VSWHERE_PATH="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %VSWHERE_PATH% (
    set VSWHERE_PATH="%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
)

if exist %VSWHERE_PATH% (
    for /f "usebackq tokens=*" %%i in (`%VSWHERE_PATH% -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
        set "MSBUILD_PATH=%%i"
        goto msbuild_found
    )
)

:: 3. 既知の標準パスをフォールバック検索
set CANDIDATES[0]=%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe
set CANDIDATES[1]=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe
set CANDIDATES[2]=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe
set CANDIDATES[3]=%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe
set CANDIDATES[4]=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe
set CANDIDATES[5]=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe
set CANDIDATES[6]=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\MSBuild.exe
set CANDIDATES[7]=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe

for /l %%i in (0,1,7) do (
    call set "CANDIDATE=%%CANDIDATES[%%i]%%"
    if exist "!CANDIDATE!" (
        set "MSBUILD_PATH=!CANDIDATE!"
        goto msbuild_found
    )
)

:: -------------------------------------------------------------
:: MSBuild が見つからなかった場合
:: -------------------------------------------------------------
echo [ERROR] MSBuild.exe was not found.
echo.
echo Visual Studio or Visual Studio Build Tools is required to compile.
echo Please make sure 'Desktop development with C++' workload is installed.
echo.
echo Download Build Tools:
echo https://visualstudio.microsoft.com/visual-cpp-build-tools/
echo ========================================================
if %NO_PAUSE%==0 pause
exit /b 1

:: -------------------------------------------------------------
:: ビルド実行
:: -------------------------------------------------------------
:msbuild_found
echo [INFO] Found MSBuild:
echo        "%MSBUILD_PATH%"
echo.

set PROJ_FILE=KTEngine.vcxproj
if not exist "%PROJ_FILE%" set PROJ_FILE=KTEngine.sln

echo [INFO] Starting build: %PROJ_FILE%
echo        Target: %TARGET%
echo        Config: %CONFIG%
echo        Platform: x64
echo.

"%MSBUILD_PATH%" "%PROJ_FILE%" /t:%TARGET% /p:Configuration=%CONFIG% /p:Platform=x64 /m /v:minimal /clp:Summary

set BUILD_STATUS=%ERRORLEVEL%
echo.
if %BUILD_STATUS% NEQ 0 (
    echo ========================================================
    echo   [FAILED] Build failed with exit code %BUILD_STATUS%.
    echo ========================================================
    if %NO_PAUSE%==0 pause
    exit /b %BUILD_STATUS%
)

:: -------------------------------------------------------------
:: 必要なアセット・シェーダー等の出力フォルダへの差分同期
:: -------------------------------------------------------------
if /i not "%TARGET%"=="Clean" call :sync_assets

echo ========================================================
echo   [SUCCESS] Build and asset synchronization succeeded.
echo ========================================================

if %NO_PAUSE%==0 pause
exit /b 0

:: -------------------------------------------------------------
:: サブルーチン: アセット同期
:: -------------------------------------------------------------
:sync_assets
echo [INFO] Syncing assets, shaders, and configs to output folder...
set "OUT_DIR=%~dp0x64\%CONFIG%"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

if exist asset (
    robocopy asset "%OUT_DIR%\asset" /E /XO /NP /NFL /NDL /NJH /NJS >nul
)
if exist shader (
    robocopy shader "%OUT_DIR%\shader" /E /XO /NP /NFL /NDL /NJH /NJS >nul
)
if exist engine_config.json (
    copy /Y engine_config.json "%OUT_DIR%\" >nul 2>nul
)
if exist imgui.ini (
    copy /Y imgui.ini "%OUT_DIR%\" >nul 2>nul
)
echo [INFO] Assets synced successfully.
exit /b 0
