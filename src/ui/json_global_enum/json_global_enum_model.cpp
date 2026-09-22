/**
 * @file json_global_enum_model.cpp
 * @brief .jsonglobalenum 文件数据模型实现
 */

#include "json_global_enum_model.h"

#include <QJsonDocument>
#include <QJsonParseError>

// ════════════════════════════════════════════════════════════
//  JsonGlobalEnumOption
// ════════════════════════════════════════════════════════════

QJsonObject JsonGlobalEnumOption::toJson() const {
  QJsonObject obj;
  obj[QString::fromLatin1(JsonGlobalEnumKey::kKey)] = key;
  obj[QString::fromLatin1(JsonGlobalEnumKey::kLabel)] = label;
  // value 始终存字符串保真；数字类型额外写 valueType 标记
  obj[QString::fromLatin1(JsonGlobalEnumKey::kValue)] = value;
  if (valueType == QStringLiteral("number")) {
    obj[QString::fromLatin1(JsonGlobalEnumKey::kValueType)] = valueType;
  }
  return obj;
}

JsonGlobalEnumOption JsonGlobalEnumOption::fromJson(const QJsonObject &obj) {
  JsonGlobalEnumOption o;
  o.key = obj.value(QString::fromLatin1(JsonGlobalEnumKey::kKey)).toString();
  o.label = obj.value(QString::fromLatin1(JsonGlobalEnumKey::kLabel)).toString();
  const QJsonValue v = obj.value(QString::fromLatin1(JsonGlobalEnumKey::kValue));
  // value 兼容旧数据里存数字的情况（Qt6 的 toString 不转换数值，需显式处理）
  if (v.isDouble()) {
    o.value = QString::number(v.toDouble());
    o.valueType = obj.value(QString::fromLatin1(JsonGlobalEnumKey::kValueType)).toString();
    // 旧数据 value 直接存数字 → 视为数字类型
    if (o.valueType.isEmpty()) o.valueType = QStringLiteral("number");
  } else if (v.isString()) {
    o.value = v.toString();
    o.valueType = obj.value(QString::fromLatin1(JsonGlobalEnumKey::kValueType)).toString();
  } else {
    o.value = v.toVariant().toString();
  }
  return o;
}

// ════════════════════════════════════════════════════════════
//  JsonGlobalEnum
// ════════════════════════════════════════════════════════════

QJsonObject JsonGlobalEnum::toJson() const {
  QJsonObject obj;
  if (!id.isEmpty()) obj[QString::fromLatin1(JsonGlobalEnumKey::kId)] = id;
  obj[QString::fromLatin1(JsonGlobalEnumKey::kName)] = name;
  obj[QString::fromLatin1(JsonGlobalEnumKey::kRemark)] = remark;
  QJsonArray arr;
  for (const auto &o : options) arr.append(o.toJson());
  obj[QString::fromLatin1(JsonGlobalEnumKey::kOptions)] = arr;
  return obj;
}

JsonGlobalEnum JsonGlobalEnum::fromJson(const QJsonObject &obj) {
  JsonGlobalEnum e;
  e.id = obj.value(QString::fromLatin1(JsonGlobalEnumKey::kId)).toString();
  e.name = obj.value(QString::fromLatin1(JsonGlobalEnumKey::kName)).toString();
  e.remark = obj.value(QString::fromLatin1(JsonGlobalEnumKey::kRemark)).toString();
  const QJsonArray arr = obj.value(QString::fromLatin1(JsonGlobalEnumKey::kOptions)).toArray();
  for (const auto &v : arr) {
    if (v.isObject()) e.options.append(JsonGlobalEnumOption::fromJson(v.toObject()));
  }
  return e;
}

// ════════════════════════════════════════════════════════════
//  JsonGlobalEnumConfig
// ════════════════════════════════════════════════════════════

QJsonObject JsonGlobalEnumConfig::toJsonObject() const {
  QJsonObject root;
  QJsonArray arr;
  for (const auto &e : enums) arr.append(e.toJson());
  root[QString::fromLatin1(JsonGlobalEnumKey::kEnums)] = arr;
  return root;
}

QString JsonGlobalEnumConfig::toJsonString() const { return toJsonString(toJsonObject()); }

QString JsonGlobalEnumConfig::toJsonString(const QJsonObject &root) {
  return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

JsonGlobalEnumConfig JsonGlobalEnumConfig::fromJson(const QJsonObject &obj) {
  JsonGlobalEnumConfig cfg;
  const QJsonArray arr = obj.value(QString::fromLatin1(JsonGlobalEnumKey::kEnums)).toArray();
  for (const auto &v : arr) {
    if (v.isObject()) cfg.enums.append(JsonGlobalEnum::fromJson(v.toObject()));
  }
  return cfg;
}

JsonGlobalEnumConfig JsonGlobalEnumConfig::fromJsonString(const QString &jsonStr) {
  QJsonParseError perr;
  QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &perr);
  if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
    return JsonGlobalEnumConfig();
  }
  return fromJson(doc.object());
}
