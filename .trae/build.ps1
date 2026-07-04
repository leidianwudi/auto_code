# 构建脚本 - 清除 SAFE_RM_* 环境变量后运行 cmake
# 这些环境变量由 Trae IDE 注入，包含 ; 分隔符，会被 PowerShell 误解析为多条命令

# 清除所有 SAFE_RM_* 环境变量
Get-ChildItem env:SAFE_RM_* -ErrorAction SilentlyContinue | ForEach-Object {
  Remove-Item "env:$($_.Name)" -Force -ErrorAction SilentlyContinue
}

# 执行 cmake 构建
$cmake = "D:\tool\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake --build d:\work\github\auto_code\build --config Debug --target all --
exit $LASTEXITCODE