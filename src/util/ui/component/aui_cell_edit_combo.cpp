/**
 * @file aui_cell_edit_combo.cpp
 * @brief 表格单元格常驻「输入框 + 三角下拉」控件实现
 */

#include "aui_cell_edit_combo.h"

#include <QAction>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPoint>
#include <QTableWidget>

#include "aui_style.h"

namespace {

/// 编辑态守卫（装在内部输入框上）：
/// - press：把所在行设为当前行（不拦截，点击仍由输入框原生处理——落光标/拖选）；
/// - Esc：退出编辑并还原进入前的文本（与单元格编辑器「Esc=取消」一致）；
/// - 回车：退出编辑（值已通过 textEdited/valueCommitted 上报，等同提交）；
/// - 失焦：退出编辑（点其他格子/控件时自动收起白底编辑框）。
class CellEditFilter : public QObject {
 public:
  explicit CellEditFilter(AuiCellEditCombo *cell) : QObject(cell), m_cell(cell) {}

  bool eventFilter(QObject *obj, QEvent *e) override {
    auto *le = qobject_cast<QLineEdit *>(obj);
    if (!le) return QObject::eventFilter(obj, e);
    if (e->type() == QEvent::MouseButtonPress) {
      m_cell->activateRow();
    } else if (e->type() == QEvent::KeyPress) {
      const auto *ke = static_cast<QKeyEvent *>(e);
      if (ke->key() == Qt::Key_Escape) {
        m_cell->exitEdit(true);
        return true;
      }
      if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
        m_cell->exitEdit(false);
        return true;
      }
    } else if (e->type() == QEvent::FocusOut) {
      m_cell->exitEdit(false);
    }
    return QObject::eventFilter(obj, e);
  }

 private:
  AuiCellEditCombo *m_cell = nullptr;
};

}  // namespace

AuiCellEditCombo::AuiCellEditCombo(QTableWidget *table, int col, const QStringList &presets,
                                   QWidget *parent, bool readOnly)
    : QWidget(parent), m_table(table), m_col(col), m_presets(presets), m_editable(!readOnly) {
  m_edit = new QLineEdit(this);
  m_edit->setFrame(false);
  m_edit->setFocusPolicy(Qt::NoFocus);  // 焦点只由 enterEdit 显式给予（点击/Tab 不抢）
  // 默认非编辑态：只读 + 鼠标穿透 —— 单击只选中单元格、无光标（与普通单元格一致）；
  // 鼠标穿透后点击/拖选/双击全部落回表格与容器，由容器决定何时进入编辑态
  m_edit->setReadOnly(true);
  m_edit->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  // 自足样式：扁平无边框 + 透明背景（行背景透出）+ 2px 文字边距（与表格
  // 单元格条目文字对齐）；文字颜色/字号仍继承 app/面板级 QSS 的 QLineEdit 规则
  m_edit->setStyleSheet(
      QStringLiteral("QLineEdit { border: none; background: transparent;"
                     "  padding: 0px 2px; }"));
  m_edit->installEventFilter(new CellEditFilter(this));
}

QString AuiCellEditCombo::text() const { return m_edit->text(); }

void AuiCellEditCombo::setText(const QString &t) { m_edit->setText(t); }

void AuiCellEditCombo::setPresets(const QStringList &presets) { m_presets = presets; }

QLineEdit *AuiCellEditCombo::lineEdit() const { return m_edit; }

void AuiCellEditCombo::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  // 输入框占满整格、右侧留出箭头热区（18px）
  m_edit->setGeometry(0, 0, width() - 18, height());
}

void AuiCellEditCombo::mousePressEvent(QMouseEvent *e) {
  activateRow();  // 点箭头/文字区都同步当前行
  // 右缘 18px 箭头热区：单击展开/收起候选（两种状态下均可）
  if (e->button() == Qt::LeftButton && e->position().x() >= width() - 18) {
    toggleCandidates();
    return;
  }
  // 非编辑态的文字区点击不进编辑（与普通单元格一致：单击只选中格子，
  // 双击才进入编辑态）；编辑态的文字区点击由输入框自身处理（落光标/拖选）
  QWidget::mousePressEvent(e);
}

void AuiCellEditCombo::mouseDoubleClickEvent(QMouseEvent *e) {
  // 双击进入编辑态（白底 + 光标/拖选），与普通单元格编辑器一致；
  // 编辑态内的双击由输入框原生处理（按词选词），不重复进入
  if (e->button() == Qt::LeftButton && !editing()) {
    enterEdit();
    return;
  }
  QWidget::mouseDoubleClickEvent(e);
}

void AuiCellEditCombo::paintEvent(QPaintEvent *e) {
  QWidget::paintEvent(e);
  QPainter p(this);
  // 编辑态画白底（无边框，盖住行高亮）——与普通输入框编辑器一致，
  // 浅蓝文字选区在白底上清晰可见；非编辑态完全透明（行背景透出）
  if (editing()) p.fillRect(rect(), AuiStyle::panelBackground());
  // 实心三角箭头：几何/颜色与 AuiComboBoxWidget::paintEvent 一致
  AuiStyle::drawDownArrow(p, QPointF(width() - 10, height() / 2.0 + 0.5),
                          isEnabled() ? AuiStyle::textColor() : AuiStyle::mutedTextColor());
}

bool AuiCellEditCombo::editing() const {
  return !m_edit->testAttribute(Qt::WA_TransparentForMouseEvents);
}

void AuiCellEditCombo::enterEdit() {
  m_editOriginal = m_edit->text();
  m_edit->setReadOnly(!m_editable);  // 只读列进入编辑后仍不可改字（仅选中复制）
  m_edit->setAttribute(Qt::WA_TransparentForMouseEvents, false);
  update();  // 立即画白底
  m_edit->setFocus(Qt::MouseFocusReason);
  m_edit->selectAll();  // 与普通单元格编辑器一致：双击打开即全选内容
}

void AuiCellEditCombo::exitEdit(bool revert) {
  if (revert) m_edit->setText(m_editOriginal);  // 程序性赋值不触发用户信号，无误报
  m_edit->setReadOnly(true);
  m_edit->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  m_edit->clearFocus();
  update();  // 恢复透明（行背景透出）
}

void AuiCellEditCombo::activateRow() {
  if (!m_table) return;
  for (int r = 0; r < m_table->rowCount(); ++r) {
    if (m_table->cellWidget(r, m_col) == this) {
      m_table->setCurrentCell(r, m_col);
      return;
    }
  }
}

void AuiCellEditCombo::toggleCandidates() {
  if (m_menu) {
    m_menu->close();
    return;
  }
  // 收起瞬间的同一次点击可能被转发到箭头区，250ms 内不重复展开
  if (m_closedAt.isValid() && m_closedAt.elapsed() < 250) return;
  activateRow();
  m_menu = new QMenu(this);
  m_menu->setAttribute(Qt::WA_DeleteOnClose);
  connect(m_menu, &QObject::destroyed, this, [this] { m_menu = nullptr; });
  connect(m_menu, &QMenu::aboutToHide, this, [this] { m_closedAt.start(); });
  connect(m_menu, &QMenu::triggered, this, [this](QAction *act) {
    if (act && act->text() != m_edit->text()) {
      m_edit->setText(act->text());
      if (valueCommitted) valueCommitted();
    }
  });
  const QString cur = m_edit->text();
  for (const QString &p : m_presets) {
    QAction *act = m_menu->addAction(p);
    act->setCheckable(true);
    act->setChecked(p == cur);
  }
  m_menu->popup(mapToGlobal(QPoint(0, height())));
}
