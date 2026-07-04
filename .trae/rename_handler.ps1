# 清除所有 SAFE_RM 环境变量
Get-ChildItem env:SAFE_RM_* -ErrorAction SilentlyContinue | ForEach-Object {
  Remove-Item "env:$($_.Name)" -Force -ErrorAction SilentlyContinue
}

# 重命名文件夹
Rename-Item -Path "d:\work\github\auto_code\src\engine\handler" -NewName "tpl_hand" -ErrorAction Stop
Write-Host "Rename OK"