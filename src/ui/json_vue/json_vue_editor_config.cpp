/**
 * @file json_vue_editor_config.cpp
 * @brief JsonVueEditor 的配置对话框与操作按钮管理实现
 *
 * 从 json_vue_editor.cpp 拆分：onConfigureCombobox / onConfigureQuerySelect /
 * onAddButton / onEditButton / onRemoveButton。
 */

#include <QComboBox>
#include <QPushButton>
#include <QTableWidgetItem>

#include "button_config_dialog.h"
#include "combobox_config_dialog.h"
#include "json_vue_editor.h"
#include "json_vue_editor_helpers.h"
#include "src/util/ui/component/aui_message_box.h"
#include "style_config_dialog.h"

// ════════════════════════════════════════════════════════════
//  下拉框配置对话框
// ════════════════════════════════════════════════════════════

void JsonVueEditor::onConfigureCombobox() {
  int row = m_columnTable->currentRow();
  if (row < 0) return;

  auto *configBtn = qobject_cast<QPushButton *>(m_columnTable->cellWidget(row, ColConfig));
  if (!configBtn) return;

  // 读取当前配置到临时 ColumnConfig（含 editStyle/editEditable/switchEditable/displayType 等）
  ColumnConfig col;
  readColumnConfig(configBtn, col);

  // 列配置样式：使用 ColumnStyleDialog 配置表格列显示、编辑样式、通用配置等
  ColumnStyleDialog dialog(col.editStyle, this);
  dialog.setHttpConfig(m_baseUrl, m_authHeader, m_postData);
  dialog.setEditStyle(col.editStyle);
  dialog.setEditEditable(col.editEditable);
  dialog.setSwitchEditable(col.switchEditable);
  dialog.setPlaceholder(col.placeholder);
  dialog.setMaxlength(col.maxlength);
  dialog.setMinValue(col.minValue);
  dialog.setMaxValue(col.maxValue);
  dialog.setPrecision(col.precision);
  dialog.setDateFormat(col.dateFormat);
  dialog.setTextareaRows(col.textareaRows);
  dialog.setRequired(col.required);
  dialog.setColumnWidth(col.columnWidth);
  dialog.setColumnFixed(col.columnFixed);
  dialog.setFormatter(col.formatter);
  dialog.setFormSpan(col.formSpan);
  dialog.setDisplayType(col.displayType);
  dialog.setTagItems(col.tagItems);
  dialog.setBoolTrueText(col.boolTrueText);
  dialog.setBoolFalseText(col.boolFalseText);
  // 下拉框数据源（Select 编辑样式时在对话框内配置）
  dialog.setSelectUrl(col.selectUrl);
  dialog.setSelectValueField(col.selectValueField);
  dialog.setSelectLabelField(col.selectLabelField);
  dialog.setSelectPaged(col.selectPaged);
  dialog.setSelectPageKey(col.selectPageKey);
  dialog.setSelectPageSizeKey(col.selectPageSizeKey);
  dialog.setSelectPageSize(col.selectPageSize);
  dialog.setSelectSearchTitle(col.selectSearchTitle);
  dialog.setSelectSearchField(col.selectSearchField);
  dialog.setSelectMethod(col.selectMethod);
  dialog.setDefaultValue(col.defaultValue);
  dialog.setDefaultSort(col.defaultSort);
  if (dialog.exec() == QDialog::Accepted) {
    col.editStyle = dialog.editStyle();
    col.editEditable = dialog.editEditable();
    col.switchEditable = dialog.switchEditable();
    col.placeholder = dialog.placeholder();
    col.maxlength = dialog.maxlength();
    col.minValue = dialog.minValue();
    col.maxValue = dialog.maxValue();
    col.precision = dialog.precision();
    col.dateFormat = dialog.dateFormat();
    col.textareaRows = dialog.textareaRows();
    col.required = dialog.required();
    col.columnWidth = dialog.columnWidth();
    col.columnFixed = dialog.columnFixed();
    col.formatter = dialog.formatter();
    col.formSpan = dialog.formSpan();
    col.displayType = dialog.displayType();
    col.tagItems = dialog.tagItems();
    col.boolTrueText = dialog.boolTrueText();
    col.boolFalseText = dialog.boolFalseText();
    // 下拉框数据源（在对话框内已配置完成）
    col.selectUrl = dialog.selectUrl();
    col.selectValueField = dialog.selectValueField();
    col.selectLabelField = dialog.selectLabelField();
    col.selectPaged = dialog.selectPaged();
    col.selectPageKey = dialog.selectPageKey();
    col.selectPageSizeKey = dialog.selectPageSizeKey();
    col.selectPageSize = dialog.selectPageSize();
    col.selectSearchTitle = dialog.selectSearchTitle();
    col.selectSearchField = dialog.selectSearchField();
    col.selectMethod = dialog.selectMethod();
    col.defaultValue = dialog.defaultValue();
    col.defaultSort = dialog.defaultSort();
    storeColumnConfig(configBtn, col);
    configBtn->setText(columnConfigSummary(col));
    emit configChanged();
  }
}

void JsonVueEditor::onConfigureQuerySelect() {
  int row = m_queryTable->currentRow();
  if (row < 0) return;

  auto *configBtn = qobject_cast<QPushButton *>(m_queryTable->cellWidget(row, QColConfig));
  if (!configBtn) return;

  // 读取当前查询输入样式
  auto *inputStyle = qobject_cast<QComboBox *>(m_queryTable->cellWidget(row, QColInputStyle));
  if (!inputStyle) return;
  QueryInputStyle style = stringToQueryInputStyle(inputStyle->currentText());

  // 读取当前配置到临时 QueryFieldConfig
  QueryFieldConfig q;
  readQueryConfig(configBtn, q);
  q.inputStyle = style;

  if (style == QueryInputStyle::Select) {
    // select 样式使用 ComboboxConfigDialog
    ComboboxConfigDialog dialog(this);
    dialog.setConfig(q.selectUrl, q.selectValueField, q.selectLabelField);
    dialog.setPagedConfig(q.selectPaged, q.selectPageKey, q.selectPageSizeKey, q.selectPageSize,
                      q.selectSearchTitle, q.selectSearchField, q.selectMethod);
    dialog.setHttpConfig(m_baseUrl, m_authHeader, m_postData);
    if (dialog.exec() == QDialog::Accepted) {
      q.selectUrl = dialog.url();
      q.selectValueField = dialog.valueField();
      q.selectLabelField = dialog.labelField();
      q.selectPaged = dialog.paged();
      q.selectPageKey = dialog.pageKey();
      q.selectPageSizeKey = dialog.pageSizeKey();
      q.selectPageSize = dialog.pageSize();
      q.selectSearchTitle = dialog.searchTitle();
      q.selectSearchField = dialog.searchField();
      q.selectMethod = dialog.method();
      storeQueryConfig(configBtn, q);
      configBtn->setText(queryConfigSummary(q));
      emit configChanged();
    }
  } else {
    // text/date 样式使用 QueryStyleDialog
    QueryStyleDialog dialog(style, this);
    dialog.setPlaceholder(q.placeholder);
    dialog.setDateFormat(q.dateFormat);
    if (dialog.exec() == QDialog::Accepted) {
      q.placeholder = dialog.placeholder();
      q.dateFormat = dialog.dateFormat();
      storeQueryConfig(configBtn, q);
      configBtn->setText(queryConfigSummary(q));
      emit configChanged();
    }
  }
}

// ════════════════════════════════════════════════════════════
//  操作按钮管理
// ════════════════════════════════════════════════════════════

void JsonVueEditor::onAddButton() {
  ButtonConfigDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted) {
    ButtonConfig btn = dialog.getData();
    if (btn.label.isEmpty()) {
      return;
    }
    // 如果 actionKey 为空，自动生成
    if (btn.actionKey.isEmpty()) {
      btn.actionKey = QStringLiteral("btn%1").arg(m_buttons.size() + 1);
    }
    m_buttons.append(btn);
    // 刷新表格
    int row = m_buttonTable->rowCount();
    m_buttonTable->insertRow(row);
    m_buttonTable->setItem(row, BColLabel, new QTableWidgetItem(btn.label));
    m_buttonTable->setItem(row, BColActionKey, new QTableWidgetItem(btn.actionKey));
    m_buttonTable->setItem(row, BColPosition,
                           new QTableWidgetItem(buttonPositionToString(btn.position)));
    m_buttonTable->setItem(row, BColActionType,
                           new QTableWidgetItem(buttonActionTypeToString(btn.actionType)));

    // 配置按钮（⚙ + 摘要文本），双击打开配置对话框
    auto *configBtn = new QPushButton(buttonConfigSummary(btn), this);
    m_buttonTable->setCellWidget(row, BColConfig, configBtn);
    configBtn->installEventFilter(this);  // 双击打开按钮配置

    emit configChanged();
  }
}

void JsonVueEditor::onEditButton(int row) {
  if (row < 0 || row >= m_buttons.size()) {
    return;
  }
  ButtonConfigDialog dialog(m_buttons[row], this);
  if (dialog.exec() == QDialog::Accepted) {
    ButtonConfig btn = dialog.getData();
    if (btn.label.isEmpty()) {
      return;
    }
    m_buttons[row] = btn;
    // 刷新表格行
    m_buttonTable->item(row, BColLabel)->setText(btn.label);
    m_buttonTable->item(row, BColActionKey)->setText(btn.actionKey);
    m_buttonTable->item(row, BColPosition)->setText(buttonPositionToString(btn.position));
    m_buttonTable->item(row, BColActionType)->setText(buttonActionTypeToString(btn.actionType));
    // 刷新配置按钮摘要
    auto *configBtn = qobject_cast<QPushButton *>(m_buttonTable->cellWidget(row, BColConfig));
    if (configBtn) {
      configBtn->setText(buttonConfigSummary(btn));
    }
    emit configChanged();
  }
}

void JsonVueEditor::onRemoveButton() {
  int row = m_buttonTable->currentRow();
  if (row < 0) {
    return;
  }
  if (!AuiMessageBox::confirm(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定要删除当前操作按钮吗？"))) {
    return;
  }
  m_buttonTable->removeRow(row);
  if (row < m_buttons.size()) {
    m_buttons.removeAt(row);
  }
  emit configChanged();
}
