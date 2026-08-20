/**
 * @file aui_combo_delete.cpp
 * @brief 可删除项下拉框控件实现
 *
 * 通过给下拉列表（view）安装自定义 delegate 绘制文本与右侧删除按钮（×），
 * 并在 viewport 上安装事件过滤器处理悬停高亮与点击删除。
 */

#include "aui_combo_delete.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>

#include "aui_style.h"

// ════════════════════════════════════════════════════════════
//  自定义委托 — 绘制「文本 + 右侧删除按钮」
// ════════════════════════════════════════════════════════════

/**
 * @brief 为下拉列表每一项绘制左下文本 + 右上/右侧的删除按钮（×）
 *
 * 文本区域让出右端的 kBtnScaffoldWidth 宽度，文本超长省略号截断；
 * 删除按钮悬停时（由 AuiComboDelete::hoverRow 告知）绘制底色提示。
 */
class AuiComboDeleteDelegate : public QStyledItemDelegate {
public:
  explicit AuiComboDeleteDelegate(const AuiComboDelete *combo)
      : m_combo(combo) {}

protected:
  void paint(QPainter *p, const QStyleOptionViewItem &opt,
             const QModelIndex &index) const override {
    // ── 1. 先让默认实现绘制选中/悬停背景与文本框架 ──
    // 但默认实现会绘制完整文本，可能覆盖到删除按钮区域。这里手动控制。
    QStyleOptionViewItem o = opt;
    initStyleOption(&o, index);

    p->save();
    // ── 背景（选中 / 悬停）──
    if (o.state & QStyle::State_Selected) {
      p->fillRect(o.rect, AuiStyle::listSelectionBackground());
    } else if (o.state & QStyle::State_MouseOver) {
      p->fillRect(o.rect, AuiStyle::listHoverBackground());
    }

    // ── 文本（让出右侧删除按钮区域）──
    QRect textRect = o.rect.adjusted(0, 0, -AuiComboDelete::kBtnScaffoldWidth, 0);
    textRect.adjust(AuiComboDelete::kBtnPaddingLeft, 0, 0, 0);
    p->setPen(AuiStyle::textColor());
    p->setFont(o.font);
    p->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                p->fontMetrics().elidedText(o.text, Qt::ElideRight, textRect.width()));

    // ── 删除按钮（×）──
    bool hover = m_combo && (m_combo->hoverRow() == index.row());
    QRect btnRect = m_combo ? m_combo->buttonRect(o.rect) : QRect();

    if (hover) {
      // 悬停时画一个圆角底色，提示可点击
      p->setRenderHint(QPainter::Antialiasing, true);
      p->setPen(Qt::NoPen);
      p->setBrush(AuiStyle::hoverBackground());
      p->drawRoundedRect(btnRect.adjusted(1, 1, -1, -1), 3, 3);

      // 画两条白色斜线构成 ×（在底色上更醒目）
      p->setPen(QPen(AuiStyle::textColor(), 1.4));
      const int pad = 4;
      const QPointF c = btnRect.center();
      p->drawLine(QPointF(c.x() - pad, c.y() - pad), QPointF(c.x() + pad, c.y() + pad));
      p->drawLine(QPointF(c.x() - pad, c.y() + pad), QPointF(c.x() + pad, c.y() - pad));
    } else {
      // 未悬停：画浅色细 ×
      p->setRenderHint(QPainter::Antialiasing, true);
      p->setPen(QPen(AuiStyle::mutedTextColor(), 1.2));
      const int pad = 4;
      const QPointF c = btnRect.center();
      p->drawLine(QPointF(c.x() - pad, c.y() - pad), QPointF(c.x() + pad, c.y() + pad));
      p->drawLine(QPointF(c.x() - pad, c.y() + pad), QPointF(c.x() + pad, c.y() - pad));
    }

    p->restore();
  }

private:
  const AuiComboDelete *m_combo = nullptr;  ///< 所属下拉框（取 hoverRow / buttonRect）
};

// ════════════════════════════════════════════════════════════
//  AuiComboDelete 实现
// ════════════════════════════════════════════════════════════

AuiComboDelete::AuiComboDelete(QWidget *parent) : QComboBox(parent) {
  // 下拉列表使用自定义委托绘制「文本 + 删除按钮」
  setItemDelegate(new AuiComboDeleteDelegate(this));
  // 允许查看的 viewport 接收鼠标移动事件，用于悬停高亮
  view()->viewport()->setAttribute(Qt::WA_MouseTracking, true);
  view()->viewport()->installEventFilter(this);
}

AuiComboDelete *AuiComboDelete::create(QWidget *parent) {
  auto *combo = new AuiComboDelete(parent);
  return combo;
}

QRect AuiComboDelete::buttonRect(const QRect &itemRect) const {
  const int x = itemRect.right() - kBtnScaffoldWidth + (kBtnScaffoldWidth - kBtnSize) / 2;
  const int y = itemRect.top() + (itemRect.height() - kBtnSize) / 2;
  return QRect(x, y, kBtnSize, kBtnSize);
}

bool AuiComboDelete::eventFilter(QObject *watched, QEvent *event) {
  if (watched == view()->viewport()) {
    const QAbstractItemView *v = view();
    const QMouseEvent *me = nullptr;

    if (event->type() == QEvent::MouseMove) {
      me = static_cast<QMouseEvent *>(event);
      const QModelIndex idx = v->indexAt(me->pos());
      const int row = idx.isValid() ? idx.row() : -1;
      // 只有确实位于删除按钮区域内才视为悬停
      const QRect itemRect = idx.isValid() ? v->visualRect(idx) : QRect();
      const int hover = (!itemRect.isNull() && buttonRect(itemRect).contains(me->pos()))
                            ? row
                            : -1;
      if (hover != m_hoverRow) {
        m_hoverRow = hover;
        view()->viewport()->update();  // 重绘以刷新按钮高亮
      }
      return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::Leave) {
      if (m_hoverRow != -1) {
        m_hoverRow = -1;
        view()->viewport()->update();
      }
      return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonRelease) {
      me = static_cast<QMouseEvent *>(event);
      if (me->button() == Qt::LeftButton) {
        const QModelIndex idx = v->indexAt(me->pos());
        if (idx.isValid() && buttonRect(v->visualRect(idx)).contains(me->pos())) {
          // 点击删除按钮：只发信号，不改变当前选中项
          m_hoverRow = -1;
          emit itemDeleteRequested(idx.row());
          // 保持弹层展开：Qt 的 popup 仅在 showEvent 时按项数计算高度，
          // 删除后需手动按新项数重设弹层尺寸，否则底部残留空白。
          if (QFrame *popup = qobject_cast<QFrame *>(view()->parentWidget())) {
            QSize s = view()->sizeHint().expandedTo(popup->minimumSize());
            popup->setFixedSize(s);
          }
          return true;  // 消费该事件，阻止选中该项
        }
      }
    }
  }
  return QObject::eventFilter(watched, event);
}

void AuiComboDelete::paintEvent(QPaintEvent *event) {
  // 让 QComboBox 自身（含样式表）完成框架渲染
  QComboBox::paintEvent(event);

  // 在 drop-down 区域绘制向下三角箭头（与 AuiComboBoxWidget 一致）
  QStyleOptionComboBox opt;
  initStyleOption(&opt);
  if (!opt.editable) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(AuiStyle::textColor());

    const QRect arrowRect =
        style()->subControlRect(QStyle::CC_ComboBox, &opt, QStyle::SC_ComboBoxArrow, this);
    const qreal aw = 8.0;
    const qreal ah = 4.5;
    const QPointF c(arrowRect.center().x(), arrowRect.center().y() + 0.5);
    QPolygonF tri;
    tri << QPointF(c.x() - aw / 2.0, c.y() - ah / 2.0)
        << QPointF(c.x() + aw / 2.0, c.y() - ah / 2.0) << QPointF(c.x(), c.y() + ah / 2.0);
    painter.drawPolygon(tri);
  }
}