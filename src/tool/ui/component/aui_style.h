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

class QLabel;
class QTabBar;

class AuiStyle {
public:
  // ════════════════════════════════════════════════════════════
  //  基础颜色常量
  // ════════════════════════════════════════════════════════════

  /// 窗口背景色（状态栏、边框、对话框等）
  static QColor background() { return QColor(0xe8, 0xe8, 0xe8); }

  /// 自定义标题栏背景色（比 background 深，形成层次感）
  static QColor titleBarBackground() { return QColor(0xd9, 0xd9, 0xd9); }

  /// 标题文本 / 图标画笔颜色
  static QColor textColor() { return QColor(0x33, 0x33, 0x33); }

  /// AC 程序图标颜色（蓝色）
  static QColor appIconColor() { return QColor(0x33, 0x33, 0x33); }

  /// 按钮 / 控件 hover 背景色
  static QColor hoverBackground() { return QColor(0xc0, 0xc0, 0xc0); }

  /// 非活跃面板的标签页文字颜色（灰色）
  static QColor inactiveTabColor() { return QColor(0x88, 0x88, 0x88); }

  /// 编译按钮颜色，绿色
  static QColor compileButtonColor() { return QColor(0x44, 0x99, 0x00); }

  /// 文件修改标记颜色，红色
  static QColor modifiedColor() { return QColor(0xcc, 0x33, 0x33); }

  /// 错误文本颜色（输出面板用），红色
  static QColor errorTextColor() { return QColor(0xf4, 0x47, 0x47); }

  /// 通用边框颜色（浅灰），对应 #c8c8c8
  static QColor borderColor() { return QColor(0xb0, 0xb0, 0xb0); }

  /// 深边框颜色（中灰），对应 #999 / #999999
  static QColor borderDarkColor() { return QColor(0x99, 0x99, 0x99); }

  /// 面板背景色（白色），对应 #ffffff
  static QColor panelBackground() { return QColor(Qt::white); }

  /// 图标按钮 hover 半透明背景色
  static QColor iconButtonHoverBg() { return QColor(128, 128, 128, 40); }

  /// 图标按钮 pressed 半透明背景色
  static QColor iconButtonPressedBg() { return QColor(128, 128, 128, 80); }

  /// Tab 未选中背景色，对应 #e8e8e8
  static QColor tabUnselectedBackground() { return QColor(0xe8, 0xe8, 0xe8); }

  /// Tab hover 背景色，对应 #dcdcdc
  static QColor tabHoverBackground() { return QColor(0xdc, 0xdc, 0xdc); }

  /// 错误提示弹窗背景色（淡黄），对应 #ffffcc
  static QColor errorToolTipBackground() { return QColor(0xff, 0xff, 0xcc); }

  /// 保存全部按钮背景层颜色（灰色），与 inactiveTabColor 值相同
  static QColor saveAllButtonBgColor() { return inactiveTabColor(); }

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
  static QColor lineNumberBackground() { return QColor(Qt::lightGray).lighter(110); }

  /// 当前行高亮背景色（淡黄）
  static QColor currentLineBackground() { return QColor(Qt::yellow).lighter(180); }

  /// 括号匹配高亮背景色（青色）
  static QColor bracketMatchColor() { return QColor(0, 200, 200); }

  /// 错误波浪线颜色（红色）
  static QColor errorUnderlineColor() { return QColor(Qt::red); }

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

  // ════════════════════════════════════════════════════════════
  //  风格适配（跨平台兼容）
  // ════════════════════════════════════════════════════════════
  /// 确保 QTabBar 使用 Fusion 风格（Windows 原生风格忽略 setTabTextColor）
  static void ensureFusionTabBar(QTabBar *bar);

  /// QTabBar 样式表
  static QString tabBarStyleSheet();

  /// 补全弹出列表样式表
  static QString popupListStyleSheet();

  /// 错误提示弹窗样式表
  static QString errorToolTipStyleSheet();

  /// 设置标题栏标签的样式（文字颜色、字号、透明背景）
  static void applyTitleLabelStyle(QLabel *label);

  /// 设置标题栏容器的样式（背景色、负外边距覆盖窗口边框）
  static void applyTitleBarStyle(QWidget *titleBar);

private:
  AuiStyle() = delete;
};