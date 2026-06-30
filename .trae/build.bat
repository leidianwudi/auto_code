@echo off
setlocal

:: 清除所有可能干扰 cmake 的 SAFE_RM 环境变量
for /f "tokens=1* delims==" %%a in ('set SAFE_RM_ 2^>nul') do set "%%a="

:: 确保 VS 开发环境已加载（兼容 VS 2022）
if not defined VSCMD_VER (
    if exist "D:\tool\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" (
        call "D:\tool\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
    )
)

:: 执行构建
"D:\tool\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build d:\work\github\auto_code\build --config Debug --target all --
exit /b %ERRORLEVEL%