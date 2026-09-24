/**
 * @file json_source_dialog.cpp
 * @brief .jsonsource 数据源编辑对话框实现
 */

#include "json_source_dialog.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "src/ui/json_vue/config_dialog_common.h"
#include "src/ui/json_vue/select_source_panel.h"
#include "src/util/common/code_constants.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/component/aui_message_box.h"
#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  构造 / 界面构建
// ════════════════════════════════════════════════════════════

JsonSourceDialog::JsonSourceDialog(QWidget *parent) : QDialog(parent) { setupUI(); }

void JsonSourceDialog::setupUI() {
  ConfigDialogFrame frame =
      beginConfigDialog(this, QStringLiteral("数据源配置"), QMargins(12, 10, 12, 10), 6);
  auto *layout = frame.contentLayout;

  // ── 类型 + 说明 ──
  auto *topRow = new QHBoxLayout;
  topRow->addWidget(new QLabel(QStringLiteral("类型:")));
  m_typeCombo = AuiComboBox::create(frame.contentWidget);
  m_typeCombo->addItem(QStringLiteral("静态数据源"), QString::fromLatin1(JsonSourceType::kStatic));
  m_typeCombo->addItem(QStringLiteral("动态数据源"), QString::fromLatin1(JsonSourceType::kDynamic));
  m_typeCombo->setMinimumWidth(120);
  topRow->addWidget(m_typeCombo);
  topRow->addSpacing(16);
  topRow->addWidget(new QLabel(QStringLiteral("说明:")));
  m_remarkEdit = new QLineEdit(frame.contentWidget);
  m_remarkEdit->setPlaceholderText(QStringLiteral("数据源说明，如下拉框用途"));
  topRow->addWidget(m_remarkEdit, 1);
  layout->addLayout(topRow);

  // ── 静态/动态配置区 ──
  m_stack = new QStackedWidget(frame.contentWidget);
  setupStaticPage();
  setupDynamicPage();
  layout->addWidget(m_stack, 1);

  connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &JsonSourceDialog::onTypeChanged);

  finishConfigDialog(this, frame);
  setMinimumSize(560, 420);
  onTypeChanged(m_typeCombo->currentIndex());
}

void JsonSourceDialog::setupStaticPage() {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  // 函数 URL（必填）：作为生成 api 函数名的依据（文件名Static + url名），
  // 避免用序号命名。与动态数据源 url 类似，如 enableState
  auto *urlRow = new QHBoxLayout;
  auto *urlLabel = new QLabel(QStringLiteral("函数URL:"), page);
  urlLabel->setStyleSheet(QStringLiteral("color: #d03050;"));
  urlRow->addWidget(urlLabel);
  m_staticUrlEdit = new QLineEdit(page);
  m_staticUrlEdit->setPlaceholderText(
      QStringLiteral("必填，用于生成函数名，如 enableState（生成 booleanStaticEnableState）"));
  m_staticUrlEdit->setMinimumHeight(28);
  urlRow->addWidget(m_staticUrlEdit, 1);
  layout->addLayout(urlRow);

  auto *btnRow = new QHBoxLayout;
  m_addOptionBtn = new QPushButton(QStringLiteral("+ 添加选项"), page);
  m_removeOptionBtn = new QPushButton(QStringLiteral("- 删除选项"), page);
  m_optionUpBtn = new QPushButton(QStringLiteral("↑ 上移"), page);
  m_optionDownBtn = new QPushButton(QStringLiteral("↓ 下移"), page);
  btnRow->addWidget(m_addOptionBtn);
  btnRow->addWidget(m_removeOptionBtn);
  btnRow->addWidget(m_optionUpBtn);
  btnRow->addWidget(m_optionDownBtn);
  btnRow->addStretch();
  layout->addLayout(btnRow);

  m_optionTable = makeConfigTable({{QStringLiteral("显示文本"), QHeaderView::Stretch, 0},
                                   {QStringLiteral("实际值"), QHeaderView::Stretch, 0},
                                   {QStringLiteral("类型"), QHeaderView::Interactive, 100}},
                                  page, 100, 0, QAbstractItemView::SelectRows);
  layout->addWidget(m_optionTable, 1);

  connect(m_addOptionBtn, &QPushButton::clicked, this, &JsonSourceDialog::onAddOption);
  connect(m_removeOptionBtn, &QPushButton::clicked, this, &JsonSourceDialog::onRemoveOption);
  connect(m_optionUpBtn, &QPushButton::clicked, this, &JsonSourceDialog::onOptionUp);
  connect(m_optionDownBtn, &QPushButton::clicked, this, &JsonSourceDialog::onOptionDown);

  m_stack->addWidget(page);
}

void JsonSourceDialog::setupDynamicPage() {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  m_panel = new SelectSourcePanel(page);
  layout->addWidget(m_panel, 1);

  m_stack->addWidget(page);
}

// ════════════════════════════════════════════════════════════
//  类型切换
// ════════════════════════════════════════════════════════════

void JsonSourceDialog::onTypeChanged(int index) {
  if (m_stack) m_stack->setCurrentIndex(index >= 1 ? 1 : 0);
}

// ════════════════════════════════════════════════════════════
//  配置读写
// ════════════════════════════════════════════════════════════

void JsonSourceDialog::setSource(const JsonSource &source) {
  m_id = source.id;
  const int typeIdx = source.isStatic() ? 0 : 1;
  m_typeCombo->setCurrentIndex(typeIdx);
  m_remarkEdit->setText(source.remark);
  if (source.isStatic()) {
    m_staticUrlEdit->setText(source.url);
    populateOptions(source.options);
  } else {
    m_panel->setData(source.url, source.method, source.valueField, source.labelField, source.paged,
                     source.pageKey, source.pageSizeKey, source.pageSize, source.searchTitle,
                     source.searchField);
    m_panel->setTags(source.tags);
  }
  onTypeChanged(typeIdx);
}

void JsonSourceDialog::setHttpConfig(const QString &baseUrl, const QString &authHeader,
                                     const QString &postData) {
  m_baseUrl = baseUrl;
  m_authHeader = authHeader;
  m_postData = postData;
  if (m_panel) m_panel->setHttpConfig(baseUrl, authHeader, postData);
}

void JsonSourceDialog::accept() {
  // 静态数据源必须填写函数 URL（作为生成函数名的依据）
  if (m_typeCombo->currentData().toString() == QString::fromLatin1(JsonSourceType::kStatic) &&
      m_staticUrlEdit->text().trimmed().isEmpty()) {
    AuiMessageBox::show(this, QStringLiteral("提示"), QStringLiteral("请填写函数URL"));
    return;
  }
  QDialog::accept();
}

JsonSource JsonSourceDialog::source() const {
  JsonSource s;
  s.id = m_id;
  s.type = m_typeCombo->currentData().toString();
  s.remark = m_remarkEdit->text().trimmed();
  if (s.isStatic()) {
    s.url = m_staticUrlEdit->text().trimmed();
    s.options = collectOptions();
  } else {
    s.url = m_panel->url();
    s.method = m_panel->method();
    s.valueField = m_panel->valueField();
    s.labelField = m_panel->labelField();
    s.paged = m_panel->paged();
    s.pageKey = m_panel->pageKey();
    s.pageSizeKey = m_panel->pageSizeKey();
    s.pageSize = m_panel->pageSize();
    s.searchTitle = m_panel->searchTitle();
    s.searchField = m_panel->searchField();
    s.tags = m_panel->tags();
  }
  return s;
}

// ════════════════════════════════════════════════════════════
//  静态选项操作
// ════════════════════════════════════════════════════════════

void JsonSourceDialog::onAddOption() {
  int row = m_optionTable->rowCount();
  m_optionTable->insertRow(row);
  m_optionTable->setItem(row, 0, new QTableWidgetItem());
  m_optionTable->setItem(row, 1, new QTableWidgetItem());
  // 类型列：字符串（默认）/ 数字；固定最小宽度保证下拉箭头完整可点
  auto *combo = AuiComboBox::create(m_optionTable);
  combo->addItem(QStringLiteral("字符串"), QString());
  combo->addItem(QStringLiteral("数字"), QStringLiteral("number"));
  combo->setMinimumWidth(80);
  m_optionTable->setCellWidget(row, 2, combo);
  m_optionTable->setCurrentCell(row, 0);
}

void JsonSourceDialog::onRemoveOption() {
  int row = m_optionTable->currentRow();
  if (row < 0) return;
  m_optionTable->removeRow(row);
}

void JsonSourceDialog::onOptionUp() {
  int row = m_optionTable->currentRow();
  if (row <= 0) return;
  swapRows(row, row - 1);
  m_optionTable->selectRow(row - 1);
}

void JsonSourceDialog::onOptionDown() {
  int row = m_optionTable->currentRow();
  if (row < 0 || row >= m_optionTable->rowCount() - 1) return;
  swapRows(row, row + 1);
  m_optionTable->selectRow(row + 1);
}

void JsonSourceDialog::swapRows(int a, int b) {
  // 交换两行的显示文本/实际值 item（takeItem 解除所有权后再放置，安全）
  for (int c = 0; c < 2; ++c) {
    QTableWidgetItem *ia = m_optionTable->takeItem(a, c);
    QTableWidgetItem *ib = m_optionTable->takeItem(b, c);
    m_optionTable->setItem(a, c, ib);
    m_optionTable->setItem(b, c, ia);
  }
  // 类型列是 cellWidget：setCellWidget 会删除目标单元格旧控件，直接交换指针
  // 会产生悬垂引用（上移/下移后再点添加选项即崩溃），改为交换下拉框当前选中项
  auto *ca = qobject_cast<QComboBox *>(m_optionTable->cellWidget(a, 2));
  auto *cb = qobject_cast<QComboBox *>(m_optionTable->cellWidget(b, 2));
  if (ca && cb) {
    const int idxA = ca->currentIndex();
    ca->setCurrentIndex(cb->currentIndex());
    cb->setCurrentIndex(idxA);
  }
}

QVector<JsonSourceOption> JsonSourceDialog::collectOptions() const {
  QVector<JsonSourceOption> out;
  for (int r = 0; r < m_optionTable->rowCount(); ++r) {
    JsonSourceOption o;
    auto *label = m_optionTable->item(r, 0);
    auto *value = m_optionTable->item(r, 1);
    o.label = label ? label->text().trimmed() : QString();
    o.value = value ? value->text().trimmed() : QString();
    // 类型列（cellWidget）：字符串（data 空）/ 数字（data="number"）
    if (auto *combo = qobject_cast<QComboBox *>(m_optionTable->cellWidget(r, 2))) {
      o.valueType = combo->currentData().toString();
    }
    if (!o.label.isEmpty() || !o.value.isEmpty()) out.append(o);
  }
  return out;
}

void JsonSourceDialog::populateOptions(const QVector<JsonSourceOption> &options) {
  m_optionTable->setRowCount(0);
  for (const auto &o : options) {
    int row = m_optionTable->rowCount();
    m_optionTable->insertRow(row);
    m_optionTable->setItem(row, 0, new QTableWidgetItem(o.label));
    m_optionTable->setItem(row, 1, new QTableWidgetItem(o.value));
    auto *combo = AuiComboBox::create(m_optionTable);
    combo->addItem(QStringLiteral("字符串"), QString());
    combo->addItem(QStringLiteral("数字"), QStringLiteral("number"));
    combo->setMinimumWidth(80);
    combo->setCurrentIndex(o.valueType == QStringLiteral("number") ? 1 : 0);
    m_optionTable->setCellWidget(row, 2, combo);
  }
}
