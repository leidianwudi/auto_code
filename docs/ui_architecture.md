# Auto Code 项目 UI 架构与样式规范

## 1. 技术栈

- **Qt 6.12.0** (msvc2022_64)
- **构建系统**：CMake + Ninja
- **编译器**：MSVC 2022
- **代码缩进**：2 空格（`.editorconfig`）
- **注释规范**：Doxygen 风格，注释内容用中文

## 2. UI 架构总览

```
src/util/ui/
├── aui_window.h/.cpp          # 无边框窗口工具类（标题栏、边框、拖拽）
├── aui_message_box.h/.cpp     # 自定义消息框
├── rename_dialog.cpp          # 重命名对话框
└── component/
    ├── aui_style.h/.cpp       # 全局颜色、字体、样式表
    ├── aui_button.h/.cpp      # 按钮工厂方法 + 样式
    ├── aui_icon.h/.cpp        # 图标生成
    └── aui_combo.h/.cpp       # 无边框下拉框
```

### 2.1 核心工具类

#### AuiStyle（[aui_style.h](file:///d:/work/github/auto_code/src/util/ui/component/aui_style.h)）
集中管理所有颜色常量、字体、样式表。**所有颜色必须从此类获取，禁止硬编码**。

| 方法 | 用途 | 颜色值 |
|------|------|--------|
| `background()` | 窗口背景 | #e8e8e8 |
| `titleBarBackground()` | 标题栏背景 | #d9d9d9 |
| `textColor()` | 文字颜色 | #333333 |
| `hoverBackground()` | hover 高亮 | #c0c0c0 |
| `borderColor()` | 浅边框 | #b0b0b0 |
| `borderDarkColor()` | 深边框 | #999999 |
| `panelBackground()` | 面板背景 | #ffffff |
| `inactiveTabColor()` | 非活跃标签 | #888888 |
| `modifiedColor()` | 修改标记 | #cc3333 |
| `errorTextColor()` | 错误文字 | #f44747 |

#### AuiButton（[aui_button.h](file:///d:/work/github/auto_code/src/util/ui/component/aui_button.h)）
按钮样式工厂。**所有对话框按钮必须使用此类的方法**。

| 方法 | 用途 |
|------|------|
| `applyDialogButtonStyle(btn)` | 对话框按钮样式（带边框 + hover/pressed） |
| `applyCommonStyle(btn)` | 标题栏按钮样式（透明 + hover） |
| `applyIconButtonStyle(btn)` | 图标按钮样式（透明 + hover 半透明） |
| `createDialogButtons(parent, showCancel)` | 创建对话框按钮行（居中布局） |
| `dialogButtonStyleSheet()` | 按钮样式表字符串 |

#### AuiWindow（[aui_window.h](file:///d:/work/github/auto_code/src/util/ui/aui_window.h)）
无边框窗口工具类。

| 方法 | 用途 |
|------|------|
| `setupFramelessWindow(window)` | 主窗口无边框设置 |
| `setupFramelessDialog(dialog)` | 对话框无边框设置 |
| `createTitleBar(window, options)` | 创建标题栏（图标 + 标题 + 控制按钮） |
| `applyWindowFrame(window, titleBar, content)` | 应用窗口外框（1px 边框） |
| `installModalOverlay(dialog)` | 模态遮罩 |
| `enableWin32Resize(window)` | Win32 拉伸边框 |

## 3. 对话框开发规范

### 3.1 标准对话框结构

所有对话框必须遵循以下结构：

```cpp
#include "src/util/ui/aui_window.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"

void MyDialog::setupUi() {
  // 1. 无边框对话框设置
  AuiWindow::setupFramelessDialog(this);

  // 2. 创建内容控件
  auto *contentWidget = new QWidget;
  auto *contentLayout = new QVBoxLayout(contentWidget);
  // ... 添加内容 ...

  // 3. 创建标题栏
  TitleBarOptions tb;
  tb.title = QStringLiteral("对话框标题");
  tb.closeRejectsDialog = true;
  TitleBarResult titleBar = AuiWindow::createTitleBar(this, tb);

  // 4. 应用窗口框架
  AuiWindow::applyWindowFrame(this, titleBar.titleBar, contentWidget);
}
```

### 3.2 底部按钮规范

**所有对话框的底部按钮必须使用 `AuiButton::applyDialogButtonStyle`**，按钮之间间隔 8px：

```cpp
auto *btnLayout = new QHBoxLayout;
btnLayout->addStretch();

auto *okBtn = new QPushButton(QStringLiteral("确定"));
auto *cancelBtn = new QPushButton(QStringLiteral("取消"));
okBtn->setMinimumWidth(80);
cancelBtn->setMinimumWidth(80);
AuiButton::applyDialogButtonStyle(okBtn);
AuiButton::applyDialogButtonStyle(cancelBtn);

btnLayout->addWidget(okBtn);
btnLayout->addSpacing(8);
btnLayout->addWidget(cancelBtn);
```

### 3.3 布局间距规范

| 场景 | 值 |
|------|------|
| 对话框外边距（margins） | `(8, 4, 8, 8)` |
| 对话框内容内边距 | `(8, 8, 8, 8)` |
| 控件间垂直间距（spacing） | `4-8px` |
| 按钮间水平间距 | `8px` |
| 按钮最小宽度 | `80px` |

### 3.4 字体规范

| 场景 | 字体 | 大小 |
|------|------|------|
| 编辑器 | Consolas（等宽） | 11pt |
| 对话框 | 系统默认 | 13px |
| 标题栏 | 系统默认 | 12px |
| 日志面板 | Consolas | 11pt |

## 4. Tab 标签规范

### 4.1 Windows 平台 Tab 文字颜色

Windows 原生 Qt 风格（`QWindowsVistaStyle`/`QWindows11Style`）通过 `DrawThemeText` 绘制 tab 文字，**完全忽略** `setTabTextColor`、QSS、QPalette 等颜色设置。

**解决方案**：对需要自定义 tab 文字颜色的 QTabBar，使用 Fusion 风格：

```cpp
QStyle *fs = QStyleFactory::create("Fusion");
if (fs) {
  fs->setParent(tabBar());
  tabBar()->setStyle(fs);
}
```

或使用 `AuiStyle::ensureFusionTabBar(bar)`。

### 4.2 Tab 样式表

使用 `AuiStyle::tabBarStyleSheet()` 获取统一 tab 样式。

## 5. 图标选择器规范

### 5.1 图标来源

图标列表来自 ERP 管理后台 `src/components/icon_picker/src/data/` 下的三个文件：
- `icons.ep.ts` — Element Plus 图标库
- `icons.ant-design.ts` — Ant Design 图标库
- `icons.tdesign.ts` — TDesign 图标库

所有图标带 `vi-` 前缀（如 `vi-ep:add-location`），通过 Iconify API 加载 SVG。

### 5.2 IconLoader 单例

- 全局缓存，避免重复请求
- 支持系统代理：`QNetworkProxyFactory::setUseSystemConfiguration(true)`
- 8 秒超时
- 占位图标不缓存
- 多图标源 URL fallback（iconify.design → simplesvg.com）

## 6. QSS 样式规范

### 6.1 颜色使用

**禁止硬编码颜色值**。必须使用 `AuiStyle::xxxColor()` 获取颜色，然后在 QSS 中用 `%1` 占位：

```cpp
// 正确
QString style = QStringLiteral("QPushButton { background: %1; border: 1px solid %2; }")
    .arg(AuiStyle::background().name(), AuiStyle::borderColor().name());

// 错误 - 硬编码
QString style = "QPushButton { background: #e8e8e8; border: 1px solid #b0b0b0; }";
```

### 6.2 全局样式表

- 主窗口：`AuiStyle::mainStyleSheet()`
- 对话框：`AuiStyle::dialogStyleSheet()`
- Tab 标签：`AuiStyle::tabBarStyleSheet()`
- 弹出列表：`AuiStyle::popupListStyleSheet()`

## 7. 模态对话框规范

### 7.1 窗口标志

QDialog 使用 `exec()` 时**必须保留原生标题栏**，不能使用 `FramelessWindowHint`。使用 `AuiWindow::setupDialogStyle()` 代替 `setupFramelessDialog()`。

### 7.2 模态遮罩

模态对话框显示前调用 `installModalOverlay()`，关闭时调用 `removeModalOverlay()`。

## 8. 常见陷阱

### 8.1 QJsonValue::toInt() 溢出

Qt 6 中 `QJsonValue::toInt()` 对超出 INT_MAX 的值触发断言。使用 `safeJsonToInt()` 或 `toDouble()` 替代。

### 8.2 QString::SkipEmptyParts 命名空间

Qt 6 中为 `Qt::SkipEmptyParts`，非 `QString::SkipEmptyParts`。

### 8.3 QStyledItemDelegate::paint 断言

自定义 delegate 的 `paint()` 中不要传 `QModelIndex()` 给基类 `paint()`，会触发 `index.isValid()` 断言。使用 `QStyle::drawControl` 直接绘制。

### 8.4 编辑限制

对同一个文件，`Edit` 工具一次只修改 5-6 处。超过 6 处需拆分到多次编辑。
