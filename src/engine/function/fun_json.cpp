/**
 * @file fun_json.cpp
 * @brief JSON 函数实现
 */

#include "fun_json.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include "../ac_language.h"
#include "fun_mgr.h"


// init — 注册 JSON 函数到 FunMgr（builtin 伪类）
void FunJson::init() {
  FunMgr::ins().registerFuncs(QString::fromLatin1(AcRuntime::kBuiltinClass),
                              {{QString::fromLatin1(AcBuiltin::kReadJson), readJson}});
}

// readJson — 读取 JSON 文件
QJsonValue FunJson::readJson(const QJsonArray &args) {
  if (args.isEmpty() || !args[0].isString()) {
    FunMgr::setError(QStringLiteral("readJson() requires a file path argument"));
    return QJsonObject();
  }

  QFile f(args[0].toString());
  if (!f.open(QIODevice::ReadOnly)) {
    FunMgr::setError(QStringLiteral("readJson() cannot open file: '%1'").arg(args[0].toString()));
    return QJsonObject();
  }

  QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) {
    FunMgr::setError(QStringLiteral("readJson() file is not a valid JSON object"));
    return QJsonValue();
  }
  return QJsonValue(doc.object());
}