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
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "config_dialog_common.h"
#include "src/util/common/code_constants.h"
#include "src/util/common/http_client.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/component/aui_message_box.h"
#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

ComboboxConfigDialog::ComboboxConfigDialog(QWidget *parent) : QDialog(parent) { setupUI(); }

// ════════════════════════════════════════════════════════════
//  界面构建
// ════════════════════════════════════════════════════════════

void ComboboxConfigDialog::setupUI() {
  ConfigDialogFrame frame =
      beginConfigDialog(this, QStringLiteral("下拉框数据源配置"), QMargins(12, 10, 12, 10), 6);
  auto *layout = frame.contentLayout;

  // ── URL 输入行（含请求方式，普通/查询分页均需要）──
  auto *urlRow = new QHBoxLayout;
  urlRow->addWidget(new QLabel(QStringLiteral("请求URL:")));
  // 方法下拉框（GET/POST，隐藏三角箭头使文字完整显示），置于 URL 输入框之前
  m_methodCombo = new QComboBox(frame.contentWidget);
  m_methodCombo->addItems(
      {QString::fromLatin1(JsonVueHttp::kPost), QString::fromLatin1(JsonVueHttp::kGet)});
  m_methodCombo->setCurrentIndex(0);  // 默认 POST
  m_methodCombo->setFixedWidth(60);
  AuiComboBox::hideArrow(m_methodCombo);
  urlRow->addWidget(m_methodCombo);
  urlRow->addSpacing(10);
  m_urlEdit = new QLineEdit(frame.contentWidget);
  m_urlEdit->setPlaceholderText(QStringLiteral("/api/xxx/list"));
  urlRow->addWidget(m_urlEdit, 1);
  m_testBtn = new QPushButton(QStringLiteral("测试"), frame.contentWidget);
  urlRow->addWidget(m_testBtn);
  // 让方法下拉框与 URL 输入框高度一致，避免并排时错位
  m_methodCombo->setFixedHeight(m_urlEdit->sizeHint().height());
  layout->addLayout(urlRow);

  // ── 加载方式行（普通 / 查询分页）──
  auto *typeRow = new QHBoxLayout;
  typeRow->addWidget(new QLabel(QStringLiteral("加载方式:")));
  m_typeCombo = new QComboBox(frame.contentWidget);
  m_typeCombo->addItem(QStringLiteral("普通加载(一次性)"), false);
  m_typeCombo->addItem(QStringLiteral("查询分页加载"), true);
  m_typeCombo->setMinimumWidth(180);
  typeRow->addWidget(m_typeCombo);
  typeRow->addStretch();
  layout->addLayout(typeRow);

  // ── 查询分页配置区域（仅在"查询分页加载"时显示）──
  m_pagedGroup = new QWidget(frame.contentWidget);
  auto *pagedCol = new QVBoxLayout(m_pagedGroup);
  pagedCol->setContentsMargins(0, 0, 0, 0);
  pagedCol->setSpacing(6);

  // 行1：页码参数 / 页大小参数 / 默认页大小
  auto *pagedRow1 = new QHBoxLayout;
  pagedRow1->setSpacing(6);
  pagedRow1->addWidget(new QLabel(QStringLiteral("页码参数:")));
  m_pageKeyEdit = new QLineEdit(m_pagedGroup);
  m_pageKeyEdit->setText(QStringLiteral("page"));
  m_pageKeyEdit->setMaximumWidth(90);
  pagedRow1->addWidget(m_pageKeyEdit);
  pagedRow1->addSpacing(12);
  pagedRow1->addWidget(new QLabel(QStringLiteral("页大小参数:")));
  m_pageSizeKeyEdit = new QLineEdit(m_pagedGroup);
  m_pageSizeKeyEdit->setText(QStringLiteral("pageSize"));
  m_pageSizeKeyEdit->setMaximumWidth(90);
  pagedRow1->addWidget(m_pageSizeKeyEdit);
  pagedRow1->addSpacing(12);
  pagedRow1->addWidget(new QLabel(QStringLiteral("默认页大小:")));
  m_pageSizeSpin = new QSpinBox(m_pagedGroup);
  m_pageSizeSpin->setRange(1, 500);
  m_pageSizeSpin->setValue(20);
  pagedRow1->addWidget(m_pageSizeSpin);
  pagedRow1->addStretch();
  pagedCol->addLayout(pagedRow1);

  // 行2：提示（搜索框提示）/ 字段名（搜索参数 key）
  auto *pagedRow2 = new QHBoxLayout;
  pagedRow2->setSpacing(6);
  pagedRow2->addWidget(new QLabel(QStringLiteral("提示:")));
  m_searchTitleEdit = new QLineEdit(m_pagedGroup);
  m_searchTitleEdit->setPlaceholderText(QStringLiteral("搜索框提示文字"));
  m_searchTitleEdit->setMaximumWidth(120);
  pagedRow2->addWidget(m_searchTitleEdit);
  pagedRow2->addSpacing(12);
  pagedRow2->addWidget(new QLabel(QStringLiteral("字段名:")));
  m_searchFieldEdit = new QLineEdit(m_pagedGroup);
  m_searchFieldEdit->setPlaceholderText(QStringLiteral("搜索参数key，如 name"));
  m_searchFieldEdit->setMaximumWidth(120);
  pagedRow2->addWidget(m_searchFieldEdit);
  pagedRow2->addStretch();
  pagedCol->addLayout(pagedRow2);

  layout->addWidget(m_pagedGroup);

  // ── 加载方式切换：显示/隐藏分页配置区 ──
  auto applyType = [this](int index) {
    const bool paged = (index >= 0) && m_typeCombo->itemData(index).toBool();
    m_pagedGroup->setVisible(paged);
  };
  connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyType);
  applyType(m_typeCombo->currentIndex());

  // ── 状态标签 ──
  m_statusLabel = new QLabel(frame.contentWidget);
  m_statusLabel->setStyleSheet(
      QStringLiteral("color: %1; font-size: 12px;").arg(AuiStyle::mutedTextColor().name()));
  layout->addWidget(m_statusLabel);

  // ── 数据预览表格 ──
  layout->addWidget(new QLabel(QStringLiteral("返回数据示例:")));
  m_previewTable = new QTableWidget(frame.contentWidget);
  m_previewTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_previewTable->horizontalHeader()->setStretchLastSection(true);
  layout->addWidget(m_previewTable, 1);

  // ── 字段选择行 ──
  auto *fieldRow = new QHBoxLayout;

  fieldRow->addWidget(new QLabel(QStringLiteral("Label字段(显示文本):")));
  m_labelCombo = new QComboBox(frame.contentWidget);
  m_labelCombo->setMinimumWidth(120);
  fieldRow->addWidget(m_labelCombo);

  fieldRow->addSpacing(20);

  fieldRow->addWidget(new QLabel(QStringLiteral("Value字段(实际值):")));
  m_valueCombo = new QComboBox(frame.contentWidget);
  m_valueCombo->setMinimumWidth(120);
  fieldRow->addWidget(m_valueCombo);

  fieldRow->addStretch();
  layout->addLayout(fieldRow);

  // 确定/取消按钮由 finishConfigDialog 统一添加（复用通用样式）
  finishConfigDialog(this, frame);

  setMinimumSize(500, 400);

  connect(m_testBtn, &QPushButton::clicked, this, &ComboboxConfigDialog::onTest);
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

void ComboboxConfigDialog::setPagedConfig(bool paged, const QString &pageKey,
                                          const QString &pageSizeKey, int pageSize,
                                          const QString &searchTitle, const QString &searchField,
                                          const QString &method) {
  const int idx = (paged ? 1 : 0);
  if (idx < m_typeCombo->count()) m_typeCombo->setCurrentIndex(idx);
  if (!pageKey.isEmpty()) m_pageKeyEdit->setText(pageKey);
  if (!pageSizeKey.isEmpty()) m_pageSizeKeyEdit->setText(pageSizeKey);
  if (pageSize > 0) m_pageSizeSpin->setValue(pageSize);
  if (!searchTitle.isEmpty()) m_searchTitleEdit->setText(searchTitle);
  if (!searchField.isEmpty()) m_searchFieldEdit->setText(searchField);
  if (!method.isEmpty()) m_methodCombo->setCurrentText(method);
  m_pagedGroup->setVisible(paged);
}

QString ComboboxConfigDialog::url() const { return m_urlEdit->text().trimmed(); }

QString ComboboxConfigDialog::valueField() const { return m_valueCombo->currentText(); }

QString ComboboxConfigDialog::labelField() const { return m_labelCombo->currentText(); }

bool ComboboxConfigDialog::paged() const {
  return m_typeCombo->currentIndex() >= 1 &&
         (m_typeCombo->itemData(m_typeCombo->currentIndex()).toBool());
}

QString ComboboxConfigDialog::pageKey() const {
  const QString v = m_pageKeyEdit->text().trimmed();
  return v.isEmpty() ? QStringLiteral("page") : v;
}

QString ComboboxConfigDialog::pageSizeKey() const {
  const QString v = m_pageSizeKeyEdit->text().trimmed();
  return v.isEmpty() ? QStringLiteral("pageSize") : v;
}

int ComboboxConfigDialog::pageSize() const {
  return m_pageSizeSpin ? m_pageSizeSpin->value() : 20;
}

QString ComboboxConfigDialog::searchTitle() const {
  return m_searchTitleEdit ? m_searchTitleEdit->text().trimmed() : QString();
}

QString ComboboxConfigDialog::searchField() const {
  return m_searchFieldEdit ? m_searchFieldEdit->text().trimmed() : QString();
}

QString ComboboxConfigDialog::method() const {
  return m_methodCombo ? m_methodCombo->currentText() : QString::fromLatin1(JsonVueHttp::kPost);
}

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
    AuiMessageBox::show(this, QStringLiteral("提示"), QStringLiteral("请输入请求URL"));
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

  // 使用 POST 或 GET 发送请求，只回调给本对话框绑定的回调（HttpClient 区分发起方，不广播）
  HttpClient::Method method = bodyObj.isEmpty() ? HttpClient::Get : HttpClient::Post;
  HttpClient::instance().request(
      method, fullUrl, bodyObj, headers, [this](const QJsonDocument &doc) { onHttpFinished(doc); },
      [this](const QString &errorMsg) { onHttpError(errorMsg); }, this);
}

void ComboboxConfigDialog::onHttpFinished(const QJsonDocument &doc) {
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

void ComboboxConfigDialog::onHttpError(const QString &errorMsg) {
  m_statusLabel->setText(QStringLiteral("请求失败: %1").arg(errorMsg));
  m_statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(AuiStyle::errorTextColor().name()));
}
