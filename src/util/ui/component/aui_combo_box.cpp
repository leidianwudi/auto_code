/**
 * @file aui_combo_box.cpp
 * @brief 样式化下拉框工具类实现
 */

#include "aui_combo_box.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QEvent>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QStyleOption>
#include <QStylePainter>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>

#include "aui_style.h"

// ════════════════════════════════════════════════════════════
//  内部控件 — 自定义绘制三角箭头
// ════════════════════════════════════════════════════════════

/**
 * @brief 继承 QComboBox，在 paintEvent 中绘制自定义向下三角箭头
 *
 * 样式表负责框架外观（背景、边框、圆角），
 * 此类在样式表渲染完成后，在 drop-down 区域绘制三角箭头。
 * 弹出列表「永远向下」的行为统一由 ComboPopDownFilter 全局过滤器处理。
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
//  全局过滤器 — 所有下拉框弹出列表一律向下展开
// ════════════════════════════════════════════════════════════

/**
 * @brief 全局事件过滤器：拦截所有 QComboBox 弹出层（Qt::Popup）的显示事件，
 *        在弹出后将其强制移动到下拉框正下方（左对齐 + 顶边对齐）。
 *
 * 无论下拉框是 AuiComboBox::create() 创建还是直接 new QComboBox，均生效。
 * 若下方空间不足，只压缩列表高度而不向上弹。
 */
class ComboPopDownFilter : public QObject {
public:
  using QObject::QObject;

  bool eventFilter(QObject *watched, QEvent *event) override {
    if (event->type() == QEvent::Show) {
      auto *w = qobject_cast<QWidget *>(watched);
      if (w && w->isWindow() && (w->windowType() == Qt::Popup)) {
        if (auto *combo = qobject_cast<QComboBox *>(w->parentWidget())) {
          // 弹出列表已经显示，等当前事件处理完（Qt 内部布局完成）后再移动到正下方
          const QPoint pos = combo->mapToGlobal(QPoint(0, combo->height()));
          QTimer::singleShot(0, w, [w, pos]() {
            QRect g = w->geometry();
            g.moveTopLeft(pos);
            // 始终向下：若超出屏幕底部，压缩高度而非向上弹
            if (QScreen *screen = QGuiApplication::screenAt(pos)) {
              const QRect avail = screen->availableGeometry();
              if (g.bottom() > avail.bottom()) g.setHeight(avail.bottom() - g.top() + 1);
            }
            w->setGeometry(g);
          });
        }
      }
    }
    return QObject::eventFilter(watched, event);
  }
};

// ════════════════════════════════════════════════════════════
//  全局过滤器 — 禁止滚轮悬停时改动下拉框值
// ════════════════════════════════════════════════════════════

/**
 * @brief 全局事件过滤器：拦截所有 QComboBox（含其编辑框子控件）的滚轮事件。
 *
 * 鼠标悬停在下拉框上滚动滚轮时，Qt 默认会切换当前选中项，极易误改数据；
 * 这里在弹出列表未展开时直接把滚轮吞掉（event->ignore() 并返回 true）。
 * 弹出列表展开时不拦截，列表项仍可正常滚动浏览。
 *
 * 无论下拉框是 AuiComboBox::create() 创建、直接 new QComboBox，
 * 还是 NoBorderCombo 等子类，均自动生效，无需逐处修改调用方。
 */
class ComboWheelSafeFilter : public QObject {
public:
  using QObject::QObject;

  bool eventFilter(QObject *watched, QEvent *event) override {
    if (event->type() == QEvent::Wheel) {
      // 从接收者向上追溯所属下拉框；滚轮可能先被可编辑下拉框的内部输入框接收
      QWidget *w = qobject_cast<QWidget *>(watched);
      while (w) {
        if (auto *combo = qobject_cast<QComboBox *>(w)) {
          // 弹出列表已展开时不拦截，让用户能滚动浏览列表项
          const bool listOpen = combo->view() && combo->view()->window()->isVisible();
          if (!listOpen) {
            event->ignore();
            return true;  // 吞掉滚轮，阻止选中值被改动
          }
          break;
        }
        w = w->parentWidget();
      }
    }
    return QObject::eventFilter(watched, event);
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

void AuiComboBox::ensureGlobalPopDown() {
  static bool installed = false;
  if (installed) return;
  installed = true;
  qApp->installEventFilter(new ComboPopDownFilter(qApp));
}

void AuiComboBox::ensureGlobalWheelSafe() {
  static bool installed = false;
  if (installed) return;
  installed = true;
  qApp->installEventFilter(new ComboWheelSafeFilter(qApp));
}