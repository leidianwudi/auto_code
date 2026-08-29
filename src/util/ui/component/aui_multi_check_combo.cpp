/**
 * @file aui_multi_check_combo.cpp
 * @brief 多选下拉框实现（自管理弹出列表，复选不收起）
 */

#include "aui_multi_check_combo.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QKeyEvent>
#include <QListWidget>
#include <QPainter>
#include <QPolygonF>
#include <QStyle>
#include <QStyleOption>
#include <QVBoxLayout>

#include "aui_style.h"

AuiMultiCheckCombo::AuiMultiCheckCombo(QWidget *parent) : QToolButton(parent) {
  // 自管理弹出列表（Qt::Popup）：点击外部自动收起，勾选复选框由我们控制、绝不收起。
  // 不用 QMenu/QComboBox 内置弹出机制（它们点击菜单项会自动收起）。
  m_popup = new QWidget(nullptr, Qt::Popup | Qt::FramelessWindowHint);
  m_popup->setAttribute(Qt::WA_DeleteOnClose, false);
  auto *lay = new QVBoxLayout(m_popup);
  lay->setContentsMargins(1, 1, 1, 1);
  lay->setSpacing(0);
  m_list = new QListWidget(m_popup);
  m_list->setSelectionMode(QAbstractItemView::NoSelection);  // 不产生选中高亮
  m_list->setFocusPolicy(Qt::StrongFocus);
  lay->addWidget(m_list);
  m_popup->setLayout(lay);

  // 点击按钮：展开/收起弹窗（Qt::Popup 点击外部时自动收起，且该点击不会穿透到按钮）
  connect(this, &QToolButton::clicked, this, [this]() { togglePopup(); });
  // 监听弹窗收起（含点击外部自动收起），用于重置按钮的聚焦/按下残留变色
  m_popup->installEventFilter(this);

  // 文字与向下箭头由 paintEvent 自绘（紧排），不再用原生 text+icon 布局；
  // 宽度由 sizeHint 固定（最宽汇总文字），避免文字切换导致宽度跳动
  updateDisplay();
  connect(m_list, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
    const QString data = item->data(Qt::UserRole).toString();
    if (data.isEmpty()) return;
    if (item->checkState() == Qt::Checked)
      m_checked.insert(data);
    else
      m_checked.remove(data);
    updateDisplay();
    // 仅用户交互（m_updating 为 false）时通知外部；程序化还原不触发
    if (!m_updating) emit checkedChanged();
  });

  refreshStyle();
  updateDisplay();
}

void AuiMultiCheckCombo::addOption(const QString &label, const QString &data) {
  if (data.isEmpty() || m_items.contains(data)) return;
  auto *item = new QListWidgetItem(label, m_list);
  item->setData(Qt::UserRole, data);
  item->setFlags(item->flags() | Qt::ItemIsEnabled);
  item->setCheckState(Qt::Checked);  // 默认勾选
  m_items.insert(data, item);
  m_dataOrder.append(data);
  m_checked.insert(data);
  updateDisplay();
}

void AuiMultiCheckCombo::setChecked(const QString &data, bool checked) {
  auto it = m_items.find(data);
  if (it == m_items.end()) return;
  m_updating = true;
  it.value()->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
  m_updating = false;
  if (checked)
    m_checked.insert(data);
  else
    m_checked.remove(data);
  updateDisplay();
}

bool AuiMultiCheckCombo::isChecked(const QString &data) const {
  return m_checked.contains(data);
}

QStringList AuiMultiCheckCombo::checkedData() const {
  QStringList out;
  for (const QString &d : m_dataOrder)
    if (m_checked.contains(d)) out.append(d);
  return out;
}

bool AuiMultiCheckCombo::allChecked() const {
  return !m_dataOrder.isEmpty() && m_checked.size() == m_dataOrder.size();
}

void AuiMultiCheckCombo::setAllChecked(bool all) {
  m_updating = true;
  const Qt::CheckState st = all ? Qt::Checked : Qt::Unchecked;
  for (QListWidgetItem *item : m_items) item->setCheckState(st);
  m_updating = false;
  m_checked.clear();
  if (all) {
    for (const QString &d : m_dataOrder) m_checked.insert(d);
  }
  updateDisplay();
}

void AuiMultiCheckCombo::refreshStyle() {
  // 按钮样式：与共享菜单按钮同色，但左右内边距更小（6px → 3px），避免文字左右空白过大。
  // 不能直接改 AuiStyle::applyMenuButtonStyle（文件/视图/帮助等按钮共用），故单独设置。
  // hover 背景：相对面板背景做轻微明暗（浅色下仅略深一点、深色下略亮）。
  // 注意不能直接用 tabHoverBackground(#dcdcdc)——tab 栏底色是 #e8e8e8 才显浅，
  // 下拉框在白色面板上会显得过深。
  const QColor panel = AuiStyle::panelBackground();
  const QColor hoverBg = panel.lightness() > 128 ? panel.darker(106) : panel.lighter(120);
  setStyleSheet(
      QStringLiteral("QToolButton { color: %1; background: transparent; "
                     "border: 1px solid transparent; padding: 2px 3px; }"
                     "QToolButton:hover { background: %2; }")
          .arg(AuiStyle::textColor().name(), hoverBg.name()));
  // 调色板双保险：Fusion 绘制 QToolButton 文字优先读调色板 ButtonText/WindowText
  QPalette p = palette();
  const QColor tc = AuiStyle::textColor();
  p.setColor(QPalette::ButtonText, tc);
  p.setColor(QPalette::WindowText, tc);
  p.setColor(QPalette::Text, tc);
  p.setColor(QPalette::HighlightedText, tc);
  setPalette(p);

  if (m_list) {
    // 弹出列表背景/文字/边框随主题（与菜单风格一致）；条目 hover 用标准列表 hover 色
    m_list->setStyleSheet(
        QStringLiteral("QListWidget { color: %1; background: %2; border: 1px solid %3; }"
                       "QListWidget::item { padding: 4px 8px; }"
                       "QListWidget::item:hover { background: %4; }")
            .arg(AuiStyle::textColor().name(), AuiStyle::background().name(),
                 AuiStyle::borderColor().name(), AuiStyle::listHoverBackground().name()));
    m_list->viewport()->update();
  }
  update();
}

void AuiMultiCheckCombo::togglePopup() {
  if (!m_popup) return;
  if (m_popup->isVisible()) {
    m_popup->hide();
    return;
  }
  // 弹层展开前先让按钮让出焦点/按下态：Qt::Popup 不夺取键盘焦点，
  // 否则按钮的聚焦/按下变色会一直残留，只有应用失活才恢复（反复出现的问题）
  clearFocus();
  setDown(false);
  relayoutPopup();
  m_popup->move(mapToGlobal(QPoint(0, height())));
  m_popup->show();
  m_popup->raise();
  m_list->setFocus();
  // Qt::Popup 不夺取键盘焦点（输入框聚焦状态不自动清除），通知外部主动让出焦点
  emit popupOpened();
}

bool AuiMultiCheckCombo::eventFilter(QObject *watched, QEvent *event) {
  // 弹窗收起（含点击外部自动收起）时，重置按钮聚焦/按下态并强制重绘，
  // 避免"只有点击应用外部按钮才恢复白色"的残留变色
  if (watched == m_popup && event->type() == QEvent::Hide) {
    clearFocus();
    setDown(false);
    update();
  }
  return QToolButton::eventFilter(watched, event);
}

void AuiMultiCheckCombo::relayoutPopup() {
  // 宽度至少等于按钮宽度，能容纳最宽的项；高度按内容行数，最多限高
  const int rowH = m_list->sizeHintForRow(0);
  const int contentW = m_list->sizeHintForColumn(0);
  const int popupW = qMax(width(), contentW + 30);
  const int popupH = qBound(20, rowH * m_list->count() + 8, 260);
  m_popup->resize(popupW, popupH);
}

void AuiMultiCheckCombo::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape && m_popup && m_popup->isVisible()) {
    m_popup->hide();
    event->accept();
    return;
  }
  QToolButton::keyPressEvent(event);
}

void AuiMultiCheckCombo::updateDisplay() {
  if (allChecked())
    setText(QStringLiteral("全部类型"));
  else if (m_checked.isEmpty())
    setText(QStringLiteral("无类型"));
  else
    setText(QStringLiteral("%1 种类型").arg(m_checked.size()));
}

void AuiMultiCheckCombo::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QStyleOptionToolButton opt;
  initStyleOption(&opt);
  // 原生布局在文字与箭头之间留白较大；清空原生 text/icon，
  // 仅让 QSS（含 hover 背景/边框）绘制按钮，文字与箭头手动紧排
  opt.text.clear();
  opt.icon = QIcon();
  QPainter p(this);
  // QToolButton 是复杂控件（CC_ToolButton），通过它绘制 QSS 背景/边框（含 hover），
  // 但清空 text/icon，文字与箭头由下面手动紧排
  style()->drawComplexControl(QStyle::CC_ToolButton, &opt, &p, this);

  // 紧排：左边距 → 文字 → 小间隔 → 向下箭头
  constexpr int kLeftPad = 2;
  constexpr int kTextArrowGap = 2;
  constexpr qreal kArrowW = 8.0;   // 箭头宽
  constexpr qreal kArrowH = 4.5;   // 箭头高
  p.setPen(AuiStyle::textColor());
  p.setFont(font());
  const QString disp = text();  // updateDisplay 已把汇总文字 setText 到按钮
  const int textW = fontMetrics().horizontalAdvance(disp);
  p.drawText(QRect(kLeftPad, 0, textW, height()), Qt::AlignVCenter | Qt::AlignLeft, disp);

  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(Qt::NoPen);
  p.setBrush(AuiStyle::textColor());
  const qreal cx = kLeftPad + textW + kTextArrowGap + kArrowW / 2.0;
  const qreal cy = height() / 2.0 + 0.5;
  QPolygonF tri;
  tri << QPointF(cx - kArrowW / 2.0, cy - kArrowH / 2.0)
      << QPointF(cx + kArrowW / 2.0, cy - kArrowH / 2.0) << QPointF(cx, cy + kArrowH / 2.0);
  p.drawPolygon(tri);
}

QSize AuiMultiCheckCombo::sizeHint() const {
  // 以最宽汇总文字定宽（考虑"N 种类型"随选项数量增长），避免文字切换导致宽度跳动 /
  // 右侧出现多余空白：左边距 + 文字 + 间隔 + 箭头 + 右边距
  int widest = fontMetrics().horizontalAdvance(QStringLiteral("全部类型"));
  widest = qMax(widest, fontMetrics().horizontalAdvance(QStringLiteral("无类型")));
  if (!m_dataOrder.isEmpty())
    widest = qMax(widest, fontMetrics().horizontalAdvance(
                              QStringLiteral("%1 种类型").arg(m_dataOrder.size())));
  const int w = 2 + widest + 2 + 8 + 2;
  return QSize(qMax(w, QToolButton::sizeHint().width()), QToolButton::sizeHint().height());
}
