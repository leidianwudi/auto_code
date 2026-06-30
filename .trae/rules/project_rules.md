# Project Rules

## Qt 开发注意事项

### Tab 文字颜色设置（Windows 平台）

Windows 原生 Qt 风格（`QWindowsVistaStyle`/`QWindows11Style`）通过 `DrawThemeText` Win32 API 绘制 QTabBar 的 tab 文字，**完全忽略**以下 Qt API 的颜色设置：

- `QTabBar::setTabTextColor()`
- `QTabBar::setStyleSheet("QTabBar::tab { color: ... }")`
- `QPalette` 颜色修改
- `QProxyStyle` 拦截 `CE_TabBarTab`

**解决方案**：对需要自定义 tab 文字颜色的 QTabBar，使用 Qt 自带的 `Fusion` 风格替代原生风格：

```cpp
QStyle *fs = QStyleFactory::create("Fusion");
if (fs) {
  fs->setParent(tabBar());
  tabBar()->setStyle(fs);
}
```

Fusion 风格使用 `QPainter` 自绘所有控件，100% 遵循 `setTabTextColor`。

### 项目架构

- 构建系统：CMake + Ninja
- 编译器：MSVC 2022
- Qt 版本：6.12.0
- 代码缩进：2 空格（`.editorconfig`）
- 注释规范：Doxygen

### 构建方法

如果 Trae IDE 直接运行 cmake 时报 `SAFE_RM_*` 环境变量格式错误，请改用项目自带的构建脚本：

```powershell
# PowerShell 中运行
cmd.exe /c .trae\build.bat
```

该脚本会自动清理 `SAFE_RM_*` 环境变量并加载 VS 开发环境。