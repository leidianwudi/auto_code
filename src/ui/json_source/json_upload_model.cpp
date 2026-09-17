/**
 * @file json_upload_model.cpp
 * @brief .jsonupload 文件数据模型实现
 */

#include "json_upload_model.h"

#include <QJsonDocument>

// ════════════════════════════════════════════════════════════
//  JsonUploadParam
// ════════════════════════════════════════════════════════════

QJsonObject JsonUploadParam::toJson() const {
  QJsonObject obj;
  obj[QString::fromLatin1(JsonUploadKey::kName)] = name;
  // value 始终存字符串保真；数字类型额外写 valueType 标记
  obj[QString::fromLatin1(JsonUploadKey::kValue)] = value;
  if (valueType == QStringLiteral("number")) {
    obj[QString::fromLatin1(JsonUploadKey::kValueType)] = valueType;
  }
  return obj;
}

JsonUploadParam JsonUploadParam::fromJson(const QJsonObject &obj) {
  JsonUploadParam p;
  p.name = obj.value(QString::fromLatin1(JsonUploadKey::kName)).toString();
  const QJsonValue v = obj.value(QString::fromLatin1(JsonUploadKey::kValue));
  // value 兼容旧数据里存数字的情况（Qt6 的 toString 不转换数值，需显式处理）
  if (v.isDouble()) {
    p.value = QString::number(v.toDouble());
    p.valueType = obj.value(QString::fromLatin1(JsonUploadKey::kValueType)).toString();
    // 旧数据 value 直接存数字 → 视为数字类型
    if (p.valueType.isEmpty()) p.valueType = QStringLiteral("number");
  } else if (v.isString()) {
    p.value = v.toString();
    p.valueType = obj.value(QString::fromLatin1(JsonUploadKey::kValueType)).toString();
  } else {
    p.value = v.toVariant().toString();
  }
  return p;
}

// ════════════════════════════════════════════════════════════
//  JsonUpload
// ════════════════════════════════════════════════════════════

QJsonObject JsonUpload::toJson() const {
  QJsonObject obj;
  obj[QString::fromLatin1(JsonUploadKey::kId)] = id;
  obj[QString::fromLatin1(JsonUploadKey::kRemark)] = remark;
  obj[QString::fromLatin1(JsonUploadKey::kUrl)] = url;
  obj[QString::fromLatin1(JsonUploadKey::kMethod)] = method;
  obj[QString::fromLatin1(JsonUploadKey::kFileField)] = fileField;

  if (!params.isEmpty()) {
    QJsonArray paramArr;
    for (const auto &p : params) paramArr.append(p.toJson());
    obj[QString::fromLatin1(JsonUploadKey::kParams)] = paramArr;
  }

  obj[QString::fromLatin1(JsonUploadKey::kResponsePath)] = responsePath;
  obj[QString::fromLatin1(JsonUploadKey::kMaxCount)] = maxCount;
  // valueType 仅非空（显式覆盖）时写出
  if (!valueType.isEmpty()) {
    obj[QString::fromLatin1(JsonUploadKey::kValueType)] = valueType;
  }
  return obj;
}

JsonUpload JsonUpload::fromJson(const QJsonObject &obj) {
  JsonUpload u;
  u.id = obj.value(QString::fromLatin1(JsonUploadKey::kId)).toString();
  u.remark = obj.value(QString::fromLatin1(JsonUploadKey::kRemark)).toString();
  u.url = obj.value(QString::fromLatin1(JsonUploadKey::kUrl)).toString();
  u.method = obj.value(QString::fromLatin1(JsonUploadKey::kMethod))
                 .toString(QStringLiteral("POST"));
  u.fileField = obj.value(QString::fromLatin1(JsonUploadKey::kFileField))
                    .toString(QStringLiteral("file"));

  const QJsonArray paramArr = obj.value(QString::fromLatin1(JsonUploadKey::kParams)).toArray();
  for (const auto &v : paramArr) {
    if (v.isObject()) u.params.append(JsonUploadParam::fromJson(v.toObject()));
  }

  u.responsePath = obj.value(QString::fromLatin1(JsonUploadKey::kResponsePath))
                       .toString(QStringLiteral("data.url"));
  u.maxCount = obj.value(QString::fromLatin1(JsonUploadKey::kMaxCount)).toInt(1);
  if (u.maxCount < 1) u.maxCount = 1;
  u.valueType = obj.value(QString::fromLatin1(JsonUploadKey::kValueType)).toString();
  return u;
}

// ════════════════════════════════════════════════════════════
//  JsonUploadConfig
// ════════════════════════════════════════════════════════════

int JsonUploadConfig::indexOfUpload(const QString &id) const {
  for (int i = 0; i < uploads.size(); ++i) {
    if (uploads[i].id == id) return i;
  }
  return -1;
}

const JsonUpload *JsonUploadConfig::uploadById(const QString &id) const {
  const int idx = indexOfUpload(id);
  return idx >= 0 ? &uploads[idx] : nullptr;
}

QJsonObject JsonUploadConfig::toJsonObject() const {
  QJsonObject root;
  QJsonArray arr;
  for (const auto &u : uploads) arr.append(u.toJson());
  root[QString::fromLatin1(JsonUploadKey::kUploads)] = arr;
  return root;
}

QString JsonUploadConfig::toJsonString() const { return toJsonString(toJsonObject()); }

QString JsonUploadConfig::toJsonString(const QJsonObject &root) {
  return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

JsonUploadConfig JsonUploadConfig::fromJson(const QJsonObject &obj) {
  JsonUploadConfig cfg;
  const QJsonArray arr = obj.value(QString::fromLatin1(JsonUploadKey::kUploads)).toArray();
  for (const auto &v : arr) {
    if (v.isObject()) cfg.uploads.append(JsonUpload::fromJson(v.toObject()));
  }
  return cfg;
}

JsonUploadConfig JsonUploadConfig::fromJsonString(const QString &jsonStr, QString *error) {
  Q_UNUSED(error)  // 与 JsonSourceConfig 保持一致；解析失败时返回空配置
  QJsonParseError perr;
  QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &perr);
  if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
    return JsonUploadConfig();
  }
  return fromJson(doc.object());
}
