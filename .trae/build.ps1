# 清除所有 SAFE_RM 环境变量
Get-ChildItem env:SAFE_RM_* | ForEach-Object { Remove-Item "env:$($_.Name)" -Force -ErrorAction SilentlyContinue }

# 执行构建
& "D:\tool\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build d:/work/github/auto_code/build --config Debug --target all --