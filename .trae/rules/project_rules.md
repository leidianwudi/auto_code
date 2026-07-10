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

**✅ 有效解决方案**：在每条命令前加 `; ` 前缀（分号+空格），即可绕过注入。分号在 PowerShell 中作为语句分隔符，将命令与注入文本隔离开：

```powershell
; <你的命令>
; cmake --version
; .trae\build.bat
; echo "Hello"
```

### ⏰ Trae 编辑会话超时断开

Trae AI 在长时间无操作（约 5 分钟）后，编辑会话会超时断开，导致 `Edit` 工具无法修改文件（`old_string` 匹配失败或写入无响应）。

**解决方案**：
1. **让 AI 一次性完成所有修改**：将需要改动的多个文件统一交给 AI 处理，避免分步操作中间等待。
2. **断开后重新读取文件再编辑**：如果编辑已断开，先用 `Read` 工具重新读取文件内容，确认当前状态后再尝试 `Edit`。
3. **优先使用 `replace_all` 批量替换**：对于多处相同模式的替换，使用 `replace_all: true` 参数一次完成，减少交互次数。
4. **保存文件后重试**：在 IDE 中手动保存文件（Ctrl+S）可刷新文件锁，然后重新发起编辑请求。