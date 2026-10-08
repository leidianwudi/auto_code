/**
 * @file json_table_editor.cpp
 * @brief .jsontable 可视化编辑器面板实现
 */

#include "json_table_editor.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "json_table_dialog.h"
#include "src/ui/json_vue/config_dialog_common.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/component/aui_message_box.h"
#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  构造 / 界面构建
// ════════════════════════════════════════════════════════════

JsonTableEditor::JsonTableEditor(QWidget *parent) : QWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);
  setupUI();
}

void JsonTableEditor::setupUI() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(2, 2, 2, 2);
  mainLayout->setSpacing(2);

  auto *group = new QGroupBox(QStringLiteral("表定义列表"), this);
  auto *layout = new QVBoxLayout(group);
  layout->setSpacing(2);
  layout->setContentsMargins(2, 2, 2, 2);

  // 操作按钮行
  auto *btnRow = new QHBoxLayout;
  m_addBtn = new QPushButton(QStringLiteral("+ 添加表"), group);
  m_removeBtn = new QPushButton(QStringLiteral("- 删除表"), group);
  btnRow->addWidget(m_addBtn);
  btnRow->addWidget(m_removeBtn);
  btnRow->addStretch();
  layout->addLayout(btnRow);

  m_table = makeConfigTable(
      {{QStringLiteral("表名"), QHeaderView::Interactive, 150},
       {QStringLiteral("模型名"), QHeaderView::Interactive, 150},
       {QStringLiteral("列数"), QHeaderView::Interactive, 60},
       {QStringLiteral("表注释"), QHeaderView::Interactive, 180},
       {QStringLiteral("设计"), QHeaderView::Interactive, 220}},
      group, 100, 0, QAbstractItemView::SelectRows);
  // 所有列均 Interactive 可拖动调整宽度，末列拉伸填满剩余空间避免右侧留白
  m_table->horizontalHeader()->setStretchLastSection(true);
  // 表格只读：编辑必须双击数据行进入子对话框
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  layout->addWidget(m_table, 1);

  mainLayout->addWidget(group, 1);

  connect(m_addBtn, &QPushButton::clicked, this, &JsonTableEditor::onAddTable);
  connect(m_removeBtn, &QPushButton::clicked, this, &JsonTableEditor::onRemoveTable);

  // 双击数据行进入子界面编辑（设计按钮为另一入口，行为一致）
  connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
    m_table->selectRow(row);
    onEditTable();
  });

  // 程序化写入单元格（编辑对话框保存回填）→ 配置变化
  connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *) {
    if (!m_loading) emit configChanged();
  });

  applyStyle();
}

void JsonTableEditor::applyStyle() {
  const auto fontPx = QString::number(AuiStyle::dialogFontSize());
  const auto text = AuiStyle::textColor().name();
  const auto border = AuiStyle::borderColor().name();
  const auto bg = AuiStyle::panelBackground().name();
  const auto headerBg = AuiStyle::background().name();
  const auto alt = AuiStyle::listAlternateBackground().name();

  QString qss = QStringLiteral(
                    "QGroupBox {"
                    "  font-size: %1px; font-weight: bold; color: %2;"
                    "  border: 1px solid %3; border-radius: 4px;"
                    "  margin-top: 10px; padding-top: 5px;"
                    "  background: transparent;"
                    "}"
                    "QGroupBox::title {"
                    "  subcontrol-origin: margin; left: 2px; padding: 0 2px; color: %2;"
                    "}"
                    "QLabel { font-size: %1px; color: %2; background: transparent; }"
                    "QPushButton { padding: 2px 8px; font-size: %1px; color: %2; }"
                    "QTableWidget { font-size: %1px; background: %4; color: %2;"
                    "  gridline-color: %3; alternate-background-color: %5; }"
                    "QTableWidget::item { color: %2; }"
                    "QHeaderView::section { padding: 2px; font-size: %1px;"
                    "  background: %6; color: %2; border: none;"
                    "  border-right: 1px solid %3; border-bottom: 1px solid %3; }"
                    "QScrollBar:vertical, QScrollBar:horizontal { background: %4; }")
                    .arg(fontPx, text, border, bg, alt, headerBg);
  setStyleSheet(qss);
}

void JsonTableEditor::reloadStyle() { applyStyle(); }

// ════════════════════════════════════════════════════════════
//  表操作
// ════════════════════════════════════════════════════════════

QPushButton *JsonTableEditor::makeDesignButton() {
  auto *designBtn = new QPushButton(QStringLiteral("⚙"), this);
  connect(designBtn, &QPushButton::clicked, this, [this, designBtn]() {
    for (int r = 0; r < m_table->rowCount(); ++r) {
      if (m_table->cellWidget(r, JADesign) == designBtn) {
        m_table->selectRow(r);
        onEditTable();
        break;
      }
    }
  });
  return designBtn;
}

void JsonTableEditor::onAddTable() {
  JsonTableDialog dialog(JsonTableTable(), collectConfig(), this);
  if (dialog.exec() == QDialog::Accepted) {
    JsonTableTable t = dialog.resultTable();
    if (t.tableName.isEmpty()) t.tableName = QStringLiteral("table%1").arg(m_tables.size() + 1);
    m_tables.append(t);
    appendTableRow(t);
    m_table->selectRow(m_table->rowCount() - 1);
    if (!m_loading) emit configChanged();
  }
}

void JsonTableEditor::onEditTable() {
  int row = m_table->currentRow();
  if (row < 0 || row >= m_tables.size()) return;
  JsonTableDialog dialog(m_tables[row], collectConfig(), this);
  if (dialog.exec() == QDialog::Accepted) {
    m_tables[row] = dialog.resultTable();
    refreshRow(row);
    if (!m_loading) emit configChanged();
  }
}

void JsonTableEditor::onRemoveTable() {
  int row = m_table->currentRow();
  if (row < 0 || row >= m_tables.size()) return;
  if (!AuiMessageBox::confirm(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定要删除当前表吗？"))) {
    return;
  }
  m_table->removeRow(row);
  m_tables.removeAt(row);
  if (!m_loading) emit configChanged();
}

void JsonTableEditor::appendTableRow(const JsonTableTable &t) {
  int row = m_table->rowCount();
  m_table->insertRow(row);
  m_table->setItem(row, JATableName, new QTableWidgetItem(t.tableName));
  m_table->setItem(row, JAModelName, new QTableWidgetItem(t.modelName));
  m_table->setItem(row, JAColNum, new QTableWidgetItem(QString::number(t.columns.size())));
  m_table->setItem(row, JATableComment, new QTableWidgetItem(t.tableComment));
  m_table->setCellWidget(row, JADesign, makeDesignButton());
}

void JsonTableEditor::refreshRow(int row) {
  if (row < 0 || row >= m_tables.size()) return;
  const JsonTableTable &t = m_tables[row];
  m_table->item(row, JATableName)->setText(t.tableName);
  m_table->item(row, JAModelName)->setText(t.modelName);
  m_table->item(row, JAColNum)->setText(QString::number(t.columns.size()));
  m_table->item(row, JATableComment)->setText(t.tableComment);
}

// ════════════════════════════════════════════════════════════
//  加载 / 收集配置
// ════════════════════════════════════════════════════════════

void JsonTableEditor::loadConfig(const JsonTableConfig &config) {
  m_loading = true;
  m_tables = config.tables;
  m_table->setRowCount(0);
  for (const auto &t : m_tables) {
    appendTableRow(t);
  }
  m_loading = false;
}

JsonTableConfig JsonTableEditor::collectConfig() const {
  JsonTableConfig cfg;
  // 表格只读，数据仅来源于设计对话框写入的 m_tables
  cfg.tables = m_tables;
  return cfg;
}

void JsonTableEditor::setPreservedSource(const QString &src) {
  if (src.trimmed().isEmpty()) {
    m_preserved = QJsonObject();
    return;
  }
  QJsonParseError perr;
  QJsonDocument doc = UtilJson::fromJson(src, &perr);
  if (perr.error == QJsonParseError::NoError && doc.isObject()) {
    m_preserved = doc.object();
  } else {
    m_preserved = QJsonObject();
  }
}

QJsonObject JsonTableEditor::collectMergedObject() const {
  JsonTableConfig cfg = collectConfig();
  QJsonObject root = cfg.toJsonObject();

  if (m_preserved.isEmpty()) return root;

  // 顶层保留其它未知键
  for (auto it = m_preserved.begin(); it != m_preserved.end(); ++it) {
    if (!root.contains(it.key())) root.insert(it.key(), it.value());
  }
  return root;
}
