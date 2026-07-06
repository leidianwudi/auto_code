/**
 * @file aui_style.cpp
 * @brief UI 公共样式工具类实现
 */

#include "aui_style.h"

#include <QStyle>
#include <QStyleFactory>
#include <QTabBar>

// ════════════════════════════════════════════════════════════
//  全局样式表
// ════════════════════════════════════════════════════════════
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
      "#WindowFrame { background: #e0e0e0; }");
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