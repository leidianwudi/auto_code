/**
 * @file json_vue_editor_config.cpp
 * @brief JsonVueEditor 的配置对话框与操作按钮管理实现
 *
 * 从 json_vue_editor.cpp 拆分：onConfigureCombobox / onConfigureQuerySelect /
 * onAddButton / onEditButton / onRemoveButton。
 */

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "button_config_dialog.h"
#include "combobox_config_dialog.h"
#include "config_dialog_common.h"
#include "json_vue_editor.h"
#include "json_vue_editor_helpers.h"
#include "src/util/ui/component/aui_message_box.h"
#include "src/util/ui/component/aui_tree_combo.h"
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
  // 字段设置面板：顶部显示当前编辑的字段名。
  // 注意 dataName 存在表格「字段名」单元格上（storeColumnConfig 不存它），
  // 需从表格项读取，col.dataName 恒为空
  auto *dnItem = m_columnTable->item(row, ColDataName);
  const QString dataName = dnItem ? dnItem->text().trimmed() : QString();
  dialog.setFieldName(dataName.isEmpty() ? QStringLiteral("（未填字段名）") : dataName);
  dialog.setHttpConfig(m_baseUrl, m_authHeader, m_postData);
  dialog.setSearchRoot(m_jsonvueDir);
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
  dialog.setBoolSourceRef(col.boolSourceFile, col.boolSourceId);
  // 图片上传预设（Image 编辑样式时在对话框内配置）
  dialog.setUploadSourceRef(col.uploadSourceFile, col.uploadSourceId);
  // 下拉框数据源（Select 编辑样式时在对话框内配置）
  dialog.setSelectUrl(col.selectUrl);
  dialog.setSelectSourceFile(col.selectSourceFile);
  dialog.setSelectSourceId(col.selectSourceId);
  dialog.setSelectValueField(col.selectValueField);
  dialog.setSelectLabelField(col.selectLabelField);
  dialog.setSelectPaged(col.selectPaged);
  dialog.setSelectPageKey(col.selectPageKey);
  dialog.setSelectPageSizeKey(col.selectPageSizeKey);
  dialog.setSelectPageSize(col.selectPageSize);
  dialog.setSelectSearchTitle(col.selectSearchTitle);
  dialog.setSelectSearchField(col.selectSearchField);
  dialog.setSelectMethod(col.selectMethod);
  // 字段取值域（方案 B：顶部声明区）。必须放在 select 组之后：
  // setDomainUrl 走 selectUrl 缓存，setDomainSourceRef 同步 select 源缓存，
  // 后调用者胜出，保证域值不被旧键空值覆盖
  dialog.setDomainType(col.domainType);
  dialog.setDomainSourceRef(col.domainSourceFile, col.domainSourceId);
  dialog.setDomainUrl(col.domainUrl);
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
    col.boolSourceFile = dialog.boolSourceFile();
    col.boolSourceId = dialog.boolSourceId();
    // 字段取值域（方案 B：列渲染/编辑/查询三处共享的字段级声明）
    col.domainType = dialog.domainType();
    col.domainSourceFile = dialog.domainSourceFile();
    col.domainSourceId = dialog.domainSourceId();
    col.domainUrl = dialog.domainUrl();
    // 图片上传预设（在对话框内已配置完成）
    col.uploadSourceFile = dialog.uploadSourceFile();
    col.uploadSourceId = dialog.uploadSourceId();
    // 下拉框数据源（在对话框内已配置完成）
    col.selectUrl = dialog.selectUrl();
    col.selectSourceFile = dialog.selectSourceFile();
    col.selectSourceId = dialog.selectSourceId();
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
    // 查询筛选取值域设置（方案 B）：
    //   沿用（默认）→ 生成时从同名列取值域实时推导（含布尔枚举列，自动带「全部」空选项），
    //                 列配置修改后查询自动跟随；
    //   覆盖 → 从静态源/全局枚举候选中选择独立数据源（写入查询字段自身引用键）
    auto *dataCombo = qobject_cast<QComboBox *>(m_queryTable->cellWidget(row, QColDataName));
    const QString dataName = dataCombo ? dataCombo->currentText().trimmed() : QString();

    // 找同名列的取值域（摘要展示 + 继承默认值判定）
    ColumnConfig col;
    bool colReady = false;
    for (int r = 0; r < m_columnTable->rowCount(); ++r) {
      auto *item = m_columnTable->item(r, ColDataName);
      if (!item || item->text().trimmed() != dataName) continue;
      auto *cbtn = qobject_cast<QPushButton *>(m_columnTable->cellWidget(r, ColConfig));
      if (cbtn) {
        readColumnConfig(cbtn, col);
        colReady = true;
      }
      break;
    }
    const bool colHasDomain = colReady && !col.domainType.isEmpty();

    QDialog dlg(configBtn);
    dlg.setWindowTitle(QStringLiteral("查询数据源设置"));
    auto *lay = new QVBoxLayout(&dlg);
    lay->addWidget(new QLabel(QStringLiteral("字段「%1」列取值域：%2")
                                  .arg(dataName, domainSummary(col)),
                              &dlg));
    auto *inheritCheck =
        new QCheckBox(QStringLiteral("沿用列配置取值域（列修改后查询自动跟随）"), &dlg);
    inheritCheck->setChecked(q.domainInherit && colHasDomain);
    lay->addWidget(inheritCheck);

    auto *srcCombo = new AuiTreeCombo(&dlg);
    srcCombo->setMinimumWidth(360);
    QHash<QString, QPair<QString, QString>> optionTexts;
    QHash<QString, QString> optionPreviews;
    buildDomainSourceCandidates(srcCombo, m_jsonvueDir, false, &optionTexts, &optionPreviews);
    if (q.selectSourceFile.isEmpty() && q.selectSourceId.isEmpty()) {
      srcCombo->selectByData(QVariant(QString()));
    } else {
      srcCombo->selectByData(QVariant(q.selectSourceFile + QStringLiteral("#") +
                                      q.selectSourceId));
    }
    srcCombo->setEnabled(!inheritCheck->isChecked());
    lay->addWidget(srcCombo);
    connect(inheritCheck, &QCheckBox::toggled, srcCombo, &QWidget::setEnabled);

    auto *btnRow = new QHBoxLayout;
    auto *okBtn = new QPushButton(QStringLiteral("确定"), &dlg);
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), &dlg);
    btnRow->addStretch();
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    lay->addLayout(btnRow);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
      q.domainInherit = inheritCheck->isChecked();
      if (!q.domainInherit) {
        // 覆盖模式：读取独立数据源引用
        const QString ref = srcCombo->currentEntryData().toString();
        q.selectSourceFile.clear();
        q.selectSourceId.clear();
        if (!ref.isEmpty()) {
          const int sep = ref.lastIndexOf(QLatin1Char('#'));
          q.selectSourceFile = ref.left(sep);
          q.selectSourceId = ref.mid(sep + 1);
        }
        if (q.selectSourceFile.isEmpty()) {
          AuiMessageBox::show(this, QStringLiteral("未选择数据源"),
                              QStringLiteral("取消沿用列取值域时必须选择独立数据源"));
          return;
        }
      }
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
  dialog.setSearchRoot(m_jsonvueDir);
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
  dialog.setSearchRoot(m_jsonvueDir);
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

void JsonVueEditor::onButtonMoveUp() {
  int row = m_buttonTable->currentRow();
  if (row <= 0 || row >= m_buttons.size()) return;
  m_buttons.swapItemsAt(row, row - 1);
  refreshButtonRow(row - 1);
  refreshButtonRow(row);
  m_buttonTable->selectRow(row - 1);
  emit configChanged();
}

void JsonVueEditor::onButtonMoveDown() {
  int row = m_buttonTable->currentRow();
  if (row < 0 || row >= m_buttonTable->rowCount() - 1) return;
  if (row >= m_buttons.size() - 1) return;
  m_buttons.swapItemsAt(row, row + 1);
  refreshButtonRow(row);
  refreshButtonRow(row + 1);
  m_buttonTable->selectRow(row + 1);
  emit configChanged();
}

/// 把 m_buttons[row] 应用到表格行显示（文字/标识/位置/行为/配置摘要）
void JsonVueEditor::refreshButtonRow(int row) {
  if (row < 0 || row >= m_buttons.size()) return;
  const ButtonConfig &btn = m_buttons[row];
  m_buttonTable->item(row, BColLabel)->setText(btn.label);
  m_buttonTable->item(row, BColActionKey)->setText(btn.actionKey);
  m_buttonTable->item(row, BColPosition)->setText(buttonPositionToString(btn.position));
  m_buttonTable->item(row, BColActionType)->setText(buttonActionTypeToString(btn.actionType));
  auto *configBtn = qobject_cast<QPushButton *>(m_buttonTable->cellWidget(row, BColConfig));
  if (configBtn) configBtn->setText(buttonConfigSummary(btn));
}
