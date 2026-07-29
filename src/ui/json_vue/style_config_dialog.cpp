/**
 * @file style_config_dialog.cpp
 * @brief 字段样式配置对话框实现
 */

#include "style_config_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "src/util/ui/aui_window.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

StyleConfigDialog::StyleConfigDialog(EditStyle style, QWidget *parent) : QDialog(parent) {
  m_editStyle = style;
  m_isColumnMode = true;
  setupUI();
}

StyleConfigDialog::StyleConfigDialog(QueryInputStyle style, QWidget *parent) : QDialog(parent) {
  m_queryStyle = style;
  m_isColumnMode = false;
  setupUI();
}

// ════════════════════════════════════════════════════════════
//  界面构建
// ════════════════════════════════════════════════════════════

void StyleConfigDialog::setupUI() {
  setWindowTitle(QStringLiteral("样式配置"));
  setMinimumWidth(300);

  // ── 无边框对话框 ──
  AuiWindow::setupFramelessDialog(this);

  // ── 自定义标题栏（无最大化/最小化按钮）──
  TitleBarOptions opts;
  opts.title = QStringLiteral("样式配置");
  opts.showMinButton = false;
  opts.showMaxButton = false;
  opts.closeRejectsDialog = true;
  auto tb = AuiWindow::createTitleBar(this, opts);

  // ── 内容区域 ──
  auto *contentWidget = new QWidget;
  auto *mainLayout = new QVBoxLayout(contentWidget);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(6);

  m_formLayout = new QFormLayout;
  m_formLayout->setContentsMargins(0, 0, 0, 0);
  m_formLayout->setSpacing(6);
  m_formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  mainLayout->addLayout(m_formLayout);

  if (m_isColumnMode) {
    // ── 表格列显示 ──
    auto *querySep = new QLabel(QStringLiteral("── 表格列显示 ──"), this);
    querySep->setAlignment(Qt::AlignCenter);
    m_formLayout->addRow(QString(), querySep);

    // 表格列宽
    m_columnWidthCombo = new QComboBox(this);
    m_columnWidthCombo->addItem(QStringLiteral("自动"), 0);
    m_columnWidthCombo->addItem(QStringLiteral("60"), 60);
    m_columnWidthCombo->addItem(QStringLiteral("80"), 80);
    m_columnWidthCombo->addItem(QStringLiteral("100"), 100);
    m_columnWidthCombo->addItem(QStringLiteral("120"), 120);
    m_columnWidthCombo->addItem(QStringLiteral("150"), 150);
    m_columnWidthCombo->addItem(QStringLiteral("200"), 200);
    m_columnWidthCombo->addItem(QStringLiteral("250"), 250);
    m_columnWidthCombo->addItem(QStringLiteral("300"), 300);
    addRow(QStringLiteral("表格列宽:"), m_columnWidthCombo);

    // 固定列
    m_columnFixedCombo = new QComboBox(this);
    m_columnFixedCombo->addItem(QStringLiteral("不固定"), QString());
    m_columnFixedCombo->addItem(QStringLiteral("固定左侧"), QStringLiteral("left"));
    m_columnFixedCombo->addItem(QStringLiteral("固定右侧"), QStringLiteral("right"));
    addRow(QStringLiteral("固定列:"), m_columnFixedCombo);

    // 格式化类型
    m_formatterCombo = new QComboBox(this);
    m_formatterCombo->addItem(QStringLiteral("无"), QString());
    m_formatterCombo->addItem(QStringLiteral("日期"), QStringLiteral("date"));
    m_formatterCombo->addItem(QStringLiteral("状态"), QStringLiteral("status"));
    m_formatterCombo->addItem(QStringLiteral("金额"), QStringLiteral("currency"));
    addRow(QStringLiteral("格式化:"), m_formatterCombo);

    // 表格列显示类型
    m_displayTypeCombo = new QComboBox(this);
    m_displayTypeCombo->addItem(QStringLiteral("纯文本"), QStringLiteral(""));
    m_displayTypeCombo->addItem(QStringLiteral("标签(tag)"), QStringLiteral("tag"));
    m_displayTypeCombo->addItem(QStringLiteral("图标(icon)"), QStringLiteral("icon"));
    m_displayTypeCombo->addItem(QStringLiteral("布尔文字(boolean)"), QStringLiteral("boolean"));
    addRow(QStringLiteral("显示类型:"), m_displayTypeCombo);

    // tag 类型的子字段
    m_tagTrueTextEdit = new QLineEdit(this);
    m_tagTrueTextEdit->setPlaceholderText(QStringLiteral("如: 启用"));
    addRow(QStringLiteral("  tag真值文字:"), m_tagTrueTextEdit);

    m_tagTrueColorCombo = new QComboBox(this);
    m_tagTrueColorCombo->addItem(QStringLiteral("success"), QStringLiteral("success"));
    m_tagTrueColorCombo->addItem(QStringLiteral("primary"), QStringLiteral("primary"));
    m_tagTrueColorCombo->addItem(QStringLiteral("warning"), QStringLiteral("warning"));
    m_tagTrueColorCombo->addItem(QStringLiteral("info"), QStringLiteral("info"));
    addRow(QStringLiteral("  tag真值颜色:"), m_tagTrueColorCombo);

    m_tagFalseTextEdit = new QLineEdit(this);
    m_tagFalseTextEdit->setPlaceholderText(QStringLiteral("如: 禁用"));
    addRow(QStringLiteral("  tag假值文字:"), m_tagFalseTextEdit);

    m_tagFalseColorCombo = new QComboBox(this);
    m_tagFalseColorCombo->addItem(QStringLiteral("danger"), QStringLiteral("danger"));
    m_tagFalseColorCombo->addItem(QStringLiteral("info"), QStringLiteral("info"));
    m_tagFalseColorCombo->addItem(QStringLiteral("warning"), QStringLiteral("warning"));
    addRow(QStringLiteral("  tag假值颜色:"), m_tagFalseColorCombo);

    // boolean 类型的子字段
    m_boolTrueTextEdit = new QLineEdit(this);
    m_boolTrueTextEdit->setPlaceholderText(QStringLiteral("如: 显示"));
    addRow(QStringLiteral("  布尔真值文字:"), m_boolTrueTextEdit);

    m_boolFalseTextEdit = new QLineEdit(this);
    m_boolFalseTextEdit->setPlaceholderText(QStringLiteral("如: 隐藏"));
    addRow(QStringLiteral("  布尔假值文字:"), m_boolFalseTextEdit);

    // ── 编辑表单 ──
    auto *editSep = new QLabel(QStringLiteral("── 编辑表单 ──"), this);
    editSep->setAlignment(Qt::AlignCenter);
    m_formLayout->addRow(QString(), editSep);

    // 根据 EditStyle 创建对应控件
    switch (m_editStyle) {
      case EditStyle::Text: {
        m_placeholderEdit = new QLineEdit(this);
        m_placeholderEdit->setPlaceholderText(QStringLiteral("请输入占位提示"));
        addRow(QStringLiteral("占位提示:"), m_placeholderEdit);

        m_maxlengthCombo = new QComboBox(this);
        m_maxlengthCombo->addItem(QStringLiteral("不限"), 0);
        m_maxlengthCombo->addItem(QStringLiteral("10"), 10);
        m_maxlengthCombo->addItem(QStringLiteral("20"), 20);
        m_maxlengthCombo->addItem(QStringLiteral("50"), 50);
        m_maxlengthCombo->addItem(QStringLiteral("100"), 100);
        m_maxlengthCombo->addItem(QStringLiteral("200"), 200);
        m_maxlengthCombo->addItem(QStringLiteral("500"), 500);
        m_maxlengthCombo->addItem(QStringLiteral("1000"), 1000);
        addRow(QStringLiteral("最大长度:"), m_maxlengthCombo);
        break;
      }
      case EditStyle::Int: {
        m_minValueCombo = new QComboBox(this);
        m_minValueCombo->setEditable(true);
        m_minValueCombo->addItem(QStringLiteral("0"), 0.0);
        m_minValueCombo->addItem(QStringLiteral("1"), 1.0);
        m_minValueCombo->addItem(QStringLiteral("10"), 10.0);
        m_minValueCombo->addItem(QStringLiteral("100"), 100.0);
        m_minValueCombo->addItem(QStringLiteral("1000"), 1000.0);
        m_minValueCombo->addItem(QStringLiteral("-1"), -1.0);
        m_minValueCombo->addItem(QStringLiteral("-10"), -10.0);
        m_minValueCombo->addItem(QStringLiteral("-100"), -100.0);
        addRow(QStringLiteral("最小值:"), m_minValueCombo);

        m_maxValueCombo = new QComboBox(this);
        m_maxValueCombo->setEditable(true);
        m_maxValueCombo->addItem(QStringLiteral("1"), 1.0);
        m_maxValueCombo->addItem(QStringLiteral("10"), 10.0);
        m_maxValueCombo->addItem(QStringLiteral("100"), 100.0);
        m_maxValueCombo->addItem(QStringLiteral("1000"), 1000.0);
        m_maxValueCombo->addItem(QStringLiteral("9999"), 9999.0);
        m_maxValueCombo->addItem(QStringLiteral("99999"), 99999.0);
        m_maxValueCombo->addItem(QStringLiteral("999999"), 999999.0);
        addRow(QStringLiteral("最大值:"), m_maxValueCombo);
        break;
      }
      case EditStyle::Float: {
        m_precisionCombo = new QComboBox(this);
        m_precisionCombo->addItem(QStringLiteral("0"), 0);
        m_precisionCombo->addItem(QStringLiteral("1"), 1);
        m_precisionCombo->addItem(QStringLiteral("2"), 2);
        m_precisionCombo->addItem(QStringLiteral("3"), 3);
        m_precisionCombo->addItem(QStringLiteral("4"), 4);
        m_precisionCombo->addItem(QStringLiteral("6"), 6);
        m_precisionCombo->setCurrentIndex(2);
        addRow(QStringLiteral("小数位数:"), m_precisionCombo);

        m_minValueCombo = new QComboBox(this);
        m_minValueCombo->setEditable(true);
        m_minValueCombo->addItem(QStringLiteral("0"), 0.0);
        m_minValueCombo->addItem(QStringLiteral("1"), 1.0);
        m_minValueCombo->addItem(QStringLiteral("10"), 10.0);
        m_minValueCombo->addItem(QStringLiteral("100"), 100.0);
        m_minValueCombo->addItem(QStringLiteral("1000"), 1000.0);
        m_minValueCombo->addItem(QStringLiteral("-1"), -1.0);
        m_minValueCombo->addItem(QStringLiteral("-10"), -10.0);
        m_minValueCombo->addItem(QStringLiteral("-100"), -100.0);
        addRow(QStringLiteral("最小值:"), m_minValueCombo);

        m_maxValueCombo = new QComboBox(this);
        m_maxValueCombo->setEditable(true);
        m_maxValueCombo->addItem(QStringLiteral("1"), 1.0);
        m_maxValueCombo->addItem(QStringLiteral("10"), 10.0);
        m_maxValueCombo->addItem(QStringLiteral("100"), 100.0);
        m_maxValueCombo->addItem(QStringLiteral("1000"), 1000.0);
        m_maxValueCombo->addItem(QStringLiteral("9999"), 9999.0);
        m_maxValueCombo->addItem(QStringLiteral("99999"), 99999.0);
        m_maxValueCombo->addItem(QStringLiteral("999999"), 999999.0);
        addRow(QStringLiteral("最大值:"), m_maxValueCombo);
        break;
      }
      case EditStyle::Date: {
        m_dateFormatCombo = new QComboBox(this);
        m_dateFormatCombo->addItem(QStringLiteral("年月日时分秒"), QStringLiteral("datetime"));
        m_dateFormatCombo->addItem(QStringLiteral("年月日"), QStringLiteral("date"));
        m_dateFormatCombo->addItem(QStringLiteral("年月"), QStringLiteral("month"));
        m_dateFormatCombo->addItem(QStringLiteral("年"), QStringLiteral("year"));
        m_dateFormatCombo->addItem(QStringLiteral("日期范围"), QStringLiteral("daterange"));
        addRow(QStringLiteral("日期格式:"), m_dateFormatCombo);
        break;
      }
      case EditStyle::TextArea: {
        m_textareaRowsCombo = new QComboBox(this);
        m_textareaRowsCombo->addItem(QStringLiteral("2"), 2);
        m_textareaRowsCombo->addItem(QStringLiteral("3"), 3);
        m_textareaRowsCombo->addItem(QStringLiteral("4"), 4);
        m_textareaRowsCombo->addItem(QStringLiteral("5"), 5);
        m_textareaRowsCombo->addItem(QStringLiteral("6"), 6);
        m_textareaRowsCombo->addItem(QStringLiteral("8"), 8);
        m_textareaRowsCombo->addItem(QStringLiteral("10"), 10);
        m_textareaRowsCombo->setCurrentIndex(1);
        addRow(QStringLiteral("行数:"), m_textareaRowsCombo);

        m_placeholderEdit = new QLineEdit(this);
        m_placeholderEdit->setPlaceholderText(QStringLiteral("请输入占位提示"));
        addRow(QStringLiteral("占位提示:"), m_placeholderEdit);
        break;
      }
      case EditStyle::Select:
        // select 样式由 ComboboxConfigDialog 处理，此处不处理
        break;
    }

    // 编辑时通用项
    m_requiredCheck = new QCheckBox(this);
    addRow(QStringLiteral("必填:"), m_requiredCheck);

    m_formSpanCombo = new QComboBox(this);
    m_formSpanCombo->addItem(QStringLiteral("整行"), 24);
    m_formSpanCombo->addItem(QStringLiteral("半行"), 12);
    m_formSpanCombo->addItem(QStringLiteral("三分之一"), 8);
    addRow(QStringLiteral("表单布局:"), m_formSpanCombo);

    // ── 通用配置 ──
    auto *commonSep = new QLabel(QStringLiteral("── 通用配置 ──"), this);
    commonSep->setAlignment(Qt::AlignCenter);
    m_formLayout->addRow(QString(), commonSep);

    // 新增记录默认值
    m_defaultValueEdit = new QLineEdit(this);
    m_defaultValueEdit->setPlaceholderText(QStringLiteral("新增记录时的默认值"));
    addRow(QStringLiteral("默认值:"), m_defaultValueEdit);

    // 表格默认排序
    m_defaultSortCombo = new QComboBox(this);
    m_defaultSortCombo->addItem(QStringLiteral("无"), QString());
    m_defaultSortCombo->addItem(QStringLiteral("升序"), QStringLiteral("asc"));
    m_defaultSortCombo->addItem(QStringLiteral("降序"), QStringLiteral("desc"));
    addRow(QStringLiteral("默认排序:"), m_defaultSortCombo);
  } else {
    // 查询字段模式：根据 QueryInputStyle 创建对应控件
    switch (m_queryStyle) {
      case QueryInputStyle::Text: {
        m_placeholderEdit = new QLineEdit(this);
        m_placeholderEdit->setPlaceholderText(QStringLiteral("请输入占位提示"));
        addRow(QStringLiteral("占位提示:"), m_placeholderEdit);
        break;
      }
      case QueryInputStyle::Date: {
        m_dateFormatCombo = new QComboBox(this);
        m_dateFormatCombo->addItem(QStringLiteral("年月日时分秒"), QStringLiteral("datetime"));
        m_dateFormatCombo->addItem(QStringLiteral("年月日"), QStringLiteral("date"));
        m_dateFormatCombo->addItem(QStringLiteral("年月"), QStringLiteral("month"));
        m_dateFormatCombo->addItem(QStringLiteral("年"), QStringLiteral("year"));
        m_dateFormatCombo->addItem(QStringLiteral("日期范围"), QStringLiteral("daterange"));
        addRow(QStringLiteral("日期格式:"), m_dateFormatCombo);
        break;
      }
      case QueryInputStyle::Select:
        // select 样式由 ComboboxConfigDialog 处理，此处不处理
        break;
    }
  }

  // ── 底部按钮 ──
  auto btns = AuiButton::createDialogButtons(this);
  mainLayout->addLayout(btns.layout);

  connect(btns.okBtn, &QPushButton::clicked, this, &QDialog::accept);
  connect(btns.cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

  // ── 应用窗口框架 ──
  AuiWindow::applyWindowFrame(this, tb.titleBar, contentWidget);
}

void StyleConfigDialog::addRow(const QString &labelText, QWidget *widget) {
  if (m_formLayout) m_formLayout->addRow(labelText, widget);
}

// ════════════════════════════════════════════════════════════
//  配置读写
// ════════════════════════════════════════════════════════════

void StyleConfigDialog::setPlaceholder(const QString &v) {
  if (m_placeholderEdit) m_placeholderEdit->setText(v);
}

QString StyleConfigDialog::placeholder() const {
  return m_placeholderEdit ? m_placeholderEdit->text().trimmed() : QString();
}

void StyleConfigDialog::setMaxlength(int v) {
  if (m_maxlengthCombo) {
    int idx = m_maxlengthCombo->findData(v);
    if (idx >= 0) m_maxlengthCombo->setCurrentIndex(idx);
  }
}

int StyleConfigDialog::maxlength() const {
  return m_maxlengthCombo ? m_maxlengthCombo->currentData().toInt() : 0;
}

void StyleConfigDialog::setMinValue(double v) {
  if (m_minValueCombo) {
    int idx = m_minValueCombo->findData(v);
    if (idx >= 0) {
      m_minValueCombo->setCurrentIndex(idx);
    } else {
      m_minValueCombo->setEditText(QString::number(v));
    }
  }
}

double StyleConfigDialog::minValue() const {
  if (!m_minValueCombo) return 0;
  QVariant d = m_minValueCombo->currentData();
  return d.isValid() ? d.toDouble() : m_minValueCombo->currentText().toDouble();
}

void StyleConfigDialog::setMaxValue(double v) {
  if (m_maxValueCombo) {
    int idx = m_maxValueCombo->findData(v);
    if (idx >= 0) {
      m_maxValueCombo->setCurrentIndex(idx);
    } else {
      m_maxValueCombo->setEditText(QString::number(v));
    }
  }
}

double StyleConfigDialog::maxValue() const {
  if (!m_maxValueCombo) return 0;
  QVariant d = m_maxValueCombo->currentData();
  return d.isValid() ? d.toDouble() : m_maxValueCombo->currentText().toDouble();
}

void StyleConfigDialog::setPrecision(int v) {
  if (m_precisionCombo) {
    int idx = m_precisionCombo->findData(v);
    if (idx >= 0) m_precisionCombo->setCurrentIndex(idx);
  }
}

int StyleConfigDialog::precision() const {
  return m_precisionCombo ? m_precisionCombo->currentData().toInt() : 2;
}

void StyleConfigDialog::setDateFormat(const QString &v) {
  if (m_dateFormatCombo) {
    int idx = m_dateFormatCombo->findData(v);
    if (idx >= 0) m_dateFormatCombo->setCurrentIndex(idx);
  }
}

QString StyleConfigDialog::dateFormat() const {
  return m_dateFormatCombo ? m_dateFormatCombo->currentData().toString() : QString();
}

void StyleConfigDialog::setTextareaRows(int v) {
  if (m_textareaRowsCombo) {
    int idx = m_textareaRowsCombo->findData(v);
    if (idx >= 0) m_textareaRowsCombo->setCurrentIndex(idx);
  }
}

int StyleConfigDialog::textareaRows() const {
  return m_textareaRowsCombo ? m_textareaRowsCombo->currentData().toInt() : 3;
}

// ── 通用配置（3-1~3-4）──

void StyleConfigDialog::setRequired(bool v) {
  if (m_requiredCheck) m_requiredCheck->setChecked(v);
}

bool StyleConfigDialog::required() const {
  return m_requiredCheck ? m_requiredCheck->isChecked() : false;
}

void StyleConfigDialog::setColumnWidth(int v) {
  if (m_columnWidthCombo) {
    int idx = m_columnWidthCombo->findData(v);
    if (idx >= 0) m_columnWidthCombo->setCurrentIndex(idx);
  }
}

int StyleConfigDialog::columnWidth() const {
  return m_columnWidthCombo ? m_columnWidthCombo->currentData().toInt() : 0;
}

void StyleConfigDialog::setColumnFixed(const QString &v) {
  if (m_columnFixedCombo) {
    int idx = m_columnFixedCombo->findData(v);
    if (idx >= 0) m_columnFixedCombo->setCurrentIndex(idx);
  }
}

QString StyleConfigDialog::columnFixed() const {
  return m_columnFixedCombo ? m_columnFixedCombo->currentData().toString() : QString();
}

void StyleConfigDialog::setFormatter(const QString &v) {
  if (m_formatterCombo) {
    int idx = m_formatterCombo->findData(v);
    if (idx >= 0) m_formatterCombo->setCurrentIndex(idx);
  }
}

QString StyleConfigDialog::formatter() const {
  return m_formatterCombo ? m_formatterCombo->currentData().toString() : QString();
}

void StyleConfigDialog::setFormSpan(int v) {
  if (m_formSpanCombo) {
    int idx = m_formSpanCombo->findData(v);
    if (idx >= 0) m_formSpanCombo->setCurrentIndex(idx);
  }
}

int StyleConfigDialog::formSpan() const {
  return m_formSpanCombo ? m_formSpanCombo->currentData().toInt() : 24;
}

// ════════════════════════════════════════════════════════════
//  表格列显示类型（3-5）
// ════════════════════════════════════════════════════════════

void StyleConfigDialog::setDisplayType(const QString &v) {
  if (m_displayTypeCombo) {
    int idx = m_displayTypeCombo->findData(v);
    m_displayTypeCombo->setCurrentIndex(idx >= 0 ? idx : 0);
  }
}

QString StyleConfigDialog::displayType() const {
  return m_displayTypeCombo ? m_displayTypeCombo->currentData().toString() : QString();
}

void StyleConfigDialog::setTagTrueText(const QString &v) {
  if (m_tagTrueTextEdit) m_tagTrueTextEdit->setText(v);
}

QString StyleConfigDialog::tagTrueText() const {
  return m_tagTrueTextEdit ? m_tagTrueTextEdit->text().trimmed() : QString();
}

void StyleConfigDialog::setTagTrueColor(const QString &v) {
  if (m_tagTrueColorCombo) {
    int idx = m_tagTrueColorCombo->findData(v);
    if (idx >= 0) m_tagTrueColorCombo->setCurrentIndex(idx);
  }
}

QString StyleConfigDialog::tagTrueColor() const {
  return m_tagTrueColorCombo ? m_tagTrueColorCombo->currentData().toString()
                             : QStringLiteral("success");
}

void StyleConfigDialog::setTagFalseText(const QString &v) {
  if (m_tagFalseTextEdit) m_tagFalseTextEdit->setText(v);
}

QString StyleConfigDialog::tagFalseText() const {
  return m_tagFalseTextEdit ? m_tagFalseTextEdit->text().trimmed() : QString();
}

void StyleConfigDialog::setTagFalseColor(const QString &v) {
  if (m_tagFalseColorCombo) {
    int idx = m_tagFalseColorCombo->findData(v);
    if (idx >= 0) m_tagFalseColorCombo->setCurrentIndex(idx);
  }
}

QString StyleConfigDialog::tagFalseColor() const {
  return m_tagFalseColorCombo ? m_tagFalseColorCombo->currentData().toString()
                              : QStringLiteral("danger");
}

void StyleConfigDialog::setBoolTrueText(const QString &v) {
  if (m_boolTrueTextEdit) m_boolTrueTextEdit->setText(v);
}

QString StyleConfigDialog::boolTrueText() const {
  return m_boolTrueTextEdit ? m_boolTrueTextEdit->text().trimmed() : QString();
}

void StyleConfigDialog::setBoolFalseText(const QString &v) {
  if (m_boolFalseTextEdit) m_boolFalseTextEdit->setText(v);
}

QString StyleConfigDialog::boolFalseText() const {
  return m_boolFalseTextEdit ? m_boolFalseTextEdit->text().trimmed() : QString();
}

// ════════════════════════════════════════════════════════════
//  通用配置（3-6）
// ════════════════════════════════════════════════════════════

void StyleConfigDialog::setDefaultValue(const QString &v) {
  if (m_defaultValueEdit) m_defaultValueEdit->setText(v);
}

QString StyleConfigDialog::defaultValue() const {
  return m_defaultValueEdit ? m_defaultValueEdit->text().trimmed() : QString();
}

void StyleConfigDialog::setDefaultSort(const QString &v) {
  if (m_defaultSortCombo) {
    int idx = m_defaultSortCombo->findData(v);
    m_defaultSortCombo->setCurrentIndex(idx >= 0 ? idx : 0);
  }
}

QString StyleConfigDialog::defaultSort() const {
  return m_defaultSortCombo ? m_defaultSortCombo->currentData().toString() : QString();
}
