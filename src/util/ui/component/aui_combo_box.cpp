/**
 * @file aui_combo_box.cpp
 * @brief 样式化下拉框工具类实现
 */

#include "aui_combo_box.h"

#include <QAbstractItemView>
#include <QPainter>
#include <QStyleOption>
#include <QStylePainter>
#include <QTimer>

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
  // 强制弹出列表始终向下展开：标题栏处的下拉框若可向下空间不足，Qt 默认会往上弹，
  // 视觉上很别扭；这里在弹出后把列表移动到下拉框正下方（左对齐 + 顶边对齐）
  void showPopup() override {
    QComboBox::showPopup();
    if (QWidget *popup = view()->window()) {
      const QPoint pos = mapToGlobal(QPoint(0, height()));
      QTimer::singleShot(0, popup, [popup, pos]() { popup->move(pos); });
    }
  }

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

      // 在 drop-down 区域中央绘制一个小尺寸倒三角（固定尺寸，避免随区域收缩变形）
      const qreal aw = 8.0;  // 箭头宽
      const qreal ah = 4.5;  // 箭头高
      const QPointF c(arrowRect.center().x(), arrowRect.center().y() + 0.5);
      QPolygonF tri;
      tri << QPointF(c.x() - aw / 2.0, c.y() - ah / 2.0)
          << QPointF(c.x() + aw / 2.0, c.y() - ah / 2.0) << QPointF(c.x(), c.y() + ah / 2.0);
      painter.drawPolygon(tri);
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
  // 颜色统一由 app 级 mainStyleSheet 动态管理（含背景/边框/文字/下拉列表），
  // 不在控件上单独 setStyleSheet，避免切换主题时文字固化旧色导致深色下看不清
  combo->setProperty("auiNoSheet", true);
  combo->update();
}