/**
 * @file json_vue_model.cpp
 * @brief .jsonvue 文件数据模型实现
 */

#include "json_vue_model.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

#include "src/util/common/util_json.h"

// ════════════════════════════════════════════════════════════
//  枚举转换
// ════════════════════════════════════════════════════════════

QString listStyleToString(ListStyle style) {
  return style == ListStyle::Switch ? QStringLiteral("switch") : QStringLiteral("text");
}

ListStyle stringToListStyle(const QString &s) {
  return s == QStringLiteral("switch") ? ListStyle::Switch : ListStyle::Text;
}

QString editStyleToString(EditStyle style) {
  switch (style) {
    case EditStyle::Int:
      return QStringLiteral("int");
    case EditStyle::Float:
      return QStringLiteral("float");
    case EditStyle::Date:
      return QStringLiteral("date");
    case EditStyle::ComboBox:
      return QStringLiteral("combobox");
    case EditStyle::TextArea:
      return QStringLiteral("textarea");
    default:
      return QStringLiteral("text");
  }
}

EditStyle stringToEditStyle(const QString &s) {
  if (s == QStringLiteral("int")) return EditStyle::Int;
  if (s == QStringLiteral("float")) return EditStyle::Float;
  if (s == QStringLiteral("date")) return EditStyle::Date;
  if (s == QStringLiteral("combobox")) return EditStyle::ComboBox;
  if (s == QStringLiteral("textarea")) return EditStyle::TextArea;
  return EditStyle::Text;
}

QString queryRelationToString(QueryRelation r) {
  switch (r) {
    case QueryRelation::Like:
      return QStringLiteral("like");
    case QueryRelation::GreaterEqual:
      return QStringLiteral(">=");
    case QueryRelation::LessEqual:
      return QStringLiteral("<=");
    case QueryRelation::Greater:
      return QStringLiteral(">");
    case QueryRelation::Less:
      return QStringLiteral("<");
    default:
      return QStringLiteral("=");
  }
}

QueryRelation stringToQueryRelation(const QString &s) {
  if (s == QStringLiteral("like")) return QueryRelation::Like;
  if (s == QStringLiteral(">=")) return QueryRelation::GreaterEqual;
  if (s == QStringLiteral("<=")) return QueryRelation::LessEqual;
  if (s == QStringLiteral(">")) return QueryRelation::Greater;
  if (s == QStringLiteral("<")) return QueryRelation::Less;
  return QueryRelation::Equal;
}

QString queryInputStyleToString(QueryInputStyle s) {
  return s == QueryInputStyle::Time ? QStringLiteral("time") : QStringLiteral("text");
}

QueryInputStyle stringToQueryInputStyle(const QString &s) {
  return s == QStringLiteral("time") ? QueryInputStyle::Time : QueryInputStyle::Text;
}

// ════════════════════════════════════════════════════════════
//  JsonVueMeta
// ════════════════════════════════════════════════════════════

QJsonObject JsonVueMeta::toJson() const {
  QJsonObject obj;
  obj["dataMethod"] = dataMethod;
  obj["dataUrl"] = dataUrl;
  obj["queryApi"] = queryApi;
  obj["deleteApi"] = deleteApi;
  obj["noDelete"] = noDelete;
  obj["updateApi"] = updateApi;
  obj["noEdit"] = noEdit;
  return obj;
}

JsonVueMeta JsonVueMeta::fromJson(const QJsonObject &obj) {
  JsonVueMeta m;
  m.dataMethod = obj.value("dataMethod").toString(QStringLiteral("GET"));
  m.dataUrl = obj.value("dataUrl").toString();
  m.queryApi = obj.value("queryApi").toString();
  m.deleteApi = obj.value("deleteApi").toString();
  m.noDelete = obj.value("noDelete").toBool(false);
  m.updateApi = obj.value("updateApi").toString();
  m.noEdit = obj.value("noEdit").toBool(false);
  return m;
}

// ════════════════════════════════════════════════════════════
//  ColumnConfig
// ════════════════════════════════════════════════════════════

QJsonObject ColumnConfig::toJson() const {
  QJsonObject obj;
  obj["dataName"] = dataName;
  obj["queryVisible"] = queryVisible;
  obj["queryName"] = queryName;
  obj["queryStyle"] = listStyleToString(queryStyle);
  obj["switchEditable"] = switchEditable;
  obj["editVisible"] = editVisible;
  obj["editName"] = editName;
  obj["editStyle"] = editStyleToString(editStyle);
  obj["editEditable"] = editEditable;
  return obj;
}

ColumnConfig ColumnConfig::fromJson(const QJsonObject &obj) {
  ColumnConfig c;
  c.dataName = obj.value("dataName").toString();
  c.queryVisible = obj.value("queryVisible").toBool(true);
  c.queryName = obj.value("queryName").toString();
  c.queryStyle = stringToListStyle(obj.value("queryStyle").toString(QStringLiteral("text")));
  c.switchEditable = obj.value("switchEditable").toBool(false);
  c.editVisible = obj.value("editVisible").toBool(true);
  c.editName = obj.value("editName").toString();
  c.editStyle = stringToEditStyle(obj.value("editStyle").toString(QStringLiteral("text")));
  c.editEditable = obj.value("editEditable").toBool(true);
  return c;
}

// ════════════════════════════════════════════════════════════
//  QueryFieldConfig
// ════════════════════════════════════════════════════════════

QJsonObject QueryFieldConfig::toJson() const {
  QJsonObject obj;
  obj["displayName"] = displayName;
  obj["dataName"] = dataName;
  obj["inputStyle"] = queryInputStyleToString(inputStyle);
  obj["relation"] = queryRelationToString(relation);
  return obj;
}

QueryFieldConfig QueryFieldConfig::fromJson(const QJsonObject &obj) {
  QueryFieldConfig q;
  q.displayName = obj.value("displayName").toString();
  q.dataName = obj.value("dataName").toString();
  q.inputStyle = stringToQueryInputStyle(obj.value("inputStyle").toString(QStringLiteral("text")));
  q.relation = stringToQueryRelation(obj.value("relation").toString(QStringLiteral("=")));
  return q;
}

// ════════════════════════════════════════════════════════════
//  JsonVueConfig
// ════════════════════════════════════════════════════════════

QJsonObject JsonVueConfig::toJsonObject() const {
  QJsonObject root;
  root["meta"] = meta.toJson();

  QJsonArray colArr;
  for (const auto &c : columns) colArr.append(c.toJson());
  root["columns"] = colArr;

  QJsonArray qArr;
  for (const auto &q : queryFields) qArr.append(q.toJson());
  root["queryFields"] = qArr;

  return root;
}

QString JsonVueConfig::toJsonString() const {
  // 生成 JSON5 格式（AutoCode 支持的 JSON 格式）
  // 键名无引号、字符串单引号、允许尾随逗号、键按字母序排列
  auto esc = [](const QString &s) {
    QString r = s;
    r.replace('\\', QStringLiteral("\\\\"));
    r.replace('\'', QStringLiteral("\\'"));
    r.replace('\n', QStringLiteral("\\n"));
    r.replace('\r', QStringLiteral("\\r"));
    r.replace('\t', QStringLiteral("\\t"));
    return r;
  };
  auto boolStr = [](bool b) { return b ? QStringLiteral("true") : QStringLiteral("false"); };

  QString out;
  out += "{\n";

  // columns（按字母序）
  out += QStringLiteral("  columns: [\n");
  for (const auto &col : columns) {
    out += QStringLiteral("    {\n");
    out += QStringLiteral("      dataName: '%1',\n").arg(esc(col.dataName));
    out += QStringLiteral("      editEditable: %1,\n").arg(boolStr(col.editEditable));
    out += QStringLiteral("      editName: '%1',\n").arg(esc(col.editName));
    out += QStringLiteral("      editStyle: '%1',\n").arg(editStyleToString(col.editStyle));
    out += QStringLiteral("      editVisible: %1,\n").arg(boolStr(col.editVisible));
    out += QStringLiteral("      queryName: '%1',\n").arg(esc(col.queryName));
    out += QStringLiteral("      queryStyle: '%1',\n").arg(listStyleToString(col.queryStyle));
    out += QStringLiteral("      queryVisible: %1,\n").arg(boolStr(col.queryVisible));
    out += QStringLiteral("      switchEditable: %1,\n").arg(boolStr(col.switchEditable));
    out += QStringLiteral("    },\n");
  }
  out += QStringLiteral("  ],\n");

  // meta（按字母序）
  out += QStringLiteral("  meta: {\n");
  out += QStringLiteral("    dataMethod: '%1',\n").arg(esc(meta.dataMethod));
  out += QStringLiteral("    dataUrl: '%1',\n").arg(esc(meta.dataUrl));
  out += QStringLiteral("    deleteApi: '%1',\n").arg(esc(meta.deleteApi));
  out += QStringLiteral("    noDelete: %1,\n").arg(boolStr(meta.noDelete));
  out += QStringLiteral("    noEdit: %1,\n").arg(boolStr(meta.noEdit));
  out += QStringLiteral("    queryApi: '%1',\n").arg(esc(meta.queryApi));
  out += QStringLiteral("    updateApi: '%1',\n").arg(esc(meta.updateApi));
  out += QStringLiteral("  },\n");

  // queryFields（按字母序）
  out += QStringLiteral("  queryFields: [\n");
  for (const auto &q : queryFields) {
    out += QStringLiteral("    {\n");
    out += QStringLiteral("      dataName: '%1',\n").arg(esc(q.dataName));
    out += QStringLiteral("      displayName: '%1',\n").arg(esc(q.displayName));
    out += QStringLiteral("      inputStyle: '%1',\n").arg(queryInputStyleToString(q.inputStyle));
    out += QStringLiteral("      relation: '%1',\n").arg(queryRelationToString(q.relation));
    out += QStringLiteral("    },\n");
  }
  out += QStringLiteral("  ],\n");

  out += "}\n";
  return out;
}

JsonVueConfig JsonVueConfig::fromJson(const QJsonObject &obj) {
  JsonVueConfig cfg;
  cfg.meta = JsonVueMeta::fromJson(obj.value("meta").toObject());

  const QJsonArray colArr = obj.value("columns").toArray();
  for (const auto &v : colArr) {
    cfg.columns.append(ColumnConfig::fromJson(v.toObject()));
  }

  const QJsonArray qArr = obj.value("queryFields").toArray();
  for (const auto &v : qArr) {
    cfg.queryFields.append(QueryFieldConfig::fromJson(v.toObject()));
  }
  return cfg;
}

JsonVueConfig JsonVueConfig::fromJsonString(const QString &jsonStr, QString *error) {
  if (jsonStr.trimmed().isEmpty()) {
    return JsonVueConfig{};  // 空文件返回默认配置
  }

  QJsonParseError parseErr;
  QJsonDocument doc = UtilJson::fromJson(jsonStr, &parseErr);
  if (parseErr.error != QJsonParseError::NoError) {
    if (error) *error = parseErr.errorString();
    return JsonVueConfig{};
  }
  return fromJson(doc.object());
}
