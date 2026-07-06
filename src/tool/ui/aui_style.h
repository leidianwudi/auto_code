/**
 * @file aui_style.h
 * @brief UI 公共样式工具类
 *
 * 集中管理全局颜色、样式表、风格适配等，供所有界面组件共用。
 */

#pragma once

#include <QColor>
#include <QString>

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