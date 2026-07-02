/**
 * @file aui_button.cpp
 * @brief UI 公共按钮工具类实现
 */

#include "aui_button.h"
#include "aui_icon.h"
#include "aui_style.h"

#include <QPainter>
#include <QPixmap>
#include <QPolygonF>

// ════════════════════════════════════════════════════════════
//  通用按钮样式
// ════════════════════════════════════════════════════════════

void AuiButton::applyCommonStyle(QPushButton *btn) {
  btn->setFixedSize(36, 26);
  btn->setFlat(true);
  btn->setFocusPolicy(Qt::NoFocus);
}

// ════════════════════════════════════════════════════════════
//  工厂方法
// ════════════════════════════════════════════════════════════
QPushButton *AuiButton::createSplitButton() {
  auto *btn = new QPushButton;
  {
    QPixmap px(20, 20);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(AuiStyle::textColor(), 1.5));
    p.drawRect(2, 3, 16, 14);                    // 外框
    p.drawLine(QPointF(10, 3), QPointF(10, 17)); // 中分线
    p.end();
    btn->setIcon(QIcon(px));
    btn->setIconSize(QSize(20, 20));
  }
  btn->setToolTip(QStringLiteral("向右拆分编辑器 (Ctrl+\\)"));
  applyCommonStyle(btn);
  return btn;
}

QPushButton *AuiButton::createMinButton() {
  auto *btn = new QPushButton(QStringLiteral("\u2014")); // em dash
  applyCommonStyle(btn);
  return btn;
}

QPushButton *AuiButton::createMaxButton() {
  auto *btn = new QPushButton;
  updateMaximizeIcon(btn, false);
  applyCommonStyle(btn);
  return btn;
}

QPushButton *AuiButton::createCloseButton() {
  auto *btn = new QPushButton(QStringLiteral("\u2715")); // multiplication sign
  applyCommonStyle(btn);
  return btn;
}

// ════════════════════════════════════════════════════════════
//  构建 / 生成按钮
// ════════════════════════════════════════════════════════════

QPushButton *AuiButton::createBuildButton(int size) {
  auto *btn = new QPushButton;
  btn->setIcon(AuiIcon::createBuildIcon(size));
  btn->setIconSize(QSize(size, size));
  btn->setCursor(Qt::PointingHandCursor);
  btn->setFocusPolicy(Qt::NoFocus);
  btn->setStyleSheet(QStringLiteral("QPushButton {"
                                    "  background: transparent;"
                                    "  border: none;"
                                    "  margin: 2px 4px;"
                                    "  padding: 2px 4px;"
                                    "}"
                                    "QPushButton:hover {"
                                    "  background: rgba(128, 128, 128, 40);"
                                    "}"
                                    "QPushButton:pressed {"
                                    "  background: rgba(128, 128, 128, 80);"
                                    "}"));
  return btn;
}

// ════════════════════════════════════════════════════════════
//  保存按钮
// ════════════════════════════════════════════════════════════

QPushButton *AuiButton::createSaveButton(int size) {
  auto *btn = new QPushButton;
  // 绘制软盘图标
  QPixmap px(size, size);
  px.fill(Qt::transparent);
  QPainter p(&px);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(AuiStyle::textColor(), 1.2));

  int m = 2; // margin
  // 软盘外框
  p.drawRect(m, m, size - 2 * m, size - 2 * m);
  // 软盘标签（上部小矩形）
  p.drawRect(m + 3, m, size - 2 * m - 6, size / 3);
  // 底部横线
  p.drawLine(m + 2, size - m - 4, size - m - 2, size - m - 4);

  p.end();
  btn->setIcon(QIcon(px));
  btn->setIconSize(QSize(size, size));
  btn->setCursor(Qt::PointingHandCursor);
  btn->setFocusPolicy(Qt::NoFocus);
  btn->setToolTip(QStringLiteral("保存 (Ctrl+S)"));
  btn->setStyleSheet(QStringLiteral("QPushButton {"
                                    "  background: transparent;"
                                    "  border: none;"
                                    "  margin: 2px 4px;"
                                    "  padding: 2px 4px;"
                                    "}"
                                    "QPushButton:hover {"
                                    "  background: rgba(128, 128, 128, 40);"
                                    "}"
                                    "QPushButton:pressed {"
                                    "  background: rgba(128, 128, 128, 80);"
                                    "}"
                                    "QPushButton:disabled {"
                                    "  color: transparent;"
                                    "}"));
  return btn;
}

// ════════════════════════════════════════════════════════════
//  保存全部按钮
// ════════════════════════════════════════════════════════════

QPushButton *AuiButton::createSaveAllButton(int size) {
  auto *btn = new QPushButton;
  QPixmap px(size, size);
  px.fill(Qt::transparent);
  QPainter p(&px);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(AuiStyle::textColor(), 1.2));

  int m = 2;
  // 后层软盘（略微偏移）
  p.drawRect(m + 1, m + 1, size - 2 * m, size - 2 * m);
  p.drawRect(m + 4, m + 1, size - 2 * m - 6, size / 3);
  p.drawLine(m + 3, size - m - 3, size - m - 1, size - m - 3);

  // 前层软盘
  p.drawRect(m, m, size - 2 * m, size - 2 * m);
  p.drawRect(m + 3, m, size - 2 * m - 6, size / 3);
  p.drawLine(m + 2, size - m - 4, size - m - 2, size - m - 4);

  p.end();
  btn->setIcon(QIcon(px));
  btn->setIconSize(QSize(size, size));
  btn->setCursor(Qt::PointingHandCursor);
  btn->setFocusPolicy(Qt::NoFocus);
  btn->setToolTip(QStringLiteral("保存全部"));
  btn->setStyleSheet(QStringLiteral("QPushButton {"
                                    "  background: transparent;"
                                    "  border: none;"
                                    "  margin: 2px 4px;"
                                    "  padding: 2px 4px;"
                                    "}"
                                    "QPushButton:hover {"
                                    "  background: rgba(128, 128, 128, 40);"
                                    "}"
                                    "QPushButton:pressed {"
                                    "  background: rgba(128, 128, 128, 80);"
                                    "}"
                                    "QPushButton:disabled {"
                                    "  color: transparent;"
                                    "}"));
  return btn;
}

// ════════════════════════════════════════════════════════════
//  图标更新
// ════════════════════════════════════════════════════════════
void AuiButton::updateMaximizeIcon(QPushButton *btn, bool isMaximized) {
  QPixmap px(14, 14);
  px.fill(Qt::transparent);
  QPainter p(&px);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(AuiStyle::textColor(), 1.2));

  if (isMaximized) {
    p.drawRect(1, 4, 9, 7);
    p.drawRect(5, 1, 9, 7);
  } else {
    p.drawRect(1, 1, 12, 12);
  }
  p.end();
  btn->setIcon(QIcon(px));
  btn->setIconSize(QSize(14, 14));
}