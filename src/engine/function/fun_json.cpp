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
#include "fun_args.h"
#include "fun_mgr.h"
#include "src/util/common/util_json.h"

// init — 注册 JSON 函数到 FunMgr（builtin 伪类）
void FunJson::init() {
  FunMgr::ins().registerFuncs(QString::fromLatin1(AcRuntime::kBuiltinClass),
                              {{QString::fromLatin1(AcBuiltin::kReadJson), readJson}});
}

// readJson — 读取 JSON 文件
accore::AcJsonValue FunJson::readJson(const accore::AcJsonValue &args) {
  if (!FunArgs::requireString(args, 0,
                              QStringLiteral("readJson() requires a file path argument")))
    return accore::AcJsonValue();

  QJsonParseError parseError;
  QJsonDocument doc = UtilJson::loadFile(args.at(0).toString(), &parseError);
  if (parseError.error != QJsonParseError::NoError || doc.isNull()) {
    FunMgr::setError(
        QStringLiteral("readJson() cannot open or parse file: '%1'").arg(args.at(0).toString()));
    return accore::AcJsonValue();
  }

  if (!doc.isObject() && !doc.isArray()) {
    FunMgr::setError(QStringLiteral("readJson() file is not a valid JSON object or array"));
    return accore::AcJsonValue();
  }
  // UtilJson/QJsonDocument 为解析边界（QJson 解析器），此处一次性转入 accore 值模型
  if (doc.isArray()) return accore::AcJsonValue::fromQJsonValue(doc.array());
  return accore::AcJsonValue::fromQJsonValue(doc.object());
}
