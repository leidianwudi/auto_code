@echo off
setlocal

:: 清除所有 SAFE_RM_* 环境变量
for /f "tokens=1* delims==" %%a in ('set SAFE_RM_ 2^>nul') do set "%%a="

:: 设置 VS 开发环境（如果尚未加载）
:: 优先用 vswhere 自动定位本机 VS 安装路径（各机器安装目录可能不同，如 D:\tool\unityzy），
:: 找不到时回退到默认路径 D:\tool\Microsoft Visual Studio\2022\Community
if defined VSCMD_VER goto :env_ok
call :find_vsdev
if defined VSDEV call "%VSDEV%" -arch=x64 -host_arch=x64 >nul 2>&1
:env_ok

:: 构建配置（默认 Debug；切 Release 用: build.bat Release）
set "CFG=Debug"
if not "%1"=="" set "CFG=%1"

:: 执行构建（%~dp0 为本脚本所在目录，..\build 即工程根下的 build，跨机器路径无关）
:: 注意：VS 生成器不指定 --target 时默认构建全部目标（ALL_BUILD），不能用 --target all（VS 里不存在 all.vcxproj）
cmake --build "%~dp0..\build" --config %CFG%
exit /b %ERRORLEVEL%

:: ── 通过 vswhere 定位 VsDevCmd.bat，定位不到则回退到默认安装路径 ──
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