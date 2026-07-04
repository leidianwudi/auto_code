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

### QJsonValue::toString() 不会转换数值（Qt 6 行为）

在 Qt 6 中，`QJsonValue::toString()` **不会**将数值类型自动转为字符串，而是返回空 `QString`。
这与 Qt 5 的行为不同（Qt 5 中 `QJsonValue(10).toString()` 返回 `"10"`）。

**错误写法（依赖隐式转换）**：
```cpp
QJsonValue l = QJsonValue("count: ");  // String 类型
QJsonValue r = QJsonValue(10.0);       // Double 类型
return QJsonValue(l.toString() + r.toString()); // Qt 6 中结果是 "count: "，不是 "count: 10"！
```

**正确写法（显式转换）**：
```cpp
QString ls = l.isString() ? l.toString() : QString::number(l.toDouble());
QString rs = r.isString() ? r.toString() : QString::number(r.toDouble());
return QJsonValue(ls + rs);
```

### 项目架构

- 构建系统：CMake + Ninja
- 编译器：MSVC 2022
- Qt 版本：6.12.0
- 代码缩进：2 空格（`.editorconfig`）
- 注释规范：使用 Doxygen 风格，**注释内容用中文**

### 构建方法

如果 Trae IDE 直接运行 cmake 时报 `SAFE_RM_*` 环境变量格式错误，请改用项目自带的构建脚本：

```powershell
# PowerShell 中运行
cmd.exe /c .trae\build.bat
```

该脚本会自动清理 `SAFE_RM_*` 环境变量并加载 VS 开发环境。

### 🔒 SAFE_RM_* 环境变量问题

Trae IDE 默认启用了安全删除保护（safe-rm），会在终端注入 `SAFE_RM_*` 环境变量。但这些变量使用 bash 语法格式（`KEY=VALUE`），与 PowerShell 不兼容，导致**所有终端命令都无法执行**。

**解决办法**：在项目根目录创建 `.vscode/settings.json` 或 `.trae/settings.json`，添加以下配置：

```json
{
  "trae.safe-rm.enabled": false,
  "terminal.integrated.env.windows": {
    "SAFE_RM_ALLOWED_PATH": null,
    "SAFE_RM_DENIED_PATH": null,
    "SAFE_RM_AUTO_ADD_TEMP": null,
    "SAFE_RM_PROTECTION_FLAG": null
  }
}
```

如果上述配置无效，请在 Trae IDE 的 **设置 → 扩展 → safe-rm** 中手动关闭，或通过 **命令面板（Ctrl+Shift+P）→ Preferences: Open User Settings** 搜索 `safe-rm` 禁用。