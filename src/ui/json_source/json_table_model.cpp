/**
 * @file json_table_model.cpp
 * @brief .jsontable 文件数据模型实现
 */

#include "json_table_model.h"

#include <QJsonDocument>

#include "src/util/common/util_json.h"

// ════════════════════════════════════════════════════════════
//  JsonTableColumn
// ════════════════════════════════════════════════════════════

QJsonObject JsonTableColumn::toJsonObject() const {
  QJsonObject obj;
  obj[QString::fromLatin1(JsonTableKey::kName)] = name;
  obj[QString::fromLatin1(JsonTableKey::kMysqlType)] = mysqlType;
  obj[QString::fromLatin1(JsonTableKey::kUnsigned)] = isUnsigned;
  obj[QString::fromLatin1(JsonTableKey::kNullable)] = nullable;
  obj[QString::fromLatin1(JsonTableKey::kIsPrimary)] = isPrimary;
  obj[QString::fromLatin1(JsonTableKey::kIsAutoInc)] = isAutoInc;
  // default 空串表示无默认值，不落键（与 "default": null 语义等价）
  if (!defaultValue.isEmpty()) {
    obj[QString::fromLatin1(JsonTableKey::kDefault)] = defaultValue;
  }
  obj[QString::fromLatin1(JsonTableKey::kComment)] = comment;
  obj[QString::fromLatin1(JsonTableKey::kList)] = list;
  obj[QString::fromLatin1(JsonTableKey::kSearch)] = search;
  obj[QString::fromLatin1(JsonTableKey::kForm)] = form;
  obj[QString::fromLatin1(JsonTableKey::kRequired)] = required;
  return obj;
}

JsonTableColumn JsonTableColumn::fromJsonObject(const QJsonObject &obj) {
  JsonTableColumn c;
  c.name = obj.value(QString::fromLatin1(JsonTableKey::kName)).toString();
  c.mysqlType =
      obj.value(QString::fromLatin1(JsonTableKey::kMysqlType)).toString(QStringLiteral("bigint"));
  c.isUnsigned = obj.value(QString::fromLatin1(JsonTableKey::kUnsigned)).toBool(false);
  c.nullable = obj.value(QString::fromLatin1(JsonTableKey::kNullable)).toBool(false);
  c.isPrimary = obj.value(QString::fromLatin1(JsonTableKey::kIsPrimary)).toBool(false);
  c.isAutoInc = obj.value(QString::fromLatin1(JsonTableKey::kIsAutoInc)).toBool(false);
  // default 兼容数字字面量（Qt6 的 toString 不转换数值，需显式处理）；null/缺省=无默认值
  const QJsonValue dv = obj.value(QString::fromLatin1(JsonTableKey::kDefault));
  if (dv.isString()) {
    c.defaultValue = dv.toString();
  } else if (dv.isDouble()) {
    c.defaultValue = QString::number(dv.toDouble());
  }
  c.comment = obj.value(QString::fromLatin1(JsonTableKey::kComment)).toString();
  c.list = obj.value(QString::fromLatin1(JsonTableKey::kList)).toBool(true);
  c.search = obj.value(QString::fromLatin1(JsonTableKey::kSearch)).toBool(false);
  c.form = obj.value(QString::fromLatin1(JsonTableKey::kForm)).toString(QStringLiteral("input"));
  c.required = obj.value(QString::fromLatin1(JsonTableKey::kRequired)).toBool(false);
  return c;
}

bool JsonTableColumn::operator==(const JsonTableColumn &other) const {
  return name == other.name && mysqlType == other.mysqlType && isUnsigned == other.isUnsigned &&
         nullable == other.nullable && isPrimary == other.isPrimary &&
         isAutoInc == other.isAutoInc && defaultValue == other.defaultValue &&
         comment == other.comment && list == other.list && search == other.search &&
         form == other.form && required == other.required;
}

// ════════════════════════════════════════════════════════════
//  JsonTableIndex
// ════════════════════════════════════════════════════════════

QJsonObject JsonTableIndex::toJsonObject() const {
  QJsonObject obj;
  obj[QString::fromLatin1(JsonTableKey::kName)] = name;
  QJsonArray colArr;
  for (const QString &c : cols) colArr.append(c);
  obj[QString::fromLatin1(JsonTableKey::kCols)] = colArr;
  obj[QString::fromLatin1(JsonTableKey::kUnique)] = unique;
  return obj;
}

JsonTableIndex JsonTableIndex::fromJsonObject(const QJsonObject &obj) {
  JsonTableIndex ix;
  ix.name = obj.value(QString::fromLatin1(JsonTableKey::kName)).toString();
  const QJsonArray colArr = obj.value(QString::fromLatin1(JsonTableKey::kCols)).toArray();
  for (const auto &v : colArr) {
    if (v.isString()) ix.cols.append(v.toString());
  }
  ix.unique = obj.value(QString::fromLatin1(JsonTableKey::kUnique)).toBool(false);
  return ix;
}

bool JsonTableIndex::operator==(const JsonTableIndex &other) const {
  return name == other.name && cols == other.cols && unique == other.unique;
}

// ════════════════════════════════════════════════════════════
//  JsonTableTable
// ════════════════════════════════════════════════════════════

QJsonObject JsonTableTable::toJsonObject() const {
  // 未知键保真：先以 extra 铺底（i18n/globalEnumCols/joinTable 等原样写回），
  // 已知键随后覆盖写入（extra 中不含已知键，见 fromJsonObject）
  QJsonObject obj = extra;
  obj[QString::fromLatin1(JsonTableKey::kTableName)] = tableName;
  obj[QString::fromLatin1(JsonTableKey::kModelName)] = modelName;
  obj[QString::fromLatin1(JsonTableKey::kTableComment)] = tableComment;
  obj[QString::fromLatin1(JsonTableKey::kManualUpdateTime)] = manualUpdateTime;

  QJsonArray colArr;
  for (const auto &c : columns) colArr.append(c.toJsonObject());
  obj[QString::fromLatin1(JsonTableKey::kColumns)] = colArr;

  if (!indexes.isEmpty()) {
    QJsonArray idxArr;
    for (const auto &ix : indexes) idxArr.append(ix.toJsonObject());
    obj[QString::fromLatin1(JsonTableKey::kIndexes)] = idxArr;
  }
  return obj;
}

JsonTableTable JsonTableTable::fromJsonObject(const QJsonObject &obj) {
  JsonTableTable t;
  // 未知键保真：整体拷贝后剔除已知键，剩余即进阶节点/未来扩展键
  t.extra = obj;
  t.extra.remove(QString::fromLatin1(JsonTableKey::kTableName));
  t.extra.remove(QString::fromLatin1(JsonTableKey::kModelName));
  t.extra.remove(QString::fromLatin1(JsonTableKey::kTableComment));
  t.extra.remove(QString::fromLatin1(JsonTableKey::kManualUpdateTime));
  t.extra.remove(QString::fromLatin1(JsonTableKey::kColumns));
  t.extra.remove(QString::fromLatin1(JsonTableKey::kIndexes));

  t.tableName = obj.value(QString::fromLatin1(JsonTableKey::kTableName)).toString();
  t.modelName = obj.value(QString::fromLatin1(JsonTableKey::kModelName)).toString();
  t.tableComment = obj.value(QString::fromLatin1(JsonTableKey::kTableComment)).toString();
  t.manualUpdateTime =
      obj.value(QString::fromLatin1(JsonTableKey::kManualUpdateTime)).toBool(false);

  const QJsonArray colArr = obj.value(QString::fromLatin1(JsonTableKey::kColumns)).toArray();
  for (const auto &v : colArr) {
    if (v.isObject()) t.columns.append(JsonTableColumn::fromJsonObject(v.toObject()));
  }

  const QJsonArray idxArr = obj.value(QString::fromLatin1(JsonTableKey::kIndexes)).toArray();
  for (const auto &v : idxArr) {
    if (v.isObject()) t.indexes.append(JsonTableIndex::fromJsonObject(v.toObject()));
  }
  return t;
}

bool JsonTableTable::operator==(const JsonTableTable &other) const {
  return tableName == other.tableName && modelName == other.modelName &&
         tableComment == other.tableComment && manualUpdateTime == other.manualUpdateTime &&
         columns == other.columns && indexes == other.indexes && extra == other.extra;
}

// ════════════════════════════════════════════════════════════
//  JsonTableConfig
// ════════════════════════════════════════════════════════════

QJsonObject JsonTableConfig::toJsonObject() const {
  // meta：未知键保真铺底 + 已知键覆盖（extra 中不含已知键，见 fromJson）
  QJsonObject meta = extra;
  meta[QString::fromLatin1(JsonTableKey::kProject)] = metaProject;

  QJsonObject db;
  db[QString::fromLatin1(JsonTableKey::kHost)] = dbHost;
  db[QString::fromLatin1(JsonTableKey::kPort)] = dbPort;
  db[QString::fromLatin1(JsonTableKey::kUser)] = dbUser;
  db[QString::fromLatin1(JsonTableKey::kPassword)] = dbPassword;
  db[QString::fromLatin1(JsonTableKey::kDatabase)] = dbDatabase;
  meta[QString::fromLatin1(JsonTableKey::kDb)] = db;

  meta[QString::fromLatin1(JsonTableKey::kGoBasePath)] = goBasePath;
  meta[QString::fromLatin1(JsonTableKey::kWebBasePath)] = webBasePath;

  QJsonObject root;
  root[QString::fromLatin1(JsonTableKey::kMeta)] = meta;

  QJsonArray arr;
  for (const auto &t : tables) arr.append(t.toJsonObject());
  root[QString::fromLatin1(JsonTableKey::kTables)] = arr;
  return root;
}

QString JsonTableConfig::toJsonString() const { return toJsonString(toJsonObject()); }

QString JsonTableConfig::toJsonString(const QJsonObject &root) {
  return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

JsonTableConfig JsonTableConfig::fromJson(const QJsonObject &obj) {
  JsonTableConfig cfg;
  const QJsonObject meta = obj.value(QString::fromLatin1(JsonTableKey::kMeta)).toObject();
  // meta 下未知键保真：整体拷贝后剔除已知键
  cfg.extra = meta;
  cfg.extra.remove(QString::fromLatin1(JsonTableKey::kProject));
  cfg.extra.remove(QString::fromLatin1(JsonTableKey::kDb));
  cfg.extra.remove(QString::fromLatin1(JsonTableKey::kGoBasePath));
  cfg.extra.remove(QString::fromLatin1(JsonTableKey::kWebBasePath));

  cfg.metaProject = meta.value(QString::fromLatin1(JsonTableKey::kProject)).toString();

  const QJsonObject db = meta.value(QString::fromLatin1(JsonTableKey::kDb)).toObject();
  cfg.dbHost = db.value(QString::fromLatin1(JsonTableKey::kHost)).toString();
  cfg.dbPort = db.value(QString::fromLatin1(JsonTableKey::kPort)).toInt(3306);
  cfg.dbUser = db.value(QString::fromLatin1(JsonTableKey::kUser)).toString();
  cfg.dbPassword = db.value(QString::fromLatin1(JsonTableKey::kPassword)).toString();
  cfg.dbDatabase = db.value(QString::fromLatin1(JsonTableKey::kDatabase)).toString();

  cfg.goBasePath = meta.value(QString::fromLatin1(JsonTableKey::kGoBasePath)).toString();
  cfg.webBasePath = meta.value(QString::fromLatin1(JsonTableKey::kWebBasePath)).toString();

  const QJsonArray arr = obj.value(QString::fromLatin1(JsonTableKey::kTables)).toArray();
  for (const auto &v : arr) {
    if (v.isObject()) cfg.tables.append(JsonTableTable::fromJsonObject(v.toObject()));
  }
  return cfg;
}

JsonTableConfig JsonTableConfig::fromJsonString(const QString &jsonStr, QString *error) {
  Q_UNUSED(error)  // 与 JsonUploadConfig 保持一致；解析失败时返回空配置
  QJsonParseError perr;
  // JSON5 兼容解析（UtilJson）：手写/既有 .jsontable 可能含无引号键、单引号、注释、
  // 尾逗号，严格 QJsonDocument 会解析失败导致有数据的文件可视化显示为空
  QJsonDocument doc = UtilJson::fromJson(jsonStr, &perr);
  if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
    return JsonTableConfig();
  }
  return fromJson(doc.object());
}
