/**
 * @file json_source_model.cpp
 * @brief .jsonsource 文件数据模型实现
 */

#include "json_source_model.h"

#include <QJsonDocument>

// ════════════════════════════════════════════════════════════
//  JsonSourceOption
// ════════════════════════════════════════════════════════════

QJsonObject JsonSourceOption::toJson() const {
  QJsonObject obj;
  obj[QString::fromLatin1(JsonSourceKey::kLabel)] = label;
  // value 始终存字符串保真；数字类型额外写 valueType 标记
  obj[QString::fromLatin1(JsonSourceKey::kValue)] = value;
  if (valueType == QStringLiteral("number")) {
    obj[QString::fromLatin1(JsonSourceKey::kValueType)] = valueType;
  }
  return obj;
}

JsonSourceOption JsonSourceOption::fromJson(const QJsonObject &obj) {
  JsonSourceOption o;
  o.label = obj.value(QString::fromLatin1(JsonSourceKey::kLabel)).toString();
  const QJsonValue v = obj.value(QString::fromLatin1(JsonSourceKey::kValue));
  // value 兼容旧数据里存数字的情况（Qt6 的 toString 不转换数值，需显式处理）
  if (v.isDouble()) {
    o.value = QString::number(v.toDouble());
    o.valueType = obj.value(QString::fromLatin1(JsonSourceKey::kValueType)).toString();
    // 旧数据 value 直接存数字 → 视为数字类型
    if (o.valueType.isEmpty()) o.valueType = QStringLiteral("number");
  } else if (v.isString()) {
    o.value = v.toString();
    o.valueType = obj.value(QString::fromLatin1(JsonSourceKey::kValueType)).toString();
  } else {
    o.value = v.toVariant().toString();
  }
  return o;
}

// ════════════════════════════════════════════════════════════
//  JsonSource
// ════════════════════════════════════════════════════════════

QJsonObject JsonSource::toJson() const {
  QJsonObject obj;
  obj[QString::fromLatin1(JsonSourceKey::kId)] = id;
  obj[QString::fromLatin1(JsonSourceKey::kType)] = type;
  obj[QString::fromLatin1(JsonSourceKey::kRemark)] = remark;

  if (isDynamic()) {
    obj[QString::fromLatin1(JsonSourceKey::kUrl)] = url;
    obj[QString::fromLatin1(JsonSourceKey::kMethod)] = method;
    obj[QString::fromLatin1(JsonSourceKey::kLabelField)] = labelField;
    obj[QString::fromLatin1(JsonSourceKey::kValueField)] = valueField;
    if (paged) {
      obj[QString::fromLatin1(JsonSourceKey::kPaged)] = true;
      obj[QString::fromLatin1(JsonSourceKey::kPageKey)] = pageKey;
      obj[QString::fromLatin1(JsonSourceKey::kPageSizeKey)] = pageSizeKey;
      obj[QString::fromLatin1(JsonSourceKey::kPageSize)] = pageSize;
      obj[QString::fromLatin1(JsonSourceKey::kSearchTitle)] = searchTitle;
      obj[QString::fromLatin1(JsonSourceKey::kSearchField)] = searchField;
    }
  } else {
    QJsonArray arr;
    for (const auto &o : options) arr.append(o.toJson());
    obj[QString::fromLatin1(JsonSourceKey::kOptions)] = arr;
    // 静态数据源也存 url（作为生成函数名的依据，如 booleanStatic + url名）
    if (!url.isEmpty()) obj[QString::fromLatin1(JsonSourceKey::kUrl)] = url;
  }

  return obj;
}

JsonSource JsonSource::fromJson(const QJsonObject &obj) {
  JsonSource s;
  s.id = obj.value(QString::fromLatin1(JsonSourceKey::kId)).toString();
  s.type = obj.value(QString::fromLatin1(JsonSourceKey::kType))
               .toString(QString::fromLatin1(JsonSourceType::kDynamic));
  s.remark = obj.value(QString::fromLatin1(JsonSourceKey::kRemark)).toString();

  if (s.isDynamic()) {
    s.url = obj.value(QString::fromLatin1(JsonSourceKey::kUrl)).toString();
    s.method = obj.value(QString::fromLatin1(JsonSourceKey::kMethod))
                   .toString(QString::fromLatin1("POST"));
    s.labelField = obj.value(QString::fromLatin1(JsonSourceKey::kLabelField)).toString();
    s.valueField = obj.value(QString::fromLatin1(JsonSourceKey::kValueField)).toString();
    s.paged = obj.value(QString::fromLatin1(JsonSourceKey::kPaged)).toBool(false);
    s.pageKey = obj.value(QString::fromLatin1(JsonSourceKey::kPageKey))
                    .toString(QStringLiteral("page"));
    s.pageSizeKey = obj.value(QString::fromLatin1(JsonSourceKey::kPageSizeKey))
                        .toString(QStringLiteral("pageSize"));
    s.pageSize = obj.value(QString::fromLatin1(JsonSourceKey::kPageSize)).toInt(20);
    s.searchTitle = obj.value(QString::fromLatin1(JsonSourceKey::kSearchTitle)).toString();
    s.searchField = obj.value(QString::fromLatin1(JsonSourceKey::kSearchField)).toString();
  } else {
    const QJsonArray arr = obj.value(QString::fromLatin1(JsonSourceKey::kOptions)).toArray();
    for (const auto &v : arr) {
      if (v.isObject()) s.options.append(JsonSourceOption::fromJson(v.toObject()));
    }
    s.url = obj.value(QString::fromLatin1(JsonSourceKey::kUrl)).toString();
  }

  return s;
}

// ════════════════════════════════════════════════════════════
//  JsonSourceConfig
// ════════════════════════════════════════════════════════════

int JsonSourceConfig::indexOfSource(const QString &id) const {
  for (int i = 0; i < sources.size(); ++i) {
    if (sources[i].id == id) return i;
  }
  return -1;
}

const JsonSource *JsonSourceConfig::sourceById(const QString &id) const {
  const int idx = indexOfSource(id);
  return idx >= 0 ? &sources[idx] : nullptr;
}

QJsonObject JsonSourceConfig::toJsonObject() const {
  QJsonObject root;
  QJsonArray arr;
  for (const auto &s : sources) arr.append(s.toJson());
  root[QString::fromLatin1(JsonSourceKey::kSources)] = arr;
  return root;
}

QString JsonSourceConfig::toJsonString() const { return toJsonString(toJsonObject()); }

QString JsonSourceConfig::toJsonString(const QJsonObject &root) {
  return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

JsonSourceConfig JsonSourceConfig::fromJson(const QJsonObject &obj) {
  JsonSourceConfig cfg;
  const QJsonArray arr = obj.value(QString::fromLatin1(JsonSourceKey::kSources)).toArray();
  for (const auto &v : arr) {
    if (v.isObject()) cfg.sources.append(JsonSource::fromJson(v.toObject()));
  }
  return cfg;
}

JsonSourceConfig JsonSourceConfig::fromJsonString(const QString &jsonStr, QString *error) {
  Q_UNUSED(error)  // 与 JsonVueConfig 保持一致；解析失败时返回空配置
  QJsonParseError perr;
  QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &perr);
  if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
    return JsonSourceConfig();
  }
  return fromJson(doc.object());
}
