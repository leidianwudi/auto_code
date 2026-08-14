/**
 * @file json_vue_editor.cpp
 * @brief .jsonvue 可视化编辑器面板实现
 */

#include "json_vue_editor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

#include "button_config_dialog.h"
#include "combobox_config_dialog.h"
#include "src/util/common/code_constants.h"
#include "src/util/common/http_client.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_message_box.h"
#include "src/util/ui/component/aui_style.h"
#include "style_config_dialog.h"

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
//  辅助函数：配置属性存储/读取/摘要
// ════════════════════════════════════════════════════════════

namespace {

/// 接口鉴权/连接配置文件（baseUrl、authHeader、postData），从 .jsonvue 所在目录向上查找
const QString kApiAuthDataAcFile = QStringLiteral("api_auth_data.ac");

/// 自绘 QComboBox：无边框，只有文字 + 箭头，文字区域不受原生样式限制
class NoBorderCombo : public QComboBox {
public:
  explicit NoBorderCombo(QWidget *parent = nullptr) : QComboBox(parent) {}

protected:
  // 强制弹出列表始终向下展开：若控件下方空间不足，Qt 默认会往上弹，
  // 视觉上很别扭；这里在弹出后把列表移动到下拉框正下方（左对齐 + 顶边对齐）
  void showPopup() override {
    QComboBox::showPopup();
    if (QWidget *popup = view()->window()) {
      const QPoint pos = mapToGlobal(QPoint(0, height()));
      QTimer::singleShot(0, popup, [popup, pos]() { popup->move(pos); });
    }
  }

  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 背景（直接用主题色，深色主题下也能看清）
    QColor bg = hasFocus() ? AuiStyle::listSelectionBackground() : AuiStyle::panelBackground();
    p.fillRect(rect(), bg);

    // 文字
    QRect textRect = rect().adjusted(2, 0, -18, 0);
    p.setPen(AuiStyle::textColor());
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, currentText());

    // 向下箭头
    QRect arrowRect(rect().right() - 18, 0, 18, rect().height());
    QStyleOption arrowOpt;
    arrowOpt.rect = arrowRect.adjusted(3, 0, -3, 0);
    arrowOpt.palette = palette();
    arrowOpt.state = QStyle::State_Enabled;
    p.setPen(AuiStyle::textColor());
    style()->drawPrimitive(QStyle::PE_IndicatorArrowDown, &arrowOpt, &p, this);
  }
};

/// 创建用于表格单元格的 QComboBox（自绘，无边框，文字区域最大）
QComboBox *newTableCombo() { return new NoBorderCombo; }

/// 将 ColumnConfig 的所有配置存储到 QPushButton 的动态属性中
void storeColumnConfig(QPushButton *btn, const ColumnConfig &col) {
  btn->setProperty(JsonVueKey::kSelectUrl, col.selectUrl);
  btn->setProperty(JsonVueKey::kSelectValueField, col.selectValueField);
  btn->setProperty(JsonVueKey::kSelectLabelField, col.selectLabelField);
  btn->setProperty(JsonVueKey::kPlaceholder, col.placeholder);
  btn->setProperty(JsonVueKey::kMaxlength, col.maxlength);
  btn->setProperty(JsonVueKey::kMinValue, col.minValue);
  btn->setProperty(JsonVueKey::kMaxValue, col.maxValue);
  btn->setProperty(JsonVueKey::kPrecision, col.precision);
  btn->setProperty(JsonVueKey::kDateFormat, col.dateFormat);
  btn->setProperty(JsonVueKey::kTextareaRows, col.textareaRows);
  btn->setProperty(JsonVueKey::kRequired, col.required);
  btn->setProperty(JsonVueKey::kColumnWidth, col.columnWidth);
  btn->setProperty(JsonVueKey::kColumnFixed, col.columnFixed);
  btn->setProperty(JsonVueKey::kFormatter, col.formatter);
  btn->setProperty(JsonVueKey::kFormSpan, col.formSpan);
  btn->setProperty(JsonVueKey::kDisplayType, col.displayType);
  // tagItems 存为 QVariantList（每个元素为 QVariantMap）
  QVariantList tagItemsVar;
  for (const auto &t : col.tagItems) {
    QVariantMap m;
    m[JsonVueKey::kTagValue] = t.value;
    m[JsonVueKey::kTagText] = t.text;
    m[JsonVueKey::kTagColor] = t.color;
    tagItemsVar.append(m);
  }
  btn->setProperty(JsonVueKey::kTagItems, tagItemsVar);
  btn->setProperty(JsonVueKey::kBoolTrueText, col.boolTrueText);
  btn->setProperty(JsonVueKey::kBoolFalseText, col.boolFalseText);
  btn->setProperty(JsonVueKey::kDefaultValue, col.defaultValue);
  btn->setProperty(JsonVueKey::kDefaultSort, col.defaultSort);
  // 编辑样式/编辑可编辑/开关可编辑（从表格列移入对话框后，存到 property 供读取）
  btn->setProperty(JsonVueKey::kEditStyle, editStyleToString(col.editStyle));
  btn->setProperty(JsonVueKey::kEditEditable, col.editEditable);
  btn->setProperty(JsonVueKey::kSwitchEditable, col.switchEditable);
}

/// 从 QPushButton 的动态属性读取 ColumnConfig 的所有配置
void readColumnConfig(QPushButton *btn, ColumnConfig &col) {
  col.selectUrl = btn->property(JsonVueKey::kSelectUrl).toString();
  col.selectValueField = btn->property(JsonVueKey::kSelectValueField).toString();
  col.selectLabelField = btn->property(JsonVueKey::kSelectLabelField).toString();
  col.placeholder = btn->property(JsonVueKey::kPlaceholder).toString();
  col.maxlength = btn->property(JsonVueKey::kMaxlength).toInt();
  col.minValue = btn->property(JsonVueKey::kMinValue).toDouble();
  col.maxValue = btn->property(JsonVueKey::kMaxValue).toDouble();
  col.precision = btn->property(JsonVueKey::kPrecision).isValid()
                      ? btn->property(JsonVueKey::kPrecision).toInt()
                      : 2;
  col.dateFormat = btn->property(JsonVueKey::kDateFormat).toString();
  col.textareaRows = btn->property(JsonVueKey::kTextareaRows).isValid()
                         ? btn->property(JsonVueKey::kTextareaRows).toInt()
                         : 3;
  col.required = btn->property(JsonVueKey::kRequired).toBool();
  col.columnWidth = btn->property(JsonVueKey::kColumnWidth).toInt();
  col.columnFixed = btn->property(JsonVueKey::kColumnFixed).toString();
  col.formatter = btn->property(JsonVueKey::kFormatter).toString();
  col.formSpan = btn->property(JsonVueKey::kFormSpan).isValid()
                     ? btn->property(JsonVueKey::kFormSpan).toInt()
                     : 24;
  col.displayType = btn->property(JsonVueKey::kDisplayType).toString();
  // tagItems 从 QVariantList 读取
  col.tagItems.clear();
  const QVariantList tagItemsVar = btn->property(JsonVueKey::kTagItems).toList();
  for (const auto &v : tagItemsVar) {
    QVariantMap m = v.toMap();
    TagItem t;
    t.value = m.value(JsonVueKey::kTagValue).toString();
    t.text = m.value(JsonVueKey::kTagText).toString();
    t.color = m.value(JsonVueKey::kTagColor).toString();
    col.tagItems.append(t);
  }
  col.boolTrueText = btn->property(JsonVueKey::kBoolTrueText).toString();
  col.boolFalseText = btn->property(JsonVueKey::kBoolFalseText).toString();
  col.defaultValue = btn->property(JsonVueKey::kDefaultValue).toString();
  col.defaultSort = btn->property(JsonVueKey::kDefaultSort).toString();
  // 编辑样式/编辑可编辑/开关可编辑
  col.editStyle = stringToEditStyle(btn->property(JsonVueKey::kEditStyle).toString());
  col.editEditable = btn->property(JsonVueKey::kEditEditable).isValid()
                         ? btn->property(JsonVueKey::kEditEditable).toBool()
                         : true;
  col.switchEditable = btn->property(JsonVueKey::kSwitchEditable).toBool();
}

/// 生成列配置摘要文本
QString columnConfigSummary(const ColumnConfig &col) {
  QStringList parts;
  // 关键配置：显示样式、编辑样式始终显示，便于一眼看出列的渲染/编辑方式
  // displayType 为空字符串时表示"纯文本(text)"
  parts << QStringLiteral("显示:%1").arg(
      col.displayType.isEmpty() ? QString::fromLatin1(JsonVueStyle::kText) : col.displayType);
  parts << QStringLiteral("编辑:%1").arg(editStyleToString(col.editStyle));
  // 判断是否为开关样式：(displayType == boolean || tag) && switchEditable
  bool isSwitch =
      (col.displayType == JsonVueStyle::kBoolean || col.displayType == JsonVueStyle::kTag) &&
      col.switchEditable;
  if (isSwitch) {
    parts << QStringLiteral("开关");
    // 编辑样式摘要（开关列也显示编辑样式信息）
    switch (col.editStyle) {
      case EditStyle::Text:
        if (col.required) parts << QString::fromUtf8(CodeConstants::UiText::kRequired);
        if (col.maxlength > 0) parts << QStringLiteral("max:%1").arg(col.maxlength);
        if (!col.placeholder.isEmpty()) parts << col.placeholder;
        break;
      case EditStyle::Int:
        if (col.required) parts << QString::fromUtf8(CodeConstants::UiText::kRequired);
        if (col.minValue != 0 || col.maxValue != 0) {
          parts << QStringLiteral("%1~%2").arg(col.minValue).arg(col.maxValue);
        }
        break;
      case EditStyle::Float:
      case EditStyle::Money:
        if (col.required) parts << QString::fromUtf8(CodeConstants::UiText::kRequired);
        parts << QStringLiteral("%1位小数").arg(col.precision);
        if (col.minValue != 0 || col.maxValue != 0) {
          parts << QStringLiteral("%1~%2").arg(col.minValue).arg(col.maxValue);
        }
        break;
      case EditStyle::Date: {
        if (col.required) parts << QString::fromUtf8(CodeConstants::UiText::kRequired);
        QString fmt = col.dateFormat;
        if (fmt == JsonVueStyle::kDatetime)
          parts << QString::fromUtf8(CodeConstants::UiText::kDatetimeFull);
        else if (fmt == JsonVueStyle::kDate)
          parts << QStringLiteral("年月日");
        else if (fmt == JsonVueStyle::kMonth)
          parts << QString::fromUtf8(CodeConstants::UiText::kYearMonth);
        else if (fmt == JsonVueStyle::kYear)
          parts << QStringLiteral("年");
        else if (fmt == JsonVueStyle::kDaterange)
          parts << QString::fromUtf8(CodeConstants::UiText::kDateRange);
        break;
      }
      case EditStyle::Select:
        if (!col.selectUrl.isEmpty()) {
          if (!col.selectValueField.isEmpty() && !col.selectLabelField.isEmpty()) {
            parts << QStringLiteral("%1→%2").arg(col.selectValueField, col.selectLabelField);
          } else {
            parts << QStringLiteral("已配置");
          }
        }
        break;
      case EditStyle::TextArea:
        if (col.required) parts << QString::fromUtf8(CodeConstants::UiText::kRequired);
        parts << QStringLiteral("%1行").arg(col.textareaRows);
        if (!col.placeholder.isEmpty()) parts << col.placeholder;
        break;
      default:
        break;
    }
  } else {
    // 编辑样式摘要
    switch (col.editStyle) {
      case EditStyle::Text:
        if (col.required) parts << QString::fromUtf8(CodeConstants::UiText::kRequired);
        if (col.maxlength > 0) parts << QStringLiteral("max:%1").arg(col.maxlength);
        if (!col.placeholder.isEmpty()) parts << col.placeholder;
        break;
      case EditStyle::Int:
        if (col.required) parts << QString::fromUtf8(CodeConstants::UiText::kRequired);
        if (col.minValue != 0 || col.maxValue != 0) {
          parts << QStringLiteral("%1~%2").arg(col.minValue).arg(col.maxValue);
        }
        break;
      case EditStyle::Float:
      case EditStyle::Money:
        if (col.required) parts << QString::fromUtf8(CodeConstants::UiText::kRequired);
        parts << QStringLiteral("%1位小数").arg(col.precision);
        if (col.minValue != 0 || col.maxValue != 0) {
          parts << QStringLiteral("%1~%2").arg(col.minValue).arg(col.maxValue);
        }
        break;
      case EditStyle::Date: {
        if (col.required) parts << QString::fromUtf8(CodeConstants::UiText::kRequired);
        QString fmt = col.dateFormat;
        if (fmt == JsonVueStyle::kDatetime)
          parts << QString::fromUtf8(CodeConstants::UiText::kDatetimeFull);
        else if (fmt == JsonVueStyle::kDate)
          parts << QStringLiteral("年月日");
        else if (fmt == JsonVueStyle::kMonth)
          parts << QString::fromUtf8(CodeConstants::UiText::kYearMonth);
        else if (fmt == JsonVueStyle::kYear)
          parts << QStringLiteral("年");
        else if (fmt == JsonVueStyle::kDaterange)
          parts << QString::fromUtf8(CodeConstants::UiText::kDateRange);
        break;
      }
      case EditStyle::Select:
        if (!col.selectUrl.isEmpty()) {
          if (!col.selectValueField.isEmpty() && !col.selectLabelField.isEmpty()) {
            parts << QStringLiteral("%1→%2").arg(col.selectValueField, col.selectLabelField);
          } else {
            parts << QStringLiteral("已配置");
          }
        }
        break;
      case EditStyle::TextArea:
        if (col.required) parts << QString::fromUtf8(CodeConstants::UiText::kRequired);
        parts << QStringLiteral("%1行").arg(col.textareaRows);
        if (!col.placeholder.isEmpty()) parts << col.placeholder;
        break;
      default:
        break;
    }
  }
  // 通用配置
  if (col.columnWidth > 0) parts << QStringLiteral("宽%1").arg(col.columnWidth);
  if (!col.columnFixed.isEmpty()) parts << QStringLiteral("固定%1").arg(col.columnFixed);
  if (!col.formatter.isEmpty()) parts << QStringLiteral("格式:%1").arg(col.formatter);
  if (col.formSpan != 24) parts << QStringLiteral("span:%1").arg(col.formSpan);
  if (!col.defaultValue.isEmpty()) parts << QStringLiteral("默认:%1").arg(col.defaultValue);
  if (!col.defaultSort.isEmpty()) parts << QStringLiteral("排序:%1").arg(col.defaultSort);

  if (parts.isEmpty()) return QStringLiteral("⚙");
  return QStringLiteral("⚙ ") + parts.join(QStringLiteral(", "));
}

/// 将 QueryFieldConfig 的所有配置存储到 QPushButton 的动态属性中
void storeQueryConfig(QPushButton *btn, const QueryFieldConfig &q) {
  btn->setProperty(JsonVueKey::kSelectUrl, q.selectUrl);
  btn->setProperty(JsonVueKey::kSelectValueField, q.selectValueField);
  btn->setProperty(JsonVueKey::kSelectLabelField, q.selectLabelField);
  btn->setProperty(JsonVueKey::kPlaceholder, q.placeholder);
  btn->setProperty(JsonVueKey::kDateFormat, q.dateFormat);
}

/// 从 QPushButton 的动态属性读取 QueryFieldConfig 的所有配置
void readQueryConfig(QPushButton *btn, QueryFieldConfig &q) {
  q.selectUrl = btn->property(JsonVueKey::kSelectUrl).toString();
  q.selectValueField = btn->property(JsonVueKey::kSelectValueField).toString();
  q.selectLabelField = btn->property(JsonVueKey::kSelectLabelField).toString();
  q.placeholder = btn->property(JsonVueKey::kPlaceholder).toString();
  q.dateFormat = btn->property(JsonVueKey::kDateFormat).toString();
}

/// 生成查询字段配置摘要文本
QString queryConfigSummary(const QueryFieldConfig &q) {
  QStringList parts;
  switch (q.inputStyle) {
    case QueryInputStyle::Text:
      if (!q.placeholder.isEmpty()) parts << q.placeholder;
      break;
    case QueryInputStyle::Date: {
      QString fmt = q.dateFormat;
      if (fmt == JsonVueStyle::kDatetime)
        parts << QString::fromUtf8(CodeConstants::UiText::kDatetimeFull);
      else if (fmt == JsonVueStyle::kDate)
        parts << QStringLiteral("年月日");
      else if (fmt == JsonVueStyle::kMonth)
        parts << QString::fromUtf8(CodeConstants::UiText::kYearMonth);
      else if (fmt == JsonVueStyle::kYear)
        parts << QStringLiteral("年");
      else if (fmt == JsonVueStyle::kDaterange)
        parts << QString::fromUtf8(CodeConstants::UiText::kDateRange);
      break;
    }
    case QueryInputStyle::Select:
      if (!q.selectUrl.isEmpty()) {
        if (!q.selectValueField.isEmpty() && !q.selectLabelField.isEmpty()) {
          parts << QStringLiteral("%1→%2").arg(q.selectValueField, q.selectLabelField);
        } else {
          parts << QStringLiteral("已配置");
        }
      }
      break;
  }
  if (parts.isEmpty()) return QStringLiteral("⚙");
  return QStringLiteral("⚙ ") + parts.join(QStringLiteral(", "));
}

/// 生成操作按钮配置摘要文本（按行为类型显示关键配置）
QString buttonConfigSummary(const ButtonConfig &btn) {
  QStringList parts;
  switch (btn.actionType) {
    case ButtonActionType::Ajax:
      if (!btn.apiName.isEmpty()) parts << btn.apiName;
      break;
    case ButtonActionType::Confirm:
      if (!btn.apiName.isEmpty()) parts << btn.apiName;
      if (!btn.confirmText.isEmpty()) parts << QStringLiteral("确认:%1").arg(btn.confirmText);
      break;
    case ButtonActionType::Dialog:
      if (!btn.dialogTitle.isEmpty()) parts << btn.dialogTitle;
      parts << QStringLiteral("字段数:%1").arg(btn.dialogFields.size());
      break;
    case ButtonActionType::Link:
      if (!btn.linkPath.isEmpty()) parts << btn.linkPath;
      break;
  }
  // 按钮样式（primary/success/danger/warning 等）
  if (!btn.buttonType.isEmpty()) parts << QStringLiteral("样式:%1").arg(btn.buttonType);
  if (parts.isEmpty()) return QStringLiteral("⚙");
  return QStringLiteral("⚙ ") + parts.join(QStringLiteral(", "));
}

}  // namespace

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
//  HTTP 生成
// ════════════════════════════════════════════════════════════

QString JsonVueEditor::findNearestApiAuthDataAc(const QString &jsonvueFilePath) {
  if (jsonvueFilePath.isEmpty()) return {};
  // 从 .jsonvue 文件所在目录开始，逐级向上查找 api_auth_data.ac
  QDir dir = QFileInfo(jsonvueFilePath).absoluteDir();
  while (true) {
    QFileInfo candidate(dir.absoluteFilePath(kApiAuthDataAcFile));
    if (candidate.isFile()) {
      return candidate.absoluteFilePath();
    }
    if (!dir.cdUp()) break;  // 已到根目录
  }
  return {};
}

void JsonVueEditor::loadHttpConfigFromAcFile(const QString &acFilePath) {
  if (acFilePath.isEmpty()) return;
  m_acConfigFilePath = acFilePath;  // 记住路径，供点击"生成"时重读
  QFile f(acFilePath);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  QString content = QString::fromUtf8(f.readAll());
  f.close();

  // 用正则提取 AC 脚本中的常量定义
  // 匹配: let varName: String = "value"; 或 public varName: String = "value";
  // （支持转义引号 \"）
  static const QRegularExpression rx(
      QStringLiteral(R"rx((?:let|public)\s+(\w+)\s*:\s*String\s*=\s*"((?:[^"\\]|\\.)*)")rx"));
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
  // 每次点击"生成"时重新读取 api_auth_data.ac 的 HTTP 配置（baseUrl/authHeader/postData），
  // 保证 ac 文件被修改后无需重启程序即可生效
  if (!m_acConfigFilePath.isEmpty()) {
    loadHttpConfigFromAcFile(m_acConfigFilePath);
  }

  QString url = m_dataUrlEdit->text().trimmed();
  if (url.isEmpty()) {
    AuiMessageBox::show(this, QStringLiteral("提示"), QStringLiteral("请输入生成数据URL"));
    return;
  }

  // 拼接 baseUrl
  QString fullUrl = url;
  if (m_baseUrl.isEmpty()) {
    // baseUrl 为空时直接使用 url（必须为完整 URL）
    if (!url.startsWith(QStringLiteral("http://")) && !url.startsWith(QStringLiteral("https://"))) {
      AuiMessageBox::show(this, QStringLiteral("提示"),
                          QStringLiteral("baseUrl 为空时，URL 必须以 http:// 或 https:// 开头"));
      return;
    }
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
  if (methodStr == JsonVueHttp::kPost)
    method = HttpClient::Post;
  else if (methodStr == JsonVueHttp::kPut)
    method = HttpClient::Put;
  else if (methodStr == JsonVueHttp::kDelete)
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

  m_pendingUrl = fullUrl;
  HttpClient::instance().request(method, fullUrl, bodyObj, headers, this);
}

void JsonVueEditor::onHttpFinished(const QString &url, const QJsonDocument &doc) {
  if (url != m_pendingUrl) return;  // 不是本编辑器发起的请求，忽略
  m_pendingUrl.clear();
  m_generateBtn->setEnabled(true);
  m_generateBtn->setText(QStringLiteral("生成"));

  int added = populateColumnsFromHttp(doc);
  if (added < 0) return;  // 解析/校验失败，错误提示已在 populateColumnsFromHttp 中弹出

  // 生成成功提示
  QString msg;
  if (added > 0) {
    msg = QStringLiteral("生成成功，已从接口获取 %1 个新列并追加到列配置。").arg(added);
  } else {
    msg = QStringLiteral("生成成功，但网络返回的列已全部存在于列配置中，未新增任何列。");
  }
  AuiMessageBox::show(this, QStringLiteral("生成成功"), msg);
}

void JsonVueEditor::onHttpError(const QString &url, const QString &errorMsg) {
  if (url != m_pendingUrl) return;  // 不是本编辑器发起的请求，忽略
  m_pendingUrl.clear();
  m_generateBtn->setEnabled(true);
  m_generateBtn->setText(QStringLiteral("生成"));

  // 详细错误信息，便于用户定位问题
  QString method = m_methodCombo->currentText();
  QString msg =
      QStringLiteral("请求地址：%1\n请求方法：%2\n错误详情：%3\n\n").arg(url, method, errorMsg);
  msg += QStringLiteral("排查建议：\n");
  msg += QStringLiteral("1. 确认后端服务已启动，且端口未被占用；\n");
  msg +=
      QStringLiteral("2. 检查 baseUrl 配置是否正确（可在 %1 中修改）；\n").arg(kApiAuthDataAcFile);
  msg += QStringLiteral("3. 检查 Authorization 令牌是否过期或无效；\n");
  msg += QStringLiteral("4. 可在浏览器中直接访问上述地址，验证接口是否可用。");
  AuiMessageBox::show(this, QStringLiteral("请求失败"), msg);
}

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

int JsonVueEditor::populateColumnsFromHttp(const QJsonDocument &doc) {
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
    AuiMessageBox::show(this, QStringLiteral("提示"), QStringLiteral("返回数据中未找到 list 数组"));
    return -1;
  }

  // 取第一行的列名
  QJsonObject firstRow = listArr.at(0).toObject();
  if (firstRow.isEmpty()) {
    AuiMessageBox::show(this, QStringLiteral("提示"), QStringLiteral("返回数据第一行为空"));
    return -1;
  }

  // 收集当前列配置中已有的 dataName（已有列不允许任何修改）
  QSet<QString> existingNames;
  for (int row = 0; row < m_columnTable->rowCount(); ++row) {
    auto *item = m_columnTable->item(row, ColDataName);
    if (item && !item->text().trimmed().isEmpty()) {
      existingNames.insert(item->text().trimmed());
    }
  }

  // 仅把网络数据中新增（当前列配置里没有）的列追加到表格末尾，已有列保持原顺序不变
  int addedCount = 0;
  m_loading = true;
  for (auto it = firstRow.begin(); it != firstRow.end(); ++it) {
    QString colName = it.key();
    if (existingNames.contains(colName)) continue;  // 已有列跳过，不修改

    ColumnConfig col;
    col.dataName = colName;
    // 默认查询列名和编辑列名等于数据列名
    col.queryName = colName;
    col.editName = colName;

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

    ++addedCount;  // 记录新增列数
  }
  m_loading = false;

  // 刷新查询字段下拉框
  refreshQueryFieldDataNames();

  emit configChanged();
  return addedCount;
}

// ════════════════════════════════════════════════════════════
//  列表操作
// ════════════════════════════════════════════════════════════

void JsonVueEditor::onMoveUp() {
  int row = m_columnTable->currentRow();
  if (row <= 0) return;
  // 简单实现：用 collectConfig + loadConfig 重新加载（避免 cellWidget 复杂性）
  JsonVueConfig cfg = collectConfig();
  if (row - 1 >= 0 && row < cfg.columns.size()) {
    cfg.columns.swapItemsAt(row, row - 1);
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

  m_columnTable->setItem(row, ColTitle, new QTableWidgetItem());

  auto *eVis = new QCheckBox;
  eVis->setChecked(true);
  m_columnTable->setCellWidget(row, ColEditVisible, eVis);
  connectCellWidgetSignals(eVis);

  // 配置按钮（⚙ + 摘要文本，含显示类型/编辑样式/通用配置）
  ColumnConfig emptyCol;
  auto *configBtn = new QPushButton(columnConfigSummary(emptyCol), this);
  storeColumnConfig(configBtn, emptyCol);
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

  emit configChanged();
}

void JsonVueEditor::onRemoveColumn() {
  int row = m_columnTable->currentRow();
  if (row < 0) return;
  if (!AuiMessageBox::confirm(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定要删除当前列配置吗？"))) {
    return;
  }
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

  auto *dataCombo = newTableCombo();
  dataCombo->addItems(columnDataNames());
  m_queryTable->setCellWidget(row, QColDataName, dataCombo);
  connectCellWidgetSignals(dataCombo);

  auto *inputStyle = newTableCombo();
  inputStyle->addItems({QString::fromLatin1(JsonVueStyle::kText),
                        QString::fromLatin1(JsonVueStyle::kDate),
                        QString::fromLatin1(JsonVueStyle::kSelect)});
  m_queryTable->setCellWidget(row, QColInputStyle, inputStyle);
  connectCellWidgetSignals(inputStyle);

  auto *relCombo = newTableCombo();
  relCombo->addItem(QStringLiteral("普通"), QStringLiteral("="));
  relCombo->addItem(QStringLiteral("范围"), QString::fromLatin1(JsonVueStyle::kRange));
  m_queryTable->setCellWidget(row, QColRelation, relCombo);
  connectCellWidgetSignals(relCombo);

  // 配置按钮（⚙ + 摘要文本）
  QueryFieldConfig emptyQ;
  auto *qConfigBtn = new QPushButton(queryConfigSummary(emptyQ), this);
  storeQueryConfig(qConfigBtn, emptyQ);
  m_queryTable->setCellWidget(row, QColConfig, qConfigBtn);
  qConfigBtn->installEventFilter(this);  // 双击打开查询样式配置

  emit configChanged();
}

void JsonVueEditor::onRemoveQueryField() {
  int row = m_queryTable->currentRow();
  if (row < 0) return;
  if (!AuiMessageBox::confirm(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定要删除当前查询字段吗？"))) {
    return;
  }
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

// ════════════════════════════════════════════════════════════
//  下拉框配置对话框
// ════════════════════════════════════════════════════════════

void JsonVueEditor::onConfigureCombobox() {
  int row = m_columnTable->currentRow();
  if (row < 0) return;

  auto *configBtn = qobject_cast<QPushButton *>(m_columnTable->cellWidget(row, ColConfig));
  if (!configBtn) return;

  // 读取当前配置到临时 ColumnConfig（含 editStyle/editEditable/switchEditable/displayType 等）
  ColumnConfig col;
  readColumnConfig(configBtn, col);

  // 列配置样式：使用 ColumnStyleDialog 配置表格列显示、编辑样式、通用配置等
  ColumnStyleDialog dialog(col.editStyle, this);
  dialog.setHttpConfig(m_baseUrl, m_authHeader, m_postData);
  dialog.setEditStyle(col.editStyle);
  dialog.setEditEditable(col.editEditable);
  dialog.setSwitchEditable(col.switchEditable);
  dialog.setPlaceholder(col.placeholder);
  dialog.setMaxlength(col.maxlength);
  dialog.setMinValue(col.minValue);
  dialog.setMaxValue(col.maxValue);
  dialog.setPrecision(col.precision);
  dialog.setDateFormat(col.dateFormat);
  dialog.setTextareaRows(col.textareaRows);
  dialog.setRequired(col.required);
  dialog.setColumnWidth(col.columnWidth);
  dialog.setColumnFixed(col.columnFixed);
  dialog.setFormatter(col.formatter);
  dialog.setFormSpan(col.formSpan);
  dialog.setDisplayType(col.displayType);
  dialog.setTagItems(col.tagItems);
  dialog.setBoolTrueText(col.boolTrueText);
  dialog.setBoolFalseText(col.boolFalseText);
  // 下拉框数据源（Select 编辑样式时在对话框内配置）
  dialog.setSelectUrl(col.selectUrl);
  dialog.setSelectValueField(col.selectValueField);
  dialog.setSelectLabelField(col.selectLabelField);
  dialog.setDefaultValue(col.defaultValue);
  dialog.setDefaultSort(col.defaultSort);
  if (dialog.exec() == QDialog::Accepted) {
    col.editStyle = dialog.editStyle();
    col.editEditable = dialog.editEditable();
    col.switchEditable = dialog.switchEditable();
    col.placeholder = dialog.placeholder();
    col.maxlength = dialog.maxlength();
    col.minValue = dialog.minValue();
    col.maxValue = dialog.maxValue();
    col.precision = dialog.precision();
    col.dateFormat = dialog.dateFormat();
    col.textareaRows = dialog.textareaRows();
    col.required = dialog.required();
    col.columnWidth = dialog.columnWidth();
    col.columnFixed = dialog.columnFixed();
    col.formatter = dialog.formatter();
    col.formSpan = dialog.formSpan();
    col.displayType = dialog.displayType();
    col.tagItems = dialog.tagItems();
    col.boolTrueText = dialog.boolTrueText();
    col.boolFalseText = dialog.boolFalseText();
    // 下拉框数据源（在对话框内已配置完成）
    col.selectUrl = dialog.selectUrl();
    col.selectValueField = dialog.selectValueField();
    col.selectLabelField = dialog.selectLabelField();
    col.defaultValue = dialog.defaultValue();
    col.defaultSort = dialog.defaultSort();
    storeColumnConfig(configBtn, col);
    configBtn->setText(columnConfigSummary(col));
    emit configChanged();
  }
}

void JsonVueEditor::onConfigureQuerySelect() {
  int row = m_queryTable->currentRow();
  if (row < 0) return;

  auto *configBtn = qobject_cast<QPushButton *>(m_queryTable->cellWidget(row, QColConfig));
  if (!configBtn) return;

  // 读取当前查询输入样式
  auto *inputStyle = qobject_cast<QComboBox *>(m_queryTable->cellWidget(row, QColInputStyle));
  if (!inputStyle) return;
  QueryInputStyle style = stringToQueryInputStyle(inputStyle->currentText());

  // 读取当前配置到临时 QueryFieldConfig
  QueryFieldConfig q;
  readQueryConfig(configBtn, q);
  q.inputStyle = style;

  if (style == QueryInputStyle::Select) {
    // select 样式使用 ComboboxConfigDialog
    ComboboxConfigDialog dialog(this);
    dialog.setConfig(q.selectUrl, q.selectValueField, q.selectLabelField);
    dialog.setHttpConfig(m_baseUrl, m_authHeader, m_postData);
    if (dialog.exec() == QDialog::Accepted) {
      q.selectUrl = dialog.url();
      q.selectValueField = dialog.valueField();
      q.selectLabelField = dialog.labelField();
      storeQueryConfig(configBtn, q);
      configBtn->setText(queryConfigSummary(q));
      emit configChanged();
    }
  } else {
    // text/date 样式使用 QueryStyleDialog
    QueryStyleDialog dialog(style, this);
    dialog.setPlaceholder(q.placeholder);
    dialog.setDateFormat(q.dateFormat);
    if (dialog.exec() == QDialog::Accepted) {
      q.placeholder = dialog.placeholder();
      q.dateFormat = dialog.dateFormat();
      storeQueryConfig(configBtn, q);
      configBtn->setText(queryConfigSummary(q));
      emit configChanged();
    }
  }
}

// ════════════════════════════════════════════════════════════
//  操作按钮管理
// ════════════════════════════════════════════════════════════

void JsonVueEditor::onAddButton() {
  ButtonConfigDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted) {
    ButtonConfig btn = dialog.getData();
    if (btn.label.isEmpty()) {
      return;
    }
    // 如果 actionKey 为空，自动生成
    if (btn.actionKey.isEmpty()) {
      btn.actionKey = QStringLiteral("btn%1").arg(m_buttons.size() + 1);
    }
    m_buttons.append(btn);
    // 刷新表格
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

    emit configChanged();
  }
}

void JsonVueEditor::onEditButton(int row) {
  if (row < 0 || row >= m_buttons.size()) {
    return;
  }
  ButtonConfigDialog dialog(m_buttons[row], this);
  if (dialog.exec() == QDialog::Accepted) {
    ButtonConfig btn = dialog.getData();
    if (btn.label.isEmpty()) {
      return;
    }
    m_buttons[row] = btn;
    // 刷新表格行
    m_buttonTable->item(row, BColLabel)->setText(btn.label);
    m_buttonTable->item(row, BColActionKey)->setText(btn.actionKey);
    m_buttonTable->item(row, BColPosition)->setText(buttonPositionToString(btn.position));
    m_buttonTable->item(row, BColActionType)->setText(buttonActionTypeToString(btn.actionType));
    // 刷新配置按钮摘要
    auto *configBtn = qobject_cast<QPushButton *>(m_buttonTable->cellWidget(row, BColConfig));
    if (configBtn) {
      configBtn->setText(buttonConfigSummary(btn));
    }
    emit configChanged();
  }
}

void JsonVueEditor::onRemoveButton() {
  int row = m_buttonTable->currentRow();
  if (row < 0) {
    return;
  }
  if (!AuiMessageBox::confirm(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定要删除当前操作按钮吗？"))) {
    return;
  }
  m_buttonTable->removeRow(row);
  if (row < m_buttons.size()) {
    m_buttons.removeAt(row);
  }
  emit configChanged();
}
