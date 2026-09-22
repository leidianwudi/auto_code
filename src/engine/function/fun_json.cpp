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
                              {{QString::fromLatin1(AcBuiltin::kReadJson), readJson},
                               {QString::fromLatin1(AcBuiltin::kToJsonString), toJsonString}});
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

// toJsonString — 把对象/数组序列化为紧凑 JSON 字符串
//   与 readJson 成对（读文件 ↔ 序列化写出），供 .ac 脚本把内存对象写回
//   JSON 配置文件（如全局枚举同步生成 .jsonsource）；转义由 QJsonDocument
//   统一处理，避免脚本侧手工拼 JSON 字符串的转义问题
accore::AcJsonValue FunJson::toJsonString(const accore::AcJsonValue &args) {
  if (!FunArgs::requireCount(args, 1,
                              QStringLiteral("toJsonString() requires 1 argument: object or array"))) {
    return accore::AcJsonValue();
  }
  const accore::AcJsonValue &v = args.at(0);
  if (!v.isObject() && !v.isArray()) {
    FunMgr::setError(QStringLiteral("toJsonString() argument must be an object or array"));
    return accore::AcJsonValue();
  }
  // toQJsonValue 为序列化边界（QJson 序列化器），与 readJson 的解析边界对称
  const QJsonValue jv = v.toQJsonValue();
  const QJsonDocument doc(jv.isArray() ? QJsonDocument(jv.toArray()) : QJsonDocument(jv.toObject()));
  return accore::AcJsonValue(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}
