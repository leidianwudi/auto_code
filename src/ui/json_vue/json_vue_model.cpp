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
    case EditStyle::Select:
      return QStringLiteral("select");
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
  // 兼容旧名称 "combobox"
  if (s == QStringLiteral("select") || s == QStringLiteral("combobox")) return EditStyle::Select;
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
  switch (s) {
    case QueryInputStyle::Date:
      return QStringLiteral("date");
    case QueryInputStyle::Select:
      return QStringLiteral("select");
    default:
      return QStringLiteral("text");
  }
}

QueryInputStyle stringToQueryInputStyle(const QString &s) {
  // 兼容旧名称 "time"
  if (s == QStringLiteral("date") || s == QStringLiteral("time")) return QueryInputStyle::Date;
  if (s == QStringLiteral("select")) return QueryInputStyle::Select;
  return QueryInputStyle::Text;
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
  obj["noDetail"] = noDetail;
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
  m.noDetail = obj.value("noDetail").toBool(false);
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
  // 下拉框配置（仅 Select 时序列化）
  if (editStyle == EditStyle::Select) {
    obj["selectUrl"] = selectUrl;
    obj["selectValueField"] = selectValueField;
    obj["selectLabelField"] = selectLabelField;
  }
  // 样式特定配置
  if (editStyle == EditStyle::Text) {
    obj["placeholder"] = placeholder;
    obj["maxlength"] = maxlength;
  }
  if (editStyle == EditStyle::Int) {
    obj["minValue"] = minValue;
    obj["maxValue"] = maxValue;
  }
  if (editStyle == EditStyle::Float) {
    obj["precision"] = precision;
    obj["minValue"] = minValue;
    obj["maxValue"] = maxValue;
  }
  if (editStyle == EditStyle::Date) {
    obj["dateFormat"] = dateFormat;
  }
  if (editStyle == EditStyle::TextArea) {
    obj["placeholder"] = placeholder;
    obj["textareaRows"] = textareaRows;
  }
  // 通用配置（所有样式都输出）
  obj["required"] = required;
  obj["columnWidth"] = columnWidth;
  if (!columnFixed.isEmpty()) obj["columnFixed"] = columnFixed;
  if (!formatter.isEmpty()) obj["formatter"] = formatter;
  if (formSpan != 24) obj["formSpan"] = formSpan;
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
  // 兼容旧字段名 comboboxUrl/comboboxValueField/comboboxLabelField
  c.selectUrl = obj.value("selectUrl").toString(obj.value("comboboxUrl").toString());
  c.selectValueField =
      obj.value("selectValueField").toString(obj.value("comboboxValueField").toString());
  c.selectLabelField =
      obj.value("selectLabelField").toString(obj.value("comboboxLabelField").toString());
  // 样式特定配置
  c.placeholder = obj.value("placeholder").toString();
  c.maxlength = obj.value("maxlength").toInt(0);
  c.minValue = obj.value("minValue").toDouble(0);
  c.maxValue = obj.value("maxValue").toDouble(0);
  c.precision = obj.value("precision").toInt(2);
  c.dateFormat = obj.value("dateFormat").toString();
  c.textareaRows = obj.value("textareaRows").toInt(3);
  // 通用配置
  c.required = obj.value("required").toBool(false);
  c.columnWidth = obj.value("columnWidth").toInt(0);
  c.columnFixed = obj.value("columnFixed").toString();
  c.formatter = obj.value("formatter").toString();
  c.formSpan = obj.value("formSpan").toInt(24);
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
  // 下拉框配置（仅 Select 时序列化）
  if (inputStyle == QueryInputStyle::Select) {
    obj["selectUrl"] = selectUrl;
    obj["selectValueField"] = selectValueField;
    obj["selectLabelField"] = selectLabelField;
  }
  if (inputStyle == QueryInputStyle::Text) {
    obj["placeholder"] = placeholder;
  }
  if (inputStyle == QueryInputStyle::Date) {
    obj["dateFormat"] = dateFormat;
  }
  return obj;
}

QueryFieldConfig QueryFieldConfig::fromJson(const QJsonObject &obj) {
  QueryFieldConfig q;
  q.displayName = obj.value("displayName").toString();
  q.dataName = obj.value("dataName").toString();
  q.inputStyle = stringToQueryInputStyle(obj.value("inputStyle").toString(QStringLiteral("text")));
  q.relation = stringToQueryRelation(obj.value("relation").toString(QStringLiteral("=")));
  q.selectUrl = obj.value("selectUrl").toString();
  q.selectValueField = obj.value("selectValueField").toString();
  q.selectLabelField = obj.value("selectLabelField").toString();
  q.placeholder = obj.value("placeholder").toString();
  q.dateFormat = obj.value("dateFormat").toString();
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
    // 下拉框配置（仅 Select 时输出）
    if (col.editStyle == EditStyle::Select) {
      out += QStringLiteral("      selectLabelField: '%1',\n").arg(esc(col.selectLabelField));
      out += QStringLiteral("      selectUrl: '%1',\n").arg(esc(col.selectUrl));
      out += QStringLiteral("      selectValueField: '%1',\n").arg(esc(col.selectValueField));
    }
    out += QStringLiteral("      dataName: '%1',\n").arg(esc(col.dataName));
    out += QStringLiteral("      editEditable: %1,\n").arg(boolStr(col.editEditable));
    out += QStringLiteral("      editName: '%1',\n").arg(esc(col.editName));
    out += QStringLiteral("      editStyle: '%1',\n").arg(editStyleToString(col.editStyle));
    out += QStringLiteral("      editVisible: %1,\n").arg(boolStr(col.editVisible));
    out += QStringLiteral("      queryName: '%1',\n").arg(esc(col.queryName));
    out += QStringLiteral("      queryStyle: '%1',\n").arg(listStyleToString(col.queryStyle));
    out += QStringLiteral("      queryVisible: %1,\n").arg(boolStr(col.queryVisible));
    out += QStringLiteral("      switchEditable: %1,\n").arg(boolStr(col.switchEditable));
    // 样式特定配置（按字母序）
    if (!col.columnFixed.isEmpty()) {
      out += QStringLiteral("      columnFixed: '%1',\n").arg(esc(col.columnFixed));
    }
    if (col.columnWidth > 0) {
      out += QStringLiteral("      columnWidth: %1,\n").arg(col.columnWidth);
    }
    if (col.editStyle == EditStyle::Date && !col.dateFormat.isEmpty()) {
      out += QStringLiteral("      dateFormat: '%1',\n").arg(esc(col.dateFormat));
    }
    if (col.formSpan != 24) {
      out += QStringLiteral("      formSpan: %1,\n").arg(col.formSpan);
    }
    if (!col.formatter.isEmpty()) {
      out += QStringLiteral("      formatter: '%1',\n").arg(esc(col.formatter));
    }
    if (col.editStyle == EditStyle::Text && col.maxlength > 0) {
      out += QStringLiteral("      maxlength: %1,\n").arg(col.maxlength);
    }
    if ((col.editStyle == EditStyle::Int || col.editStyle == EditStyle::Float) &&
        col.maxValue != 0) {
      out += QStringLiteral("      maxValue: %1,\n").arg(col.maxValue);
    }
    if ((col.editStyle == EditStyle::Int || col.editStyle == EditStyle::Float) &&
        col.minValue != 0) {
      out += QStringLiteral("      minValue: %1,\n").arg(col.minValue);
    }
    if ((col.editStyle == EditStyle::Text || col.editStyle == EditStyle::TextArea) &&
        !col.placeholder.isEmpty()) {
      out += QStringLiteral("      placeholder: '%1',\n").arg(esc(col.placeholder));
    }
    if (col.editStyle == EditStyle::Float && col.precision != 2) {
      out += QStringLiteral("      precision: %1,\n").arg(col.precision);
    }
    if (col.required) {
      out += QStringLiteral("      required: true,\n");
    }
    if (col.editStyle == EditStyle::TextArea && col.textareaRows != 3) {
      out += QStringLiteral("      textareaRows: %1,\n").arg(col.textareaRows);
    }
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
  out += QStringLiteral("    noDetail: %1,\n").arg(boolStr(meta.noDetail));
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
    // 下拉框配置（仅 Select 时输出）
    if (q.inputStyle == QueryInputStyle::Select) {
      out += QStringLiteral("      selectLabelField: '%1',\n").arg(esc(q.selectLabelField));
      out += QStringLiteral("      selectUrl: '%1',\n").arg(esc(q.selectUrl));
      out += QStringLiteral("      selectValueField: '%1',\n").arg(esc(q.selectValueField));
    }
    if (q.inputStyle == QueryInputStyle::Date && !q.dateFormat.isEmpty()) {
      out += QStringLiteral("      dateFormat: '%1',\n").arg(esc(q.dateFormat));
    }
    if (q.inputStyle == QueryInputStyle::Text && !q.placeholder.isEmpty()) {
      out += QStringLiteral("      placeholder: '%1',\n").arg(esc(q.placeholder));
    }
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
