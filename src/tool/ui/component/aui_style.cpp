/**
 * @file aui_style.cpp
 * @brief UI 公共样式工具类实现
 */

#include "aui_style.h"

#include <QStyle>
#include <QStyleFactory>
#include <QTabBar>

// ════════════════════════════════════════════════════════════
//  全局样式表 — 所有窗口共用
// ════════════════════════════════════════════════════════════

/// 返回项目中所有窗口公用的全局样式表，统一各窗口（MainDevUi、DemoUi、CreateUi）
/// 的视觉风格，包括标题栏、按钮、状态栏和窗口边框。
///
/// 选择的颜色值：
/// - #e0e0e0 — 浅灰背景（标题栏、状态栏、窗口框架）
/// - #333    — 深灰文字，保证可读性
/// - #d0d0d0 — hover 高亮底色
/// - #999999 — 窗口外边框（1px 实线）
///
/// 各选择器说明：
/// - #TitleBar       : 自定义标题栏背景
/// - #TitleLabel     : 标题栏窗口标题文字
/// - QToolButton     : 标题栏菜单按钮（文件/视图等）
/// - QToolButton:hover : 菜单按钮鼠标悬停
/// - QPushButton     : 通用按钮（窗口控制、生成按钮等）
/// - QPushButton:hover : 按钮鼠标悬停
/// - QStatusBar      : 底部状态栏
/// - QStatusBar::item : 状态栏内部子控件之间的分隔线（去掉默认边框）
/// - #WindowFrame    : applyWindowFrame() 创建的窗口外层框架
QString AuiStyle::mainStyleSheet() {
  return QStringLiteral(
      "#TitleBar { background: #e0e0e0; }"
      "#TitleLabel { color: #333; font-size: 12px; }"
      "QToolButton { color: #333; border: none; padding: 2px 6px; }"
      "QToolButton:hover { background: #d0d0d0; }"
      "QPushButton { color: #333; border: none; }"
      "QPushButton:hover { background: #d0d0d0; }"
      "QStatusBar { background: #e0e0e0; color: #333; }"
      "QStatusBar::item { border: none; }"
      "#WindowFrame { background: #e0e0e0; border: 1px solid #999999; }");
}

QString AuiStyle::tabBarStyleSheet() {
  return QStringLiteral(
      "QTabBar::tab {"
      "  padding: 6px 2px 6px 4px;"
      "  border: 1px solid #c8c8c8;"
      "  border-bottom: none;"
      "  border-top-left-radius: 4px;"
      "  border-top-right-radius: 4px;"
      "  background: #e8e8e8;"
      "  margin-right: 0px;"
      "}"
      "QTabBar::tab:selected {"
      "  background: #ffffff;"
      "}"
      "QTabBar::tab:hover:!selected {"
      "  background: #dcdcdc;"
      "}"
      "QTabBar::close-button {"
      "  padding: 0px;"
      "  margin: 0px;"
      "}");
}

QString AuiStyle::popupListStyleSheet() {
  return QStringLiteral(
      "QListView {"
      "  border: 1px solid #999;"
      "  border-radius: 0px;"
      "  padding: 0px;"
      "  margin: 0px;"
      "  background: white;"
      "}"
      "QListView::item {"
      "  padding: 1px 4px;"
      "  min-height: 18px;"
      "}");
}

QString AuiStyle::errorToolTipStyleSheet() {
  return QStringLiteral(
      "#AuiErrorToolTip {"
      "  background: #ffffcc;"
      "  border: 1px solid #999999;"
      "  border-radius: 3px;"
      "  padding: 4px 8px;"
      "}");
}

// ════════════════════════════════════════════════════════════
//  风格适配（跨平台兼容）
// ════════════════════════════════════════════════════════════
void AuiStyle::ensureFusionTabBar(QTabBar *bar) {
#ifdef Q_OS_WIN
  bool isFusion = bar->style() && QString::fromLatin1(bar->style()->metaObject()->className())
                                      .contains(QStringLiteral("Fusion"));
  if (!isFusion) {
    QStyle *fs = QStyleFactory::create(QStringLiteral("Fusion"));
    if (fs) {
      fs->setParent(bar);
      bar->setStyle(fs);
    }
  }
#else
  Q_UNUSED(bar);
#endif
}