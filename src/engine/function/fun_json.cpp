/**
 * @file fun_json.cpp
 * @brief JSON 函数实现
 */

#include "fun_json.h"
#include "../ac_language.h"
#include "fun_mgr.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

// init — 注册 JSON 函数到 FunMgr（builtin 伪类）
void FunJson::init() {
  FunMgr::ins().registerFuncs(
      QStringLiteral("builtin"),
      {{QString::fromLatin1(AcBuiltin::kReadJson), readJson}});
}

// readJson — 读取 JSON 文件
QJsonValue FunJson::readJson(const QJsonArray &args) {
  if (args.isEmpty() || !args[0].isString())
    return QJsonObject();

  QFile f(args[0].toString());
  if (!f.open(QIODevice::ReadOnly))
    return QJsonObject();

  QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  return doc.isObject() ? QJsonValue(doc.object()) : QJsonValue();
}