@echo off
setlocal

:: clear all SAFE_RM_* env vars
for /f "tokens=1* delims==" %%a in ('set SAFE_RM_ 2^>nul') do set "%%a="

:: setup VS dev environment if not loaded yet
:: prefer locating VS via vswhere (install dir may differ across machines),
:: fall back to default D:\tool\Microsoft Visual Studio\2022\Community
if defined VSCMD_VER goto :env_ok
call :find_vsdev
if defined VSDEV call "%VSDEV%" -arch=x64 -host_arch=x64 >nul 2>&1
:env_ok

:: build config (default Debug; switch to Release with: build.bat Release)
set "CFG=Debug"
if not "%1"=="" set "CFG=%1"

:: build in "%~dp0..\build" (relative to this script, machine independent)
:: NOTE: VS generator builds ALL_BUILD by default; do NOT pass --target all (no all.vcxproj)
cmake --build "%~dp0..\build" --config %CFG%
exit /b %ERRORLEVEL%

:: locate VsDevCmd.bat via vswhere, fall back to default install path
:find_vsdev
set "VSDEV="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do (
        if exist "%%i\Common7\Tools\VsDevCmd.bat" set "VSDEV=%%i\Common7\Tools\VsDevCmd.bat"
    )
)
if not defined VSDEV (
    if exist "D:\tool\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" (
        set "VSDEV=D:\tool\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
    )
)
goto :eof
