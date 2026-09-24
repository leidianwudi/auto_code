/**
 * @file aui_tree_combo.cpp
 * @brief 树形下拉框控件实现
 */

#include "aui_tree_combo.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QItemSelectionModel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeView>

#include "src/util/ui/component/aui_style.h"

// #region debug-point reporter:tree-combo-popup
/// 调试插桩（临时，验证后删除）：NDJSON 追加到 .dbg 日志文件
static void dbgTreeLog(const char *location, const QJsonObject &data) {
  QJsonObject ev;
  ev.insert(QStringLiteral("location"), QLatin1String(location));
  ev.insert(QStringLiteral("data"), data);
  ev.insert(QStringLiteral("ts"), qint64(QDateTime::currentMSecsSinceEpoch()));
  QFile f(QStringLiteral(
      "d:/work/github/auto_code/.dbg/trae-debug-log-tree-combo-popup.ndjson"));
  if (f.open(QIODevice::Append | QIODevice::Text)) {
    f.write(QJsonDocument(ev).toJson(QJsonDocument::Compact));
    f.write("\n");
    f.close();
  }
}
// #endregion

// ════════════════════════════════════════════════════════════
//  条目委托：行高与 AuiComboItemDelegate 同口径（字体行高 + 8px）
// ════════════════════════════════════════════════════════════

namespace {
/// 条目委托：行高与 AuiComboItemDelegate 同口径（字体行高 + 8px）。
/// 高亮自管：全局样式表没有 ::item:selected 规则，选中底色依赖原生主题绘制，
/// 会被悬停状态干扰。因此与 AuiComboDeleteDelegate 同口径确定性绘制：
/// 选中行恒用 listSelectionBackground，悬停行用 listHoverBackground，两色不同。
/// 选中判定取自控件记录的当前选中索引（currentHighlightIndex），不依赖弹层
/// selection model——悬停引发的视图选择变化不会冲掉选中高亮
class AuiTreeComboDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  AuiTreeComboDelegate(const AuiTreeCombo *combo, QObject *parent)
      : QStyledItemDelegate(parent), m_combo(combo) {}

  QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &index) const override {
    Q_UNUSED(index);
    return {0, QFontMetrics(opt.font).height() + 8};
  }

  void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index) const override {
    QStyleOptionViewItem o = opt;
    initStyleOption(&o, index);
    // 背景：选中优先于悬停（选中行被悬停时仍显示选中色）
    const bool selected = m_combo && m_combo->currentHighlightIndex() == index;
    p->save();
    if (selected) {
      p->fillRect(o.rect, AuiStyle::listSelectionBackground());
    } else if (o.state & QStyle::State_MouseOver) {
      p->fillRect(o.rect, AuiStyle::listHoverBackground());
    }
    // 文本：与全局样式表 ::item 的 8px 左内边距对齐，超长省略号截断；
    // 组标题的粗体由条目 FontRole 传入（o.font）
    const QRect textRect = o.rect.adjusted(8, 0, -4, 0);
    p->setPen(AuiStyle::textColor());
    p->setFont(o.font);
    p->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                p->fontMetrics().elidedText(o.text, Qt::ElideRight, textRect.width()));
    p->restore();
  }

private:
  const AuiTreeCombo *m_combo = nullptr;  ///< 所属控件（读取当前选中索引）
};

/// 弹层树视图：分支指示区（缩进/展开箭头列）与条目区用同一套高亮配色绘制。
/// 原生 drawBranches 在选中时用调色板高亮色填充分支区，与委托绘制的条目底色
/// 不一致，造成"同一行两种颜色"——这里改为与条目区同色，箭头手动绘制。
/// 悬停索引由本视图自行跟踪（QAbstractItemView 内部 hover 不暴露给子类）
class AuiTreePopupView : public QTreeView {
public:
  explicit AuiTreePopupView(const AuiTreeCombo *combo, QWidget *parent = nullptr)
      : QTreeView(parent), m_combo(combo) {}

protected:
  void mouseMoveEvent(QMouseEvent *e) override {
    QTreeView::mouseMoveEvent(e);
    const QModelIndex idx = indexAt(e->position().toPoint());
    if (idx != m_hover) {
      m_hover = QPersistentModelIndex(idx);
      viewport()->update();
    }
  }

  void leaveEvent(QEvent *e) override {
    QTreeView::leaveEvent(e);
    if (m_hover.isValid()) {
      m_hover = QPersistentModelIndex();
      viewport()->update();
    }
  }

  void drawBranches(QPainter *p, const QRect &rect, const QModelIndex &index) const override {
    // 背景：与条目区同色（选中优先于悬停）
    const bool selected = m_combo && m_combo->currentHighlightIndex() == index;
    const bool hovered = !selected && m_hover == index;
    p->save();
    if (selected) {
      p->fillRect(rect, AuiStyle::listSelectionBackground());
    } else if (hovered) {
      p->fillRect(rect, AuiStyle::listHoverBackground());
    }
    // 展开/收起箭头：仅组节点（有子级）绘制；状态不带 Selected/MouseOver，
    // 防止原生 PE_IndicatorBranch 路径再次填出调色板高亮底色。
    // 注：不能用 QAbstractItemView::initStyleOption（Qt 6.12 中不可见，会解析到
    // QFrame 的 1 参重载），分支绘制手工构造选项即可
    if (model() && model()->hasChildren(index)) {
      QStyleOptionViewItem opt;
      opt.palette = palette();
      opt.direction = layoutDirection();
      opt.rect = rect;
      opt.state = QStyle::State_Enabled | QStyle::State_Item | QStyle::State_Children
                  | (isExpanded(index) ? QStyle::State_Open : QStyle::State_None);
      style()->drawPrimitive(QStyle::PE_IndicatorBranch, &opt, p, this);
    }
    p->restore();
  }

private:
  const AuiTreeCombo *m_combo = nullptr;  ///< 所属控件（读取当前选中索引）
  QPersistentModelIndex m_hover;          ///< 鼠标悬停条目（分支区悬停底色判定）
};
}  // namespace

// ════════════════════════════════════════════════════════════
//  AuiTreeCombo
// ════════════════════════════════════════════════════════════

AuiTreeCombo::AuiTreeCombo(QWidget *parent) : QComboBox(parent) {
  m_model = new QStandardItemModel(this);
  m_treeView = new AuiTreePopupView(this, this);
  m_treeView->setHeaderHidden(true);
  m_treeView->setRootIsDecorated(true);  // 组节点显示展开/收起箭头
  m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_treeView->setItemDelegate(new AuiTreeComboDelegate(this, m_treeView));
  // 悬停高亮需要 viewport 接收鼠标移动事件（委托按 State_MouseOver 绘制悬停底色）。
  // 本控件未挂任何 selectionChanged/currentChanged 钩子，悬停不会引发选择变化
  m_treeView->viewport()->setAttribute(Qt::WA_MouseTracking, true);
  setView(m_treeView);
  setModel(m_model);
  // 可编辑 + 只读输入框：树形模型下 QComboBox 的行号显示文本无意义，
  // 选中后的显示文字由本控件经 setEditText 写入（见 eventFilter/selectByData）
  setEditable(true);
  if (QLineEdit *le = lineEdit()) {
    le->setReadOnly(true);
    le->setPlaceholderText(QStringLiteral("请选择数据源"));
    // 鼠标事件穿透到下拉框本体：否则只读输入框会吃掉点击，
    // 导致点击文字区域无法弹出（箭头区域仍由 QComboBox 原生处理）
    le->setAttribute(Qt::WA_TransparentForMouseEvents);
  }
  // 标记树形弹层：全局 ComboPopDownFilter 跳过高度钳制（本控件自行管理高度）
  view()->setProperty("auiTreePopup", true);
  m_treeView->viewport()->installEventFilter(this);
  // 弹层窗口开合状态跟踪（供文字区域点击的弹出/收起切换判断）
  if (QWidget *w = view()->window()) w->installEventFilter(this);
  // 选择处理全部收口到 eventFilter 的释放事件拦截（见 eventFilter）：
  // QComboBox 在条目激活（activated，单击即触发）时会按行号同步弹层选择，
  // 树形模型下行号无意义、会把选择清空——这是"点击选择失效"的根源。
  // 因此这里不挂 currentChanged / selectionChanged / clicked 任何一个。
  // 展开/收起后延迟一拍重算高度：等 QTreeView 布局结算完再测量（否则高度不准）
  connect(m_treeView, &QTreeView::expanded, this,
          [this]() { QTimer::singleShot(0, this, [this]() { updatePopupHeight(); }); });
  connect(m_treeView, &QTreeView::collapsed, this,
          [this]() { QTimer::singleShot(0, this, [this]() { updatePopupHeight(); }); });
}

QStandardItem *AuiTreeCombo::addGroup(const QString &title) {
  QStandardItem *it = new QStandardItem(title);
  it->setFlags(Qt::ItemIsEnabled);  // 组标题：不可选中（仅展开/收起）
  QFont hf = it->font();
  hf.setBold(true);
  it->setFont(hf);
  m_model->appendRow(it);
  m_needsExpand = true;
  return it;
}

void AuiTreeCombo::addEntry(QStandardItem *group, const QString &text, const QVariant &data) {
  QStandardItem *it = new QStandardItem(text);
  it->setData(data, Qt::UserRole);
  if (group) {
    group->appendRow(it);
  } else {
    m_model->appendRow(it);
  }
  m_needsExpand = true;
}

QModelIndex AuiTreeCombo::findIndexByData(const QVariant &data) const {
  // 深度优先查找 UserRole 匹配的条目
  std::function<QModelIndex(const QModelIndex &)> find =
      [&](const QModelIndex &parent) -> QModelIndex {
    for (int i = 0; i < m_model->rowCount(parent); ++i) {
      const QModelIndex idx = m_model->index(i, 0, parent);
      if (idx.data(Qt::UserRole) == data) return idx;
      const QModelIndex sub = find(idx);
      if (sub.isValid()) return sub;
    }
    return QModelIndex();
  };
  return find(QModelIndex());
}

void AuiTreeCombo::selectByData(const QVariant &data) {
  const QModelIndex idx = findIndexByData(data);
  if (!idx.isValid()) return;
  // 展开父级链（顶层 → 父节点），保证选中项可见
  QList<QModelIndex> chain;
  for (QModelIndex p = idx.parent(); p.isValid(); p = p.parent()) chain.prepend(p);
  for (const QModelIndex &a : chain) m_treeView->expand(a);
  m_internalSelect = true;
  m_treeView->setCurrentIndex(idx);
  m_treeView->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect);
  m_internalSelect = false;
  m_currentRef = data;
  m_currentIdx = idx;
  setEditText(idx.data(Qt::DisplayRole).toString());  // 按钮显示选中条目文字
  // 显式抛出选中信号（程序性选中已屏蔽 selectionChanged）
  emit itemSelected(data);
}

QVariant AuiTreeCombo::currentEntryData() const {
  const QModelIndex cur = m_treeView->currentIndex();
  if (!cur.isValid() || !(cur.flags() & Qt::ItemIsSelectable)) return QVariant();
  return cur.data(Qt::UserRole);
}

void AuiTreeCombo::showPopup() {
  if (m_needsExpand) {
    m_treeView->expandAll();  // 内容有变化：先全部展开，用户可自行收起
    m_needsExpand = false;
  }
  QComboBox::showPopup();
  // 高亮当前选中条目。必须放在 showPopup 之后：基类 showPopup 会重置视图选择，
  // 先选中再弹出会被清掉，导致看不出当前选中的是哪个
  const QModelIndex cur = findIndexByData(m_currentRef);
  if (cur.isValid()) {
    for (QModelIndex p = cur.parent(); p.isValid(); p = p.parent()) m_treeView->expand(p);
    m_internalSelect = true;
    m_treeView->setCurrentIndex(cur);
    m_treeView->selectionModel()->select(cur, QItemSelectionModel::ClearAndSelect);
    m_internalSelect = false;
    m_treeView->scrollTo(cur, QAbstractItemView::EnsureVisible);
    m_currentIdx = cur;
  }
  // 测量容器与视图的高度差（弹层窗口钉高度时需要），并按实际内容校准高度
  if (QWidget *w = view()->window()) {
    m_containerChrome = qMax(0, w->height() - m_treeView->height());
    dbgTreeLog("show", {{"winH", w->height()},
                        {"viewH", m_treeView->height()},
                        {"chrome", m_containerChrome},
                        {"modelRows", m_model->rowCount()}});
  }
  QTimer::singleShot(0, this, [this]() { updatePopupHeight(); });
}

void AuiTreeCombo::hidePopup() {
  QComboBox::hidePopup();
  m_popupOpen = false;
}

void AuiTreeCombo::updatePopupHeight() {
  if (!m_treeView) return;
  // 精确统计可见行数：indexBelow 沿展开链逐行下走，收起的组不计子节点；
  // 行高统一为委托高度（字体行高 + 8px），行数 × 行高 = 精确内容高度
  int count = 0;
  for (QModelIndex it = m_model->index(0, 0); it.isValid(); it = m_treeView->indexBelow(it)) {
    ++count;
  }
  const int rowH = QFontMetrics(m_treeView->font()).height() + 8;
  const int h = qMin(count * rowH + 2 * m_treeView->frameWidth() + 2, 600);
  // 视图与弹层窗口同时钉住高度：容器可能滞留在展开时的高度（收起后不自动收缩）
  m_treeView->setFixedHeight(h);
  if (QWidget *w = view()->window()) {
    w->setFixedHeight(h + m_containerChrome);
    w->resize(w->width(), h + m_containerChrome);
  }
  dbgTreeLog("height", {{"visibleRows", count},
                        {"rowH", rowH},
                        {"h", h},
                        {"chrome", m_containerChrome},
                        {"viewH", m_treeView->height()},
                        {"winH", view()->window() ? view()->window()->height() : -1}});
}

void AuiTreeCombo::paintEvent(QPaintEvent *ev) {
  // 框架/背景由 QComboBox（含样式表）渲染；全局样式表把原生 down-arrow 压为
  // 0 尺寸，可编辑模式下没有箭头——这里与 AuiComboBoxWidget 一致补绘三角
  QComboBox::paintEvent(ev);
  QStyleOptionComboBox opt;
  initStyleOption(&opt);
  opt.currentText = lineEdit() ? lineEdit()->text() : currentText();
  QRect arrowRect =
      style()->subControlRect(QStyle::CC_ComboBox, &opt, QStyle::SC_ComboBoxArrow, this);
  const qreal halfW = 4.0;
  const qreal cx =
      qBound<qreal>(halfW + 2.0, qreal(arrowRect.center().x()), qreal(width()) - halfW - 2.0);
  const qreal cy = height() / 2.0 + 0.5;
  QPainter p(this);
  AuiStyle::drawDownArrow(p, QPointF(cx, cy), AuiStyle::textColor());
}

void AuiTreeCombo::mousePressEvent(QMouseEvent *ev) {
  QStyleOptionComboBox opt;
  initStyleOption(&opt);
  const QRect arrowRect =
      style()->subControlRect(QStyle::CC_ComboBox, &opt, QStyle::SC_ComboBoxArrow, this);
  if (arrowRect.contains(ev->position().toPoint())) {
    QComboBox::mousePressEvent(ev);  // 箭头区域：Qt 原生开/关弹层
    return;
  }
  // 文字区域：若弹层展开中，先关闭。Qt::Popup 会在这类"其外按下"时自动关闭
  // 弹层（随后本次按下被递送给本控件）；suppressNextShow 抑制释放时重新弹出
  if (m_popupOpen) {
    hidePopup();
    m_suppressNextShow = true;
  }
}

void AuiTreeCombo::mouseReleaseEvent(QMouseEvent *ev) {
  QStyleOptionComboBox opt;
  initStyleOption(&opt);
  const QRect arrowRect =
      style()->subControlRect(QStyle::CC_ComboBox, &opt, QStyle::SC_ComboBoxArrow, this);
  if (arrowRect.contains(ev->position().toPoint())) {
    QComboBox::mouseReleaseEvent(ev);  // 箭头区域：Qt 原生开/关弹层
    return;
  }
  // 文字区域：释放时弹出（除非这次点击已在按下阶段关闭了弹层）
  if (m_suppressNextShow) {
    m_suppressNextShow = false;
    return;
  }
  showPopup();
}

bool AuiTreeCombo::eventFilter(QObject *obj, QEvent *ev) {
  // 弹层窗口事件：开合状态跟踪 + "控件区域按下关闭弹层"识别
  if (obj == view()->window()) {
    if (ev->type() == QEvent::Hide) m_popupOpen = false;
    if (ev->type() == QEvent::Show) {
      m_popupOpen = true;
      m_suppressNextShow = false;
    }
    // 弹层展开时持有鼠标抓取：控件区域（弹层之外）的按下会先到达弹层容器——
    // 这次按下的用途就是关闭弹层，置抑制标记，使随后的释放不再重新弹出。
    // 若不处理，Qt 关闭弹层后释放会被重放给本控件，导致"关了又立刻弹开"
    if (ev->type() == QEvent::MouseButtonPress) {
      auto *me = static_cast<QMouseEvent *>(ev);
      const QRect comboRect(this->mapToGlobal(QPoint(0, 0)), size());
      if (comboRect.contains(me->globalPosition().toPoint())) m_suppressNextShow = true;
    }
  }
  // 选择收口：在弹层视口的"释放"事件处拦截——命中可选项时应用选择、收起弹层，
  // 并消费事件（return true）。消费是关键：若放行，视图的单击激活会触发
  // QComboBox 的行号同步，把树形弹层的选择清空（点击选择失效的根源）。
  // 按下不拦截：视图原生选中该行（即时高亮反馈）；组标题不可选，不进此分支。
  if (obj == m_treeView->viewport() && ev->type() == QEvent::MouseButtonRelease) {
    auto *me = static_cast<QMouseEvent *>(ev);
    const QModelIndex idx = m_treeView->indexAt(me->position().toPoint());
    if (idx.isValid() && (idx.flags() & Qt::ItemIsSelectable)) {
      m_internalSelect = true;
      m_treeView->setCurrentIndex(idx);
      m_treeView->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect);
      m_internalSelect = false;
      m_currentRef = idx.data(Qt::UserRole);
      m_currentIdx = idx;
      setEditText(idx.data(Qt::DisplayRole).toString());
      emit itemSelected(m_currentRef);
      hidePopup();
      return true;
    }
  }
  return QComboBox::eventFilter(obj, ev);
}

#include "aui_tree_combo.moc"
