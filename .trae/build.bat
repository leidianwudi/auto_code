@echo off
setlocal

:: 清除所有 SAFE_RM_* 环境变量
for /f "tokens=1* delims==" %%a in ('set SAFE_RM_ 2^>nul') do set "%%a="

:: 设置 VS 开发环境（如果尚未加载）
if not defined VSCMD_VER (
    if exist "D:\tool\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" (
        call "D:\tool\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul 2>&1
    )
)

:: 构建配置（默认 Debug；切 Release 用: build.bat Release）
set "CFG=Debug"
if not "%1"=="" set "CFG=%1"

:: 执行构建（%~dp0 为本脚本所在目录，..\build 即工程根下的 build，跨机器路径无关）
:: 注意：VS 生成器不指定 --target 时默认构建全部目标（ALL_BUILD），不能用 --target all（VS 里不存在 all.vcxproj）
cmake --build "%~dp0..\build" --config %CFG%
exit /b %ERRORLEVEL%