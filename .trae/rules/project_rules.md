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

### 表格 cellWidget 选中态/只读单元格可复制（反复踩坑总结）

QSS 环境下，**不要**用以下方式给表格做"选中变色"，项目已多轮验证全部不可靠：

- `QTableWidget::item:selected { background: ... }`（全局 QSS 生效后 item 选中背景不绘制）
- 子控件 `:focus { background-color: ... }`（QLineEdit 等聚焦伪状态背景不绘制）
- `QTableWidgetItem::setFlags(ItemIsSelectable)` 单独使用（item 文本能被选中，但无可见高亮）

**纯 item 单元格**（无 cellWidget，如白名单注释列）：由 `CenterCheckDelegate::paint`
在 `drawControl` **之前**显式 `fillRect(rect, AuiStyle::listSelectionBackground())`
铺选中底色（绘制层确定生效；先铺底色再交样式画勾选框/焦点框，互不遮挡）。
`makeConfigTable` 已统一装该委托，全项目配置表格自动生效——不要改用 QSS。

**挂 cellWidget 的只读文本单元格**：单元格归控件绘制，委托的 fillRect 会被
widget 覆盖，必须走下面的两态方案（表格原生绘制高亮）。

**唯一可靠方案**（已在 `AuiCellEditCombo` 中验证，参考实现）：cellWidget 两态 + 让表格自己画选中高亮：

1. **非编辑态**：内嵌输入框 `Qt::WA_TransparentForMouseEvents`（鼠标穿透）
   → 点击落回容器 → 容器 `setCurrentCell(row, col)` → **表格原生绘制整格选中蓝**
   → 文字**始终**由控件显示（与编辑态同源，双击不位移）；item 文字由
   `CenterCheckDelegate` 跳过绘制（**避免 widget 与 item 双画文字导致重影**）
2. **双击进入编辑态**：取消穿透 + `setFocus()` + `selectAll()` + 容器 `paintEvent` 画白底
   （`panelBackground()` 盖住行高亮）→ 拖选/双击选词/Ctrl+A/Ctrl+C 与普通输入框一致
3. **退出**：Esc / 回车 / FocusOut → 恢复穿透 + 重绘（文字位置不变）

**不要设置 `font-size`**（QSS 会覆盖表格字体，导致"点击后字变小"）；不要加 `border`
（会多出一圈边框）。

**文字位置必须同源**：单元格挂常驻控件时，文字统一由控件显示（两态同一来源），
`CenterCheckDelegate` 检测到 `cellWidget` 即跳过 item 文字绘制。可编辑列由委托
自绘 item 文字（左缘 +4px = 输入框 QSS `padding: 0px 2px` + QLineEdit 内部 2px）
并用 `createEditor` 返回无边框、`padding: 0px 2px` 的 QLineEdit——两态同一基准，
双击进入编辑态文字不位移。不要靠调 QSS `item { padding }` 去对齐"item 文字 ↔
编辑器文字"——QSS/原生样式差异会让 2px 级的偏移对不齐，表现为双击时文字横跳，
或 item 文字与控件文字双层错位（重影）。

**通用控件**：只读文本列直接用 `AuiCellReadBox`（`src/util/ui/component/`），
可编辑下拉列用 `AuiCellEditCombo`，**不要为同类需求另造控件**。
行交换（`takeItem` 搬条目）时 cellWidget 固定在行位，需按条目文本重新 `setText`。

### 图标不要用 emoji（颜色不可控）

emoji（🔗 🌐 ⚙ 等）的颜色由系统 emoji 字体决定，**无法**用 `setForeground` /
QSS `color` 修改——同类标记想统一颜色时必然出现色差。需要彩色/同色小图标时用
`QPainter` 自绘 `QPixmap` + `QIcon`（参考 `viewBadgeIcon()`，12px、2x 采样、
`setDevicePixelRatio(2.0)`），统一用同一颜色绘制。

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
