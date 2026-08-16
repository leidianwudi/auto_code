# 构建配置（默认 Debug；切 Release 用: .trae\build.ps1 Release）
param([string]$Config = "Debug")

# 清除所有 SAFE_RM_* 环境变量
Get-ChildItem Env:SAFE_RM_* | Remove-Item -ErrorAction SilentlyContinue

# 设置 VS 开发环境（如果尚未加载）
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    $vsDevCmd = "D:\tool\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
    if (Test-Path $vsDevCmd) {
        cmd /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 >nul 2>&1 && set"
    }
}

# 执行构建（$PSScriptRoot 为本脚本所在目录，..\build 即工程根下的 build，跨机器路径无关）
# 注意：VS 生成器不指定 --target 时默认构建全部目标（ALL_BUILD），不能用 --target all（VS 里不存在 all.vcxproj）
$buildDir = Join-Path $PSScriptRoot '..\build'
cmake --build $buildDir --config $Config
exit $LASTEXITCODE