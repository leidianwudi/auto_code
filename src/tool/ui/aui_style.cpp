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

// ════════════════════════════════════════════════════════════
//  风格适配（跨平台兼容）
// ════════════════════════════════════════════════════════════
void AuiStyle::ensureFusionTabBar(QTabBar *bar) {
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