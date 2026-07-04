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

:: 执行构建
cmake --build d:\work\github\auto_code\build --config Debug --target all --
exit /b %ERRORLEVEL%