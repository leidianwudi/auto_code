/**
 * @file json_source_editor.cpp
 * @brief .jsonsource 可视化编辑器面板实现
 */

#include "json_source_editor.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUuid>
#include <QVBoxLayout>

#include "json_source_dialog.h"
#include "src/ui/json_vue/config_dialog_common.h"
#include "src/util/common/code_constants.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/component/aui_message_box.h"
#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  构造 / 界面构建
// ════════════════════════════════════════════════════════════

JsonSourceEditor::JsonSourceEditor(QWidget *parent) : QWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);
  setupUI();
}

void JsonSourceEditor::setupUI() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(2, 2, 2, 2);
  mainLayout->setSpacing(2);

  auto *group = new QGroupBox(QStringLiteral("数据源列表"), this);
  auto *layout = new QVBoxLayout(group);
  layout->setSpacing(2);
  layout->setContentsMargins(2, 2, 2, 2);

  // 操作按钮行
  auto *btnRow = new QHBoxLayout;
  m_addBtn = new QPushButton(QStringLiteral("+ 添加数据源"), group);
  m_removeBtn = new QPushButton(QStringLiteral("- 删除数据源"), group);
  m_moveUpBtn = new QPushButton(QStringLiteral("↑ 上移"), group);
  m_moveDownBtn = new QPushButton(QStringLiteral("↓ 下移"), group);
  btnRow->addWidget(m_addBtn);
  btnRow->addWidget(m_removeBtn);
  btnRow->addWidget(m_moveUpBtn);
  btnRow->addWidget(m_moveDownBtn);
  btnRow->addStretch();
  layout->addLayout(btnRow);

  m_table = makeConfigTable(
      {{QStringLiteral("说明"), QHeaderView::Interactive, 160},
       {QStringLiteral("类型"), QHeaderView::Interactive, 80},
       {QStringLiteral("URL/函数名"), QHeaderView::Interactive, 260},
       {QString::fromUtf8(CodeConstants::UiText::kConfig), QHeaderView::Interactive, 220}},
      group, 100, 0, QAbstractItemView::SelectRows);
  // 所有列均 Interactive 可拖动调整宽度，末列拉伸填满剩余空间避免右侧留白
  m_table->horizontalHeader()->setStretchLastSection(true);
  // 表格只读：编辑必须双击数据行进入子对话框
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  layout->addWidget(m_table, 1);

  mainLayout->addWidget(group, 1);

  connect(m_addBtn, &QPushButton::clicked, this, &JsonSourceEditor::onAddSource);
  connect(m_removeBtn, &QPushButton::clicked, this, &JsonSourceEditor::onRemoveSource);
  connect(m_moveUpBtn, &QPushButton::clicked, this, &JsonSourceEditor::onMoveUp);
  connect(m_moveDownBtn, &QPushButton::clicked, this, &JsonSourceEditor::onMoveDown);

  // 双击数据行进入子界面编辑（配置按钮为另一入口，行为一致）
  connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
    m_table->selectRow(row);
    onEditSource();
  });

  // 程序化写入单元格（编辑对话框保存回填）→ 配置变化
  connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *) {
    if (!m_loading) emit configChanged();
  });

  applyStyle();
}

void JsonSourceEditor::applyStyle() {
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

void JsonSourceEditor::reloadStyle() { applyStyle(); }

// ════════════════════════════════════════════════════════════
//  数据源操作
// ════════════════════════════════════════════════════════════

namespace {
/// 生成一个新的数据源唯一 id（UUID 前 8 位）
inline QString newSourceId() {
  return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}
}  // namespace

QPushButton *JsonSourceEditor::makeConfigButton() {
  auto *configBtn = new QPushButton(QStringLiteral("⚙"), this);
  connect(configBtn, &QPushButton::clicked, this, [this, configBtn]() {
    for (int r = 0; r < m_table->rowCount(); ++r) {
      if (m_table->cellWidget(r, JDColConfig) == configBtn) {
        m_table->selectRow(r);
        onEditSource();
        break;
      }
    }
  });
  return configBtn;
}

void JsonSourceEditor::onAddSource() {
  JsonSourceDialog dialog(this);
  dialog.setHttpConfig(m_baseUrl, m_authHeader, m_postData);
  if (dialog.exec() == QDialog::Accepted) {
    JsonSource s = dialog.source();
    if (s.id.isEmpty()) s.id = newSourceId();
    if (s.remark.isEmpty()) s.remark = QStringLiteral("数据源%1").arg(m_sources.size() + 1);
    m_sources.append(s);

    int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, JDColRemark, new QTableWidgetItem(s.remark));
    auto *typeItem = new QTableWidgetItem(s.isStatic() ? QStringLiteral("静态")
                                                       : QStringLiteral("动态"));
    typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, JDColType, typeItem);
    // URL/函数名列：动态显示请求地址，静态显示函数 URL（函数名来源）
    m_table->setItem(row, JDColUrl, new QTableWidgetItem(s.url));
    m_table->setCellWidget(row, JDColConfig, makeConfigButton());
    refreshSummary(row);
    m_table->selectRow(row);
    if (!m_loading) emit configChanged();
  }
}

void JsonSourceEditor::onEditSource() {
  int row = m_table->currentRow();
  if (row < 0 || row >= m_sources.size()) return;
  JsonSourceDialog dialog(this);
  dialog.setHttpConfig(m_baseUrl, m_authHeader, m_postData);
  dialog.setSource(m_sources[row]);
  if (dialog.exec() == QDialog::Accepted) {
    JsonSource s = dialog.source();
    if (s.id.isEmpty()) s.id = m_sources[row].id;
    m_sources[row] = s;
    m_table->item(row, JDColRemark)->setText(s.remark);
    m_table->item(row, JDColType)->setText(s.isStatic() ? QStringLiteral("静态")
                                                        : QStringLiteral("动态"));
    // URL/函数名列：动态显示请求地址，静态显示函数 URL（函数名来源）
    m_table->item(row, JDColUrl)->setText(s.url);
    refreshSummary(row);
    if (!m_loading) emit configChanged();
  }
}

void JsonSourceEditor::onRemoveSource() {
  int row = m_table->currentRow();
  if (row < 0 || row >= m_sources.size()) return;
  if (!AuiMessageBox::confirm(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定要删除当前数据源吗？"))) {
    return;
  }
  m_table->removeRow(row);
  m_sources.removeAt(row);
  if (!m_loading) emit configChanged();
}

void JsonSourceEditor::onMoveUp() {
  int row = m_table->currentRow();
  if (row <= 0 || row >= m_sources.size()) return;
  m_sources.swapItemsAt(row, row - 1);
  JsonSourceConfig cfg;
  cfg.sources = m_sources;
  loadConfig(cfg);
  m_table->selectRow(row - 1);
  if (!m_loading) emit configChanged();
}

void JsonSourceEditor::onMoveDown() {
  int row = m_table->currentRow();
  if (row < 0 || row >= m_sources.size() - 1) return;
  m_sources.swapItemsAt(row, row + 1);
  JsonSourceConfig cfg;
  cfg.sources = m_sources;
  loadConfig(cfg);
  m_table->selectRow(row + 1);
  if (!m_loading) emit configChanged();
}

void JsonSourceEditor::refreshSummary(int row) {
  if (row < 0 || row >= m_sources.size()) return;
  const JsonSource &s = m_sources[row];
  QStringList parts;
  if (s.isStatic()) {
    parts << QStringLiteral("静态");
    parts << QStringLiteral("%1项").arg(s.options.size());
    if (!s.options.isEmpty()) {
      parts << s.options.first().label + QStringLiteral("…");
    }
  } else {
    parts << QStringLiteral("动态");
    parts << s.method;
    if (!s.url.isEmpty()) parts << s.url;
    if (!s.valueField.isEmpty() && !s.labelField.isEmpty()) {
      parts << QStringLiteral("%1→%2").arg(s.valueField, s.labelField);
    }
    if (s.paged) parts << QStringLiteral("分页");
  }
  auto *btn = qobject_cast<QPushButton *>(m_table->cellWidget(row, JDColConfig));
  if (btn) {
    btn->setText(QStringLiteral("⚙ ") + parts.join(QStringLiteral(", ")));
  }
}

// ════════════════════════════════════════════════════════════
//  加载 / 收集配置
// ════════════════════════════════════════════════════════════

void JsonSourceEditor::loadConfig(const JsonSourceConfig &config) {
  m_loading = true;
  m_sources = config.sources;
  m_table->setRowCount(0);
  for (const auto &s : config.sources) {
    int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, JDColRemark, new QTableWidgetItem(s.remark));
    auto *typeItem = new QTableWidgetItem(s.isStatic() ? QStringLiteral("静态")
                                                       : QStringLiteral("动态"));
    typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, JDColType, typeItem);
    // URL/函数名列：动态显示请求地址，静态显示函数 URL（函数名来源）
    m_table->setItem(row, JDColUrl, new QTableWidgetItem(s.url));

    m_table->setCellWidget(row, JDColConfig, makeConfigButton());
    refreshSummary(row);
  }
  m_loading = false;
}

JsonSourceConfig JsonSourceEditor::collectConfig() const {
  JsonSourceConfig cfg;
  // 表格只读，数据仅来源于子对话框写入的 m_sources
  cfg.sources = m_sources;
  return cfg;
}

void JsonSourceEditor::setHttpConfig(const QString &baseUrl, const QString &authHeader,
                                   const QString &postData) {
  m_baseUrl = baseUrl;
  m_authHeader = authHeader;
  m_postData = postData;
}

void JsonSourceEditor::setPreservedSource(const QString &src) {
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

QJsonObject JsonSourceEditor::collectMergedObject() const {
  JsonSourceConfig cfg = collectConfig();
  QJsonObject root = cfg.toJsonObject();

  if (m_preserved.isEmpty()) return root;

  // 顶层保留其它未知键
  for (auto it = m_preserved.begin(); it != m_preserved.end(); ++it) {
    if (!root.contains(it.key())) root.insert(it.key(), it.value());
  }
  return root;
}
