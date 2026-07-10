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
             "#TitleBar { background: %1; }"
             "#TitleLabel { color: %2; font-size: 12px; }"
             "QToolButton { color: %2; border: none; padding: 2px 6px; }"
             "QToolButton:hover { background: %3; }"
             "QPushButton { color: %2; border: none; }"
             "QPushButton:hover { background: %3; }"
             "QStatusBar { background: %1; color: %2; }"
             "QStatusBar::item { border: none; }"
             "#WindowFrame { background: %1; border: 1px solid %4; }")
      .arg(barBackground().name(), textColor().name(), hoverBackground().name(),
           borderDarkColor().name());
}

QString AuiStyle::dialogStyleSheet() {
  return QStringLiteral(
             "QDialog { background: %1; }"
             "QLabel { color: %2; font-size: 13px; }"
             "QLineEdit {"
             "  border: 1px solid %3; border-radius: 3px;"
             "  padding: 4px 6px; font-size: 13px;"
             "}"
             "QPushButton {"
             "  border: 1px solid %3; border-radius: 3px;"
             "  padding: 6px 20px; font-size: 13px;"
             "}")
      .arg(barBackground().name(), textColor().name(), borderColor().name());
}

QString AuiStyle::tabBarStyleSheet() {
  return QStringLiteral(
             "QTabBar::tab {"
             "  padding: 6px 2px 6px 4px;"
             "  border: 1px solid %1;"
             "  border-bottom: none;"
             "  border-top-left-radius: 4px;"
             "  border-top-right-radius: 4px;"
             "  background: %2;"
             "  margin-right: 0px;"
             "}"
             "QTabBar::tab:selected {"
             "  background: %3;"
             "}"
             "QTabBar::tab:hover:!selected {"
             "  background: %4;"
             "}"
             "QTabBar::close-button {"
             "  padding: 0px;"
             "  margin: 0px;"
             "}")
      .arg(borderColor().name(),              // 1: #c8c8c8
           QColor(0xe8, 0xe8, 0xe8).name(),   // 2: tab 未选中背景
           panelBackground().name(),          // 3: #ffffff
           QColor(0xdc, 0xdc, 0xdc).name());  // 4: tab hover 背景
}

QString AuiStyle::popupListStyleSheet() {
  return QStringLiteral(
             "QListView {"
             "  border: 1px solid %1;"
             "  border-radius: 0px;"
             "  padding: 0px;"
             "  margin: 0px;"
             "  background: %2;"
             "}"
             "QListView::item {"
             "  padding: 1px 4px;"
             "  min-height: 18px;"
             "}")
      .arg(borderDarkColor().name(), panelBackground().name());
}

QString AuiStyle::errorToolTipStyleSheet() {
  return QStringLiteral(
             "#AuiErrorToolTip {"
             "  background: %1;"
             "  border: 1px solid %2;"
             "  border-radius: 3px;"
             "  padding: 4px 8px;"
             "}")
      .arg(QColor(0xff, 0xff, 0xcc).name(),  // #ffffcc 淡黄背景
           borderDarkColor().name());
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