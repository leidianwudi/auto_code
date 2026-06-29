/**
 * @file aui_style.h
 * @brief UI 公共样式工具类
 *
 * 集中管理全局颜色、样式表、风格适配等，供所有界面组件共用。
 */

#pragma once

#include <QColor>
#include <QString>
#include <QStyle>
#include <QStyleFactory>
#include <QTabBar>

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

  // ════════════════════════════════════════════════════════════
  //  全局样式表
  // ════════════════════════════════════════════════════════════

  /// @brief 主窗口全局样式表
  ///
  /// 定义了以下控件的样式：
  /// | 选择器 | 作用 | 关键属性 |
  /// |--------|------|---------|
  /// | `#TitleBar` | 自定义标题栏背景 | `background: #e0e0e0` |
  /// | `#TitleLabel` | 窗口标题文字 | `color: #333`, `font-size: 12px` |
  /// | `QToolButton` | 工具栏按钮（菜单按钮） | `color: #333`, 无边框, 6px
  /// 水平内边距 | | `QToolButton:hover` | 工具栏按钮悬停 | `background:
  /// #d0d0d0` | | `QPushButton` | 标题栏控制按钮 | `color: #333`, 无边框 | |
  /// `QPushButton:hover` | 标题栏按钮悬停 | `background: #d0d0d0` | |
  /// `QStatusBar` | 底部状态栏 | `background: #e0e0e0`, `color: #333` | |
  /// `QStatusBar::item` | 状态栏子项分隔线 | 无边框 | | `#WindowFrame` |
  /// 窗口中央容器背景 | `background: #e0e0e0` |
  ///
  /// @note 所有颜色值可通过修改 AuiStyle 颜色常量统一调整。
  static QString mainStyleSheet() {
    return QStringLiteral(
        "#TitleBar { background: #e0e0e0; }"
        "#TitleLabel { color: #333; font-size: 12px; }"
        "QToolButton { color: #333; border: none; padding: 2px 6px; }"
        "QToolButton:hover { background: #d0d0d0; }"
        "QPushButton { color: #333; border: none; }"
        "QPushButton:hover { background: #d0d0d0; }"
        "QStatusBar { background: #e0e0e0; color: #333; }"
        "QStatusBar::item { border: none; }"
        "#WindowFrame { background: #e0e0e0; }");
  }

  // ════════════════════════════════════════════════════════════
  //  风格适配（跨平台兼容）
  // ════════════════════════════════════════════════════════════

  /// 确保 QTabBar 使用 Fusion 风格（Windows 原生风格忽略 setTabTextColor）
  static void ensureFusionTabBar(QTabBar *bar) {
#ifdef Q_OS_WIN
    if (bar->style() &&
        QString::fromLatin1(bar->style()->metaObject()->className())
            .contains(QStringLiteral("Fusion")))
      return;
    QStyle *fs = QStyleFactory::create(QStringLiteral("Fusion"));
    if (fs) {
      fs->setParent(bar);
      bar->setStyle(fs);
    }
#else
    Q_UNUSED(bar);
#endif
  }

private:
  AuiStyle() = delete;
};