/**
 * @file json_vue_editor.cpp
 * @brief .jsonvue 可视化编辑器面板实现
 */

#include "json_vue_editor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSplitter>
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

JsonVueEditor::JsonVueEditor(QWidget *parent) : QWidget(parent) { setupUI(); }

// ════════════════════════════════════════════════════════════
//  界面构建
// ════════════════════════════════════════════════════════════

void JsonVueEditor::setupUI() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(2, 2, 2, 2);
  mainLayout->setSpacing(2);

  // 接口配置区
  mainLayout->addWidget(buildMetaSection());

  // 列配置和查询字段区使用可拖动分隔控件
  auto *splitter = new QSplitter(Qt::Vertical, this);
  splitter->addWidget(buildColumnsSection());
  splitter->addWidget(buildQueryFieldsSection());
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);
  mainLayout->addWidget(splitter, 1);

  applyStyle();

  // 连接 HTTP 信号
  connect(&HttpClient::instance(), &HttpClient::finished, this, &JsonVueEditor::onHttpFinished);
  connect(&HttpClient::instance(), &HttpClient::error, this, &JsonVueEditor::onHttpError);

  // 连接静态输入控件的编辑信号
  connectStaticControlSignals();

  // 表格文本单元格编辑信号
  connect(m_columnTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *) {
    if (!m_loading) emit configChanged();
  });
  connect(m_queryTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *) {
    if (!m_loading) emit configChanged();
  });
}

void JsonVueEditor::connectStaticControlSignals() {
  // 接口配置区的输入控件
  connect(m_methodCombo, &QComboBox::currentTextChanged, this, [this]() {
    if (!m_loading) emit configChanged();
  });
  connect(m_dataUrlEdit, &QLineEdit::textChanged, this, [this]() {
    if (!m_loading) emit configChanged();
  });
  connect(m_queryApiEdit, &QLineEdit::textChanged, this, [this]() {
    if (!m_loading) emit configChanged();
  });
  connect(m_deleteApiEdit, &QLineEdit::textChanged, this, [this]() {
    if (!m_loading) emit configChanged();
  });
  connect(m_noDeleteCheck, &QCheckBox::stateChanged, this, [this]() {
    if (!m_loading) emit configChanged();
  });
  connect(m_updateApiEdit, &QLineEdit::textChanged, this, [this]() {
    if (!m_loading) emit configChanged();
  });
  connect(m_noEditCheck, &QCheckBox::stateChanged, this, [this]() {
    if (!m_loading) emit configChanged();
  });
}

void JsonVueEditor::connectCellWidgetSignals(QWidget *widget) {
  if (!widget) return;
  if (auto *cb = qobject_cast<QCheckBox *>(widget)) {
    connect(cb, &QCheckBox::stateChanged, this, [this]() {
      if (!m_loading) emit configChanged();
    });
  } else if (auto *combo = qobject_cast<QComboBox *>(widget)) {
    connect(combo, &QComboBox::currentTextChanged, this, [this]() {
      if (!m_loading) emit configChanged();
    });
  }
}

void JsonVueEditor::setupQueryStyleLinkage(int row) {
  auto *qStyle = qobject_cast<QComboBox *>(m_columnTable->cellWidget(row, ColQueryStyle));
  auto *swEdit = qobject_cast<QCheckBox *>(m_columnTable->cellWidget(row, ColSwitchEditable));
  if (!qStyle || !swEdit) return;

  // 初始状态：text 时禁用，switch 时启用
  swEdit->setEnabled(qStyle->currentText() == QStringLiteral("switch"));

  // 联动：查询样式变化时切换开关可编辑的启用状态
  connect(qStyle, &QComboBox::currentTextChanged, this, [this, swEdit](const QString &text) {
    swEdit->setEnabled(text == QStringLiteral("switch"));
    if (!m_loading) emit configChanged();
  });
}

// 构建接口配置区
QWidget *JsonVueEditor::buildMetaSection() {
  auto *group = new QGroupBox(QStringLiteral("接口配置"), this);
  auto *layout = new QVBoxLayout(group);
  layout->setSpacing(2);
  layout->setContentsMargins(4, 2, 4, 2);  // 内边距

  // 第1行：生成数据 URL（方法下拉框 + URL 输入框 + 生成按钮）
  auto *row1 = new QHBoxLayout;
  row1->addWidget(new QLabel(QStringLiteral("生成数据URL:")));

  m_methodCombo = new QComboBox(this);
  m_methodCombo->addItems({QStringLiteral("GET"), QStringLiteral("POST"), QStringLiteral("PUT"),
                           QStringLiteral("DELETE")});
  m_methodCombo->setFixedWidth(80);
  row1->addWidget(m_methodCombo);

  m_dataUrlEdit = new QLineEdit(this);
  m_dataUrlEdit->setPlaceholderText(QStringLiteral("/api/xxx/getList"));
  row1->addWidget(m_dataUrlEdit, 1);

  m_generateBtn = new QPushButton(QStringLiteral("生成"), this);
  row1->addWidget(m_generateBtn);
  layout->addLayout(row1);

  // 第2行：查询接口
  auto *row2 = new QHBoxLayout;
  row2->addWidget(new QLabel(QStringLiteral("查询接口:")));
  m_queryApiEdit = new QLineEdit(this);
  m_queryApiEdit->setPlaceholderText(QStringLiteral("例如 getConfigGListApi"));
  row2->addWidget(m_queryApiEdit, 1);
  layout->addLayout(row2);

  // 第3行：删除接口 + 不可删除复选框
  auto *row3 = new QHBoxLayout;
  row3->addWidget(new QLabel(QStringLiteral("删除接口:")));
  m_deleteApiEdit = new QLineEdit(this);
  m_deleteApiEdit->setPlaceholderText(QStringLiteral("例如 delConfigGListApi"));
  row3->addWidget(m_deleteApiEdit, 1);
  m_noDeleteCheck = new QCheckBox(QStringLiteral("不可删除"), this);
  row3->addWidget(m_noDeleteCheck);
  layout->addLayout(row3);

  // 第4行：修改接口 + 不可编辑复选框
  auto *row4 = new QHBoxLayout;
  row4->addWidget(new QLabel(QStringLiteral("修改接口:")));
  m_updateApiEdit = new QLineEdit(this);
  m_updateApiEdit->setPlaceholderText(QStringLiteral("例如 updConfigGListApi"));
  row4->addWidget(m_updateApiEdit, 1);
  m_noEditCheck = new QCheckBox(QStringLiteral("不可编辑"), this);
  row4->addWidget(m_noEditCheck);
  layout->addLayout(row4);

  connect(m_generateBtn, &QPushButton::clicked, this, &JsonVueEditor::onGenerate);
  return group;
}

QWidget *JsonVueEditor::buildColumnsSection() {
  auto *group = new QGroupBox(QStringLiteral("列配置（HTTP 返回的数据列）"), this);
  auto *layout = new QVBoxLayout(group);
  layout->setSpacing(2);
  layout->setContentsMargins(2, 2, 2, 2);

  // 操作按钮行
  auto *btnRow = new QHBoxLayout;
  m_moveUpBtn = new QPushButton(QStringLiteral("↑ 上移"), this);
  m_moveDownBtn = new QPushButton(QStringLiteral("↓ 下移"), this);
  m_addColBtn = new QPushButton(QStringLiteral("+ 添加列"), this);
  m_removeColBtn = new QPushButton(QStringLiteral("- 删除列"), this);
  btnRow->addWidget(m_moveUpBtn);
  btnRow->addWidget(m_moveDownBtn);
  btnRow->addWidget(m_addColBtn);
  btnRow->addWidget(m_removeColBtn);
  btnRow->addStretch();
  layout->addLayout(btnRow);

  // 列配置表格
  m_columnTable = new QTableWidget(0, ColCount, this);
  m_columnTable->setHorizontalHeaderLabels(
      {QStringLiteral("数据列名"), QStringLiteral("查询显示"), QStringLiteral("查询列名"),
       QStringLiteral("查询样式"), QStringLiteral("开关可编辑"), QStringLiteral("编辑显示"),
       QStringLiteral("编辑列名"), QStringLiteral("编辑样式"), QStringLiteral("编辑可编辑")});
  m_columnTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  m_columnTable->horizontalHeader()->setStretchLastSection(true);
  m_columnTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_columnTable->setMinimumHeight(60);
  layout->addWidget(m_columnTable);

  connect(m_moveUpBtn, &QPushButton::clicked, this, &JsonVueEditor::onMoveUp);
  connect(m_moveDownBtn, &QPushButton::clicked, this, &JsonVueEditor::onMoveDown);
  connect(m_addColBtn, &QPushButton::clicked, this, &JsonVueEditor::onAddColumn);
  connect(m_removeColBtn, &QPushButton::clicked, this, &JsonVueEditor::onRemoveColumn);

  return group;
}

QWidget *JsonVueEditor::buildQueryFieldsSection() {
  auto *group = new QGroupBox(QStringLiteral("查询设置"), this);
  auto *layout = new QVBoxLayout(group);
  layout->setSpacing(2);
  layout->setContentsMargins(2, 2, 2, 2);

  auto *btnRow = new QHBoxLayout;
  m_addQueryBtn = new QPushButton(QStringLiteral("+ 添加查询字段"), this);
  m_removeQueryBtn = new QPushButton(QStringLiteral("- 删除查询字段"), this);
  btnRow->addWidget(m_addQueryBtn);
  btnRow->addWidget(m_removeQueryBtn);
  btnRow->addStretch();
  layout->addLayout(btnRow);

  m_queryTable = new QTableWidget(0, QColCount, this);
  m_queryTable->setHorizontalHeaderLabels({QStringLiteral("显示列名"), QStringLiteral("数据列名"),
                                           QStringLiteral("输入框样式"),
                                           QStringLiteral("查询关系")});
  m_queryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  m_queryTable->horizontalHeader()->setStretchLastSection(true);
  m_queryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_queryTable->setMinimumHeight(60);
  layout->addWidget(m_queryTable);

  connect(m_addQueryBtn, &QPushButton::clicked, this, &JsonVueEditor::onAddQueryField);
  connect(m_removeQueryBtn, &QPushButton::clicked, this, &JsonVueEditor::onRemoveQueryField);

  return group;
}

/**
 * @brief 应用可视化编辑器的全局 QSS 样式表
 *
 * 统一设置所有子控件的字体大小、内边距、边框等视觉属性，
 * 使界面风格紧凑一致，避免各控件默认间距过大导致操作区域变小。
 *
 * 样式分以下几部分：
 *
 * 1. QGroupBox（接口配置 / 列配置 / 查询设置 三个分组容器）
 *    - font-size:   统一使用 AuiStyle::dialogFontSize() 返回的字号
 *    - font-weight: bold，标题文字加粗以突出分区
 *    - border:      1px 实线边框，颜色取自 AuiStyle::borderColor()
 *    - border-radius: 4px 圆角
 *    - margin-top:  10px，为标题文字预留顶部空间，避免与上方控件贴紧
 *    - padding-top: 5px，标题与下方内容之间的间距
 *
 * 2. QGroupBox::title（分组标题子控件）
 *    - subcontrol-origin: margin，标题定位基准为外边距区域
 *    - left: 2px，标题距离左边框 2px
 *    - padding: 0 2px，标题左右各留 2px 内边距
 *
 * 3. QLabel（如"生成数据URL:"等标签）
 *    - 仅设置字号，无额外内边距
 *
 * 4. QLineEdit / QComboBox（输入框和下拉框）
 *    - padding: 1px 2px，紧凑内边距
 *    - font-size: 统一字号
 *
 * 5. QPushButton（生成、上移、下移、添加列等按钮）
 *    - padding: 2px 8px，按钮内边距
 *    - font-size: 统一字号
 *
 * 6. QTableWidget（列配置表格、查询字段表格）
 *    - font-size: 统一字号
 *
 * 7. QHeaderView::section（表格表头）
 *    - padding: 2px，表头内边距
 *    - font-size: 统一字号
 *
 * @note %1 占位符统一替换为 AuiStyle::dialogFontSize() 返回的字号；
 *       %2 占位符替换为 AuiStyle::borderColor() 返回的边框颜色。
 */
void JsonVueEditor::applyStyle() {
  QString qss =
      QStringLiteral(
          // ── QGroupBox 分组容器样式 ──
          "QGroupBox {"
          "  font-size: %1px; font-weight: bold;"        // 加粗标题
          "  border: 1px solid %4; border-radius: 4px;"  // 边框 + 圆角
          "  margin-top: 10px; padding-top: 5px;"        // 顶部留白 + 标题与内容间距
          "}"
          // ── QGroupBox 标题子控件样式 ──
          "QGroupBox::title {"
          "  subcontrol-origin: margin; left: 2px; padding: 0 2px;"  // 标题定位
          "}"
          // ── QLabel 标签样式 ──
          "QLabel { font-size: %1px; }"
          // ── QLineEdit / QComboBox 输入控件样式 ──
          "QLineEdit, QComboBox { padding: 1px 2px; font-size: %1px; }"
          // ── QPushButton 按钮样式 ──
          "QPushButton { padding: 2px 8px; font-size: %1px; }"
          // ── QTableWidget 表格样式 ──
          "QTableWidget { font-size: %1px; }"
          // ── QHeaderView 表头样式 ──
          "QHeaderView::section { padding: 2px; font-size: %1px; }")
          .arg(QString::number(AuiStyle::dialogFontSize()), AuiStyle::borderColor().name());
  setStyleSheet(qss);
}

// ════════════════════════════════════════════════════════════
//  加载 / 收集配置
// ════════════════════════════════════════════════════════════

void JsonVueEditor::loadConfig(const JsonVueConfig &config) {
  m_loading = true;  // 加载期间抑制 configChanged 信号

  // 接口配置
  m_methodCombo->setCurrentText(config.meta.dataMethod);
  m_dataUrlEdit->setText(config.meta.dataUrl);
  m_queryApiEdit->setText(config.meta.queryApi);
  m_deleteApiEdit->setText(config.meta.deleteApi);
  m_noDeleteCheck->setChecked(config.meta.noDelete);
  m_updateApiEdit->setText(config.meta.updateApi);
  m_noEditCheck->setChecked(config.meta.noEdit);

  // 列配置
  m_columnTable->setRowCount(0);
  for (const auto &col : config.columns) {
    int row = m_columnTable->rowCount();
    m_columnTable->insertRow(row);

    m_columnTable->setItem(row, ColDataName, new QTableWidgetItem(col.dataName));

    auto *qVis = new QCheckBox;
    qVis->setChecked(col.queryVisible);
    m_columnTable->setCellWidget(row, ColQueryVisible, qVis);
    connectCellWidgetSignals(qVis);

    m_columnTable->setItem(row, ColQueryName, new QTableWidgetItem(col.queryName));

    auto *qStyle = new QComboBox;
    qStyle->addItems({QStringLiteral("text"), QStringLiteral("switch")});
    qStyle->setCurrentText(listStyleToString(col.queryStyle));
    m_columnTable->setCellWidget(row, ColQueryStyle, qStyle);
    connectCellWidgetSignals(qStyle);

    auto *swEdit = new QCheckBox;
    swEdit->setChecked(col.switchEditable);
    m_columnTable->setCellWidget(row, ColSwitchEditable, swEdit);
    connectCellWidgetSignals(swEdit);

    auto *eVis = new QCheckBox;
    eVis->setChecked(col.editVisible);
    m_columnTable->setCellWidget(row, ColEditVisible, eVis);
    connectCellWidgetSignals(eVis);

    m_columnTable->setItem(row, ColEditName, new QTableWidgetItem(col.editName));

    auto *eStyle = new QComboBox;
    eStyle->addItems({QStringLiteral("text"), QStringLiteral("int"), QStringLiteral("float"),
                      QStringLiteral("date"), QStringLiteral("combobox"),
                      QStringLiteral("textarea")});
    eStyle->setCurrentText(editStyleToString(col.editStyle));
    m_columnTable->setCellWidget(row, ColEditStyle, eStyle);
    connectCellWidgetSignals(eStyle);

    auto *eEdit = new QCheckBox;
    eEdit->setChecked(col.editEditable);
    m_columnTable->setCellWidget(row, ColEditEditable, eEdit);
    connectCellWidgetSignals(eEdit);

    // 查询样式与开关可编辑联动
    setupQueryStyleLinkage(row);
  }

  // 查询字段
  m_queryTable->setRowCount(0);
  for (const auto &q : config.queryFields) {
    int row = m_queryTable->rowCount();
    m_queryTable->insertRow(row);

    m_queryTable->setItem(row, QColDisplayName, new QTableWidgetItem(q.displayName));

    auto *dataCombo = new QComboBox;
    dataCombo->addItems(columnDataNames());
    dataCombo->setCurrentText(q.dataName);
    m_queryTable->setCellWidget(row, QColDataName, dataCombo);
    connectCellWidgetSignals(dataCombo);

    auto *inputStyle = new QComboBox;
    inputStyle->addItems({QStringLiteral("text"), QStringLiteral("time")});
    inputStyle->setCurrentText(queryInputStyleToString(q.inputStyle));
    m_queryTable->setCellWidget(row, QColInputStyle, inputStyle);
    connectCellWidgetSignals(inputStyle);

    auto *relCombo = new QComboBox;
    relCombo->addItems({QStringLiteral("="), QStringLiteral("like"), QStringLiteral(">="),
                        QStringLiteral("<="), QStringLiteral(">"), QStringLiteral("<")});
    relCombo->setCurrentText(queryRelationToString(q.relation));
    m_queryTable->setCellWidget(row, QColRelation, relCombo);
    connectCellWidgetSignals(relCombo);
  }

  m_loading = false;
}

JsonVueConfig JsonVueEditor::collectConfig() const {
  JsonVueConfig cfg;

  // 接口配置
  cfg.meta.dataMethod = m_methodCombo->currentText();
  cfg.meta.dataUrl = m_dataUrlEdit->text().trimmed();
  cfg.meta.queryApi = m_queryApiEdit->text().trimmed();
  cfg.meta.deleteApi = m_deleteApiEdit->text().trimmed();
  cfg.meta.noDelete = m_noDeleteCheck->isChecked();
  cfg.meta.updateApi = m_updateApiEdit->text().trimmed();
  cfg.meta.noEdit = m_noEditCheck->isChecked();

  // 列配置
  for (int row = 0; row < m_columnTable->rowCount(); ++row) {
    ColumnConfig col;
    auto *dnItem = m_columnTable->item(row, ColDataName);
    if (dnItem) col.dataName = dnItem->text().trimmed();
    auto *qVis = qobject_cast<QCheckBox *>(m_columnTable->cellWidget(row, ColQueryVisible));
    if (qVis) col.queryVisible = qVis->isChecked();
    auto *qnItem = m_columnTable->item(row, ColQueryName);
    if (qnItem) col.queryName = qnItem->text().trimmed();
    auto *qStyle = qobject_cast<QComboBox *>(m_columnTable->cellWidget(row, ColQueryStyle));
    if (qStyle) col.queryStyle = stringToListStyle(qStyle->currentText());
    auto *swEdit = qobject_cast<QCheckBox *>(m_columnTable->cellWidget(row, ColSwitchEditable));
    if (swEdit) col.switchEditable = swEdit->isChecked();
    auto *eVis = qobject_cast<QCheckBox *>(m_columnTable->cellWidget(row, ColEditVisible));
    if (eVis) col.editVisible = eVis->isChecked();
    auto *enItem = m_columnTable->item(row, ColEditName);
    if (enItem) col.editName = enItem->text().trimmed();
    auto *eStyle = qobject_cast<QComboBox *>(m_columnTable->cellWidget(row, ColEditStyle));
    if (eStyle) col.editStyle = stringToEditStyle(eStyle->currentText());
    auto *eEdit = qobject_cast<QCheckBox *>(m_columnTable->cellWidget(row, ColEditEditable));
    if (eEdit) col.editEditable = eEdit->isChecked();

    if (!col.dataName.isEmpty()) cfg.columns.append(col);
  }

  // 查询字段
  for (int row = 0; row < m_queryTable->rowCount(); ++row) {
    QueryFieldConfig q;
    auto *dnItem = m_queryTable->item(row, QColDisplayName);
    if (dnItem) q.displayName = dnItem->text().trimmed();
    auto *dataCombo = qobject_cast<QComboBox *>(m_queryTable->cellWidget(row, QColDataName));
    if (dataCombo) q.dataName = dataCombo->currentText();
    auto *inputStyle = qobject_cast<QComboBox *>(m_queryTable->cellWidget(row, QColInputStyle));
    if (inputStyle) q.inputStyle = stringToQueryInputStyle(inputStyle->currentText());
    auto *relCombo = qobject_cast<QComboBox *>(m_queryTable->cellWidget(row, QColRelation));
    if (relCombo) q.relation = stringToQueryRelation(relCombo->currentText());

    if (!q.displayName.isEmpty()) cfg.queryFields.append(q);
  }

  return cfg;
}

// ════════════════════════════════════════════════════════════
//  HTTP 生成
// ════════════════════════════════════════════════════════════

void JsonVueEditor::loadHttpConfigFromAcFile(const QString &acFilePath) {
  if (acFilePath.isEmpty()) return;
  QFile f(acFilePath);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  QString content = QString::fromUtf8(f.readAll());
  f.close();

  // 用正则提取 AC 脚本中的常量定义
  // 匹配: let varName: String = "value"; （支持转义引号 \"）
  static const QRegularExpression rx(
      QStringLiteral(R"rx(let\s+(\w+)\s*:\s*String\s*=\s*"((?:[^"\\]|\\.)*)")rx"));
  auto it = rx.globalMatch(content);
  while (it.hasNext()) {
    auto m = it.next();
    QString name = m.captured(1);
    QString value = m.captured(2);
    // 反转义: \" → "，\\ → 反斜杠
    value.replace(QStringLiteral("\\\""), QStringLiteral("\""));
    value.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    if (name == QStringLiteral("baseUrl")) {
      m_baseUrl = value;
    } else if (name == QStringLiteral("authHeader")) {
      m_authHeader = value;
    } else if (name == QStringLiteral("postData")) {
      m_postData = value;
    }
  }
}

void JsonVueEditor::onGenerate() {
  QString url = m_dataUrlEdit->text().trimmed();
  if (url.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入生成数据URL"));
    return;
  }

  // 拼接 baseUrl
  QString fullUrl = url;
  if (m_baseUrl.isEmpty()) {
    // baseUrl 为空时直接使用 url（如果 url 已是完整 URL）
  } else {
    // 拼接 baseUrl + url
    if (url.startsWith(QStringLiteral("http://")) || url.startsWith(QStringLiteral("https://"))) {
      fullUrl = url;
    } else {
      QString base = m_baseUrl;
      while (base.endsWith('/')) base.chop(1);
      if (!url.startsWith('/')) url.prepend('/');
      fullUrl = base + url;
    }
  }

  m_generateBtn->setEnabled(false);
  m_generateBtn->setText(QStringLiteral("请求中..."));

  HttpClient::Method method = HttpClient::Get;
  QString methodStr = m_methodCombo->currentText();
  if (methodStr == QStringLiteral("POST"))
    method = HttpClient::Post;
  else if (methodStr == QStringLiteral("PUT"))
    method = HttpClient::Put;
  else if (methodStr == QStringLiteral("DELETE"))
    method = HttpClient::Delete;

  // 解析 POST 数据
  QJsonObject bodyObj;
  if (method == HttpClient::Post || method == HttpClient::Put) {
    if (!m_postData.isEmpty()) {
      QJsonParseError err;
      QJsonDocument postDoc = UtilJson::fromJson(m_postData, &err);
      if (err.error == QJsonParseError::NoError && postDoc.isObject()) {
        bodyObj = postDoc.object();
      }
    }
  }

  // 构建请求头
  HttpClient::Headers headers;
  if (!m_authHeader.isEmpty()) {
    headers[QStringLiteral("Authorization")] = m_authHeader;
  }

  HttpClient::instance().request(method, fullUrl, bodyObj, headers, this);
}

void JsonVueEditor::onHttpFinished(const QString &url, const QJsonDocument &doc) {
  Q_UNUSED(url);
  m_generateBtn->setEnabled(true);
  m_generateBtn->setText(QStringLiteral("生成"));

  populateColumnsFromHttp(doc);
}

void JsonVueEditor::onHttpError(const QString &url, const QString &errorMsg) {
  Q_UNUSED(url);
  m_generateBtn->setEnabled(true);
  m_generateBtn->setText(QStringLiteral("生成"));
  QMessageBox::warning(this, QStringLiteral("请求失败"), errorMsg);
}

void JsonVueEditor::populateColumnsFromHttp(const QJsonDocument &doc) {
  // 解析 { code, msg, data: { list: [ { col1, col2, ... } ] } }
  QJsonObject root = doc.object();
  QJsonValue dataVal = root.value(QStringLiteral("data"));
  QJsonArray listArr;

  if (dataVal.isObject()) {
    listArr = dataVal.toObject().value(QStringLiteral("list")).toArray();
  } else if (dataVal.isArray()) {
    listArr = dataVal.toArray();
  } else {
    // 没有包裹层，直接是数组
    if (doc.isArray()) listArr = doc.array();
  }

  if (listArr.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("提示"),
                             QStringLiteral("返回数据中未找到 list 数组"));
    return;
  }

  // 取第一行的列名
  QJsonObject firstRow = listArr.at(0).toObject();
  if (firstRow.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("返回数据第一行为空"));
    return;
  }

  // 保留旧的列配置（用于合并），按 dataName 索引
  QHash<QString, ColumnConfig> oldCols;
  for (int row = 0; row < m_columnTable->rowCount(); ++row) {
    auto *item = m_columnTable->item(row, ColDataName);
    if (item && !item->text().trimmed().isEmpty()) {
      ColumnConfig c;
      c.dataName = item->text().trimmed();
      auto *qVis = qobject_cast<QCheckBox *>(m_columnTable->cellWidget(row, ColQueryVisible));
      if (qVis) c.queryVisible = qVis->isChecked();
      auto *qnItem = m_columnTable->item(row, ColQueryName);
      if (qnItem) c.queryName = qnItem->text().trimmed();
      auto *qStyle = qobject_cast<QComboBox *>(m_columnTable->cellWidget(row, ColQueryStyle));
      if (qStyle) c.queryStyle = stringToListStyle(qStyle->currentText());
      auto *swEdit = qobject_cast<QCheckBox *>(m_columnTable->cellWidget(row, ColSwitchEditable));
      if (swEdit) c.switchEditable = swEdit->isChecked();
      auto *eVis = qobject_cast<QCheckBox *>(m_columnTable->cellWidget(row, ColEditVisible));
      if (eVis) c.editVisible = eVis->isChecked();
      auto *enItem = m_columnTable->item(row, ColEditName);
      if (enItem) c.editName = enItem->text().trimmed();
      auto *eStyle = qobject_cast<QComboBox *>(m_columnTable->cellWidget(row, ColEditStyle));
      if (eStyle) c.editStyle = stringToEditStyle(eStyle->currentText());
      auto *eEdit = qobject_cast<QCheckBox *>(m_columnTable->cellWidget(row, ColEditEditable));
      if (eEdit) c.editEditable = eEdit->isChecked();
      oldCols[c.dataName] = c;
    }
  }

  // 清空并重新填充
  m_loading = true;
  m_columnTable->setRowCount(0);

  for (auto it = firstRow.begin(); it != firstRow.end(); ++it) {
    QString colName = it.key();
    ColumnConfig col;
    col.dataName = colName;
    // 默认查询列名和编辑列名等于数据列名
    col.queryName = colName;
    col.editName = colName;

    // 如果旧配置中已有该列，保留旧配置
    if (oldCols.contains(colName)) {
      ColumnConfig old = oldCols[colName];
      col.queryVisible = old.queryVisible;
      col.queryName = old.queryName.isEmpty() ? colName : old.queryName;
      col.queryStyle = old.queryStyle;
      col.switchEditable = old.switchEditable;
      col.editVisible = old.editVisible;
      col.editName = old.editName.isEmpty() ? colName : old.editName;
      col.editStyle = old.editStyle;
      col.editEditable = old.editEditable;
    }

    int row = m_columnTable->rowCount();
    m_columnTable->insertRow(row);

    m_columnTable->setItem(row, ColDataName, new QTableWidgetItem(col.dataName));

    auto *qVis = new QCheckBox;
    qVis->setChecked(col.queryVisible);
    m_columnTable->setCellWidget(row, ColQueryVisible, qVis);
    connectCellWidgetSignals(qVis);

    m_columnTable->setItem(row, ColQueryName, new QTableWidgetItem(col.queryName));

    auto *qStyle = new QComboBox;
    qStyle->addItems({QStringLiteral("text"), QStringLiteral("switch")});
    qStyle->setCurrentText(listStyleToString(col.queryStyle));
    m_columnTable->setCellWidget(row, ColQueryStyle, qStyle);
    connectCellWidgetSignals(qStyle);

    auto *swEdit = new QCheckBox;
    swEdit->setChecked(col.switchEditable);
    m_columnTable->setCellWidget(row, ColSwitchEditable, swEdit);
    connectCellWidgetSignals(swEdit);

    auto *eVis = new QCheckBox;
    eVis->setChecked(col.editVisible);
    m_columnTable->setCellWidget(row, ColEditVisible, eVis);
    connectCellWidgetSignals(eVis);

    m_columnTable->setItem(row, ColEditName, new QTableWidgetItem(col.editName));

    auto *eStyle = new QComboBox;
    eStyle->addItems({QStringLiteral("text"), QStringLiteral("int"), QStringLiteral("float"),
                      QStringLiteral("date"), QStringLiteral("combobox"),
                      QStringLiteral("textarea")});
    eStyle->setCurrentText(editStyleToString(col.editStyle));
    m_columnTable->setCellWidget(row, ColEditStyle, eStyle);
    connectCellWidgetSignals(eStyle);

    auto *eEdit = new QCheckBox;
    eEdit->setChecked(col.editEditable);
    m_columnTable->setCellWidget(row, ColEditEditable, eEdit);
    connectCellWidgetSignals(eEdit);

    // 查询样式与开关可编辑联动
    setupQueryStyleLinkage(row);
  }

  m_loading = false;

  // 刷新查询字段下拉框
  refreshQueryFieldDataNames();

  emit configChanged();
}

// ════════════════════════════════════════════════════════════
//  列表操作
// ════════════════════════════════════════════════════════════

void JsonVueEditor::onMoveUp() {
  int row = m_columnTable->currentRow();
  if (row <= 0) return;
  // 简单实现：交换两行数据（因为含 cellWidget，重新构造）
  // 收集两行配置
  auto collectRow = [this](int r) -> ColumnConfig {
    ColumnConfig c;
    auto *dn = m_columnTable->item(r, ColDataName);
    if (dn) c.dataName = dn->text();
    auto *qVis = qobject_cast<QCheckBox *>(m_columnTable->cellWidget(r, ColQueryVisible));
    if (qVis) c.queryVisible = qVis->isChecked();
    auto *qn = m_columnTable->item(r, ColQueryName);
    if (qn) c.queryName = qn->text();
    auto *qStyle = qobject_cast<QComboBox *>(m_columnTable->cellWidget(r, ColQueryStyle));
    if (qStyle) c.queryStyle = stringToListStyle(qStyle->currentText());
    auto *sw = qobject_cast<QCheckBox *>(m_columnTable->cellWidget(r, ColSwitchEditable));
    if (sw) c.switchEditable = sw->isChecked();
    auto *eVis = qobject_cast<QCheckBox *>(m_columnTable->cellWidget(r, ColEditVisible));
    if (eVis) c.editVisible = eVis->isChecked();
    auto *en = m_columnTable->item(r, ColEditName);
    if (en) c.editName = en->text();
    auto *eStyle = qobject_cast<QComboBox *>(m_columnTable->cellWidget(r, ColEditStyle));
    if (eStyle) c.editStyle = stringToEditStyle(eStyle->currentText());
    auto *eEdit = qobject_cast<QCheckBox *>(m_columnTable->cellWidget(r, ColEditEditable));
    if (eEdit) c.editEditable = eEdit->isChecked();
    return c;
  };

  ColumnConfig a = collectRow(row);
  ColumnConfig b = collectRow(row - 1);

  // 用 JsonVueConfig 的 loadConfig 重新加载这两行（避免 cellWidget 复杂性）
  // 这里采用简单方案：直接整体收集配置，交换位置，重新 loadConfig
  JsonVueConfig cfg = collectConfig();
  if (row - 1 >= 0 && row < cfg.columns.size()) {
    cfg.columns.swapItemsAt(row, row - 1);
    int savedQueryCount = cfg.queryFields.size();
    Q_UNUSED(savedQueryCount);
    loadConfig(cfg);
    m_columnTable->selectRow(row - 1);
    emit configChanged();
  }
}

void JsonVueEditor::onMoveDown() {
  int row = m_columnTable->currentRow();
  if (row < 0 || row >= m_columnTable->rowCount() - 1) return;

  JsonVueConfig cfg = collectConfig();
  if (row + 1 < cfg.columns.size()) {
    cfg.columns.swapItemsAt(row, row + 1);
    loadConfig(cfg);
    m_columnTable->selectRow(row + 1);
    emit configChanged();
  }
}

void JsonVueEditor::onAddColumn() {
  int row = m_columnTable->rowCount();
  m_columnTable->insertRow(row);

  m_columnTable->setItem(row, ColDataName, new QTableWidgetItem());

  auto *qVis = new QCheckBox;
  qVis->setChecked(true);
  m_columnTable->setCellWidget(row, ColQueryVisible, qVis);
  connectCellWidgetSignals(qVis);

  m_columnTable->setItem(row, ColQueryName, new QTableWidgetItem());

  auto *qStyle = new QComboBox;
  qStyle->addItems({QStringLiteral("text"), QStringLiteral("switch")});
  m_columnTable->setCellWidget(row, ColQueryStyle, qStyle);
  connectCellWidgetSignals(qStyle);

  auto *swEdit = new QCheckBox;
  m_columnTable->setCellWidget(row, ColSwitchEditable, swEdit);
  connectCellWidgetSignals(swEdit);

  auto *eVis = new QCheckBox;
  eVis->setChecked(true);
  m_columnTable->setCellWidget(row, ColEditVisible, eVis);
  connectCellWidgetSignals(eVis);

  m_columnTable->setItem(row, ColEditName, new QTableWidgetItem());

  auto *eStyle = new QComboBox;
  eStyle->addItems({QStringLiteral("text"), QStringLiteral("int"), QStringLiteral("float"),
                    QStringLiteral("date"), QStringLiteral("combobox")});
  m_columnTable->setCellWidget(row, ColEditStyle, eStyle);
  connectCellWidgetSignals(eStyle);

  auto *eEdit = new QCheckBox;
  eEdit->setChecked(true);
  m_columnTable->setCellWidget(row, ColEditEditable, eEdit);
  connectCellWidgetSignals(eEdit);

  // 查询样式与开关可编辑联动
  setupQueryStyleLinkage(row);

  emit configChanged();
}

void JsonVueEditor::onRemoveColumn() {
  int row = m_columnTable->currentRow();
  if (row < 0) return;
  m_columnTable->removeRow(row);
  refreshQueryFieldDataNames();
  emit configChanged();
}

// ════════════════════════════════════════════════════════════
//  查询字段操作
// ════════════════════════════════════════════════════════════

void JsonVueEditor::onAddQueryField() {
  int row = m_queryTable->rowCount();
  m_queryTable->insertRow(row);

  m_queryTable->setItem(row, QColDisplayName, new QTableWidgetItem());

  auto *dataCombo = new QComboBox;
  dataCombo->addItems(columnDataNames());
  m_queryTable->setCellWidget(row, QColDataName, dataCombo);
  connectCellWidgetSignals(dataCombo);

  auto *inputStyle = new QComboBox;
  inputStyle->addItems({QStringLiteral("text"), QStringLiteral("time")});
  m_queryTable->setCellWidget(row, QColInputStyle, inputStyle);
  connectCellWidgetSignals(inputStyle);

  auto *relCombo = new QComboBox;
  relCombo->addItems({QStringLiteral("="), QStringLiteral("like"), QStringLiteral(">="),
                      QStringLiteral("<="), QStringLiteral(">"), QStringLiteral("<")});
  m_queryTable->setCellWidget(row, QColRelation, relCombo);
  connectCellWidgetSignals(relCombo);

  emit configChanged();
}

void JsonVueEditor::onRemoveQueryField() {
  int row = m_queryTable->currentRow();
  if (row < 0) return;
  m_queryTable->removeRow(row);
  emit configChanged();
}

QStringList JsonVueEditor::columnDataNames() const {
  QStringList names;
  for (int row = 0; row < m_columnTable->rowCount(); ++row) {
    auto *item = m_columnTable->item(row, ColDataName);
    if (item) {
      QString name = item->text().trimmed();
      if (!name.isEmpty()) names.append(name);
    }
  }
  return names;
}

void JsonVueEditor::refreshQueryFieldDataNames() {
  QStringList names = columnDataNames();
  for (int row = 0; row < m_queryTable->rowCount(); ++row) {
    auto *combo = qobject_cast<QComboBox *>(m_queryTable->cellWidget(row, QColDataName));
    if (combo) {
      QString cur = combo->currentText();
      combo->clear();
      combo->addItems(names);
      int idx = combo->findText(cur);
      if (idx >= 0) combo->setCurrentIndex(idx);
    }
  }
}
