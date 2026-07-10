/**
 * @file aui_button.cpp
 * @brief UI 公共按钮工具类实现
 */

#include "aui_button.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>

#include "aui_icon.h"
#include "aui_style.h"

// ════════════════════════════════════════════════════════════
//  通用按钮样式
// ════════════════════════════════════════════════════════════

void AuiButton::applyCommonStyle(QPushButton *btn) {
  btn->setFixedSize(AuiStyle::titleBarButtonSize());
  btn->setFlat(true);
  btn->setFocusPolicy(Qt::NoFocus);
}

void AuiButton::applyIconButtonStyle(QPushButton *btn) {
  btn->setStyleSheet(QStringLiteral("QPushButton {"
                                    "  background: transparent;"
                                    "  border: none;"
                                    "  margin: 2px 4px;"
                                    "  padding: 2px 4px;"
                                    "}"
                                    "QPushButton:hover {"
                                    "  background: %1;"
                                    "}"
                                    "QPushButton:pressed {"
                                    "  background: %2;"
                                    "}"
                                    "QPushButton:disabled {"
                                    "  color: transparent;"
                                    "}")
                         .arg(AuiStyle::iconButtonHoverBg().name(QColor::HexArgb),
                              AuiStyle::iconButtonPressedBg().name(QColor::HexArgb)));
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
    p.drawRect(2, 3, 16, 14);                     // 外框
    p.drawLine(QPointF(10, 3), QPointF(10, 17));  // 中分线
    p.end();
    btn->setIcon(QIcon(px));
    btn->setIconSize(QSize(20, 20));
  }
  btn->setToolTip(QStringLiteral("向右拆分编辑器 (Ctrl+\\)"));
  applyCommonStyle(btn);
  return btn;
}

QPushButton *AuiButton::createMinButton() {
  auto *btn = new QPushButton(QStringLiteral("\u2014"));  // em dash
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
  auto *btn = new QPushButton(QStringLiteral("\u2715"));  // multiplication sign
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
  applyIconButtonStyle(btn);
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

  int m = 2;  // margin
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
  applyIconButtonStyle(btn);
  btn->setEnabled(false);
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

  QColor fg = AuiStyle::textColor();
  QColor bg = AuiStyle::saveAllButtonBgColor();
  int d = 2;

  // 绘制软盘图标（与 createSaveButton 相同样式）
  auto drawDisk = [&](int ox, int oy, const QColor &color) {
    p.setPen(QPen(color, 1.2));
    int m = 1 + d;  // 留出偏移空间
    int x = m + ox;
    int y = m + oy;
    int w = size - 2 * m;
    int h = size - 2 * m;
    p.drawRect(x, y, w, h);
    p.drawRect(x + 3, y, w - 6, h / 3);
    p.drawLine(x + 2, y + h - 4, x + w - 2, y + h - 4);
  };

  // 背景层 左上偏移
  drawDisk(-d, -d, bg);
  // 前景层 右下偏移
  drawDisk(+d, +d, fg);

  p.end();
  btn->setIcon(QIcon(px));
  btn->setIconSize(QSize(size, size));
  btn->setCursor(Qt::PointingHandCursor);
  btn->setFocusPolicy(Qt::NoFocus);
  btn->setToolTip(QStringLiteral("保存全部"));
  applyIconButtonStyle(btn);
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