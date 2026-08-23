/**
 * @file select_source_panel.cpp
 * @brief 动态数据源配置面板实现
 *
 * 从 ComboboxConfigDialog 中抽取的动态数据源配置 UI，
 * 供 jsonvue 下拉框配置与 .jsonsource 动态数据源编辑共用。
 */

#include "select_source_panel.h"

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

#include "json_vue_editor_helpers.h"
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

SelectSourcePanel::SelectSourcePanel(QWidget *parent) : QWidget(parent) { setupUI(); }

// ════════════════════════════════════════════════════════════
//  界面构建
// ════════════════════════════════════════════════════════════

void SelectSourcePanel::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  // ── URL 输入行（含请求方式）──
  auto *urlRow = new QHBoxLayout;
  urlRow->addWidget(new QLabel(QStringLiteral("请求URL:")));
  m_methodCombo = new QComboBox(this);
  m_methodCombo->addItems(
      {QString::fromLatin1(JsonVueHttp::kPost), QString::fromLatin1(JsonVueHttp::kGet)});
  m_methodCombo->setCurrentIndex(0);
  m_methodCombo->setFixedWidth(60);
  AuiComboBox::hideArrow(m_methodCombo);
  urlRow->addWidget(m_methodCombo);
  urlRow->addSpacing(10);
  m_urlEdit = new QLineEdit(this);
  m_urlEdit->setPlaceholderText(QStringLiteral("/api/xxx/list"));
  urlRow->addWidget(m_urlEdit, 1);
  m_testBtn = new QPushButton(QStringLiteral("测试"), this);
  urlRow->addWidget(m_testBtn);
  m_methodCombo->setFixedHeight(m_urlEdit->sizeHint().height());
  layout->addLayout(urlRow);

  // ── 加载方式行 ──
  auto *typeRow = new QHBoxLayout;
  typeRow->addWidget(new QLabel(QStringLiteral("加载方式:")));
  m_typeCombo = new QComboBox(this);
  m_typeCombo->addItem(QStringLiteral("普通加载(一次性)"), false);
  m_typeCombo->addItem(QStringLiteral("查询分页加载"), true);
  m_typeCombo->setMinimumWidth(180);
  typeRow->addWidget(m_typeCombo);
  typeRow->addStretch();
  layout->addLayout(typeRow);

  // ── 查询分页配置区域 ──
  m_pagedGroup = new QWidget(this);
  auto *pagedCol = new QVBoxLayout(m_pagedGroup);
  pagedCol->setContentsMargins(0, 0, 0, 0);
  pagedCol->setSpacing(6);

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

  auto applyType = [this](int index) {
    const bool paged = (index >= 0) && m_typeCombo->itemData(index).toBool();
    m_pagedGroup->setVisible(paged);
  };
  connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyType);
  applyType(m_typeCombo->currentIndex());

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

  // ── 字段选择行：Label/Value 为普通下拉框，点击「测试」后列出返回示例的列，供从中选择 ──
  auto *fieldRow = new QHBoxLayout;
  fieldRow->addWidget(new QLabel(QStringLiteral("Label字段(显示文本):")));
  m_labelCombo = new QComboBox(this);
  m_labelCombo->setMinimumWidth(120);
  fieldRow->addWidget(m_labelCombo);
  fieldRow->addSpacing(20);
  fieldRow->addWidget(new QLabel(QStringLiteral("Value字段(实际值):")));
  m_valueCombo = new QComboBox(this);
  m_valueCombo->setMinimumWidth(120);
  fieldRow->addWidget(m_valueCombo);
  fieldRow->addStretch();
  layout->addLayout(fieldRow);

  connect(m_testBtn, &QPushButton::clicked, this, &SelectSourcePanel::onTest);

  // 面板内部任何输入变化都上报 configChanged
  auto notify = [this]() { emit configChanged(); };
  connect(m_urlEdit, &QLineEdit::textChanged, this, notify);
  connect(m_methodCombo, &QComboBox::currentTextChanged, this, notify);
  connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notify);
  connect(m_pageKeyEdit, &QLineEdit::textChanged, this, notify);
  connect(m_pageSizeKeyEdit, &QLineEdit::textChanged, this, notify);
  connect(m_pageSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notify);
  connect(m_searchTitleEdit, &QLineEdit::textChanged, this, notify);
  connect(m_searchFieldEdit, &QLineEdit::textChanged, this, notify);
  connect(m_valueCombo, &QComboBox::currentTextChanged, this, notify);
  connect(m_labelCombo, &QComboBox::currentTextChanged, this, notify);
}

// ════════════════════════════════════════════════════════════
//  配置读写
// ════════════════════════════════════════════════════════════

void SelectSourcePanel::setData(const QString &url, const QString &method,
                                const QString &valueField, const QString &labelField, bool paged,
                                const QString &pageKey, const QString &pageSizeKey, int pageSize,
                                const QString &searchTitle, const QString &searchField) {
  m_urlEdit->setText(url);
  if (!method.isEmpty()) m_methodCombo->setCurrentText(method);
  // 已有字段名先加入下拉框（测试成功后更新完整列表）
  if (!valueField.isEmpty() && m_valueCombo->findText(valueField) < 0) {
    m_valueCombo->addItem(valueField);
  }
  if (!labelField.isEmpty() && m_labelCombo->findText(labelField) < 0) {
    m_labelCombo->addItem(labelField);
  }
  m_valueCombo->setCurrentText(valueField);
  m_labelCombo->setCurrentText(labelField);
  const int idx = (paged ? 1 : 0);
  if (idx < m_typeCombo->count()) m_typeCombo->setCurrentIndex(idx);
  if (!pageKey.isEmpty()) m_pageKeyEdit->setText(pageKey);
  if (!pageSizeKey.isEmpty()) m_pageSizeKeyEdit->setText(pageSizeKey);
  if (pageSize > 0) m_pageSizeSpin->setValue(pageSize);
  if (!searchTitle.isEmpty()) m_searchTitleEdit->setText(searchTitle);
  if (!searchField.isEmpty()) m_searchFieldEdit->setText(searchField);
  m_pagedGroup->setVisible(paged);
}

void SelectSourcePanel::setHttpConfig(const QString &baseUrl, const QString &authHeader,
                                      const QString &postData) {
  m_baseUrl = baseUrl;
  m_authHeader = authHeader;
  m_postData = postData;
}

void SelectSourcePanel::setBaseLocked(bool locked) {
  m_urlEdit->setReadOnly(locked);
  m_methodCombo->setEnabled(!locked);
  m_typeCombo->setEnabled(!locked);
  m_pagedGroup->setEnabled(!locked);
}

void SelectSourcePanel::setFieldsLocked(bool locked) {
  m_labelCombo->setEnabled(!locked);
  m_valueCombo->setEnabled(!locked);
}

QString SelectSourcePanel::url() const { return m_urlEdit->text().trimmed(); }

QString SelectSourcePanel::method() const {
  return m_methodCombo ? m_methodCombo->currentText() : QString::fromLatin1(JsonVueHttp::kPost);
}

QString SelectSourcePanel::valueField() const { return m_valueCombo->currentText(); }

QString SelectSourcePanel::labelField() const { return m_labelCombo->currentText(); }

bool SelectSourcePanel::paged() const {
  return m_typeCombo->currentIndex() >= 1 &&
         (m_typeCombo->itemData(m_typeCombo->currentIndex()).toBool());
}

QString SelectSourcePanel::pageKey() const {
  const QString v = m_pageKeyEdit->text().trimmed();
  return v.isEmpty() ? QStringLiteral("page") : v;
}

QString SelectSourcePanel::pageSizeKey() const {
  const QString v = m_pageSizeKeyEdit->text().trimmed();
  return v.isEmpty() ? QStringLiteral("pageSize") : v;
}

int SelectSourcePanel::pageSize() const { return m_pageSizeSpin ? m_pageSizeSpin->value() : 20; }

QString SelectSourcePanel::searchTitle() const {
  return m_searchTitleEdit ? m_searchTitleEdit->text().trimmed() : QString();
}

QString SelectSourcePanel::searchField() const {
  return m_searchFieldEdit ? m_searchFieldEdit->text().trimmed() : QString();
}

void SelectSourcePanel::fillSelectFields(QString *url, QString *valueField, QString *labelField,
                                         bool *paged, QString *pageKey, QString *pageSizeKey,
                                         int *pageSize, QString *searchTitle, QString *searchField,
                                         QString *method) const {
  if (url) *url = this->url();
  if (valueField) *valueField = this->valueField();
  if (labelField) *labelField = this->labelField();
  if (paged) *paged = this->paged();
  if (pageKey) *pageKey = this->pageKey();
  if (pageSizeKey) *pageSizeKey = this->pageSizeKey();
  if (pageSize) *pageSize = this->pageSize();
  if (searchTitle) *searchTitle = this->searchTitle();
  if (searchField) *searchField = this->searchField();
  if (method) *method = this->method();
}

// ════════════════════════════════════════════════════════════
//  HTTP 测试
// ════════════════════════════════════════════════════════════

void SelectSourcePanel::onTest() {
  QString url = m_urlEdit->text().trimmed();
  if (url.isEmpty()) {
    AuiMessageBox::show(this, QStringLiteral("提示"), QStringLiteral("请输入请求URL"));
    return;
  }

  QString fullUrl = url;
  if (!m_baseUrl.isEmpty() && !url.startsWith(QStringLiteral("http"))) {
    fullUrl = m_baseUrl + (url.startsWith('/') ? url : "/" + url);
  }

  m_statusLabel->setText(QStringLiteral("正在请求..."));
  m_statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(AuiStyle::mutedTextColor().name()));

  HttpClient::Headers headers;
  if (!m_authHeader.isEmpty()) {
    headers[QStringLiteral("Authorization")] = m_authHeader;
  }

  QJsonObject bodyObj;
  if (!m_postData.isEmpty()) {
    QJsonParseError err;
    QJsonDocument postDoc = UtilJson::fromJson(m_postData, &err);
    if (err.error == QJsonParseError::NoError && postDoc.isObject()) {
      bodyObj = postDoc.object();
    }
  }

  HttpClient::Method httpMethod = bodyObj.isEmpty() ? HttpClient::Get : HttpClient::Post;
  HttpClient::instance().request(
      httpMethod, fullUrl, bodyObj, headers, [this](const QJsonDocument &doc) {
        onHttpFinished(doc);
      },
      [this](const QString &errorMsg) { onHttpError(errorMsg); }, this);
}

void SelectSourcePanel::onHttpFinished(const QJsonDocument &doc) {
  QJsonObject root = doc.object();
  QJsonObject dataObj = root.value("data").toObject();

  QJsonArray list;
  if (dataObj.contains("list")) {
    list = dataObj.value("list").toArray();
  } else if (root.value("data").isArray()) {
    list = root.value("data").toArray();
  } else if (dataObj.contains("data")) {
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

  QJsonObject firstRow = list.at(0).toObject();
  QStringList fieldNames = firstRow.keys();
  if (fieldNames.isEmpty()) {
    m_statusLabel->setText(QStringLiteral("返回数据无字段"));
    m_statusLabel->setStyleSheet(
        QStringLiteral("color: %1;").arg(AuiStyle::errorTextColor().name()));
    return;
  }

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

  QString prevValue = m_valueCombo->currentText();
  QString prevLabel = m_labelCombo->currentText();
  m_valueCombo->clear();
  m_labelCombo->clear();
  m_valueCombo->addItems(fieldNames);
  m_labelCombo->addItems(fieldNames);
  if (!prevValue.isEmpty()) m_valueCombo->setCurrentText(prevValue);
  if (!prevLabel.isEmpty()) m_labelCombo->setCurrentText(prevLabel);

  m_statusLabel->setText(
      QStringLiteral("成功获取 %1 条数据，%2 个字段").arg(list.size()).arg(fieldNames.size()));
  m_statusLabel->setStyleSheet(
      QStringLiteral("color: %1;").arg(AuiStyle::successTextColor().name()));
}

void SelectSourcePanel::onHttpError(const QString &errorMsg) {
  m_statusLabel->setText(QStringLiteral("请求失败: %1").arg(errorMsg));
  m_statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(AuiStyle::errorTextColor().name()));
}
