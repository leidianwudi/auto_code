/**
 * @file aui_style.h
 * @brief UI 公共样式工具类
 *
 * 集中管理全局颜色、样式表、风格适配等，供所有界面组件共用。
 */

#pragma once

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QMargins>
#include <QSize>
#include <QString>
#include <QTextBlockFormat>

#include "src/util/ui/setting_store.h"

class QLabel;
class QStyle;
class QTabBar;
class QToolButton;
class QMenu;

class AuiStyle {
public:
  // ════════════════════════════════════════════════════════════
  //  基础颜色常量（从 SettingStore 读取，支持主题与自定义）
  // ════════════════════════════════════════════════════════════

  /// 窗口背景色（状态栏、边框、对话框等）
  static QColor background() { return SettingStore::ins().color(QStringLiteral("ui.background")); }

  /// 自定义标题栏背景色（比 background 深，形成层次感）
  static QColor titleBarBackground() {
    return SettingStore::ins().color(QStringLiteral("ui.titleBarBackground"));
  }

  /// 标题文本 / 图标画笔颜色
  static QColor textColor() { return SettingStore::ins().color(QStringLiteral("ui.textColor")); }

  /// 活动 tab 文字色：深色主题下提亮为接近白色，确保与 tab 背景有足够对比度
  static QColor activeTabTextColor() {
    QColor c = textColor();
    return (c.lightness() > 128) ? c.lighter(118) : c;
  }

  /// AC 程序图标颜色（蓝色）
  static QColor appIconColor() { return SettingStore::ins().color(QStringLiteral("ui.textColor")); }

  /// 应用程序简称（图标文字）
  static QString appName() { return QStringLiteral("AC"); }

  /// 按钮 / 控件 hover 背景色
  static QColor hoverBackground() {
    return SettingStore::ins().color(QStringLiteral("ui.hoverBackground"));
  }

  /// 次要文字（分组/分区标题等，比正文弱一档）
  static QColor secondaryTextColor() {
    return SettingStore::ins().color(QStringLiteral("ui.secondaryTextColor"));
  }

  /// 弱化文字（占位符、清空按钮等，最弱一档）
  static QColor mutedTextColor() {
    return SettingStore::ins().color(QStringLiteral("ui.mutedTextColor"));
  }

  /// 成功文字（状态提示用绿色）
  static QColor successTextColor() {
    return SettingStore::ins().color(QStringLiteral("ui.successTextColor"));
  }

  /// 非活跃面板的标签页文字颜色（灰色）
  static QColor inactiveTabColor() {
    return SettingStore::ins().color(QStringLiteral("ui.inactiveTabColor"));
  }

  /// 编译按钮颜色，绿色
  static QColor compileButtonColor() {
    return SettingStore::ins().color(QStringLiteral("ui.compileButtonColor"));
  }

  /// 文件修改标记颜色，红色
  static QColor modifiedColor() {
    return SettingStore::ins().color(QStringLiteral("ui.modifiedColor"));
  }

  /// 错误文本颜色（输出面板用），红色
  static QColor errorTextColor() {
    return SettingStore::ins().color(QStringLiteral("ui.errorTextColor"));
  }

  /// 通用边框颜色（浅灰），对应 #c8c8c8
  static QColor borderColor() {
    return SettingStore::ins().color(QStringLiteral("ui.borderColor"));
  }

  /// 深边框颜色（中灰），对应 #999 / #999999
  static QColor borderDarkColor() {
    return SettingStore::ins().color(QStringLiteral("ui.borderDarkColor"));
  }

  /// 面板背景色（白色），对应 #ffffff
  static QColor panelBackground() {
    return SettingStore::ins().color(QStringLiteral("ui.panelBackground"));
  }

  /// 列表交替行背景色（浅灰）
  static QColor listAlternateBackground() {
    return SettingStore::ins().color(QStringLiteral("ui.listAlternateBackground"));
  }

  /// 列表项悬停背景色（浅蓝）
  static QColor listHoverBackground() {
    return SettingStore::ins().color(QStringLiteral("ui.listHoverBackground"));
  }

  /// 列表项选中背景色（浅蓝）
  static QColor listSelectionBackground() {
    return SettingStore::ins().color(QStringLiteral("ui.listSelectionBackground"));
  }

  /// Tab 未选中背景色，对应 #e8e8e8
  static QColor tabUnselectedBackground() {
    return SettingStore::ins().color(QStringLiteral("ui.tabUnselectedBackground"));
  }

  /// Tab hover 背景色，对应 #dcdcdc
  static QColor tabHoverBackground() {
    return SettingStore::ins().color(QStringLiteral("ui.tabHoverBackground"));
  }

  /// 错误提示弹窗背景色（淡黄），对应 #ffffcc
  static QColor errorToolTipBackground() {
    return SettingStore::ins().color(QStringLiteral("ui.errorToolTipBackground"));
  }

  /// 保存全部按钮背景层颜色（灰色），与 inactiveTabColor 值相同
  static QColor saveAllButtonBgColor() { return inactiveTabColor(); }

  /// 模态对话框遮罩颜色（半透明黑色，覆盖父窗口形成"变暗"效果）
  static QColor modalOverlayColor() { return QColor(0, 0, 0, 80); }

  /// 图标按钮 pressed 半透明背景色（主题无关，保持半透明）
  static QColor iconButtonPressedBg() { return QColor(128, 128, 128, 80); }

  // ════════════════════════════════════════════════════════════
  //  编辑器 / 输出面板字体
  // ════════════════════════════════════════════════════════════

  /// 编辑器默认字体 family (等宽编程字体)
  static QString editorFontFamily() { return QStringLiteral("Consolas"); }
  /// 编辑器默认字体大小（磅值）
  static int editorFontSize() { return 11; }
  /// 创建编辑器默认字体对象
  static QFont createEditorFont() {
    QFont font;
    font.setFamily(editorFontFamily());
    font.setFixedPitch(true);
    font.setPointSize(editorFontSize());
    return font;
  }
  /// 创建日志面板字体对象 — 字号与编辑器相同，字间距归零
  static QFont createLogFont() {
    QFont font = createEditorFont();
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.0);  // 列间距归零
    font.setPointSize(editorFontSize());
    return font;
  }

  /// 创建日志面板行块格式 — 行间隔为 0（只使用字体本身高度，无额外间距）
  static QTextBlockFormat createLogBlockFormat(const QFont &font) {
    QFontMetrics fm(font);
    QTextBlockFormat fmt;
    // 使用字体本身高度（ascent + descent），不加任何额外间距
    fmt.setLineHeight(fm.height(), QTextBlockFormat::FixedHeight);
    return fmt;
  }

  /// 日志面板完整样式表（背景色、文字色、边框色，统一由 AuiStyle 管理）
  static QString logPanelStyleSheet() {
    return QStringLiteral(
               "QPlainTextEdit { background: %1; color: %2; "
               "border: 1px solid %3; padding: 0px; }")
        .arg(panelBackground().name(), textColor().name(), borderColor().name());
  }

  // ════════════════════════════════════════════════════════════
  //  编辑器颜色
  // ════════════════════════════════════════════════════════════

  /// 行号区域背景色（浅灰）
  static QColor lineNumberBackground() {
    return SettingStore::ins().color(QStringLiteral("editor.lineNumberBackground"));
  }

  /// 行号文字颜色
  static QColor lineNumberTextColor() {
    return SettingStore::ins().color(QStringLiteral("editor.lineNumberText"));
  }

  /// 当前行高亮背景色（浅蓝，类似 VS Code 浅色主题）
  static QColor currentLineBackground() {
    return SettingStore::ins().color(QStringLiteral("editor.currentLineBackground"));
  }

  /// 括号匹配默认背景色（青色）
  static QColor bracketMatchColor() {
    return SettingStore::ins().color(QStringLiteral("editor.bracketMatch"));
  }

  /// 圆括号 () 匹配高亮色（珊瑚红）
  static QColor bracketParenColor() {
    return SettingStore::ins().color(QStringLiteral("editor.bracketParen"));
  }

  /// 方括号 [] 匹配高亮色（海绿色）
  static QColor bracketSquareColor() {
    return SettingStore::ins().color(QStringLiteral("editor.bracketSquare"));
  }

  /// 花括号 {} 匹配高亮色（皇家蓝）
  static QColor bracketBraceColor() {
    return SettingStore::ins().color(QStringLiteral("editor.bracketBrace"));
  }

  /// 括号不匹配警告色（红色）
  static QColor bracketMismatchColor() {
    return SettingStore::ins().color(QStringLiteral("editor.bracketMismatch"));
  }

  /// 错误波浪线颜色（红色）
  static QColor errorUnderlineColor() {
    return SettingStore::ins().color(QStringLiteral("editor.errorUnderline"));
  }

  /// 错误行背景色（浅红，类似 VS Code 的 #f2dede）
  static QColor errorLineBackground() {
    return SettingStore::ins().color(QStringLiteral("editor.errorLineBackground"));
  }

  /// 代码警告文字色（浅色橙黄 / 深色黄）
  static QColor warningColor() {
    return SettingStore::ins().color(QStringLiteral("editor.warning"));
  }

  /// 缩进参考线颜色（浅灰，类似 VS Code 的 indent guide）
  static QColor indentGuideColor() {
    return SettingStore::ins().color(QStringLiteral("editor.indentGuide"));
  }

  /// 缩进参考线高亮颜色（当前行所在层级，类似 VS Code 的 active indent guide）
  static QColor indentGuideActiveColor() {
    return SettingStore::ins().color(QStringLiteral("editor.indentGuideActive"));
  }

  /// 查找匹配高亮背景色（所有匹配项，浅橙色）
  static QColor findMatchBackground() {
    return SettingStore::ins().color(QStringLiteral("editor.findMatchBackground"));
  }

  /// 查找当前匹配高亮背景色（当前选中项，深橙色）
  static QColor findCurrentMatchBackground() {
    return SettingStore::ins().color(QStringLiteral("editor.findCurrentMatchBackground"));
  }

  /// 根据括号字符返回对应的高亮颜色
  static QColor bracketColorForChar(QChar ch) {
    switch (ch.toLatin1()) {
      case '(':
      case ')':
        return bracketParenColor();
      case '[':
      case ']':
        return bracketSquareColor();
      case '{':
      case '}':
        return bracketBraceColor();
      default:
        return bracketMatchColor();
    }
  }

  // ════════════════════════════════════════════════════════════
  //  布局尺寸常量
  // ════════════════════════════════════════════════════════════

  /// 标题栏按钮固定尺寸
  static QSize titleBarButtonSize() { return QSize(36, 26); }

  /// 标题栏布局边距（左右6px内间距，上下各2px增加高度）
  static QMargins titleBarMargins() { return QMargins(6, 2, 6, 2); }

  /// 标题栏布局间距
  static int titleBarSpacing() { return 4; }

  /// 对话框字体大小（px）
  static int dialogFontSize() { return 13; }

  /// 标题栏标题字体大小（px）
  static int titleFontSize() { return 12; }

  /// QSizeGrip 固定尺寸
  static QSize sizeGripSize() { return QSize(16, 16); }

  // ════════════════════════════════════════════════════════════
  //  全局样式表
  // ════════════════════════════════════════════════════════════
  /// @brief 主窗口全局样式表
  static QString mainStyleSheet();

  /// @brief 对话框专用样式表（QDialog + QLabel + QLineEdit + QPushButton）
  static QString dialogStyleSheet();

  /// @brief 标题栏菜单按钮（QToolButton）专用样式表，直接设到按钮上
  static QString menuButtonStyleSheet();

  /// @brief 弹出菜单（QMenu）专用样式表，直接设到菜单上
  static QString menuStyleSheet();

  /// 应用「标题栏菜单按钮」的样式表 + 调色板（双保险，Fusion 风格文字走 ButtonText 角色）
  static void applyMenuButtonStyle(QToolButton *btn);

  /// 应用「弹出菜单」的样式表 + 调色板（Fusion 风格菜单文字走 Text/WindowText 角色）
  static void applyMenuStyle(QMenu *menu);

  // ════════════════════════════════════════════════════════════
  //  风格适配（跨平台兼容）
  // ════════════════════════════════════════════════════════════
  /// 确保 QTabBar 使用 Fusion 风格（Windows 原生风格忽略 setTabTextColor）
  static void ensureFusionTabBar(QTabBar *bar);

  /// QTabBar 样式表（简洁版：仅区分文字颜色，未选中灰 / 选中深）
  static QString tabBarStyleSheet();

  /// 设置 tab 头上下左右空白（像素）：用代理样式覆盖 PM_TabBarTabHSpace/VSpace
  /// 注：HSpace 取 left+right、VSpace 取 top+bottom（Qt 按每边 HSpace/2 计算）
  static void applyTabBarPadding(QTabBar *bar, int left, int top, int right, int bottom);

  /// 编辑器标签页：安装「可聚焦」代理样式（四边空白 + 按聚焦状态自绘文字颜色）
  static void applyFocusTabBar(QTabBar *bar, int left, int top, int right, int bottom);

  /// 设置标签栏聚焦状态并刷新：active=true 时当前标签用文字色、其余灰；
  /// active=false 时整体压暗为灰色（用于拆分面板区分聚焦/非聚焦）
  static void setTabFocusState(QTabBar *bar, bool active);

  /// 补全弹出列表样式表
  static QString popupListStyleSheet();

  /// 错误提示弹窗样式表
  static QString errorToolTipStyleSheet();

  /// 设置标题栏标签的样式（文字颜色、字号、透明背景）
  static void applyTitleLabelStyle(QLabel *label);

  /// 设置标题栏容器的样式（背景色、负外边距覆盖窗口边框）
  static void applyTitleBarStyle(QWidget *titleBar);

  /// 创建应用全局风格（Fusion 基础风格 + 程序化复选框/单选框指示器代理）。
  /// 指示器用 QPainter 直接绘制，绕开 Qt QSS 的 image 属性对图片支持不稳定的问题。
  static QStyle *createAppStyle();

private:
  AuiStyle() = delete;
};