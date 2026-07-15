/**
 * @file aui_style.cpp
 * @brief UI 公共样式工具类实现
 */

#include "aui_style.h"

#include <QLabel>
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
/// - #e0e0e0 — 浅灰背景（窗口背景、状态栏、窗口框架）
/// - #cccccc — 中灰背景（自定义标题栏，比窗口背景深，形成层次感）
/// - #333    — 深灰文字，保证可读性
/// - #d0d0d0 — hover 高亮底色
/// - #999999 — 窗口外边框（1px 实线）
///
/// 各选择器说明：
/// - QToolButton     : 标题栏菜单按钮（文件/视图等）
/// - QToolButton:hover : 菜单按钮鼠标悬停
/// - QPushButton     : 通用按钮（窗口控制、生成按钮等）
/// - QPushButton:hover : 按钮鼠标悬停
/// - QStatusBar      : 底部状态栏
/// - QStatusBar::item : 状态栏内部子控件之间的分隔线（去掉默认边框）
/// - #WindowFrame    : applyWindowFrame() 创建的窗口外层框架
QString AuiStyle::mainStyleSheet() {
  return QStringLiteral(
             "QToolButton { color: %2; border: 1px solid transparent; padding: 2px 6px; }"
             "QToolButton:hover { background: %3; border: 1px solid %5; }"
             "QPushButton { color: %2; border: 1px solid transparent; }"
             "QPushButton:hover { background: %3; border: 1px solid %5; }"
             "QStatusBar { background: %1; color: %2; }"
             "QStatusBar::item { border: none; }"
             "QMenu { background: %1; border: 1px solid %5; padding: 4px 0px; }"
             "QMenu::item { padding: 6px 24px; }"
             "QMenu::item:selected { background: %3; color: %2; }"
             "#WindowFrame { background: %1; border: 1px solid %4; }")
      .arg(background().name(), textColor().name(), hoverBackground().name(),
           borderDarkColor().name(), borderColor().name());
}

QString AuiStyle::dialogStyleSheet() {
  const QString fs = QString::number(dialogFontSize()) + QStringLiteral("px");
  return QStringLiteral(
             "QDialog { background: %1; }"
             "QLabel { color: %2; font-size: %3; }"
             "QLineEdit {"
             "  border: 1px solid %4; border-radius: 3px;"
             "  padding: 4px 6px; font-size: %3;"
             "}")
      .arg(background().name(), textColor().name(), fs, borderColor().name());
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
      .arg(borderColor().name(), tabUnselectedBackground().name(), panelBackground().name(),
           tabHoverBackground().name());
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
      .arg(errorToolTipBackground().name(), borderDarkColor().name());
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

void AuiStyle::applyTitleLabelStyle(QLabel *label) {
  label->setStyleSheet(QStringLiteral("color: %1; font-size: %2px; background: transparent;"
                                      "padding: 0px 2px;")
                           .arg(textColor().name(), QString::number(titleFontSize())));
  label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void AuiStyle::applyTitleBarStyle(QWidget *titleBar) {
  titleBar->setStyleSheet(QStringLiteral("background: %1; margin-left: -2px; margin-right: -2px;")
                              .arg(titleBarBackground().name()));
}