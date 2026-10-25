/**
 * @file project_config_dialog.cpp
 * @brief project.acproj 项目配置可视化编辑对话框实现
 */

#include "project_config_dialog.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "json_table_model.h"
#include "src/engine/ac_language.h"
#include "src/engine/function/fun_db.h"
#include "src/engine/function/fun_mgr.h"
#include "src/ui/json_vue/config_dialog_common.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/component/aui_message_box.h"
#include "src/util/ui/component/aui_style.h"

namespace {

QString jsonKey(const char *key) {
  return QString::fromLatin1(key);
}

/// 目录选择行（QLineEdit + 浏览按钮）
QWidget *makePathRow(QLineEdit *edit, QWidget *parent, const QString &title) {
  auto *row = new QWidget(parent);
  auto *layout = new QHBoxLayout(row);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  layout->addWidget(edit, 1);
  auto *browse = makeCompactButton(QStringLiteral("浏览..."), row);
  layout->addWidget(browse);
  QObject::connect(browse, &QPushButton::clicked, row, [edit, row, title]() {
    const QString dir = QFileDialog::getExistingDirectory(
        row, QStringLiteral("选择%1目录").arg(title), edit->text().trimmed());
    if (!dir.isEmpty()) edit->setText(QDir::toNativeSeparators(dir));
  });
  return row;
}

}  // namespace

ProjectConfigDialog::ProjectConfigDialog(const QString &filePath, QWidget *parent)
    : QDialog(parent), m_filePath(filePath) {
  setupUI();
  QString error;
  if (!loadFile(&error)) {
    AuiMessageBox::show(this, QStringLiteral("读取失败"), error);
  }
}

void ProjectConfigDialog::setupUI() {
  ConfigDialogFrame frame = beginConfigDialog(
      this, QStringLiteral("项目设置"), QMargins(12, 10, 12, 10), 6);
  auto *form = new QFormLayout;
  form->setSpacing(6);

  m_nameEdit = new QLineEdit(this);
  m_nameEdit->setPlaceholderText(QStringLiteral("项目名，如 shop"));
  form->addRow(QStringLiteral("项目名:"), m_nameEdit);

  m_hostEdit = new QLineEdit(this);
  m_hostEdit->setPlaceholderText(QStringLiteral("127.0.0.1"));
  form->addRow(QStringLiteral("数据库主机:"), m_hostEdit);

  m_portSpin = new QSpinBox(this);
  m_portSpin->setRange(1, 65535);
  m_portSpin->setValue(3306);
  form->addRow(QStringLiteral("端口:"), m_portSpin);

  m_userEdit = new QLineEdit(this);
  m_userEdit->setPlaceholderText(QStringLiteral("root"));
  form->addRow(QStringLiteral("用户:"), m_userEdit);

  m_passwordEdit = new QLineEdit(this);
  m_passwordEdit->setEchoMode(QLineEdit::Password);
  m_passwordEdit->setPlaceholderText(QStringLiteral("允许为空"));
  form->addRow(QStringLiteral("密码:"), m_passwordEdit);

  m_databaseEdit = new QLineEdit(this);
  m_databaseEdit->setPlaceholderText(QStringLiteral("shop"));
  form->addRow(QStringLiteral("数据库:"), m_databaseEdit);

  m_goBasePathEdit = new QLineEdit(this);
  m_goBasePathEdit->setPlaceholderText(QStringLiteral("Go 项目根目录，如 D:/work/github/shop/shop_api"));
  form->addRow(QStringLiteral("goBasePath:"),
               makePathRow(m_goBasePathEdit, this, QStringLiteral("Go 项目根")));

  m_webBasePathEdit = new QLineEdit(this);
  m_webBasePathEdit->setPlaceholderText(
      QStringLiteral("pure-admin 项目根目录，如 D:/work/github/shop/shop_admin"));
  form->addRow(QStringLiteral("webBasePath:"),
               makePathRow(m_webBasePathEdit, this, QStringLiteral("前端项目根")));

  frame.contentLayout->addLayout(form);

  auto *hint = new QLabel(
      QStringLiteral("project.acproj 同时作为项目根标记；根 / meta / db 三层的未知键会原样保留。"),
      this);
  hint->setWordWrap(true);
  hint->setStyleSheet(
      QStringLiteral("color: %1; font-size: 12px;").arg(AuiStyle::mutedTextColor().name()));
  frame.contentLayout->addWidget(hint);

  auto *btnRow = new QHBoxLayout;
  auto *testBtn = makeCompactButton(QStringLiteral("测试连接"), this);
  btnRow->addWidget(testBtn);
  btnRow->addStretch();
  frame.contentLayout->addLayout(btnRow);
  connect(testBtn, &QPushButton::clicked, this, [this]() {
    if (!testConnection()) {
      const QString error = FunMgr::takeError();
      AuiMessageBox::show(
          this, QStringLiteral("连接失败"),
          error.isEmpty()
              ? QStringLiteral("数据库连接失败，请检查 host/port/user/password/database。")
              : error);
    } else {
      AuiMessageBox::show(this, QStringLiteral("连接成功"),
                          QStringLiteral("数据库连接成功。"));
    }
  });

  finishConfigDialog(this, frame);
  setMinimumWidth(640);
}

bool ProjectConfigDialog::loadFile(QString *error) {
  QFile file(m_filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (error) *error = QStringLiteral("无法打开项目配置: %1").arg(m_filePath);
    return false;
  }
  const QString text = QString::fromUtf8(file.readAll());
  file.close();

  QJsonParseError perr;
  const QJsonDocument doc = UtilJson::fromJson(text, &perr);
  if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
    if (error) *error = QStringLiteral("项目配置解析失败: %1").arg(perr.errorString());
    return false;
  }
  const QJsonObject root = doc.object();
  const QJsonObject meta = root.value(jsonKey(JsonTableKey::kMeta)).toObject();
  const QJsonObject db = meta.value(jsonKey(JsonTableKey::kDb)).toObject();

  m_rootExtra = root;
  m_rootExtra.remove(jsonKey(JsonTableKey::kName));
  m_rootExtra.remove(jsonKey(JsonTableKey::kMeta));

  m_metaExtra = meta;
  m_metaExtra.remove(jsonKey(JsonTableKey::kDb));
  m_metaExtra.remove(jsonKey(JsonTableKey::kGoBasePath));
  m_metaExtra.remove(jsonKey(JsonTableKey::kWebBasePath));
  // 旧 meta.project 与新 name 重复，迁移时丢弃
  m_metaExtra.remove(jsonKey(JsonTableKey::kProject));

  m_dbExtra = db;
  m_dbExtra.remove(jsonKey(JsonTableKey::kHost));
  m_dbExtra.remove(jsonKey(JsonTableKey::kPort));
  m_dbExtra.remove(jsonKey(JsonTableKey::kUser));
  m_dbExtra.remove(jsonKey(JsonTableKey::kPassword));
  m_dbExtra.remove(jsonKey(JsonTableKey::kDatabase));

  QString name = root.value(jsonKey(JsonTableKey::kName)).toString().trimmed();
  if (name.isEmpty()) name = QFileInfo(m_filePath).dir().dirName();
  m_nameEdit->setText(name);
  m_hostEdit->setText(db.value(jsonKey(JsonTableKey::kHost)).toString());
  m_portSpin->setValue(db.value(jsonKey(JsonTableKey::kPort)).toInt(3306));
  m_userEdit->setText(db.value(jsonKey(JsonTableKey::kUser)).toString());
  m_passwordEdit->setText(db.value(jsonKey(JsonTableKey::kPassword)).toString());
  m_databaseEdit->setText(db.value(jsonKey(JsonTableKey::kDatabase)).toString());
  m_goBasePathEdit->setText(
      QDir::toNativeSeparators(meta.value(jsonKey(JsonTableKey::kGoBasePath)).toString()));
  m_webBasePathEdit->setText(
      QDir::toNativeSeparators(meta.value(jsonKey(JsonTableKey::kWebBasePath)).toString()));
  return true;
}

bool ProjectConfigDialog::validate(QString *error) const {
  if (m_nameEdit->text().trimmed().isEmpty()) {
    if (error) *error = QStringLiteral("项目名不能为空");
    return false;
  }
  // 允许只改项目名（非 jsontable 项目可能没有 db 配置）；
  // 一旦填写了数据库信息，则 host/user/database 必须齐全。
  const bool hasDb = !m_hostEdit->text().trimmed().isEmpty() ||
                     !m_userEdit->text().trimmed().isEmpty() ||
                     !m_databaseEdit->text().trimmed().isEmpty();
  if (hasDb) {
    if (m_hostEdit->text().trimmed().isEmpty()) {
      if (error) *error = QStringLiteral("数据库主机不能为空");
      return false;
    }
    if (m_userEdit->text().trimmed().isEmpty()) {
      if (error) *error = QStringLiteral("数据库用户不能为空");
      return false;
    }
    if (m_databaseEdit->text().trimmed().isEmpty()) {
      if (error) *error = QStringLiteral("数据库名不能为空");
      return false;
    }
  }
  return true;
}

bool ProjectConfigDialog::saveFile(QString *error) {
  const bool hasDb = !m_hostEdit->text().trimmed().isEmpty() ||
                     !m_userEdit->text().trimmed().isEmpty() ||
                     !m_databaseEdit->text().trimmed().isEmpty();

  QJsonObject meta = m_metaExtra;
  if (hasDb) {
    QJsonObject db = m_dbExtra;
    db.insert(jsonKey(JsonTableKey::kHost), m_hostEdit->text().trimmed());
    db.insert(jsonKey(JsonTableKey::kPort), m_portSpin->value());
    db.insert(jsonKey(JsonTableKey::kUser), m_userEdit->text().trimmed());
    db.insert(jsonKey(JsonTableKey::kPassword), m_passwordEdit->text());
    db.insert(jsonKey(JsonTableKey::kDatabase), m_databaseEdit->text().trimmed());
    meta.insert(jsonKey(JsonTableKey::kDb), db);
  } else {
    meta.remove(jsonKey(JsonTableKey::kDb));
  }
  const QString goBasePath = QDir::fromNativeSeparators(m_goBasePathEdit->text().trimmed());
  const QString webBasePath = QDir::fromNativeSeparators(m_webBasePathEdit->text().trimmed());
  if (goBasePath.isEmpty()) {
    meta.remove(jsonKey(JsonTableKey::kGoBasePath));
  } else {
    meta.insert(jsonKey(JsonTableKey::kGoBasePath), goBasePath);
  }
  if (webBasePath.isEmpty()) {
    meta.remove(jsonKey(JsonTableKey::kWebBasePath));
  } else {
    meta.insert(jsonKey(JsonTableKey::kWebBasePath), webBasePath);
  }

  QJsonObject root = m_rootExtra;
  root.insert(jsonKey(JsonTableKey::kName), m_nameEdit->text().trimmed());
  if (meta.isEmpty()) {
    root.remove(jsonKey(JsonTableKey::kMeta));
  } else {
    root.insert(jsonKey(JsonTableKey::kMeta), meta);
  }

  QFile file(m_filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    if (error) *error = QStringLiteral("无法写入项目配置: %1").arg(m_filePath);
    return false;
  }
  const QByteArray bytes =
      (JsonTableConfig::toJsonString(root) + QStringLiteral("\n")).toUtf8();
  const bool ok = file.write(bytes) == bytes.size();
  file.close();
  if (!ok && error) *error = QStringLiteral("写入项目配置失败: %1").arg(m_filePath);
  return ok;
}

bool ProjectConfigDialog::testConnection() {
  accore::AcJsonValue cfg = accore::AcJsonValue::makeObject();
  cfg.set(jsonKey(AcDB::kHost), accore::AcJsonValue(m_hostEdit->text().trimmed()));
  cfg.set(jsonKey(AcDB::kPort), accore::AcJsonValue(m_portSpin->value()));
  cfg.set(jsonKey(AcDB::kUser), accore::AcJsonValue(m_userEdit->text().trimmed()));
  cfg.set(jsonKey(AcDB::kPassword), accore::AcJsonValue(m_passwordEdit->text()));
  cfg.set(jsonKey(AcDB::kDatabase), accore::AcJsonValue(m_databaseEdit->text().trimmed()));

  accore::AcJsonValue args = accore::AcJsonValue::makeArray();
  args.append(cfg);
  const accore::AcJsonValue instance = FunDb::constructor(args);
  const bool connected =
      instance.isObject() && instance.value(jsonKey(AcDB::kConnected)).toBool(false);
  if (instance.isObject()) {
    FunDb::destructor(instance, accore::AcJsonValue::makeArray());
  }
  return connected;
}

void ProjectConfigDialog::accept() {
  QString error;
  if (!validate(&error)) {
    AuiMessageBox::show(this, QStringLiteral("无法保存"), error);
    return;
  }
  if (!saveFile(&error)) {
    AuiMessageBox::show(this, QStringLiteral("无法保存"), error);
    return;
  }
  QDialog::accept();
}
