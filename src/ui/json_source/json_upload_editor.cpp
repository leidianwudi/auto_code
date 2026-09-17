/**
 * @file json_upload_editor.cpp
 * @brief .jsonupload 可视化编辑器面板实现
 */

#include "json_upload_editor.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUuid>
#include <QVBoxLayout>

#include "json_upload_dialog.h"
#include "src/ui/json_vue/config_dialog_common.h"
#include "src/util/common/code_constants.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/component/aui_message_box.h"
#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  构造 / 界面构建
// ════════════════════════════════════════════════════════════

JsonUploadEditor::JsonUploadEditor(QWidget *parent) : QWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);
  setupUI();
}

void JsonUploadEditor::setupUI() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(2, 2, 2, 2);
  mainLayout->setSpacing(2);

  auto *group = new QGroupBox(QStringLiteral("上传预设列表"), this);
  auto *layout = new QVBoxLayout(group);
  layout->setSpacing(2);
  layout->setContentsMargins(2, 2, 2, 2);

  // 操作按钮行
  auto *btnRow = new QHBoxLayout;
  m_addBtn = new QPushButton(QStringLiteral("+ 添加上传预设"), group);
  m_removeBtn = new QPushButton(QStringLiteral("- 删除上传预设"), group);
  m_moveUpBtn = new QPushButton(QStringLiteral("↑ 上移"), group);
  m_moveDownBtn = new QPushButton(QStringLiteral("↓ 下移"), group);
  btnRow->addWidget(m_addBtn);
  btnRow->addWidget(m_removeBtn);
  btnRow->addWidget(m_moveUpBtn);
  btnRow->addWidget(m_moveDownBtn);
  btnRow->addStretch();
  layout->addLayout(btnRow);

  m_table = makeConfigTable(
      {{QStringLiteral("说明"), QHeaderView::Interactive, 150},
       {QStringLiteral("上传地址"), QHeaderView::Interactive, 220},
       {QStringLiteral("文件字段"), QHeaderView::Interactive, 90},
       {QStringLiteral("张数"), QHeaderView::Interactive, 60},
       {QString::fromUtf8(CodeConstants::UiText::kConfig), QHeaderView::Interactive, 220}},
      group, 100, 0, QAbstractItemView::SelectRows);
  // 所有列均 Interactive 可拖动调整宽度，末列拉伸填满剩余空间避免右侧留白
  m_table->horizontalHeader()->setStretchLastSection(true);
  // 表格只读：编辑必须双击数据行进入子对话框
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  layout->addWidget(m_table, 1);

  mainLayout->addWidget(group, 1);

  connect(m_addBtn, &QPushButton::clicked, this, &JsonUploadEditor::onAddUpload);
  connect(m_removeBtn, &QPushButton::clicked, this, &JsonUploadEditor::onRemoveUpload);
  connect(m_moveUpBtn, &QPushButton::clicked, this, &JsonUploadEditor::onMoveUp);
  connect(m_moveDownBtn, &QPushButton::clicked, this, &JsonUploadEditor::onMoveDown);

  // 双击数据行进入子界面编辑（配置按钮为另一入口，行为一致）
  connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
    m_table->selectRow(row);
    onEditUpload();
  });

  // 程序化写入单元格（编辑对话框保存回填）→ 配置变化
  connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *) {
    if (!m_loading) emit configChanged();
  });

  applyStyle();
}

void JsonUploadEditor::applyStyle() {
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

void JsonUploadEditor::reloadStyle() { applyStyle(); }

// ════════════════════════════════════════════════════════════
//  上传预设操作
// ════════════════════════════════════════════════════════════

namespace {
/// 生成一个新的上传预设唯一 id（UUID 前 8 位）
inline QString newUploadId() {
  return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}
}  // namespace

QPushButton *JsonUploadEditor::makeConfigButton() {
  auto *configBtn = new QPushButton(QStringLiteral("⚙"), this);
  connect(configBtn, &QPushButton::clicked, this, [this, configBtn]() {
    for (int r = 0; r < m_table->rowCount(); ++r) {
      if (m_table->cellWidget(r, JUColConfig) == configBtn) {
        m_table->selectRow(r);
        onEditUpload();
        break;
      }
    }
  });
  return configBtn;
}

void JsonUploadEditor::onAddUpload() {
  JsonUploadDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted) {
    JsonUpload u = dialog.upload();
    if (u.id.isEmpty()) u.id = newUploadId();
    if (u.remark.isEmpty()) u.remark = QStringLiteral("上传预设%1").arg(m_uploads.size() + 1);
    m_uploads.append(u);

    int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, JUColRemark, new QTableWidgetItem(u.remark));
    m_table->setItem(row, JUColUrl, new QTableWidgetItem(u.url));
    m_table->setItem(row, JUColFileField, new QTableWidgetItem(u.fileField));
    m_table->setItem(row, JUColMaxCount, new QTableWidgetItem(QString::number(u.maxCount)));
    m_table->setCellWidget(row, JUColConfig, makeConfigButton());
    refreshSummary(row);
    m_table->selectRow(row);
    if (!m_loading) emit configChanged();
  }
}

void JsonUploadEditor::onEditUpload() {
  int row = m_table->currentRow();
  if (row < 0 || row >= m_uploads.size()) return;
  JsonUploadDialog dialog(this);
  dialog.setUpload(m_uploads[row]);
  if (dialog.exec() == QDialog::Accepted) {
    JsonUpload u = dialog.upload();
    if (u.id.isEmpty()) u.id = m_uploads[row].id;
    m_uploads[row] = u;
    m_table->item(row, JUColRemark)->setText(u.remark);
    m_table->item(row, JUColUrl)->setText(u.url);
    m_table->item(row, JUColFileField)->setText(u.fileField);
    m_table->item(row, JUColMaxCount)->setText(QString::number(u.maxCount));
    refreshSummary(row);
    if (!m_loading) emit configChanged();
  }
}

void JsonUploadEditor::onRemoveUpload() {
  int row = m_table->currentRow();
  if (row < 0 || row >= m_uploads.size()) return;
  if (!AuiMessageBox::confirm(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定要删除当前上传预设吗？"))) {
    return;
  }
  m_table->removeRow(row);
  m_uploads.removeAt(row);
  if (!m_loading) emit configChanged();
}

void JsonUploadEditor::onMoveUp() {
  int row = m_table->currentRow();
  if (row <= 0 || row >= m_uploads.size()) return;
  m_uploads.swapItemsAt(row, row - 1);
  JsonUploadConfig cfg;
  cfg.uploads = m_uploads;
  loadConfig(cfg);
  m_table->selectRow(row - 1);
  if (!m_loading) emit configChanged();
}

void JsonUploadEditor::onMoveDown() {
  int row = m_table->currentRow();
  if (row < 0 || row >= m_uploads.size() - 1) return;
  m_uploads.swapItemsAt(row, row + 1);
  JsonUploadConfig cfg;
  cfg.uploads = m_uploads;
  loadConfig(cfg);
  m_table->selectRow(row + 1);
  if (!m_loading) emit configChanged();
}

void JsonUploadEditor::refreshSummary(int row) {
  if (row < 0 || row >= m_uploads.size()) return;
  const JsonUpload &u = m_uploads[row];
  QStringList parts;
  parts << u.method;
  if (!u.url.isEmpty()) parts << u.url;
  parts << QStringLiteral("%1张").arg(u.maxCount);
  if (!u.params.isEmpty()) {
    QStringList kv;
    for (const auto &p : u.params) kv << QStringLiteral("%1=%2").arg(p.name, p.value);
    parts << QStringLiteral("[%1]").arg(kv.join(QStringLiteral(", ")));
  }
  parts << QStringLiteral("→%1").arg(u.responsePath);
  auto *btn = qobject_cast<QPushButton *>(m_table->cellWidget(row, JUColConfig));
  if (btn) {
    btn->setText(QStringLiteral("⚙ ") + parts.join(QStringLiteral(", ")));
  }
}

// ════════════════════════════════════════════════════════════
//  加载 / 收集配置
// ════════════════════════════════════════════════════════════

void JsonUploadEditor::loadConfig(const JsonUploadConfig &config) {
  m_loading = true;
  m_uploads = config.uploads;
  m_table->setRowCount(0);
  for (const auto &u : config.uploads) {
    int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, JUColRemark, new QTableWidgetItem(u.remark));
    m_table->setItem(row, JUColUrl, new QTableWidgetItem(u.url));
    m_table->setItem(row, JUColFileField, new QTableWidgetItem(u.fileField));
    m_table->setItem(row, JUColMaxCount, new QTableWidgetItem(QString::number(u.maxCount)));
    m_table->setCellWidget(row, JUColConfig, makeConfigButton());
    refreshSummary(row);
  }
  m_loading = false;
}

JsonUploadConfig JsonUploadEditor::collectConfig() const {
  JsonUploadConfig cfg;
  // 表格只读，数据仅来源于子对话框写入的 m_uploads
  cfg.uploads = m_uploads;
  return cfg;
}

void JsonUploadEditor::setPreservedSource(const QString &src) {
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

QJsonObject JsonUploadEditor::collectMergedObject() const {
  JsonUploadConfig cfg = collectConfig();
  QJsonObject root = cfg.toJsonObject();

  if (m_preserved.isEmpty()) return root;

  // 顶层保留其它未知键
  for (auto it = m_preserved.begin(); it != m_preserved.end(); ++it) {
    if (!root.contains(it.key())) root.insert(it.key(), it.value());
  }
  return root;
}
