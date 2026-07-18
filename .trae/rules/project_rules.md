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

**⚠️ 当 `cmd /c` 被 Trae 安全策略阻止时**，使用以下 PowerShell 命令手动加载 VS 环境并构建：

```powershell
; $msvcRoot = 'D:\tool\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.43.34808'; $sdkRoot = 'C:\Program Files (x86)\Windows Kits\10'; $sdkVer = '10.0.26100.0'; $env:INCLUDE = "$msvcRoot\include;$sdkRoot\Include\$sdkVer\ucrt;$sdkRoot\Include\$sdkVer\um;$sdkRoot\Include\$sdkVer\shared"; $env:LIB = "$msvcRoot\lib\x64;$sdkRoot\Lib\$sdkVer\ucrt\x64;$sdkRoot\Lib\$sdkVer\um\x64"; $cleanPath = ($env:PATH -split ';' | Where-Object { $_ -notmatch 'SAFE_RM' -and $_ -notmatch '^\s*$' -and (Test-Path $_ -ErrorAction SilentlyContinue) }) -join ';'; $env:PATH = $cleanPath; $env:CMAKE_PREFIX_PATH = 'D:/tool/Qt/6.12.0/msvc2022_64'; & 'D:/tool/Qt/Tools/CMake_64/bin/cmake.exe' --build d:\work\github\auto_code\build --config Debug --target auto_code
```

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

**为什么会频繁编辑失败？**

Trae 插件在 VS Code 中维护了一个内部编辑会话，该会话依赖以下组件协同工作：
- **文件句柄（File Handle）**：插件持有文件的读写锁，超时后句柄变为"僵尸"状态，既未释放也无法重新获取。
- **文件缓存（File Cache）**：插件内部缓存了文件内容快照，超时后缓存与实际磁盘内容不同步，导致 `old_string` 匹配始终失败。
- **状态机（State Machine）**：插件的编辑状态机在超时后进入"半断开"状态，`Read` 工具可能仍能读取，但 `Edit` 工具的状态未正确重置，导致写入无响应。

普通的"重新读取文件再编辑"只能刷新文件内容，但无法修复底层的文件句柄和状态机异常。因此，**重启 VS Code** 是唯一能彻底重置所有内部状态的可靠方式。

**解决方案**：
1. **让 AI 一次性完成所有修改**：将需要改动的多个文件统一交给 AI 处理，避免分步操作中间等待。
2. **断开后重新读取文件再编辑**：如果编辑刚断开不久，先用 `Read` 工具重新读取文件内容，确认当前状态后再尝试 `Edit`（适用于轻度超时）。
4. **保存文件后重试**：在 IDE 中手动保存文件（Ctrl+S）可刷新文件锁，然后重新发起编辑请求。
5. **重启 VS Code（终极方案）**：如果上述方法均无效（频繁出现 `old_string` 匹配失败或写入无响应），重启 VS Code 可以彻底重置 Trae 插件的文件句柄、缓存和状态机，是解决编辑失败的可靠手段。

### ⚠️ 编辑限制：单次修改 5-6 处

对同一个文件，`Edit` 工具一次只修改 **5-6 处**。一次性修改过多会导致 VS Code / Trae 插件卡死，后续修改全部失效。

- 超过 6 处修改：拆分到多次编辑中完成，编辑之间至少间隔一次 Read 确认文件状态。
- 有多处重复模式需要替换时：不使用 `replace_all: true` 参数一次完成，要分批次编辑。6