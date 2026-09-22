/**
 * @file aui_combo_box.cpp
 * @brief 样式化下拉框工具类实现
 */

#include "aui_combo_box.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QEvent>
#include <QGuiApplication>
#include <QLineEdit>
#include <QPainter>
#include <QScreen>
#include <QStyle>
#include <QStyleOption>
#include <QStylePainter>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>

#include "aui_style.h"

// ════════════════════════════════════════════════════════════
//  文字左边距与普通输入框对齐
// ════════════════════════════════════════════════════════════

/**
 * @brief 让下拉框显示文字的左边距与同窗口的普通输入框（QLineEdit）完全一致
 *
 * 普通输入框的文字位置 = 内容区左边界 + QLineEdit 固定的 2px 文字边距：
 * - 主窗口裸输入框（原生 1px 边框）：文字距控件左缘 3px
 * - 对话框输入框（QSS padding: 4px 6px）：文字距控件左缘 9px
 * 下拉框（非可编辑）文字直接画在 QSS 内容区原点（边框 1px + padding-left），
 * 据此换算 padding-left 使两者文字左缘对齐；可编辑下拉框内部还有一层无边框
 * 输入框（自带 2px 文字边距），换算时需扣除。
 * 用同窗口的临时 QLineEdit 测量，主窗口/对话框两种上下文都能自动对齐。
 */
static void updateTextPadding(QComboBox *combo) {
  QLineEdit probe(combo->window());
  probe.ensurePolished();
  QStyleOptionFrame opt;
  opt.initFrom(&probe);
  // 与 QLineEdit 私有实现一致：lineWidth 取原生边框宽度（PM_DefaultFrameWidth）
  opt.lineWidth = probe.style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &opt, &probe);
  // 输入框内容区左边距（含边框/QSS padding，不含 QLineEdit 固定的 2px 文字边距）
  const int inset = probe.style()->subElementRect(QStyle::SE_LineEditContents, &opt, &probe).left();
  const int textLeft = inset + 2;  // QLineEdit 固定的 2px 文字边距
  const int pad = qMax(0, combo->isEditable() ? textLeft - 3 : textLeft - 1);
  // 结果没变化时不重设样式表，避免 showEvent 里反复重设触发无谓的重绘
  if (combo->property("auiTextPad").isValid() && combo->property("auiTextPad").toInt() == pad)
    return;
  combo->setProperty("auiTextPad", pad);
  // 仅覆盖 padding-left（无颜色），不影响切换主题时的文字颜色；
  // 右侧/上下 padding 仍走全局样式表
  combo->setStyleSheet(QStringLiteral("QComboBox { padding-left: %1px; }").arg(pad));
}

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
  void showEvent(QShowEvent *e) override {
    QComboBox::showEvent(e);
    // setEditable 可能发生在 applyStyle 之后，显示时按最终可编辑态校准文字左边距
    updateTextPadding(this);
  }

  void paintEvent(QPaintEvent *e) override {
    // ── 1. 让 QComboBox 自身（含样式表）完成框架渲染 ──
    QComboBox::paintEvent(e);

    // ── 2. 在 drop-down 区域绘制向下三角箭头 ──
    // hideArrow() 场景（auiNoArrow）：样式表已把 drop-down 区域压为 0 宽度，
    // 箭头矩形退化到控件右缘，此时若仍补画三角会越出控件被裁掉一半 ——
    // 这类下拉框的设计就是无箭头（文字占满宽度），直接跳过绘制
    if (property("auiNoArrow").toBool()) return;

    // 可编辑/不可编辑模式均绘制：可编辑时输入框只占编辑区，
    // drop-down 区域仍需箭头提示「可下拉」（否则可编辑下拉框没有任何箭头）
    QStyleOptionComboBox opt;
    initStyleOption(&opt);

    // 全局样式表已把原生 down-arrow 压为 0 尺寸（width:0 height:0），
    // SC_ComboBoxArrow 在部分样式下会返回退化/贴边的矩形，直接取其中心会把
    // 三角推出控件被裁掉一半 —— 这里钳制圆心，保证固定尺寸的三角完整落在边框内
    QRect arrowRect =
        style()->subControlRect(QStyle::CC_ComboBox, &opt, QStyle::SC_ComboBoxArrow, this);
    const qreal halfW = 4.0;  // 三角宽 8 的一半
    const qreal cx =
        qBound<qreal>(halfW + 2.0, qreal(arrowRect.center().x()), qreal(width()) - halfW - 2.0);
    const qreal cy = height() / 2.0 + 0.5;
    QPainter painter(this);
    AuiStyle::drawDownArrow(painter, QPointF(cx, cy), AuiStyle::textColor());
  }
};

// ════════════════════════════════════════════════════════════
//  弹层高度保证 — 条目口径统一的委托 + ensurePopupFit
// ════════════════════════════════════════════════════════════

namespace {
/**
 * @brief 弹层条目委托：高度恒等于「字体行高 + 全局样式表 ::item padding(4px)*2」，
 *        与实际渲染严格一致；宽度按真实文本宽给出（保留 Qt 原生「弹层可宽于
 *        下拉框以完整显示长条目」的行为）。
 *
 * 默认委托的 sizeHint 在样式表字体刷新前会给出过期值，弹层高度按过期 hint 计算
 * 会导致条目截断并出现滚动条（调试会话 combo-popup-truncated 定位）。
 */
class AuiComboItemDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  explicit AuiComboItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

  QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &index) const override {
    const QFontMetrics fm(opt.font);
    // 宽度：文本宽 + ::item padding(8px)×2；图标项另计图标宽 + 间距
    int w = fm.horizontalAdvance(index.data(Qt::DisplayRole).toString()) + 16;
    if (!opt.icon.isNull()) w += opt.decorationSize.width() + 4;
    return {w, fm.height() + 8};
  }
};
}  // namespace

void AuiComboBox::ensurePopupFit(QComboBox *combo) {
  if (!combo || !combo->view()) return;
  QAbstractItemView *view = combo->view();
  // ① 条目高度口径：安装与渲染一致的委托；定制委托（如 AuiComboDelete 的
  //    「文本 + 删除按钮」）通过 auiComboDelegateCustom 属性声明豁免
  if (!view->property("auiComboDelegateCustom").toBool() &&
      !qobject_cast<AuiComboItemDelegate *>(view->itemDelegate()))
    view->setItemDelegate(new AuiComboItemDelegate(view));
  // ② 视图高度按内容精确固定（min=max，任何后续布局 pass 都无法改变视图高度）；
  //    空列表时解除历史约束，避免空弹层残留上一次条目的固定高度
  const QAbstractItemModel *model = view->model();
  if (model && model->rowCount() > 0) {
    int contentH = 0;
    for (int i = 0; i < model->rowCount(); ++i)
      contentH += view->sizeHintForIndex(model->index(i, 0)).height();
    view->setFixedHeight(contentH + 2 * view->frameWidth());
  } else {
    view->setMinimumHeight(0);
    view->setMaximumHeight(QWIDGETSIZE_MAX);
  }
}

// ════════════════════════════════════════════════════════════
//  全局过滤器 — 所有下拉框弹出列表一律向下展开
// ════════════════════════════════════════════════════════════

/**
 * @brief 全局事件过滤器：拦截所有 QComboBox 弹出层（Qt::Popup）的显示事件，
 *        优先将其移动到下拉框正下方（左对齐 + 顶边对齐）。
 *
 * 无论下拉框是 AuiComboBox::create() 创建还是直接 new QComboBox，均生效。
 * 定位策略（与 NoBorderCombo::showPopup 的截断修复配套）：
 *  - 下方放得下：正下方展开；
 *  - 下方放不下且上方放得下：整体翻转到上方（不再压缩列表高度——压缩即截断，
 *    正是查询设置表格下拉框弹层显示不全一类问题的根源）；
 *  - 上下都放不下：贴屏幕底边，仅裁掉必然放不下的部分。
 */
class ComboPopDownFilter : public QObject {
public:
  using QObject::QObject;

  bool eventFilter(QObject *watched, QEvent *event) override {
    if (event->type() == QEvent::Show) {
      auto *w = qobject_cast<QWidget *>(watched);
      if (w && w->isWindow() && (w->windowType() == Qt::Popup)) {
        if (auto *combo = qobject_cast<QComboBox *>(w->parentWidget())) {
          // 树形弹层（AuiTreeCombo）自行管理高度：仅定位，不做高度钳制
          const bool treePopup =
              combo->view() && combo->view()->property("auiTreePopup").toBool();
          if (!treePopup) {
            // 高度保证：条目口径统一 + 视图高度固定（幂等，重复调用安全）
            AuiComboBox::ensurePopupFit(combo);
          }
          // 弹出列表已经显示，等当前事件处理完（Qt 内部布局完成）后再定位
          QTimer::singleShot(0, w, [w, combo, treePopup]() {
            QAbstractItemView *view = combo->view();
            if (!view) return;
            // 高度钳制：弹层窗口钉到视图高度。视图已精确等于内容高，多出部分必为
            // 样式盒死区（如 QComboBox padding 2px×2 传播），不足则补齐
            if (!treePopup && w->height() != view->height()) {
              w->setFixedHeight(view->height());
              w->resize(w->width(), view->height());
            }
            const QPoint comboTopLeft = combo->mapToGlobal(QPoint(0, 0));
            QScreen *screen = QGuiApplication::screenAt(comboTopLeft);
            if (!screen) return;
            const QRect avail = screen->availableGeometry();
            QRect g = w->geometry();
            g.moveTopLeft(combo->mapToGlobal(QPoint(0, combo->height())));
            if (g.bottom() > avail.bottom()) {
              const int h = g.height();
              g.moveTop(comboTopLeft.y() - h);  // 翻转到上方（底部对齐下拉框顶部）
              if (g.top() < avail.top()) g.moveTop(avail.bottom() + 1 - h);  // 贴屏幕底边
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
  // 控件样式表只覆盖 padding-left（无颜色），不会影响切换主题时的文字颜色
  combo->setProperty("auiNoSheet", true);
  // 显示文字的左边距与同窗口的普通输入框（QLineEdit）对齐；
  // setEditable 若发生在创建之后，由 AuiComboBoxWidget::showEvent 再次校准
  updateTextPadding(combo);
  combo->update();
}

void AuiComboBox::hideArrow(QComboBox *combo) {
  if (!combo) return;
  // 配合全局样式表规则 QComboBox[auiNoArrow="true"]::drop-down { width: 0 }，
  // 移除右侧预留的箭头区域，让文字占满整个下拉框宽度，避免文字被截断。
  combo->setProperty("auiNoArrow", true);

  // 弹出列表仅按当前下拉框宽度（固定的小宽度）受限时会把较长项用省略号截断，
  // 这里把弹层加宽到能容纳最宽的项，保证下拉时文字完整显示。
  if (QAbstractItemView *view = combo->view()) {
    const int content = view->sizeHintForColumn(0);
    if (content > 0) {
      const int scrollbar = combo->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
      view->setMinimumWidth(content + scrollbar + 20);
    }
  }

  combo->style()->unpolish(combo);
  combo->style()->polish(combo);
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

#include "aui_combo_box.moc"