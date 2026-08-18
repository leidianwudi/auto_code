/**
 * @file style_config_query_dialog.cpp
 * @brief 查询字段样式对话框实现（QueryStyleDialog）
 *
 * 从 style_config_dialog.cpp 拆分：ColumnStyleDialog 留在原文件，
 * QueryStyleDialog（占位提示 / 日期格式）独立于此文件。
 */

#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>

#include "config_dialog_common.h"
#include "src/util/common/code_constants.h"
#include "style_config_dialog.h"

// ════════════════════════════════════════════════════════════
//  QueryStyleDialog：查询字段样式对话框
// ════════════════════════════════════════════════════════════

QueryStyleDialog::QueryStyleDialog(QueryInputStyle style, QWidget *parent) : QDialog(parent) {
  m_queryStyle = style;
  setupUI();
}

void QueryStyleDialog::setupUI() {
  setMinimumWidth(420);
  // 不设置 minimumHeight/maximumHeight，让对话框高度随内容自适应
  auto frame = beginConfigDialog(this, QStringLiteral("查询样式配置"));
  auto *mainLayout = frame.contentLayout;

  auto *form = new QFormLayout;
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(6);
  form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  mainLayout->addLayout(form);

  switch (m_queryStyle) {
    case QueryInputStyle::Text: {
      m_placeholderEdit = new QLineEdit(this);
      m_placeholderEdit->setPlaceholderText(QStringLiteral("请输入占位提示"));
      m_placeholderEdit->setText(m_cachedPlaceholder);
      form->addRow(QStringLiteral("占位提示:"), m_placeholderEdit);
      break;
    }
    case QueryInputStyle::Date: {
      m_dateFormatCombo = new QComboBox(this);
      m_dateFormatCombo->addItem(QString::fromUtf8(CodeConstants::UiText::kDatetimeFull),
                                 QStringLiteral("datetime"));
      m_dateFormatCombo->addItem(QStringLiteral("时分秒"), QStringLiteral("time"));
      m_dateFormatCombo->addItem(QStringLiteral("年月日"), QStringLiteral("date"));
      m_dateFormatCombo->addItem(QString::fromUtf8(CodeConstants::UiText::kYearMonth),
                                 QStringLiteral("month"));
      m_dateFormatCombo->addItem(QStringLiteral("年"), QStringLiteral("year"));
      // 注：日期范围不属于格式，应通过查询关系（QColRelation）配置
      comboSelectData(m_dateFormatCombo, m_cachedDateFormat, 1);
      form->addRow(QStringLiteral("日期格式:"), m_dateFormatCombo);
      break;
    }
    case QueryInputStyle::Select:
      // Select 样式由 ComboboxConfigDialog 配置，此处不构建任何控件
      // 理论上不应进入此对话框（onConfigureQuerySelect 中 Select 直接走 ComboboxConfigDialog）
      break;
  }

  finishConfigDialog(this, frame);

  // 按内容自适应大小
  layout()->activate();
  resize(layout()->sizeHint());
}

// ── 配置读写 ──

void QueryStyleDialog::setPlaceholder(const QString &v) {
  m_cachedPlaceholder = v;
  if (m_placeholderEdit) m_placeholderEdit->setText(v);
}

QString QueryStyleDialog::placeholder() const {
  return m_placeholderEdit ? m_placeholderEdit->text().trimmed() : m_cachedPlaceholder;
}

void QueryStyleDialog::setDateFormat(const QString &v) {
  m_cachedDateFormat = v;
  if (m_dateFormatCombo) {
    // 若旧值为 "daterange"（已废弃），回退到默认 "date"
    comboSelectData(m_dateFormatCombo, v,
                    m_dateFormatCombo->findData(QString::fromLatin1(JsonVueStyle::kDate)));
  }
}

QString QueryStyleDialog::dateFormat() const {
  return m_dateFormatCombo ? m_dateFormatCombo->currentData().toString() : m_cachedDateFormat;
}
