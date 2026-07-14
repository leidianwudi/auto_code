/**
 * @file fun_json.cpp
 * @brief JSON 函数实现
 */

#include "fun_json.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

#include "../ac_language.h"
#include "fun_mgr.h"
#include "src/tool/common/tool_json.h"

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

  QJsonParseError parseError;
  QJsonDocument doc = ToolJson::loadFile(args[0].toString(), &parseError);
  if (parseError.error != QJsonParseError::NoError || doc.isNull()) {
    FunMgr::setError(
        QStringLiteral("readJson() cannot open or parse file: '%1'").arg(args[0].toString()));
    return QJsonObject();
  }

  if (!doc.isObject()) {
    FunMgr::setError(QStringLiteral("readJson() file is not a valid JSON object"));
    return QJsonValue();
  }
  return QJsonValue(doc.object());
}