# 构建配置（默认 Debug；切 Release 用: .trae\build.ps1 Release）
param([string]$Config = "Debug")

# 清除所有 SAFE_RM_* 环境变量
Get-ChildItem Env:SAFE_RM_* | Remove-Item -ErrorAction SilentlyContinue

# 设置 VS 开发环境（如果尚未加载）
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    $vsDevCmd = "D:\tool\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
    if (-not (Test-Path $vsDevCmd)) {
        Write-Error "未找到 VsDevCmd.bat：$vsDevCmd"
        exit 1
    }
    # 让 VsDevCmd 仅在子进程中执行并打印其环境，再逐行写回当前进程。
    # 不能直接 `cmd /c "... && set"` 就完事，那样环境只存在于子进程，父进程仍找不到 cmake。
    $envLines = cmd /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 >nul 2>&1 && set"
    foreach ($line in $envLines) {
        if ($line -match '^([^=]+)=(.*)$') {
            # 环境变量名可能含空格（如 Program Files），拒绝这类无效名，避免污染
            if ($matches[1] -notmatch '\s') {
                Set-Item -Path "Env:$($matches[1])" -Value $matches[2]
            }
        }
    }
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        Write-Error "VsDevCmd 已执行，但 cmake 仍未加入 PATH，请检查 VsDevCmd 路径或 CMake 安装。"
        exit 1
    }
}

# 执行构建（$PSScriptRoot 为本脚本所在目录，..\build 即工程根下的 build，跨机器路径无关）
# 注意：VS 生成器不指定 --target 时默认构建全部目标（ALL_BUILD），不能用 --target all（VS 里不存在 all.vcxproj）
$buildDir = Join-Path $PSScriptRoot '..\build'
cmake --build $buildDir --config $Config
exit $LASTEXITCODE