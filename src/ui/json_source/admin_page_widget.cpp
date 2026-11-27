/** @file admin_page_widget.cpp
 * @brief 页面设置只保留自定义保存接口和页面说明，列表/查询/表单均复用字段呈现。
 */
#include "admin_page_widget.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>

AdminPageWidget::AdminPageWidget(QWidget *parent) : QWidget(parent) {
  setObjectName("adminPageWidget");
  auto *layout = new QVBoxLayout(this);
  auto *hint = new QLabel(QStringLiteral("列表、列标题、列宽、查询、表单及开关都在“字段呈现”配置。页面标题统一取“字段”页的表注释。自定义保存接口只提交下方白名单字段，由后端负责业务校验及审计。"), this);
  hint->setWordWrap(true);
  layout->addWidget(hint);
  auto *tabs = new QTabWidget(this);
  layout->addWidget(tabs, 1);
  auto *basic = new QWidget(tabs);
  auto *form = new QFormLayout(basic);
  // 页面标题不在页面设置中维护：生成端统一取“字段”页的表注释。
  m_customSave = new QCheckBox(QStringLiteral("使用自定义保存接口"), basic);
  m_customSave->setObjectName("adminPage_customSave");
  form->addRow(m_customSave);
  m_customSaveForm = new QWidget(basic);
  auto *customForm = new QFormLayout(m_customSaveForm);
  customForm->setContentsMargins(0, 0, 0, 0);
  m_saveUrl = new QLineEdit(m_customSaveForm);
  m_saveUrl->setObjectName("adminPage_saveUrl");
  m_saveUrl->setPlaceholderText(QStringLiteral("/api/market/base-config"));
  customForm->addRow(QStringLiteral("保存接口（POST /api/…）"), m_saveUrl);
  m_keyField = new QLineEdit(m_customSaveForm);
  m_keyField->setObjectName("adminPage_keyField");
  m_keyField->setPlaceholderText(QStringLiteral("code（缺省 id）"));
  customForm->addRow(QStringLiteral("记录标识字段"), m_keyField);
  m_editFields = new QLineEdit(m_customSaveForm);
  m_editFields->setObjectName("adminPage_editFields");
  m_editFields->setPlaceholderText(QStringLiteral("value（多个字段用逗号分隔）"));
  customForm->addRow(QStringLiteral("可写入字段"), m_editFields);
  m_switchField = new QLineEdit(m_customSaveForm);
  m_switchField->setObjectName("adminPage_switchField");
  m_switchField->setPlaceholderText(QStringLiteral("enable（留空按当前开关列提交）"));
  customForm->addRow(QStringLiteral("列表开关字段"), m_switchField);
  m_customSaveForm->setEnabled(false);
  form->addRow(m_customSaveForm);
  tabs->addTab(basic, QStringLiteral("接口绑定"));
  auto *notice = new QWidget(tabs);
  auto *noticeForm = new QFormLayout(notice);
  m_notice = new QPlainTextEdit(notice);
  m_notice->setObjectName("adminPage_notice");
  m_notice->setPlaceholderText(QStringLiteral("页尾纯文本说明，显示在列表和分页下方，留空不显示"));
  noticeForm->addRow(QStringLiteral("页面说明"), m_notice);
  m_noticeType = new QComboBox(notice);
  m_noticeType->addItems({"info", "warning", "success", "error"});
  noticeForm->addRow(QStringLiteral("提示类型"), m_noticeType);
  m_noticeClosable = new QCheckBox(QStringLiteral("允许关闭"), notice);
  noticeForm->addRow(m_noticeClosable);
  tabs->addTab(notice, QStringLiteral("页面说明"));
  for (QLineEdit *edit : {m_saveUrl, m_keyField, m_editFields, m_switchField}) {
    connect(edit, &QLineEdit::textChanged, this, &AdminPageWidget::changed);
  }
  connect(m_customSave, &QCheckBox::toggled, this, [this](bool on) {
    m_customSaveForm->setEnabled(on);
    changed();
  });
  connect(m_notice, &QPlainTextEdit::textChanged, this, &AdminPageWidget::changed);
  connect(m_noticeType, &QComboBox::currentIndexChanged, this, &AdminPageWidget::changed);
  connect(m_noticeClosable, &QCheckBox::toggled, this, &AdminPageWidget::changed);
  setConfig({});
}

void AdminPageWidget::changed() { if (!m_loading) emit configChanged(); }

void AdminPageWidget::setConfig(const QJsonObject &config) {
  m_loading = true;
  m_original = config;
  m_customSave->setChecked(config.value("customSave").toBool());
  m_customSaveForm->setEnabled(m_customSave->isChecked());
  m_saveUrl->setText(config.value("saveUrl").toString());
  m_keyField->setText(config.value("keyField").toString());
  QStringList fields;
  for (const auto &value : config.value("editFields").toArray()) fields << value.toString();
  m_editFields->setText(fields.join(QStringLiteral(", ")));
  m_switchField->setText(config.value("switchField").toString());
  m_notice->setPlainText(config.value("notice").toString());
  m_noticeType->setCurrentText(config.value("noticeType").toString("info"));
  m_noticeClosable->setChecked(config.value("noticeClosable").toBool());
  // 回填后以收敛后的配置作为“未修改”基线：空配置不会因为默认 notice 键
  // 凭空生成 adminPage 节点。
  m_baseline = collectConfig();
  m_original = m_baseline;
  m_loading = false;
}

QJsonObject AdminPageWidget::collectConfig() const {
  QJsonObject result = m_original;
  if (m_customSave->isChecked()) {
    result.insert("customSave", true);
    result.insert("saveUrl", m_saveUrl->text().trimmed());
    result.insert("keyField", m_keyField->text().trimmed());
    QJsonArray fields;
    QString text = m_editFields->text();
    text.replace(QStringLiteral("，"), QStringLiteral(","));
    for (const QString &name : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
      const QString trimmed = name.trimmed();
      if (!trimmed.isEmpty()) fields.append(trimmed);
    }
    result.insert("editFields", fields);
    const QString switchField = m_switchField->text().trimmed();
    if (switchField.isEmpty()) result.remove("switchField");
    else result.insert("switchField", switchField);
  } else {
    result.remove("customSave");
    result.remove("saveUrl");
    result.remove("keyField");
    result.remove("editFields");
    result.remove("switchField");
  }
  const QString notice = m_notice->toPlainText();
  if (!notice.isEmpty() || m_original.contains("notice")) result.insert("notice", notice);
  else result.remove("notice");
  if (!notice.isEmpty() || m_original.contains("noticeType"))
    result.insert("noticeType", m_noticeType->currentText());
  else result.remove("noticeType");
  if (m_noticeClosable->isChecked() || m_original.contains("noticeClosable"))
    result.insert("noticeClosable", m_noticeClosable->isChecked());
  else result.remove("noticeClosable");
  return result;
}

QJsonObject AdminPageWidget::config() const {
  const auto draft = collectConfig();
  return draft == m_baseline ? m_original : draft;
}

bool AdminPageWidget::validate(QString *error) const {
  if (!m_customSave->isChecked()) return true;
  const auto cfg = config();
  const auto fail = [error](const QString &message) { if (error) *error = message; return false; };
  const QString url = cfg.value("saveUrl").toString();
  if (!url.startsWith("/api/") || url.contains('?') || url.contains('#') || url.contains(".."))
    return fail(QStringLiteral("保存接口必须是本项目/api/路径：saveUrl"));
  const QRegularExpression identifier("^[a-zA-Z_][a-zA-Z0-9_]*$");
  const QString keyField = cfg.value("keyField").toString();
  if (!identifier.match(keyField).hasMatch())
    return fail(QStringLiteral("记录标识字段无效：keyField"));
  const QJsonArray editFields = cfg.value("editFields").toArray();
  if (editFields.isEmpty())
    return fail(QStringLiteral("可写入字段不能为空：editFields"));
  QSet<QString> seen;
  for (const auto &value : editFields) {
    const QString name = value.toString();
    if (!identifier.match(name).hasMatch())
      return fail(QStringLiteral("可写入字段名无效：") + name);
    if (seen.contains(name))
      return fail(QStringLiteral("可写入字段重复：") + name);
    seen.insert(name);
  }
  const QString switchField = cfg.value("switchField").toString();
  if (!switchField.isEmpty() && !identifier.match(switchField).hasMatch())
    return fail(QStringLiteral("列表开关字段无效：switchField"));
  return true;
}
