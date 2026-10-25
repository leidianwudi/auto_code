/**
 * @file aui_splitter.cpp
 * @brief 统一风格的可拖动分隔控件实现
 */

#include "src/util/ui/component/aui_splitter.h"

#include <QEnterEvent>
#include <QEvent>
#include <QPaintEvent>
#include <QPainter>

#include "src/util/ui/component/aui_style.h"

namespace {

constexpr int kHandleWidth = 8;   ///< 分隔条命中区厚度（细线居中，两侧留白便于抓取）
constexpr int kDotDiameter = 3;   ///< 抓手圆点直径
constexpr int kDotGap = 8;        ///< 相邻圆点中心距

}  // namespace

AuiSplitterHandle::AuiSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
    : QSplitterHandle(orientation, parent) {
  setMouseTracking(true);
}

void AuiSplitterHandle::enterEvent(QEnterEvent *event) {
  m_hovered = true;
  update();
  QSplitterHandle::enterEvent(event);
}

void AuiSplitterHandle::leaveEvent(QEvent *event) {
  m_hovered = false;
  update();
  QSplitterHandle::leaveEvent(event);
}

void AuiSplitterHandle::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  const QColor lineColor = m_hovered ? AuiStyle::textColor() : AuiStyle::borderColor();
  const QColor dotColor = m_hovered ? AuiStyle::textColor() : AuiStyle::mutedTextColor();
  const QRect r = rect();

  // ── 主线：垂直布局的 splitter 画横线，水平布局画竖线 ──
  p.setPen(QPen(lineColor, 1));
  if (orientation() == Qt::Vertical) {
    const int y = r.center().y();
    p.drawLine(r.left(), y, r.right(), y);
  } else {
    const int x = r.center().x();
    p.drawLine(x, r.top(), x, r.bottom());
  }

  // ── 中部三点抓手：最直观的“可拖动”提示 ──
  p.setPen(Qt::NoPen);
  p.setBrush(dotColor);
  const QPointF c = r.center();
  const qreal radius = kDotDiameter / 2.0;
  for (int i = -1; i <= 1; ++i) {
    const qreal offset = i * kDotGap;
    if (orientation() == Qt::Vertical) {
      p.drawEllipse(QPointF(c.x() + offset, c.y()), radius, radius);
    } else {
      p.drawEllipse(QPointF(c.x(), c.y() + offset), radius, radius);
    }
  }
}

AuiSplitter::AuiSplitter(Qt::Orientation orientation, QWidget *parent)
    : QSplitter(orientation, parent) {
  // 只统一命中区厚度；是否允许折叠由调用方决定（保持各处的原有行为）
  setHandleWidth(kHandleWidth);
}

QSplitterHandle *AuiSplitter::createHandle() {
  return new AuiSplitterHandle(orientation(), this);
}
