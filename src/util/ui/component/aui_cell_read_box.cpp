/**
 * @file aui_cell_read_box.cpp
 * @brief 表格单元格常驻「只读文本框」控件实现
 */

#include "aui_cell_read_box.h"

#include <QColor>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QTableWidget>

#include "aui_style.h"

namespace {

/// 编辑态守卫（装在内部输入框上）：Esc/回车/失焦退出编辑态
/// （与 AuiCellEditCombo 的 CellEditFilter 同模式）
class CellReadFilter : public QObject {
 public:
  explicit CellReadFilter(AuiCellReadBox *cell) : QObject(cell), m_cell(cell) {}

  bool eventFilter(QObject *obj, QEvent *e) override {
    if (e->type() == QEvent::KeyPress) {
      const auto *ke = static_cast<QKeyEvent *>(e);
      if (ke->key() == Qt::Key_Escape || ke->key() == Qt::Key_Return ||
          ke->key() == Qt::Key_Enter) {
        m_cell->exitEdit();
        return true;
      }
    } else if (e->type() == QEvent::FocusOut) {
      m_cell->exitEdit();
    }
    return QObject::eventFilter(obj, e);
  }

 private:
  AuiCellReadBox *m_cell = nullptr;
};

}  // namespace

AuiCellReadBox::AuiCellReadBox(QTableWidget *table, int row, int col, const QString &text,
                               QWidget *parent)
    : QWidget(parent), m_table(table), m_row(row), m_col(col), m_text(text) {
  auto *lay = new QHBoxLayout(this);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);
  m_edit = new QLineEdit(this);
  m_edit->setReadOnly(true);
  m_edit->setFrame(false);
  m_edit->setFocusPolicy(Qt::NoFocus);  // 焦点只由 enterEdit 显式给予
  // 非编辑态鼠标穿透：点击/双击落回容器，表格才能绘制原生选中高亮
  //（这是"选中变色"的唯一可靠来源；QSS item:selected / :focus 均不可靠）
  m_edit->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  m_edit->setStyleSheet(AuiStyle::readBoxStyleSheet(QStringLiteral("QLineEdit")));
  m_edit->installEventFilter(new CellReadFilter(this));
  syncEditVisibleText();
  lay->addWidget(m_edit);
}

QString AuiCellReadBox::text() const {
  return m_text;
}

void AuiCellReadBox::setText(const QString &t) {
  m_text = t;
  syncEditVisibleText();
}

void AuiCellReadBox::setTip(const QString &tip) {
  if (m_edit) m_edit->setToolTip(tip);
}

void AuiCellReadBox::activateCell() {
  if (m_table) m_table->setCurrentCell(m_row, m_col);
}

bool AuiCellReadBox::editing() const {
  return m_edit && !m_edit->testAttribute(Qt::WA_TransparentForMouseEvents);
}

void AuiCellReadBox::syncEditVisibleText() {
  // 文字始终由输入框显示（两态同源，与 AuiCellEditCombo 一致）：双击进入
  // 编辑态时文字不位移；item 文字由 CenterCheckDelegate 跳过绘制（无重影）
  if (m_edit) m_edit->setText(m_text);
}

void AuiCellReadBox::enterEdit() {
  m_edit->setAttribute(Qt::WA_TransparentForMouseEvents, false);
  syncEditVisibleText();
  update();  // 立即画白底
  m_edit->setFocus(Qt::MouseFocusReason);
  m_edit->selectAll();  // 与普通单元格编辑器一致：双击打开即全选内容
}

void AuiCellReadBox::exitEdit() {
  m_edit->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  m_edit->clearFocus();
  syncEditVisibleText();
  update();  // 恢复透明（item 文字/行背景透出）
}

void AuiCellReadBox::mousePressEvent(QMouseEvent *e) {
  // 非编辑态单击：把所在格设为当前格 → 表格原生选中高亮（整格蓝）
  activateCell();
  QWidget::mousePressEvent(e);
}

void AuiCellReadBox::mouseDoubleClickEvent(QMouseEvent *e) {
  // 双击进入编辑态（白底 + 光标/拖选），与普通单元格编辑器一致
  if (e->button() == Qt::LeftButton && !editing()) {
    enterEdit();
    return;
  }
  QWidget::mouseDoubleClickEvent(e);
}

void AuiCellReadBox::paintEvent(QPaintEvent *e) {
  QWidget::paintEvent(e);
  // 编辑态画白底（盖住行高亮）——与普通输入框编辑器一致，文字选区清晰可见；
  // 非编辑态完全透明（item 文字与表格行背景透出）
  if (editing()) {
    QPainter p(this);
    p.fillRect(rect(), AuiStyle::panelBackground());
  }
}
