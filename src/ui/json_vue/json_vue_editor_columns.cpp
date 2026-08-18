/**
 * @file json_vue_editor_columns.cpp
 * @brief JsonVueEditor 的列 / 查询字段列表操作实现
 *
 * 从 json_vue_editor.cpp 拆分：onMoveUp / onMoveDown / onAddColumn / onRemoveColumn /
 * onAddQueryField / onRemoveQueryField / columnDataNames / refreshQueryFieldDataNames。
 */

#include <QCheckBox>
#include <QComboBox>
#include <QPoint>
#include <QPushButton>
#include <QTableWidgetItem>

#include "json_vue_editor.h"
#include "json_vue_editor_helpers.h"
#include "src/util/ui/component/aui_message_box.h"

// ════════════════════════════════════════════════════════════
//  列配置操作
// ════════════════════════════════════════════════════════════

void JsonVueEditor::onMoveUp() {
  int row = m_columnTable->currentRow();
  if (row <= 0) return;
  // 简单实现：用 collectConfig + loadConfig 重新加载（避免 cellWidget 复杂性）
  JsonVueConfig cfg = collectConfig();
  if (row - 1 >= 0 && row < cfg.columns.size()) {
    cfg.columns.swapItemsAt(row, row - 1);
    loadConfig(cfg);
    m_columnTable->selectRow(row - 1);
    emit configChanged();
  }
}

void JsonVueEditor::onMoveDown() {
  int row = m_columnTable->currentRow();
  if (row < 0 || row >= m_columnTable->rowCount() - 1) return;

  JsonVueConfig cfg = collectConfig();
  if (row + 1 < cfg.columns.size()) {
    cfg.columns.swapItemsAt(row, row + 1);
    loadConfig(cfg);
    m_columnTable->selectRow(row + 1);
    emit configChanged();
  }
}

void JsonVueEditor::onAddColumn() {
  int row = m_columnTable->rowCount();
  m_columnTable->insertRow(row);

  m_columnTable->setItem(row, ColDataName, new QTableWidgetItem());

  auto *qVis = new QCheckBox;
  qVis->setChecked(true);
  m_columnTable->setCellWidget(row, ColQueryVisible, qVis);
  connectCellWidgetSignals(qVis);

  m_columnTable->setItem(row, ColTitle, new QTableWidgetItem());

  auto *eVis = new QCheckBox;
  eVis->setChecked(true);
  m_columnTable->setCellWidget(row, ColEditVisible, eVis);
  connectCellWidgetSignals(eVis);

  // 配置按钮（⚙ + 摘要文本，含显示类型/编辑样式/通用配置）
  ColumnConfig emptyCol;
  auto *configBtn = new QPushButton(columnConfigSummary(emptyCol), this);
  storeColumnConfig(configBtn, emptyCol);
  m_columnTable->setCellWidget(row, ColConfig, configBtn);
  configBtn->installEventFilter(this);  // 双击打开样式配置
  // 右键菜单：复制配置 / 粘贴配置
  configBtn->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(configBtn, &QPushButton::customContextMenuRequested, this,
          [this, configBtn](const QPoint &pos) {
            for (int r = 0; r < m_columnTable->rowCount(); ++r) {
              if (m_columnTable->cellWidget(r, ColConfig) == configBtn) {
                showColumnConfigMenu(r, configBtn->mapToGlobal(pos));
                break;
              }
            }
          });

  emit configChanged();
}

void JsonVueEditor::onRemoveColumn() {
  int row = m_columnTable->currentRow();
  if (row < 0) return;
  if (!AuiMessageBox::confirm(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定要删除当前列配置吗？"))) {
    return;
  }
  m_columnTable->removeRow(row);
  refreshQueryFieldDataNames();
  emit configChanged();
}

// ════════════════════════════════════════════════════════════
//  查询字段操作
// ════════════════════════════════════════════════════════════

void JsonVueEditor::onAddQueryField() {
  int row = m_queryTable->rowCount();
  m_queryTable->insertRow(row);

  m_queryTable->setItem(row, QColDisplayName, new QTableWidgetItem());

  auto *dataCombo = newTableCombo();
  dataCombo->addItems(columnDataNames());
  m_queryTable->setCellWidget(row, QColDataName, dataCombo);
  connectCellWidgetSignals(dataCombo);

  auto *inputStyle = newTableCombo();
  inputStyle->addItems({QString::fromLatin1(JsonVueStyle::kText),
                        QString::fromLatin1(JsonVueStyle::kDate),
                        QString::fromLatin1(JsonVueStyle::kSelect)});
  m_queryTable->setCellWidget(row, QColInputStyle, inputStyle);
  connectCellWidgetSignals(inputStyle);

  auto *relCombo = newTableCombo();
  relCombo->addItem(QStringLiteral("普通"), QStringLiteral("="));
  relCombo->addItem(QStringLiteral("范围"), QString::fromLatin1(JsonVueStyle::kRange));
  m_queryTable->setCellWidget(row, QColRelation, relCombo);
  connectCellWidgetSignals(relCombo);

  // 配置按钮（⚙ + 摘要文本）
  QueryFieldConfig emptyQ;
  auto *qConfigBtn = new QPushButton(queryConfigSummary(emptyQ), this);
  storeQueryConfig(qConfigBtn, emptyQ);
  m_queryTable->setCellWidget(row, QColConfig, qConfigBtn);
  qConfigBtn->installEventFilter(this);  // 双击打开查询样式配置

  emit configChanged();
}

void JsonVueEditor::onRemoveQueryField() {
  int row = m_queryTable->currentRow();
  if (row < 0) return;
  if (!AuiMessageBox::confirm(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定要删除当前查询字段吗？"))) {
    return;
  }
  m_queryTable->removeRow(row);
  emit configChanged();
}

QStringList JsonVueEditor::columnDataNames() const {
  QStringList names;
  for (int row = 0; row < m_columnTable->rowCount(); ++row) {
    auto *item = m_columnTable->item(row, ColDataName);
    if (item) {
      QString name = item->text().trimmed();
      if (!name.isEmpty()) names.append(name);
    }
  }
  return names;
}

void JsonVueEditor::refreshQueryFieldDataNames() {
  QStringList names = columnDataNames();
  for (int row = 0; row < m_queryTable->rowCount(); ++row) {
    auto *combo = qobject_cast<QComboBox *>(m_queryTable->cellWidget(row, QColDataName));
    if (combo) {
      QString cur = combo->currentText();
      combo->clear();
      combo->addItems(names);
      int idx = combo->findText(cur);
      if (idx >= 0) combo->setCurrentIndex(idx);
    }
  }
}
