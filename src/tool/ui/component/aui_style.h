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
#include <QString>
#include <QTextBlockFormat>

class QTabBar;

class AuiStyle {
public:
  // ════════════════════════════════════════════════════════════
  //  基础颜色常量
  // ════════════════════════════════════════════════════════════

  /// 标题栏 / 状态栏 / 窗口边框背景色
  static QColor barBackground() { return QColor(0xe0, 0xe0, 0xe0); }

  /// 标题文本 / 图标画笔颜色
  static QColor textColor() { return QColor(0x33, 0x33, 0x33); }

  /// 按钮 / 控件 hover 背景色
  static QColor hoverBackground() { return QColor(0xd0, 0xd0, 0xd0); }

  /// 非活跃面板的标签页文字颜色（灰色）
  static QColor inactiveTabColor() { return QColor(0x88, 0x88, 0x88); }

  /// 编译按钮颜色，绿色
  static QColor compileButtonColor() { return QColor(0x44, 0x99, 0x00); }

  /// 文件修改标记颜色，红色
  static QColor modifiedColor() { return QColor(0xcc, 0x33, 0x33); }

  /// 错误文本颜色（输出面板用），红色
  static QColor errorTextColor() { return QColor(0xf4, 0x47, 0x47); }

  /// 通用边框颜色（浅灰），对应 #c8c8c8
  static QColor borderColor() { return QColor(0xc8, 0xc8, 0xc8); }

  /// 深边框颜色（中灰），对应 #999 / #999999
  static QColor borderDarkColor() { return QColor(0x99, 0x99, 0x99); }

  /// 面板背景色（白色），对应 #ffffff
  static QColor panelBackground() { return QColor(Qt::white); }

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

private:
  AuiStyle() = delete;
};