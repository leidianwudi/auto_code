/**
 * @file json_global_enum_editor.cpp
 * @brief .jsonglobalenum 可视化编辑器面板实现
 */

#include "json_global_enum_editor.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUuid>
#include <QVBoxLayout>

#include "json_global_enum_dialog.h"
#include "src/ui/json_vue/config_dialog_common.h"
#include "src/util/common/code_constants.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/component/aui_message_box.h"
#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  构造 / 界面构建
// ════════════════════════════════════════════════════════════

JsonGlobalEnumEditor::JsonGlobalEnumEditor(QWidget *parent) : QWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);
  setupUI();
}

void JsonGlobalEnumEditor::setupUI() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(2, 2, 2, 2);
  mainLayout->setSpacing(2);

  auto *group = new QGroupBox(QStringLiteral("全局枚举列表"), this);
  auto *layout = new QVBoxLayout(group);
  layout->setSpacing(2);
  layout->setContentsMargins(2, 2, 2, 2);

  // 操作按钮行
  auto *btnRow = new QHBoxLayout;
  m_addBtn = new QPushButton(QStringLiteral("+ 添加枚举"), group);
  m_removeBtn = new QPushButton(QStringLiteral("- 删除枚举"), group);
  m_moveUpBtn = new QPushButton(QStringLiteral("↑ 上移"), group);
  m_moveDownBtn = new QPushButton(QStringLiteral("↓ 下移"), group);
  btnRow->addWidget(m_addBtn);
  btnRow->addWidget(m_removeBtn);
  btnRow->addWidget(m_moveUpBtn);
  btnRow->addWidget(m_moveDownBtn);
  btnRow->addStretch();
  layout->addLayout(btnRow);

  m_table = makeConfigTable(
      {{QStringLiteral("名称"), QHeaderView::Interactive, 160},
       {QStringLiteral("说明"), QHeaderView::Interactive, 220},
       {QString::fromUtf8(CodeConstants::UiText::kConfig), QHeaderView::Interactive, 260}},
      group, 100, 0, QAbstractItemView::SelectRows);
  // 所有列均 Interactive 可拖动调整宽度，末列拉伸填满剩余空间避免右侧留白
  m_table->horizontalHeader()->setStretchLastSection(true);
  // 表格只读：编辑必须双击数据行进入子对话框
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  layout->addWidget(m_table, 1);

  mainLayout->addWidget(group, 1);

  connect(m_addBtn, &QPushButton::clicked, this, &JsonGlobalEnumEditor::onAddEnum);
  connect(m_removeBtn, &QPushButton::clicked, this, &JsonGlobalEnumEditor::onRemoveEnum);
  connect(m_moveUpBtn, &QPushButton::clicked, this, &JsonGlobalEnumEditor::onMoveUp);
  connect(m_moveDownBtn, &QPushButton::clicked, this, &JsonGlobalEnumEditor::onMoveDown);

  // 双击数据行进入子界面编辑（配置按钮为另一入口，行为一致）
  connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
    m_table->selectRow(row);
    onEditEnum();
  });

  // 程序化写入单元格（编辑对话框保存回填）→ 配置变化
  connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *) {
    if (!m_loading) emit configChanged();
  });

  applyStyle();
}

void JsonGlobalEnumEditor::applyStyle() {
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

void JsonGlobalEnumEditor::reloadStyle() { applyStyle(); }

// ════════════════════════════════════════════════════════════
//  枚举操作
// ════════════════════════════════════════════════════════════

namespace {
/// 生成一个新的枚举唯一 id（UUID 前 8 位，jsonvue 引用数据源时保持稳定）
inline QString newEnumId() { return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8); }
}  // namespace

QPushButton *JsonGlobalEnumEditor::makeConfigButton() {
  auto *configBtn = new QPushButton(QStringLiteral("⚙"), this);
  connect(configBtn, &QPushButton::clicked, this, [this, configBtn]() {
    for (int r = 0; r < m_table->rowCount(); ++r) {
      if (m_table->cellWidget(r, GEColConfig) == configBtn) {
        m_table->selectRow(r);
        onEditEnum();
        break;
      }
    }
  });
  return configBtn;
}

void JsonGlobalEnumEditor::onAddEnum() {
  JsonGlobalEnumDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted) {
    JsonGlobalEnum e = dialog.enumData();
    if (e.id.isEmpty()) e.id = newEnumId();
    m_enums.append(e);

    int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, GEColName, new QTableWidgetItem(e.name));
    m_table->setItem(row, GEColRemark, new QTableWidgetItem(e.remark));
    m_table->setCellWidget(row, GEColConfig, makeConfigButton());
    refreshSummary(row);
    m_table->selectRow(row);
    if (!m_loading) emit configChanged();
  }
}

void JsonGlobalEnumEditor::onEditEnum() {
  int row = m_table->currentRow();
  if (row < 0 || row >= m_enums.size()) return;
  JsonGlobalEnumDialog dialog(this);
  dialog.setEnum(m_enums[row]);
  if (dialog.exec() == QDialog::Accepted) {
    JsonGlobalEnum e = dialog.enumData();
    if (e.id.isEmpty()) e.id = m_enums[row].id;
    m_enums[row] = e;
    m_table->item(row, GEColName)->setText(e.name);
    m_table->item(row, GEColRemark)->setText(e.remark);
    refreshSummary(row);
    if (!m_loading) emit configChanged();
  }
}

void JsonGlobalEnumEditor::onRemoveEnum() {
  int row = m_table->currentRow();
  if (row < 0 || row >= m_enums.size()) return;
  if (!AuiMessageBox::confirm(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定要删除当前枚举吗？"))) {
    return;
  }
  m_table->removeRow(row);
  m_enums.removeAt(row);
  if (!m_loading) emit configChanged();
}

void JsonGlobalEnumEditor::onMoveUp() {
  int row = m_table->currentRow();
  if (row <= 0 || row >= m_enums.size()) return;
  m_enums.swapItemsAt(row, row - 1);
  JsonGlobalEnumConfig cfg;
  cfg.enums = m_enums;
  loadConfig(cfg);
  m_table->selectRow(row - 1);
  if (!m_loading) emit configChanged();
}

void JsonGlobalEnumEditor::onMoveDown() {
  int row = m_table->currentRow();
  if (row < 0 || row >= m_enums.size() - 1) return;
  m_enums.swapItemsAt(row, row + 1);
  JsonGlobalEnumConfig cfg;
  cfg.enums = m_enums;
  loadConfig(cfg);
  m_table->selectRow(row + 1);
  if (!m_loading) emit configChanged();
}

void JsonGlobalEnumEditor::refreshSummary(int row) {
  if (row < 0 || row >= m_enums.size()) return;
  const JsonGlobalEnum &e = m_enums[row];
  QStringList parts;
  parts << QStringLiteral("%1项").arg(e.options.size());
  for (const auto &o : e.options) {
    parts << QStringLiteral("%1=%2(%3)").arg(o.key, o.value, o.label);
  }
  auto *btn = qobject_cast<QPushButton *>(m_table->cellWidget(row, GEColConfig));
  if (btn) {
    btn->setText(QStringLiteral("⚙ ") + parts.join(QStringLiteral(", ")));
  }
}

// ════════════════════════════════════════════════════════════
//  加载 / 收集配置
// ════════════════════════════════════════════════════════════

void JsonGlobalEnumEditor::loadConfig(const JsonGlobalEnumConfig &config) {
  m_loading = true;
  m_enums = config.enums;
  m_table->setRowCount(0);
  for (const auto &e : config.enums) {
    int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, GEColName, new QTableWidgetItem(e.name));
    m_table->setItem(row, GEColRemark, new QTableWidgetItem(e.remark));
    m_table->setCellWidget(row, GEColConfig, makeConfigButton());
    refreshSummary(row);
  }
  m_loading = false;
}

JsonGlobalEnumConfig JsonGlobalEnumEditor::collectConfig() const {
  JsonGlobalEnumConfig cfg;
  // 表格只读，数据仅来源于子对话框写入的 m_enums
  cfg.enums = m_enums;
  return cfg;
}

void JsonGlobalEnumEditor::setPreservedSource(const QString &src) {
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

QJsonObject JsonGlobalEnumEditor::collectMergedObject() const {
  JsonGlobalEnumConfig cfg = collectConfig();
  QJsonObject root = cfg.toJsonObject();

  if (m_preserved.isEmpty()) return root;

  // 顶层保留其它未知键
  for (auto it = m_preserved.begin(); it != m_preserved.end(); ++it) {
    if (!root.contains(it.key())) root.insert(it.key(), it.value());
  }
  return root;
}
