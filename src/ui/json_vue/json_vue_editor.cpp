/**
 * @file json_vue_editor.cpp
 * @brief .jsonvue 可视化编辑器面板实现
 *
 * 按职责拆分到多个实现文件（共用辅助函数见 json_vue_editor_helpers.h）：
 * - 本文件：构造 / UI 构建 / 样式 / 配置加载与收集 / 事件过滤
 * - json_vue_editor_http.cpp：HTTP 生成与接口配置加载
 * - json_vue_editor_columns.cpp：列 / 查询字段列表操作
 * - json_vue_editor_config.cpp：配置对话框与操作按钮管理
 */

#include "json_vue_editor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "json_vue_editor_helpers.h"
#include "src/util/common/code_constants.h"
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
  splitter->addWidget(buildButtonsSection());
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);
  splitter->setStretchFactor(2, 1);
  mainLayout->addWidget(splitter, 1);

  applyStyle();

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
  connect(m_noDetailCheck, &QCheckBox::stateChanged, this, [this]() {
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
  m_methodCombo->addItems(
      {QString::fromLatin1(JsonVueHttp::kGet), QString::fromLatin1(JsonVueHttp::kPost),
       QString::fromLatin1(JsonVueHttp::kPut), QString::fromLatin1(JsonVueHttp::kDelete)});
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

  // 第5行：详情接口 + 不可查看详情复选框
  auto *row5 = new QHBoxLayout;
  row5->addWidget(new QLabel(QStringLiteral("详情接口:")));
  // 详情复用查询接口，这里只是提示
  auto *detailLabel = new QLabel(QStringLiteral("（复用编辑表单，只读模式）"), this);
  row5->addWidget(detailLabel, 1);
  m_noDetailCheck = new QCheckBox(QStringLiteral("不可查看详情"), this);
  row5->addWidget(m_noDetailCheck);
  layout->addLayout(row5);

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
      {QStringLiteral("字段名"), QStringLiteral("标题"), QStringLiteral("列表页显示"),
       QStringLiteral("编辑页显示"), QString::fromUtf8(CodeConstants::UiText::kConfig)});
  m_columnTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  m_columnTable->horizontalHeader()->setSectionResizeMode(ColConfig, QHeaderView::Stretch);
  m_columnTable->setColumnWidth(ColDataName, 120);
  m_columnTable->setColumnWidth(ColTitle, 120);
  m_columnTable->setColumnWidth(ColQueryVisible, 100);
  m_columnTable->setColumnWidth(ColEditVisible, 100);
  m_columnTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_columnTable->setMinimumHeight(60);
  m_columnTable->installEventFilter(this);  // 拦截空格键：仅进入编辑，不输入空格
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
  m_queryTable->setHorizontalHeaderLabels({QStringLiteral("字段名"), QStringLiteral("标签名"),
                                           QStringLiteral("输入框样式"), QStringLiteral("查询关系"),
                                           QString::fromUtf8(CodeConstants::UiText::kConfig)});
  m_queryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  m_queryTable->horizontalHeader()->setSectionResizeMode(QColConfig, QHeaderView::Stretch);
  m_queryTable->setColumnWidth(QColDataName, 120);
  m_queryTable->setColumnWidth(QColDisplayName, 120);
  m_queryTable->setColumnWidth(QColInputStyle, 100);
  m_queryTable->setColumnWidth(QColRelation, 80);
  m_queryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_queryTable->setMinimumHeight(60);
  layout->addWidget(m_queryTable);

  connect(m_addQueryBtn, &QPushButton::clicked, this, &JsonVueEditor::onAddQueryField);
  connect(m_removeQueryBtn, &QPushButton::clicked, this, &JsonVueEditor::onRemoveQueryField);

  return group;
}

QWidget *JsonVueEditor::buildButtonsSection() {
  auto *group = new QGroupBox(QStringLiteral("操作按钮"), this);
  auto *layout = new QVBoxLayout(group);
  layout->setSpacing(2);
  layout->setContentsMargins(2, 2, 2, 2);

  auto *btnRow = new QHBoxLayout;
  m_addButtonBtn = new QPushButton(QStringLiteral("+ 添加按钮"), this);
  m_removeButtonBtn = new QPushButton(QStringLiteral("- 删除按钮"), this);
  btnRow->addWidget(m_addButtonBtn);
  btnRow->addWidget(m_removeButtonBtn);
  btnRow->addStretch();
  layout->addLayout(btnRow);

  m_buttonTable = new QTableWidget(0, BColCount, this);
  m_buttonTable->setHorizontalHeaderLabels({QStringLiteral("按钮文字"), QStringLiteral("动作标识"),
                                            QStringLiteral("位置"), QStringLiteral("行为类型"),
                                            QString::fromUtf8(CodeConstants::UiText::kConfig)});
  m_buttonTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  m_buttonTable->horizontalHeader()->setSectionResizeMode(BColConfig, QHeaderView::Stretch);
  m_buttonTable->setColumnWidth(BColLabel, 120);
  m_buttonTable->setColumnWidth(BColActionKey, 120);
  m_buttonTable->setColumnWidth(BColPosition, 80);
  m_buttonTable->setColumnWidth(BColActionType, 100);
  m_buttonTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_buttonTable->setMinimumHeight(60);
  m_buttonTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  layout->addWidget(m_buttonTable);

  connect(m_addButtonBtn, &QPushButton::clicked, this, &JsonVueEditor::onAddButton);
  connect(m_removeButtonBtn, &QPushButton::clicked, this, &JsonVueEditor::onRemoveButton);

  return group;
}

/**
 * @brief 应用可视化编辑器的全局 QSS 样式表
 *
 * 统一设置所有子控件的字体大小、内边距、边框等视觉属性，
 * 使界面风格紧凑一致，避免各控件默认间距过大导致操作区域变小。
 */
void JsonVueEditor::applyStyle() {
  // 从主题取色，保证深色/浅色下表格行线、表头、标签、分组标题、输入框均清晰可读
  const auto fontPx = QString::number(AuiStyle::dialogFontSize());
  const auto text = AuiStyle::textColor().name();
  const auto border = AuiStyle::borderColor().name();
  const auto bg = AuiStyle::panelBackground().name();
  const auto headerBg = AuiStyle::background().name();
  const auto alt = AuiStyle::listAlternateBackground().name();

  QString qss = QStringLiteral(
                    // ── QGroupBox 分组容器 + 标题 ──
                    "QGroupBox {"
                    "  font-size: %1px; font-weight: bold; color: %2;"
                    "  border: 1px solid %3; border-radius: 4px;"
                    "  margin-top: 10px; padding-top: 5px;"
                    "  background: transparent;"
                    "}"
                    "QGroupBox::title {"
                    "  subcontrol-origin: margin; left: 2px; padding: 0 2px; color: %2;"
                    "}"
                    // ── QLabel 标签 ──
                    "QLabel { font-size: %1px; color: %2; background: transparent; }"
                    // ── QCheckBox 复选框 ──
                    "QCheckBox { font-size: %1px; color: %2; }"
                    // ── QLineEdit 输入框 ──
                    "QLineEdit { padding: 1px 2px; font-size: %1px;"
                    "  background: %4; color: %2; border: 1px solid %3; }"
                    // ── QComboBox 下拉框 ──
                    "QComboBox { padding: 0px; font-size: %1px; color: %2; background: %4; }"
                    "QComboBox QAbstractItemView { background: %4; color: %2;"
                    "  selection-background-color: %5; selection-color: %2; }"
                    // ── QPushButton 按钮 ──
                    "QPushButton { padding: 2px 8px; font-size: %1px; color: %2; }"
                    // ── QTableWidget 表格（背景/文字/行线/交替行色）──
                    "QTableWidget { font-size: %1px; background: %4; color: %2;"
                    "  gridline-color: %3; alternate-background-color: %5; }"
                    "QTableWidget::item { color: %2; }"
                    // ── QHeaderView 表头 ──
                    "QHeaderView::section { padding: 2px; font-size: %1px;"
                    "  background: %6; color: %2; border: none;"
                    "  border-right: 1px solid %3; border-bottom: 1px solid %3; }"
                    // ── 滚动条 ──
                    "QScrollBar:vertical, QScrollBar:horizontal { background: %4; }")
                    .arg(fontPx, text, border, bg, alt, headerBg);
  setStyleSheet(qss);
}

/// 主题/颜色变化后重新应用样式表
void JsonVueEditor::reloadStyle() { applyStyle(); }

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
  m_noDetailCheck->setChecked(config.meta.noDetail);

  // 列配置
  m_columnTable->setRowCount(0);
  for (const auto &col : config.columns) {
    int row = m_columnTable->rowCount();
    m_columnTable->insertRow(row);

    m_columnTable->setItem(row, ColDataName, new QTableWidgetItem(col.dataName));

    // 标题：优先用列表页列标题（queryName），为空则用编辑页标签（editName）
    QString title = col.queryName.isEmpty() ? col.editName : col.queryName;
    m_columnTable->setItem(row, ColTitle, new QTableWidgetItem(title));

    auto *qVis = new QCheckBox;
    qVis->setChecked(col.queryVisible);
    m_columnTable->setCellWidget(row, ColQueryVisible, qVis);
    connectCellWidgetSignals(qVis);

    auto *eVis = new QCheckBox;
    eVis->setChecked(col.editVisible);
    m_columnTable->setCellWidget(row, ColEditVisible, eVis);
    connectCellWidgetSignals(eVis);

    // 配置按钮（⚙ + 摘要文本，含显示类型/编辑样式/通用配置）
    auto *configBtn = new QPushButton(columnConfigSummary(col), this);
    storeColumnConfig(configBtn, col);
    m_columnTable->setCellWidget(row, ColConfig, configBtn);
    configBtn->installEventFilter(this);  // 双击打开样式配置
    // 右键菜单：复制配置 / 粘贴配置
    configBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(configBtn, &QPushButton::customContextMenuRequested, this,
            [this, configBtn](const QPoint &pos) {
              for (int r = 0; r < m_columnTable->rowCount(); ++r) {
                if (m_columnTable->cellWidget(r, ColConfig) == configBtn) {
                  showColumnConfigMenu(r, configBtn->mapToGlobal(pos));
                  break;
                }
              }
            });
  }

  // 查询字段
  m_queryTable->setRowCount(0);
  for (const auto &q : config.queryFields) {
    int row = m_queryTable->rowCount();
    m_queryTable->insertRow(row);

    m_queryTable->setItem(row, QColDisplayName, new QTableWidgetItem(q.displayName));

    auto *dataCombo = newTableCombo();
    dataCombo->addItems(columnDataNames());
    dataCombo->setCurrentText(q.dataName);
    m_queryTable->setCellWidget(row, QColDataName, dataCombo);
    connectCellWidgetSignals(dataCombo);

    auto *inputStyle = newTableCombo();
    inputStyle->addItems({QString::fromLatin1(JsonVueStyle::kText),
                          QString::fromLatin1(JsonVueStyle::kDate),
                          QString::fromLatin1(JsonVueStyle::kSelect)});
    inputStyle->setCurrentText(queryInputStyleToString(q.inputStyle));
    m_queryTable->setCellWidget(row, QColInputStyle, inputStyle);
    connectCellWidgetSignals(inputStyle);

    auto *relCombo = newTableCombo();
    relCombo->addItem(QStringLiteral("普通"), QStringLiteral("="));
    relCombo->addItem(QStringLiteral("范围"), QString::fromLatin1(JsonVueStyle::kRange));
    int relIdx = relCombo->findData(queryRelationToString(q.relation));
    if (relIdx >= 0) relCombo->setCurrentIndex(relIdx);
    m_queryTable->setCellWidget(row, QColRelation, relCombo);
    connectCellWidgetSignals(relCombo);

    // 配置按钮（⚙ + 摘要文本）
    auto *qConfigBtn = new QPushButton(queryConfigSummary(q), this);
    storeQueryConfig(qConfigBtn, q);
    m_queryTable->setCellWidget(row, QColConfig, qConfigBtn);
    qConfigBtn->installEventFilter(this);  // 双击打开查询样式配置
  }

  // 操作按钮
  m_buttons = config.buttons;
  m_buttonTable->setRowCount(0);
  for (const auto &btn : config.buttons) {
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
  cfg.meta.noDetail = m_noDetailCheck->isChecked();

  // 列配置
  for (int row = 0; row < m_columnTable->rowCount(); ++row) {
    ColumnConfig col;
    auto *dnItem = m_columnTable->item(row, ColDataName);
    if (dnItem) col.dataName = dnItem->text().trimmed();
    auto *qVis = qobject_cast<QCheckBox *>(m_columnTable->cellWidget(row, ColQueryVisible));
    if (qVis) col.queryVisible = qVis->isChecked();
    auto *eVis = qobject_cast<QCheckBox *>(m_columnTable->cellWidget(row, ColEditVisible));
    if (eVis) col.editVisible = eVis->isChecked();
    // 标题（合并后的单列）同时写回 queryName 和 editName
    auto *titleItem = m_columnTable->item(row, ColTitle);
    if (titleItem) {
      QString title = titleItem->text().trimmed();
      col.queryName = title;
      col.editName = title;
    }
    // 读取所有配置（含 editStyle/editEditable/switchEditable/displayType 等）
    auto *configBtn = qobject_cast<QPushButton *>(m_columnTable->cellWidget(row, ColConfig));
    if (configBtn) {
      readColumnConfig(configBtn, col);
    }

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
    if (relCombo) {
      QVariant d = relCombo->currentData();
      q.relation = stringToQueryRelation(d.isValid() ? d.toString() : relCombo->currentText());
    }
    // 读取所有配置
    auto *qConfigBtn = qobject_cast<QPushButton *>(m_queryTable->cellWidget(row, QColConfig));
    if (qConfigBtn) {
      readQueryConfig(qConfigBtn, q);
    }

    if (!q.displayName.isEmpty()) cfg.queryFields.append(q);
  }

  // 操作按钮（直接从 m_buttons 返回，表格只用于显示）
  cfg.buttons = m_buttons;

  return cfg;
}

// ════════════════════════════════════════════════════════════
//  事件过滤 / 配置右键菜单
// ════════════════════════════════════════════════════════════

bool JsonVueEditor::eventFilter(QObject *obj, QEvent *ev) {
  // 拦截列配置表格中的空格键：第一次按空格仅进入编辑状态，
  // 不把空格字符输入到编辑框（与 Excel 行为一致）。
  // 编辑器打开时键盘事件由编辑器（QLineEdit）处理，不会经过本过滤器，
  // 因此无需额外判断编辑状态。
  if (obj == m_columnTable && ev->type() == QEvent::KeyPress) {
    auto *keyEv = static_cast<QKeyEvent *>(ev);
    if (keyEv->key() == Qt::Key_Space && !keyEv->isAutoRepeat()) {
      QModelIndex idx = m_columnTable->currentIndex();
      if (idx.isValid() && idx.column() == ColTitle) {
        m_columnTable->edit(idx);  // 仅进入编辑，不传递空格键
        return true;               // 消费事件，阻止空格输入
      }
    }
    return QWidget::eventFilter(obj, ev);
  }

  // 配置按钮双击：列配置按钮打开样式配置，查询配置按钮打开查询样式配置
  if (ev->type() == QEvent::MouseButtonDblClick) {
    auto *btn = qobject_cast<QPushButton *>(obj);
    if (btn) {
      // 列配置表格的配置按钮
      for (int r = 0; r < m_columnTable->rowCount(); ++r) {
        if (m_columnTable->cellWidget(r, ColConfig) == btn) {
          m_columnTable->selectRow(r);
          onConfigureCombobox();
          return true;  // 消费双击事件，避免按钮再触发其它行为
        }
      }
      // 查询设置表格的配置按钮
      for (int r = 0; r < m_queryTable->rowCount(); ++r) {
        if (m_queryTable->cellWidget(r, QColConfig) == btn) {
          m_queryTable->selectRow(r);
          onConfigureQuerySelect();
          return true;
        }
      }
      // 操作按钮表格的配置按钮
      for (int r = 0; r < m_buttonTable->rowCount(); ++r) {
        if (m_buttonTable->cellWidget(r, BColConfig) == btn) {
          m_buttonTable->selectRow(r);
          onEditButton(r);
          return true;
        }
      }
    }
  }
  return QWidget::eventFilter(obj, ev);
}

void JsonVueEditor::showColumnConfigMenu(int row, const QPoint &globalPos) {
  if (row < 0 || row >= m_columnTable->rowCount()) return;

  QMenu menu(this);
  QAction *copyAct = menu.addAction(QStringLiteral("复制配置"));
  QAction *pasteAct = menu.addAction(QStringLiteral("粘贴配置"));
  pasteAct->setEnabled(m_hasCopiedConfig);

  QAction *chosen = menu.exec(globalPos);
  if (chosen == copyAct) {
    // 复制该行的配置
    auto *srcBtn = qobject_cast<QPushButton *>(m_columnTable->cellWidget(row, ColConfig));
    if (srcBtn) {
      readColumnConfig(srcBtn, m_copiedColumnConfig);
      m_hasCopiedConfig = true;
    }
  } else if (chosen == pasteAct) {
    // 粘贴配置到目标行（保留其字段名、标题、可见性，仅覆盖样式配置）
    auto *dstBtn = qobject_cast<QPushButton *>(m_columnTable->cellWidget(row, ColConfig));
    if (dstBtn) {
      storeColumnConfig(dstBtn, m_copiedColumnConfig);
      dstBtn->setText(columnConfigSummary(m_copiedColumnConfig));
      if (!m_loading) emit configChanged();
    }
  }
}
