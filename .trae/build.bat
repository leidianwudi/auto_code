@echo off
set SAFE_RM_DENIED_PATH=
set SAFE_RM_PROTECTION_FLAG=
set SAFE_RM_AUTO_ADD_TEMP=
set SAFE_RM_ALLOWED_PATH=
"D:\tool\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build d:\work\github\auto_code\build --config Debug --target all --
exit /b %ERRORLEVEL%