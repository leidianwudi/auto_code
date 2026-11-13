/**
 * @file json_global_enum_dialog.cpp
 * @brief 全局枚举编辑对话框实现
 */

#include "json_global_enum_dialog.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "src/util/ui/component/aui_cell_edit_combo.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/component/aui_config_dialog.h"
#include "src/util/ui/component/aui_message_box.h"
#include "src/util/ui/component/aui_required_label.h"

// ════════════════════════════════════════════════════════════
//  构造 / 界面构建
// ════════════════════════════════════════════════════════════

JsonGlobalEnumDialog::JsonGlobalEnumDialog(QWidget *parent) : QDialog(parent) { setupUI(); }

void JsonGlobalEnumDialog::setupUI() {
  ConfigDialogFrame frame =
      beginConfigDialog(this, QStringLiteral("全局枚举配置"), QMargins(12, 10, 12, 10), 6);
  auto *layout = frame.contentLayout;

  // ── 名称 + 说明 ──
  auto *topRow = new QHBoxLayout;
  auto *nameLabel = new AuiRequiredLabel(QStringLiteral("名称:"), frame.contentWidget);
  topRow->addWidget(nameLabel);
  m_nameEdit = new QLineEdit(frame.contentWidget);
  m_nameEdit->setPlaceholderText(
      QStringLiteral("必填，snake_case，如 is_enable（生成枚举名 EnumIsEnable）"));
  m_nameEdit->setMinimumWidth(180);
  topRow->addWidget(m_nameEdit, 1);
  topRow->addSpacing(16);
  topRow->addWidget(new QLabel(QStringLiteral("说明:")));
  m_remarkEdit = new QLineEdit(frame.contentWidget);
  m_remarkEdit->setPlaceholderText(QStringLiteral("枚举说明，如 是否启用"));
  topRow->addWidget(m_remarkEdit, 1);
  layout->addLayout(topRow);

  // ── 选项表格 ──
  auto *btnRow = new QHBoxLayout;
  m_addOptionBtn = new QPushButton(QStringLiteral("+ 添加选项"), frame.contentWidget);
  m_removeOptionBtn = new QPushButton(QStringLiteral("- 删除选项"), frame.contentWidget);
  m_optionUpBtn = new QPushButton(QStringLiteral("↑ 上移"), frame.contentWidget);
  m_optionDownBtn = new QPushButton(QStringLiteral("↓ 下移"), frame.contentWidget);
  btnRow->addWidget(m_addOptionBtn);
  btnRow->addWidget(m_removeOptionBtn);
  btnRow->addWidget(m_optionUpBtn);
  btnRow->addWidget(m_optionDownBtn);
  btnRow->addStretch();
  layout->addLayout(btnRow);

  m_optionTable = makeConfigTable({{QStringLiteral("键名"), QHeaderView::Stretch, 0},
                                   {QStringLiteral("显示文本"), QHeaderView::Stretch, 0},
                                   {QStringLiteral("实际值"), QHeaderView::Stretch, 0},
                                   {QStringLiteral("类型"), QHeaderView::Interactive, 100}},
                                  frame.contentWidget, 100, 0, QAbstractItemView::SelectRows);
  layout->addWidget(m_optionTable, 1);

  connect(m_addOptionBtn, &QPushButton::clicked, this, &JsonGlobalEnumDialog::onAddOption);
  connect(m_removeOptionBtn, &QPushButton::clicked, this, &JsonGlobalEnumDialog::onRemoveOption);
  connect(m_optionUpBtn, &QPushButton::clicked, this, &JsonGlobalEnumDialog::onOptionUp);
  connect(m_optionDownBtn, &QPushButton::clicked, this, &JsonGlobalEnumDialog::onOptionDown);

  finishConfigDialog(this, frame);
  setMinimumSize(560, 420);
}

// ════════════════════════════════════════════════════════════
//  配置读写
// ════════════════════════════════════════════════════════════

void JsonGlobalEnumDialog::setEnum(const JsonGlobalEnum &e) {
  m_id = e.id;
  m_nameEdit->setText(e.name);
  m_remarkEdit->setText(e.remark);
  populateOptions(e.options);
}

JsonGlobalEnum JsonGlobalEnumDialog::enumData() const {
  JsonGlobalEnum e;
  e.id = m_id;
  e.name = m_nameEdit->text().trimmed();
  e.remark = m_remarkEdit->text().trimmed();
  e.options = collectOptions(nullptr);
  return e;
}

void JsonGlobalEnumDialog::accept() {
  // 名称必填：后端枚举名 / 前端函数名都由它推导
  if (m_nameEdit->text().trimmed().isEmpty()) {
    AuiMessageBox::show(this, QStringLiteral("提示"), QStringLiteral("请填写枚举名称"));
    return;
  }
  // 选项完整性与非空校验（键名是后端 TS 成员名，必填）
  QString err;
  const QVector<JsonGlobalEnumOption> opts = collectOptions(&err);
  if (!err.isEmpty()) {
    AuiMessageBox::show(this, QStringLiteral("提示"), err);
    return;
  }
  if (opts.isEmpty()) {
    AuiMessageBox::show(this, QStringLiteral("提示"), QStringLiteral("请至少添加一个选项"));
    return;
  }
  QDialog::accept();
}

// ════════════════════════════════════════════════════════════
//  选项操作
// ════════════════════════════════════════════════════════════

void JsonGlobalEnumDialog::onAddOption() {
  int row = m_optionTable->rowCount();
  m_optionTable->insertRow(row);
  for (int c = 0; c < GEOColValueType; ++c) {
    m_optionTable->setItem(row, c, new QTableWidgetItem());
  }
  // 类型列：常驻只读下拉（字符串默认/数字；文字可拖选/双击全选/Ctrl+C 复制，
  // 点箭头展开候选）——与 jsontable 设计器的单元格下拉同款控件
  auto *combo = new AuiCellEditCombo(m_optionTable, GEOColValueType,
                                     {QStringLiteral("字符串"), QStringLiteral("数字")},
                                     m_optionTable, true);
  combo->setText(QStringLiteral("字符串"));
  m_optionTable->setCellWidget(row, GEOColValueType, combo);
  m_optionTable->setCurrentCell(row, GEOColKey);
}

void JsonGlobalEnumDialog::onRemoveOption() {
  int row = m_optionTable->currentRow();
  if (row < 0) return;
  m_optionTable->removeRow(row);
}

void JsonGlobalEnumDialog::onOptionUp() {
  int row = m_optionTable->currentRow();
  if (row <= 0) return;
  swapRows(row, row - 1);
  m_optionTable->selectRow(row - 1);
}

void JsonGlobalEnumDialog::onOptionDown() {
  int row = m_optionTable->currentRow();
  if (row < 0 || row >= m_optionTable->rowCount() - 1) return;
  swapRows(row, row + 1);
  m_optionTable->selectRow(row + 1);
}

void JsonGlobalEnumDialog::swapRows(int a, int b) {
  // 交换两行的 item（takeItem 解除所有权后再放置，安全）；类型列（cellWidget）单独处理
  for (int c = 0; c < m_optionTable->columnCount(); ++c) {
    if (c == GEOColValueType) continue;
    QTableWidgetItem *ia = m_optionTable->takeItem(a, c);
    QTableWidgetItem *ib = m_optionTable->takeItem(b, c);
    m_optionTable->setItem(a, c, ib);
    m_optionTable->setItem(b, c, ia);
  }
  // cellWidget 不能直接交换指针：setCellWidget 会删除目标单元格旧控件，产生悬垂引用
  // （上移/下移后再点添加选项即崩溃），改为交换下拉框当前文本（程序性赋值不触发用户信号）
  auto *ca = dynamic_cast<AuiCellEditCombo *>(m_optionTable->cellWidget(a, GEOColValueType));
  auto *cb = dynamic_cast<AuiCellEditCombo *>(m_optionTable->cellWidget(b, GEOColValueType));
  if (ca && cb) {
    const QString ta = ca->text();
    ca->setText(cb->text());
    cb->setText(ta);
  }
}

QVector<JsonGlobalEnumOption> JsonGlobalEnumDialog::collectOptions(QString *error) const {
  QVector<JsonGlobalEnumOption> out;
  for (int r = 0; r < m_optionTable->rowCount(); ++r) {
    JsonGlobalEnumOption o;
    const auto *keyItem = m_optionTable->item(r, GEOColKey);
    const auto *labelItem = m_optionTable->item(r, GEOColLabel);
    const auto *valueItem = m_optionTable->item(r, GEOColValue);
    o.key = keyItem ? keyItem->text().trimmed() : QString();
    o.label = labelItem ? labelItem->text().trimmed() : QString();
    o.value = valueItem ? valueItem->text().trimmed() : QString();
    if (o.key.isEmpty() && o.label.isEmpty() && o.value.isEmpty()) {
      continue;  // 整行为空 → 跳过
    }
    // 键名/显示文本/实际值必填（键名生成后端 TS 成员名，缺失会生成非法标识符）
    if (o.key.isEmpty() || o.label.isEmpty() || o.value.isEmpty()) {
      if (error) {
        *error = QStringLiteral("第 %1 行选项的键名/显示文本/实际值不能为空").arg(r + 1);
      }
      return {};
    }
    if (auto *combo = dynamic_cast<AuiCellEditCombo *>(m_optionTable->cellWidget(r, GEOColValueType))) {
      o.valueType = combo->text() == QStringLiteral("数字") ? QStringLiteral("number") : QString();
    }
    out.append(o);
  }
  return out;
}

void JsonGlobalEnumDialog::populateOptions(const QVector<JsonGlobalEnumOption> &options) {
  m_optionTable->setRowCount(0);
  for (const auto &o : options) {
    int row = m_optionTable->rowCount();
    m_optionTable->insertRow(row);
    m_optionTable->setItem(row, GEOColKey, new QTableWidgetItem(o.key));
    m_optionTable->setItem(row, GEOColLabel, new QTableWidgetItem(o.label));
    m_optionTable->setItem(row, GEOColValue, new QTableWidgetItem(o.value));
    auto *combo = new AuiCellEditCombo(m_optionTable, GEOColValueType,
                                       {QStringLiteral("字符串"), QStringLiteral("数字")},
                                       m_optionTable, true);
    combo->setText(o.valueType == QStringLiteral("number") ? QStringLiteral("数字")
                                                           : QStringLiteral("字符串"));
    m_optionTable->setCellWidget(row, GEOColValueType, combo);
  }
}
