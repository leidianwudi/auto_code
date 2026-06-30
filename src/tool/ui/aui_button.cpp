/**
 * @file aui_button.cpp
 * @brief UI 公共按钮工具类实现
 */

#include "aui_button.h"
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
//  构建 / 生成按钮图标
// ════════════════════════════════════════════════════════════

QIcon AuiButton::createBuildIcon(int size, const QString &text) {
  // 测量文字宽度（使用匹配图标尺寸的字体）
  QFont fnt;
  fnt.setPixelSize(qMax(1, size - 2));
  QFontMetrics fm(fnt);
  const int textW = text.isEmpty() ? 0 : fm.horizontalAdvance(text) + 4;
  const int totalW = size + textW;

  QPixmap px(totalW, size);
  px.fill(Qt::transparent);
  QPainter p(&px);
  p.setRenderHint(QPainter::Antialiasing);
  p.setFont(fnt);

  // ── 左侧三角形（接近铺满，仅留 1px 边距） ──
  p.setPen(Qt::NoPen);
  p.setBrush(AuiStyle::textColor());
  const double half = size * 0.5;
  QPolygonF tri;
  tri << QPointF(1.0, 1.0) << QPointF(size - 1.0, half)
      << QPointF(1.0, size - 1.0);
  p.drawPolygon(tri);

  // ── 右侧文字 ──
  if (!text.isEmpty()) {
    p.setPen(AuiStyle::textColor());
    p.drawText(size + 2, 0, textW, size, Qt::AlignVCenter | Qt::AlignLeft,
               text);
  }

  p.end();
  return QIcon(px);
}

QPushButton *AuiButton::createBuildButton(const QString &text) {
  auto *btn = new QPushButton;
  btn->setIcon(createBuildIcon(32, text));
  btn->setIconSize(QSize(32, 24));
  btn->setCursor(Qt::PointingHandCursor);
  btn->setFocusPolicy(Qt::NoFocus);
  btn->setStyleSheet(QStringLiteral("QPushButton {"
                                    "  background: transparent;"
                                    "  border: none;"
                                    "  margin: 2px 6px;"
                                    "  padding: 4px 8px;"
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