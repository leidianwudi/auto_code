/**
 * @file aui_combo_box.cpp
 * @brief 样式化下拉框工具类实现
 */

#include "aui_combo_box.h"

#include <QPainter>
#include <QStyleOption>
#include <QStylePainter>

#include "aui_style.h"

// ════════════════════════════════════════════════════════════
//  内部控件 — 自定义绘制三角箭头
// ════════════════════════════════════════════════════════════

/**
 * @brief 继承 QComboBox，在 paintEvent 中绘制自定义向下三角箭头
 *
 * 样式表负责框架外观（背景、边框、圆角），
 * 此类在样式表渲染完成后，在 drop-down 区域绘制三角箭头。
 */
class AuiComboBoxWidget : public QComboBox {
public:
  using QComboBox::QComboBox;

protected:
  void paintEvent(QPaintEvent *e) override {
    // ── 1. 让 QComboBox 自身（含样式表）完成框架渲染 ──
    QComboBox::paintEvent(e);

    // ── 2. 在 drop-down 区域绘制向下三角箭头 ──
    QStyleOptionComboBox opt;
    initStyleOption(&opt);

    // 只有非可编辑模式下才绘制箭头
    if (!opt.editable) {
      QPainter painter(this);
      painter.setRenderHint(QPainter::Antialiasing);
      painter.setPen(Qt::NoPen);
      painter.setBrush(AuiStyle::textColor());

      // 获取 drop-down 按钮区域
      QRect arrowRect =
          style()->subControlRect(QStyle::CC_ComboBox, &opt, QStyle::SC_ComboBoxArrow, this);

      // 居中绘制倒三角，留 6px 边距，小尺寸
      QRect r = arrowRect.adjusted(6, 7, -6, -7);
      if (r.width() >= 2 && r.height() >= 2) {
        QPolygonF tri;
        tri << QPointF(r.left(), r.top()) << QPointF(r.right(), r.top())
            << QPointF(r.center().x(), r.bottom());
        painter.drawPolygon(tri);
      }
    }
  }
};

// ════════════════════════════════════════════════════════════
//  公共 API
// ════════════════════════════════════════════════════════════

QComboBox *AuiComboBox::create(QWidget *parent) {
  auto *combo = new AuiComboBoxWidget(parent);
  applyStyle(combo);
  return combo;
}

void AuiComboBox::applyStyle(QComboBox *combo) {
  // ── 应用样式表（只负责框架，箭头由 AuiComboBoxWidget::paintEvent 绘制） ──
  combo->setStyleSheet(QStringLiteral("QComboBox {"
                                      "  background: white;"
                                      "  border: 1px solid #c8c8c8; border-radius: 3px;"
                                      "  padding: 4px 6px; font-size: 13px;"
                                      "}"
                                      "QComboBox:hover {"
                                      "  border: 1px solid #999;"
                                      "}"
                                      "QComboBox::drop-down {"
                                      "  border: none; width: 24px;"
                                      "}"
                                      "QComboBox QAbstractItemView {"
                                      "  background: white;"
                                      "  border: 1px solid #c8c8c8;"
                                      "  selection-background-color: %1;"
                                      "  selection-color: %2;"
                                      "}")
                           .arg(AuiStyle::hoverBackground().name(), AuiStyle::textColor().name()));
}