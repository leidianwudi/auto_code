/**
 * @file combobox_config_dialog.cpp
 * @brief 下拉框数据源配置对话框实现
 */

#include "combobox_config_dialog.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "src/util/common/http_client.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

ComboboxConfigDialog::ComboboxConfigDialog(QWidget *parent) : QDialog(parent) {
  setupUI();

  // 连接 HTTP 信号
  connect(&HttpClient::instance(), &HttpClient::finished, this,
          &ComboboxConfigDialog::onHttpFinished);
  connect(&HttpClient::instance(), &HttpClient::error, this, &ComboboxConfigDialog::onHttpError);
}

// ════════════════════════════════════════════════════════════
//  界面构建
// ════════════════════════════════════════════════════════════

void ComboboxConfigDialog::setupUI() {
  setWindowTitle(QStringLiteral("下拉框数据源配置"));
  setMinimumSize(500, 400);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  // ── URL 输入行 ──
  auto *urlRow = new QHBoxLayout;
  urlRow->addWidget(new QLabel(QStringLiteral("请求URL:")));
  m_urlEdit = new QLineEdit(this);
  m_urlEdit->setPlaceholderText(QStringLiteral("/api/xxx/list"));
  urlRow->addWidget(m_urlEdit, 1);
  m_testBtn = new QPushButton(QStringLiteral("测试"), this);
  urlRow->addWidget(m_testBtn);
  layout->addLayout(urlRow);

  // ── 状态标签 ──
  m_statusLabel = new QLabel(this);
  m_statusLabel->setStyleSheet(
      QStringLiteral("color: %1; font-size: 12px;").arg(AuiStyle::mutedTextColor().name()));
  layout->addWidget(m_statusLabel);

  // ── 数据预览表格 ──
  layout->addWidget(new QLabel(QStringLiteral("返回数据示例:")));
  m_previewTable = new QTableWidget(this);
  m_previewTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_previewTable->horizontalHeader()->setStretchLastSection(true);
  layout->addWidget(m_previewTable, 1);

  // ── 字段选择行 ──
  auto *fieldRow = new QHBoxLayout;

  fieldRow->addWidget(new QLabel(QStringLiteral("Value字段(实际值):")));
  m_valueCombo = new QComboBox(this);
  m_valueCombo->setMinimumWidth(120);
  fieldRow->addWidget(m_valueCombo);

  fieldRow->addSpacing(20);

  fieldRow->addWidget(new QLabel(QStringLiteral("Label字段(显示文本):")));
  m_labelCombo = new QComboBox(this);
  m_labelCombo->setMinimumWidth(120);
  fieldRow->addWidget(m_labelCombo);

  fieldRow->addStretch();
  layout->addLayout(fieldRow);

  // ── 按钮行 ──
  auto *btnRow = new QHBoxLayout;
  btnRow->addStretch();
  auto *okBtn = new QPushButton(QStringLiteral("确定"), this);
  auto *cancelBtn = new QPushButton(QStringLiteral("取消"), this);
  AuiButton::applyDialogButtonStyle(okBtn);
  AuiButton::applyDialogButtonStyle(cancelBtn);
  btnRow->addWidget(okBtn);
  btnRow->addSpacing(8);
  btnRow->addWidget(cancelBtn);
  layout->addLayout(btnRow);

  connect(m_testBtn, &QPushButton::clicked, this, &ComboboxConfigDialog::onTest);
  connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

// ════════════════════════════════════════════════════════════
//  配置读写
// ════════════════════════════════════════════════════════════

void ComboboxConfigDialog::setConfig(const QString &url, const QString &valueField,
                                     const QString &labelField) {
  m_urlEdit->setText(url);
  // 如果已有字段名，先添加到下拉框（等测试成功后会更新完整列表）
  if (!valueField.isEmpty()) m_valueCombo->addItem(valueField);
  if (!labelField.isEmpty()) m_labelCombo->addItem(labelField);
  m_valueCombo->setCurrentText(valueField);
  m_labelCombo->setCurrentText(labelField);
}

QString ComboboxConfigDialog::url() const { return m_urlEdit->text().trimmed(); }

QString ComboboxConfigDialog::valueField() const { return m_valueCombo->currentText(); }

QString ComboboxConfigDialog::labelField() const { return m_labelCombo->currentText(); }

void ComboboxConfigDialog::setHttpConfig(const QString &baseUrl, const QString &authHeader,
                                         const QString &postData) {
  m_baseUrl = baseUrl;
  m_authHeader = authHeader;
  m_postData = postData;
}

// ════════════════════════════════════════════════════════════
//  HTTP 测试
// ════════════════════════════════════════════════════════════

void ComboboxConfigDialog::onTest() {
  QString url = m_urlEdit->text().trimmed();
  if (url.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入请求URL"));
    return;
  }

  // 拼接 baseUrl
  QString fullUrl = url;
  if (!m_baseUrl.isEmpty() && !url.startsWith(QStringLiteral("http"))) {
    fullUrl = m_baseUrl + (url.startsWith('/') ? url : "/" + url);
  }

  m_statusLabel->setText(QStringLiteral("正在请求..."));
  m_statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(AuiStyle::mutedTextColor().name()));

  // 构建请求头
  HttpClient::Headers headers;
  if (!m_authHeader.isEmpty()) {
    headers[QStringLiteral("Authorization")] = m_authHeader;
  }

  // 解析 POST 数据为 JSON 对象
  QJsonObject bodyObj;
  if (!m_postData.isEmpty()) {
    QJsonParseError err;
    QJsonDocument postDoc = UtilJson::fromJson(m_postData, &err);
    if (err.error == QJsonParseError::NoError && postDoc.isObject()) {
      bodyObj = postDoc.object();
    }
  }

  // 使用 POST 或 GET 发送请求
  HttpClient::Method method = bodyObj.isEmpty() ? HttpClient::Get : HttpClient::Post;
  HttpClient::instance().request(method, fullUrl, bodyObj, headers, this);
}

void ComboboxConfigDialog::onHttpFinished(const QString &url, const QJsonDocument &doc) {
  Q_UNUSED(url);

  // 解析返回数据，提取 data.list 数组
  QJsonObject root = doc.object();
  QJsonObject dataObj = root.value("data").toObject();

  // 尝试 data.list 或 data 本身是数组
  QJsonArray list;
  if (dataObj.contains("list")) {
    list = dataObj.value("list").toArray();
  } else if (root.value("data").isArray()) {
    list = root.value("data").toArray();
  } else if (dataObj.contains("data")) {
    // 尝试 data.data.list
    QJsonObject innerData = dataObj.value("data").toObject();
    if (innerData.contains("list")) {
      list = innerData.value("list").toArray();
    }
  }

  if (list.isEmpty()) {
    m_statusLabel->setText(QStringLiteral("未找到 data.list 数据"));
    m_statusLabel->setStyleSheet(
        QStringLiteral("color: %1;").arg(AuiStyle::errorTextColor().name()));
    return;
  }

  // 取第一行数据提取字段名
  QJsonObject firstRow = list.at(0).toObject();
  QStringList fieldNames = firstRow.keys();

  if (fieldNames.isEmpty()) {
    m_statusLabel->setText(QStringLiteral("返回数据无字段"));
    m_statusLabel->setStyleSheet(
        QStringLiteral("color: %1;").arg(AuiStyle::errorTextColor().name()));
    return;
  }

  // 填充预览表格（显示前 5 行）
  int displayRows = qMin(list.size(), 5);
  m_previewTable->setRowCount(displayRows);
  m_previewTable->setColumnCount(fieldNames.size());
  m_previewTable->setHorizontalHeaderLabels(fieldNames);

  for (int r = 0; r < displayRows; ++r) {
    QJsonObject row = list.at(r).toObject();
    for (int c = 0; c < fieldNames.size(); ++c) {
      QString val = row.value(fieldNames[c]).toVariant().toString();
      m_previewTable->setItem(r, c, new QTableWidgetItem(val));
    }
  }

  // 保存当前选中的字段名
  QString prevValue = m_valueCombo->currentText();
  QString prevLabel = m_labelCombo->currentText();

  // 更新字段下拉框
  m_valueCombo->clear();
  m_labelCombo->clear();
  m_valueCombo->addItems(fieldNames);
  m_labelCombo->addItems(fieldNames);

  // 恢复之前的选择
  if (!prevValue.isEmpty()) m_valueCombo->setCurrentText(prevValue);
  if (!prevLabel.isEmpty()) m_labelCombo->setCurrentText(prevLabel);

  m_statusLabel->setText(
      QStringLiteral("成功获取 %1 条数据，%2 个字段").arg(list.size()).arg(fieldNames.size()));
  m_statusLabel->setStyleSheet(
      QStringLiteral("color: %1;").arg(AuiStyle::successTextColor().name()));
}

void ComboboxConfigDialog::onHttpError(const QString &url, const QString &errorMsg) {
  Q_UNUSED(url);
  m_statusLabel->setText(QStringLiteral("请求失败: %1").arg(errorMsg));
  m_statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(AuiStyle::errorTextColor().name()));
}
