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
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "combobox_config_dialog.h"
#include "src/util/common/http_client.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/component/aui_button.h"
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
      {QStringLiteral("数据列名"), QStringLiteral("查询显示"), QStringLiteral("查询列名"),
       QStringLiteral("查询样式"), QStringLiteral("开关可编辑"), QStringLiteral("编辑显示"),
       QStringLiteral("编辑列名"), QStringLiteral("编辑样式"), QStringLiteral("编辑可编辑"),
       QStringLiteral("配置")});
  m_columnTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  m_columnTable->horizontalHeader()->setSectionResizeMode(ColConfig, QHeaderView::Stretch);
  m_columnTable->setColumnWidth(ColDataName, 120);
  m_columnTable->setColumnWidth(ColQueryVisible, 60);
  m_columnTable->setColumnWidth(ColQueryName, 100);
  m_columnTable->setColumnWidth(ColQueryStyle, 80);
  m_columnTable->setColumnWidth(ColSwitchEditable, 80);
  m_columnTable->setColumnWidth(ColEditVisible, 60);
  m_columnTable->setColumnWidth(ColEditName, 100);
  m_columnTable->setColumnWidth(ColEditStyle, 80);
  m_columnTable->setColumnWidth(ColEditEditable, 80);
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
                                           QStringLiteral("输入框样式"), QStringLiteral("查询关系"),
                                           QStringLiteral("配置")});
  m_queryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  m_queryTable->horizontalHeader()->setSectionResizeMode(QColConfig, QHeaderView::Stretch);
  m_queryTable->setColumnWidth(QColDisplayName, 120);
  m_queryTable->setColumnWidth(QColDataName, 120);
  m_queryTable->setColumnWidth(QColInputStyle, 100);
  m_queryTable->setColumnWidth(QColRelation, 80);
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
          // ── QLineEdit 输入框样式 ──
          "QLineEdit { padding: 1px 2px; font-size: %1px; }"
          // ── QComboBox 下拉框样式（去掉内边距，给文字最大空间）──
          "QComboBox { padding: 0px; font-size: %1px; }"
          "QComboBox QAbstractItemView::item { padding: 0px 2px; }"
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
//  辅助函数：配置属性存储/读取/摘要
// ════════════════════════════════════════════════════════════

namespace {

/// 自绘 QComboBox：无边框，只有文字 + 箭头，文字区域不受原生样式限制
class NoBorderCombo : public QComboBox {
public:
  explicit NoBorderCombo(QWidget *parent = nullptr) : QComboBox(parent) {}

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 背景
    QColor bg = hasFocus() ? palette().color(QPalette::Highlight) : palette().color(QPalette::Base);
    p.fillRect(rect(), bg);

    // 文字
    QRect textRect = rect().adjusted(2, 0, -18, 0);
    QColor textColor =
        hasFocus() ? palette().color(QPalette::HighlightedText) : palette().color(QPalette::Text);
    p.setPen(textColor);
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, currentText());

    // 向下箭头
    QRect arrowRect(rect().right() - 18, 0, 18, rect().height());
    QStyleOption arrowOpt;
    arrowOpt.rect = arrowRect.adjusted(3, 0, -3, 0);
    arrowOpt.palette = palette();
    arrowOpt.state = QStyle::State_Enabled;
    p.setPen(textColor);
    style()->drawPrimitive(QStyle::PE_IndicatorArrowDown, &arrowOpt, &p, this);
  }
};

/// 创建用于表格单元格的 QComboBox（自绘，无边框，文字区域最大）
QComboBox *newTableCombo() { return new NoBorderCombo; }

/// 将 ColumnConfig 的所有配置存储到 QPushButton 的动态属性中
void storeColumnConfig(QPushButton *btn, const ColumnConfig &col) {
  btn->setProperty("selectUrl", col.selectUrl);
  btn->setProperty("selectValueField", col.selectValueField);
  btn->setProperty("selectLabelField", col.selectLabelField);
  btn->setProperty("placeholder", col.placeholder);
  btn->setProperty("maxlength", col.maxlength);
  btn->setProperty("minValue", col.minValue);
  btn->setProperty("maxValue", col.maxValue);
  btn->setProperty("precision", col.precision);
  btn->setProperty("dateFormat", col.dateFormat);
  btn->setProperty("textareaRows", col.textareaRows);
  btn->setProperty("required", col.required);
  btn->setProperty("columnWidth", col.columnWidth);
  btn->setProperty("columnFixed", col.columnFixed);
  btn->setProperty("formatter", col.formatter);
  btn->setProperty("formSpan", col.formSpan);
}

/// 从 QPushButton 的动态属性读取 ColumnConfig 的所有配置
void readColumnConfig(QPushButton *btn, ColumnConfig &col) {
  col.selectUrl = btn->property("selectUrl").toString();
  col.selectValueField = btn->property("selectValueField").toString();
  col.selectLabelField = btn->property("selectLabelField").toString();
  col.placeholder = btn->property("placeholder").toString();
  col.maxlength = btn->property("maxlength").toInt();
  col.minValue = btn->property("minValue").toDouble();
  col.maxValue = btn->property("maxValue").toDouble();
  col.precision = btn->property("precision").isValid() ? btn->property("precision").toInt() : 2;
  col.dateFormat = btn->property("dateFormat").toString();
  col.textareaRows =
      btn->property("textareaRows").isValid() ? btn->property("textareaRows").toInt() : 3;
  col.required = btn->property("required").toBool();
  col.columnWidth = btn->property("columnWidth").toInt();
  col.columnFixed = btn->property("columnFixed").toString();
  col.formatter = btn->property("formatter").toString();
  col.formSpan = btn->property("formSpan").isValid() ? btn->property("formSpan").toInt() : 24;
}

/// 生成列配置摘要文本
QString columnConfigSummary(const ColumnConfig &col) {
  QStringList parts;
  switch (col.editStyle) {
    case EditStyle::Text:
      if (col.required) parts << QStringLiteral("必填");
      if (col.maxlength > 0) parts << QStringLiteral("max:%1").arg(col.maxlength);
      if (!col.placeholder.isEmpty()) parts << col.placeholder;
      break;
    case EditStyle::Int:
      if (col.required) parts << QStringLiteral("必填");
      if (col.minValue != 0 || col.maxValue != 0) {
        parts << QStringLiteral("%1~%2").arg(col.minValue).arg(col.maxValue);
      }
      break;
    case EditStyle::Float:
      if (col.required) parts << QStringLiteral("必填");
      parts << QStringLiteral("%1位小数").arg(col.precision);
      if (col.minValue != 0 || col.maxValue != 0) {
        parts << QStringLiteral("%1~%2").arg(col.minValue).arg(col.maxValue);
      }
      break;
    case EditStyle::Date: {
      if (col.required) parts << QStringLiteral("必填");
      QString fmt = col.dateFormat;
      if (fmt == QStringLiteral("datetime"))
        parts << QStringLiteral("年月日时分秒");
      else if (fmt == QStringLiteral("date"))
        parts << QStringLiteral("年月日");
      else if (fmt == QStringLiteral("month"))
        parts << QStringLiteral("年月");
      else if (fmt == QStringLiteral("year"))
        parts << QStringLiteral("年");
      else if (fmt == QStringLiteral("daterange"))
        parts << QStringLiteral("日期范围");
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
      if (col.required) parts << QStringLiteral("必填");
      parts << QStringLiteral("%1行").arg(col.textareaRows);
      if (!col.placeholder.isEmpty()) parts << col.placeholder;
      break;
  }
  // 通用配置
  if (col.columnWidth > 0) parts << QStringLiteral("宽%1").arg(col.columnWidth);
  if (!col.columnFixed.isEmpty()) parts << QStringLiteral("固定%1").arg(col.columnFixed);
  if (!col.formatter.isEmpty()) parts << QStringLiteral("格式:%1").arg(col.formatter);
  if (col.formSpan != 24) parts << QStringLiteral("span:%1").arg(col.formSpan);

  if (parts.isEmpty()) return QStringLiteral("⚙");
  return QStringLiteral("⚙ ") + parts.join(QStringLiteral(", "));
}

/// 将 QueryFieldConfig 的所有配置存储到 QPushButton 的动态属性中
void storeQueryConfig(QPushButton *btn, const QueryFieldConfig &q) {
  btn->setProperty("selectUrl", q.selectUrl);
  btn->setProperty("selectValueField", q.selectValueField);
  btn->setProperty("selectLabelField", q.selectLabelField);
  btn->setProperty("placeholder", q.placeholder);
  btn->setProperty("dateFormat", q.dateFormat);
}

/// 从 QPushButton 的动态属性读取 QueryFieldConfig 的所有配置
void readQueryConfig(QPushButton *btn, QueryFieldConfig &q) {
  q.selectUrl = btn->property("selectUrl").toString();
  q.selectValueField = btn->property("selectValueField").toString();
  q.selectLabelField = btn->property("selectLabelField").toString();
  q.placeholder = btn->property("placeholder").toString();
  q.dateFormat = btn->property("dateFormat").toString();
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
      if (fmt == QStringLiteral("datetime"))
        parts << QStringLiteral("年月日时分秒");
      else if (fmt == QStringLiteral("date"))
        parts << QStringLiteral("年月日");
      else if (fmt == QStringLiteral("month"))
        parts << QStringLiteral("年月");
      else if (fmt == QStringLiteral("year"))
        parts << QStringLiteral("年");
      else if (fmt == QStringLiteral("daterange"))
        parts << QStringLiteral("日期范围");
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

    auto *qVis = new QCheckBox;
    qVis->setChecked(col.queryVisible);
    m_columnTable->setCellWidget(row, ColQueryVisible, qVis);
    connectCellWidgetSignals(qVis);

    m_columnTable->setItem(row, ColQueryName, new QTableWidgetItem(col.queryName));

    auto *qStyle = newTableCombo();
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

    auto *eStyle = newTableCombo();
    eStyle->addItems({QStringLiteral("text"), QStringLiteral("int"), QStringLiteral("float"),
                      QStringLiteral("date"), QStringLiteral("select"),
                      QStringLiteral("textarea")});
    eStyle->setCurrentText(editStyleToString(col.editStyle));
    m_columnTable->setCellWidget(row, ColEditStyle, eStyle);
    connectCellWidgetSignals(eStyle);

    auto *eEdit = new QCheckBox;
    eEdit->setChecked(col.editEditable);
    m_columnTable->setCellWidget(row, ColEditEditable, eEdit);
    connectCellWidgetSignals(eEdit);

    // 配置按钮（⚙ + 摘要文本）
    auto *configBtn = new QPushButton(columnConfigSummary(col), this);
    storeColumnConfig(configBtn, col);
    m_columnTable->setCellWidget(row, ColConfig, configBtn);
    connect(configBtn, &QPushButton::clicked, this, [this, configBtn]() {
      for (int r = 0; r < m_columnTable->rowCount(); ++r) {
        if (m_columnTable->cellWidget(r, ColConfig) == configBtn) {
          m_columnTable->selectRow(r);
          onConfigureCombobox();
          break;
        }
      }
    });

    // 查询样式与开关可编辑联动
    setupQueryStyleLinkage(row);
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
    inputStyle->addItems(
        {QStringLiteral("text"), QStringLiteral("date"), QStringLiteral("select")});
    inputStyle->setCurrentText(queryInputStyleToString(q.inputStyle));
    m_queryTable->setCellWidget(row, QColInputStyle, inputStyle);
    connectCellWidgetSignals(inputStyle);

    auto *relCombo = newTableCombo();
    relCombo->addItems({QStringLiteral("="), QStringLiteral("like"), QStringLiteral(">="),
                        QStringLiteral("<="), QStringLiteral(">"), QStringLiteral("<")});
    relCombo->setCurrentText(queryRelationToString(q.relation));
    m_queryTable->setCellWidget(row, QColRelation, relCombo);
    connectCellWidgetSignals(relCombo);

    // 配置按钮（⚙ + 摘要文本）
    auto *qConfigBtn = new QPushButton(queryConfigSummary(q), this);
    storeQueryConfig(qConfigBtn, q);
    m_queryTable->setCellWidget(row, QColConfig, qConfigBtn);
    connect(qConfigBtn, &QPushButton::clicked, this, [this, qConfigBtn]() {
      for (int r = 0; r < m_queryTable->rowCount(); ++r) {
        if (m_queryTable->cellWidget(r, QColConfig) == qConfigBtn) {
          m_queryTable->selectRow(r);
          onConfigureQuerySelect();
          break;
        }
      }
    });
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
    // 读取所有配置
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
    if (relCombo) q.relation = stringToQueryRelation(relCombo->currentText());
    // 读取所有配置
    auto *qConfigBtn = qobject_cast<QPushButton *>(m_queryTable->cellWidget(row, QColConfig));
    if (qConfigBtn) {
      readQueryConfig(qConfigBtn, q);
    }

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
      // 保留所有配置
      auto *cfgBtn = qobject_cast<QPushButton *>(m_columnTable->cellWidget(row, ColConfig));
      if (cfgBtn) {
        readColumnConfig(cfgBtn, c);
      }
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
      col.selectUrl = old.selectUrl;
      col.selectValueField = old.selectValueField;
      col.selectLabelField = old.selectLabelField;
      col.placeholder = old.placeholder;
      col.maxlength = old.maxlength;
      col.minValue = old.minValue;
      col.maxValue = old.maxValue;
      col.precision = old.precision;
      col.dateFormat = old.dateFormat;
      col.textareaRows = old.textareaRows;
      col.required = old.required;
      col.columnWidth = old.columnWidth;
      col.columnFixed = old.columnFixed;
      col.formatter = old.formatter;
      col.formSpan = old.formSpan;
    }

    int row = m_columnTable->rowCount();
    m_columnTable->insertRow(row);

    m_columnTable->setItem(row, ColDataName, new QTableWidgetItem(col.dataName));

    auto *qVis = new QCheckBox;
    qVis->setChecked(col.queryVisible);
    m_columnTable->setCellWidget(row, ColQueryVisible, qVis);
    connectCellWidgetSignals(qVis);

    m_columnTable->setItem(row, ColQueryName, new QTableWidgetItem(col.queryName));

    auto *qStyle = newTableCombo();
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

    auto *eStyle = newTableCombo();
    eStyle->addItems({QStringLiteral("text"), QStringLiteral("int"), QStringLiteral("float"),
                      QStringLiteral("date"), QStringLiteral("select"),
                      QStringLiteral("textarea")});
    eStyle->setCurrentText(editStyleToString(col.editStyle));
    m_columnTable->setCellWidget(row, ColEditStyle, eStyle);
    connectCellWidgetSignals(eStyle);

    auto *eEdit = new QCheckBox;
    eEdit->setChecked(col.editEditable);
    m_columnTable->setCellWidget(row, ColEditEditable, eEdit);
    connectCellWidgetSignals(eEdit);

    // 配置按钮（⚙ + 摘要文本）
    auto *configBtn = new QPushButton(columnConfigSummary(col), this);
    storeColumnConfig(configBtn, col);
    m_columnTable->setCellWidget(row, ColConfig, configBtn);
    connect(configBtn, &QPushButton::clicked, this, [this, configBtn]() {
      for (int r = 0; r < m_columnTable->rowCount(); ++r) {
        if (m_columnTable->cellWidget(r, ColConfig) == configBtn) {
          m_columnTable->selectRow(r);
          onConfigureCombobox();
          break;
        }
      }
    });

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

  auto *qStyle = newTableCombo();
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

  auto *eStyle = newTableCombo();
  eStyle->addItems({QStringLiteral("text"), QStringLiteral("int"), QStringLiteral("float"),
                    QStringLiteral("date"), QStringLiteral("select"), QStringLiteral("textarea")});
  m_columnTable->setCellWidget(row, ColEditStyle, eStyle);
  connectCellWidgetSignals(eStyle);

  auto *eEdit = new QCheckBox;
  eEdit->setChecked(true);
  m_columnTable->setCellWidget(row, ColEditEditable, eEdit);
  connectCellWidgetSignals(eEdit);

  // 配置按钮（⚙ + 摘要文本）
  ColumnConfig emptyCol;
  auto *configBtn = new QPushButton(columnConfigSummary(emptyCol), this);
  storeColumnConfig(configBtn, emptyCol);
  m_columnTable->setCellWidget(row, ColConfig, configBtn);
  connect(configBtn, &QPushButton::clicked, this, [this, configBtn]() {
    for (int r = 0; r < m_columnTable->rowCount(); ++r) {
      if (m_columnTable->cellWidget(r, ColConfig) == configBtn) {
        m_columnTable->selectRow(r);
        onConfigureCombobox();
        break;
      }
    }
  });

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

  auto *dataCombo = newTableCombo();
  dataCombo->addItems(columnDataNames());
  m_queryTable->setCellWidget(row, QColDataName, dataCombo);
  connectCellWidgetSignals(dataCombo);

  auto *inputStyle = newTableCombo();
  inputStyle->addItems({QStringLiteral("text"), QStringLiteral("date"), QStringLiteral("select")});
  m_queryTable->setCellWidget(row, QColInputStyle, inputStyle);
  connectCellWidgetSignals(inputStyle);

  auto *relCombo = newTableCombo();
  relCombo->addItems({QStringLiteral("="), QStringLiteral("like"), QStringLiteral(">="),
                      QStringLiteral("<="), QStringLiteral(">"), QStringLiteral("<")});
  m_queryTable->setCellWidget(row, QColRelation, relCombo);
  connectCellWidgetSignals(relCombo);

  // 配置按钮（⚙ + 摘要文本）
  QueryFieldConfig emptyQ;
  auto *qConfigBtn = new QPushButton(queryConfigSummary(emptyQ), this);
  storeQueryConfig(qConfigBtn, emptyQ);
  m_queryTable->setCellWidget(row, QColConfig, qConfigBtn);
  connect(qConfigBtn, &QPushButton::clicked, this, [this, qConfigBtn]() {
    for (int r = 0; r < m_queryTable->rowCount(); ++r) {
      if (m_queryTable->cellWidget(r, QColConfig) == qConfigBtn) {
        m_queryTable->selectRow(r);
        onConfigureQuerySelect();
        break;
      }
    }
  });

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

// ════════════════════════════════════════════════════════════
//  下拉框配置对话框
// ════════════════════════════════════════════════════════════

void JsonVueEditor::onConfigureCombobox() {
  int row = m_columnTable->currentRow();
  if (row < 0) return;

  auto *configBtn = qobject_cast<QPushButton *>(m_columnTable->cellWidget(row, ColConfig));
  if (!configBtn) return;

  // 读取当前编辑样式
  auto *eStyle = qobject_cast<QComboBox *>(m_columnTable->cellWidget(row, ColEditStyle));
  if (!eStyle) return;
  EditStyle style = stringToEditStyle(eStyle->currentText());

  // 读取当前配置到临时 ColumnConfig
  ColumnConfig col;
  readColumnConfig(configBtn, col);
  col.editStyle = style;

  if (style == EditStyle::Select) {
    // select 样式使用 ComboboxConfigDialog（含 HTTP 测试）
    ComboboxConfigDialog dialog(this);
    dialog.setConfig(col.selectUrl, col.selectValueField, col.selectLabelField);
    dialog.setHttpConfig(m_baseUrl, m_authHeader, m_postData);
    if (dialog.exec() == QDialog::Accepted) {
      col.selectUrl = dialog.url();
      col.selectValueField = dialog.valueField();
      col.selectLabelField = dialog.labelField();
      storeColumnConfig(configBtn, col);
      configBtn->setText(columnConfigSummary(col));
      emit configChanged();
    }
  } else {
    // 其他样式使用 StyleConfigDialog
    StyleConfigDialog dialog(style, this);
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
    if (dialog.exec() == QDialog::Accepted) {
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
      storeColumnConfig(configBtn, col);
      configBtn->setText(columnConfigSummary(col));
      emit configChanged();
    }
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
    // text/date 样式使用 StyleConfigDialog
    StyleConfigDialog dialog(style, this);
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
