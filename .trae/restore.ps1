# Restore files deleted by SAFE_RM corruption
$env:SAFE_RM_ALLOWED_PATH = ''
$env:SAFE_RM_DENIED_PATH = ''
$env:SAFE_RM_AUTO_ADD_TEMP = ''
$env:SAFE_RM_PROTECTION_FLAG = ''
$env:PATH = [Environment]::GetEnvironmentVariable('PATH', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('PATH', 'User')
# Add common paths
$env:PATH += ";$env:SystemRoot\System32;$env:SystemRoot"

git -C D:\work\github\auto_code checkout HEAD -- main_window.h main_window.cpp main_window.ui
git -C D:\work\github\auto_code checkout HEAD -- src/ui/main_dev/main_dev_ui.h src/ui/main_dev/main_dev_mgr.cpp
git -C D:\work\github\auto_code checkout HEAD -- src/ui/main_dev/main_dev_ui.cpp src/ui/main_dev/main_dev_model.cpp
git -C D:\work\github\auto_code checkout HEAD -- src/ui/demo/demo_ui.cpp src/ui/demo/demo_mgr.cpp
git -C D:\work\github\auto_code checkout HEAD -- CMakeLists.txt
Write-Host "Restore completed"