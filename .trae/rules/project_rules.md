***

## alwaysApply: true

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

### 引擎依赖边界（QtCore 允许，GUI/QJson 禁止新增）

引擎层 `src/engine`（及值类型 `src/core/json`）**允许**使用 QtCore 类型
（QString / QStringList / QVector / QHash 等），**禁止**以下依赖：

- **QtGui / QtWidgets**：引擎不得 include 任何 GUI 头（`<QtGui>`、`<QtWidgets>`、
  `QWidget` 等）。GUI 依赖只存在于 `src/ui`。当前唯一例外是 `ac_debugger.h`
  的 `Q_OBJECT`（MOC 宏，非 GUI 库），不得新增同类。

- **QJsonValue / QJsonArray / QJsonObject**：引擎内部一律使用 `accore::AcJsonValue`
  传递数据。QJson 只允许出现在两处边界：

  1. `ac_json_value.h` 的 `toQJsonValue()` / `fromQJsonValue()` 适配层（UI 消费用）；
  2. `function/fun_json.cpp`（脚本语言的内置 JSON 函数模块，其签名固定为 QJsonValue）。
     新增引擎文件不得直接 include `<QJsonValue>` 等。

**判断标准**：新增引擎代码若能用 AcJsonValue / QString 完成，就不引入新的 Qt 类型；
不改动既有已解耦的核心（AcJsonValue 的 ArrData/ObjData 存储与 parse/serialize）。

### 🚫 AI 不运行 .exe 进行 GUI/交互测试

AI 不会主动运行项目编译出的 `.exe` 文件进行功能测试，因为 AI 无法进行鼠标点击、按钮等 GUI 交互。所有功能测试由开发者手动完成。AI 仅在代码层面支持：编译验证（build），但不运行程序验证。
