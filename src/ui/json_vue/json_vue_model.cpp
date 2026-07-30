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
//  JSON5 序列化器（通用）
//  将 QJsonObject/QJsonArray 转换为 JSON5 格式字符串：
//    - 键名无引号、字符串单引号、允许尾随逗号
//    - 键按字母序排列、2 空格缩进
//    - 跳过空字符串和空数组，使输出更简洁
// ════════════════════════════════════════════════════════════

/// 转义 JSON5 单引号字符串中的特殊字符
static QString json5EscapeStr(const QString &s) {
  QString r = s;
  r.replace('\\', QStringLiteral("\\\\"));
  r.replace('\'', QStringLiteral("\\'"));
  r.replace('\n', QStringLiteral("\\n"));
  r.replace('\r', QStringLiteral("\\r"));
  r.replace('\t', QStringLiteral("\\t"));
  return r;
}

/// 将 QJsonValue 转换为 JSON5 字符串（返回空 QString 表示该字段应跳过）
static QString jsonValueToJson5(const QJsonValue &v, int indent);

/// 将 QJsonObject 转换为 JSON5 字符串（键按字母序，2 空格缩进）
static QString jsonObjectToJson5(const QJsonObject &obj, int indent) {
  if (obj.isEmpty()) return QStringLiteral("{}");
  QString pad = QString(QStringLiteral(" ")).repeated(indent);
  QString pad2 = QString(QStringLiteral(" ")).repeated(indent + 2);
  QStringList keys = obj.keys();
  keys.sort();
  QString out = "{\n";
  bool hasField = false;
  for (const auto &k : keys) {
    QString valStr = jsonValueToJson5(obj.value(k), indent + 2);
    if (valStr.isEmpty()) continue;  // 跳过空字符串/空数组
    hasField = true;
    out += pad2 + k + ": " + valStr + ",\n";
  }
  if (!hasField) return QStringLiteral("{}");
  out += pad + "}";
  return out;
}

/// 将 QJsonArray 转换为 JSON5 字符串（空数组返回空 QString 以便上层跳过）
static QString jsonArrayToJson5(const QJsonArray &arr, int indent) {
  if (arr.isEmpty()) return QString();
  QString pad = QString(QStringLiteral(" ")).repeated(indent);
  QString pad2 = QString(QStringLiteral(" ")).repeated(indent + 2);
  QString out = "[\n";
  for (const auto &v : arr) {
    out += pad2 + jsonValueToJson5(v, indent + 2) + ",\n";
  }
  out += pad + "]";
  return out;
}

/// 将 QJsonValue 转换为 JSON5 字符串
static QString jsonValueToJson5(const QJsonValue &v, int indent) {
  if (v.isString()) {
    QString s = v.toString();
    if (s.isEmpty()) return QString();  // 空字符串跳过
    return QStringLiteral("'") + json5EscapeStr(s) + QStringLiteral("'");
  }
  if (v.isBool()) return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  if (v.isDouble()) {
    double d = v.toDouble();
    if (d == static_cast<qint64>(d)) return QString::number(static_cast<qint64>(d));
    return QString::number(d);
  }
  if (v.isObject()) return jsonObjectToJson5(v.toObject(), indent);
  if (v.isArray()) return jsonArrayToJson5(v.toArray(), indent);
  return QStringLiteral("null");
}

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
    case EditStyle::Money:
      return QStringLiteral("money");
    case EditStyle::Date:
      return QStringLiteral("date");
    case EditStyle::Tag:
      return QStringLiteral("tag");
    case EditStyle::Boolean:
      return QStringLiteral("boolean");
    case EditStyle::Image:
      return QStringLiteral("image");
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
  if (s == QStringLiteral("money")) return EditStyle::Money;
  if (s == QStringLiteral("date")) return EditStyle::Date;
  if (s == QStringLiteral("tag")) return EditStyle::Tag;
  if (s == QStringLiteral("boolean")) return EditStyle::Boolean;
  if (s == QStringLiteral("image")) return EditStyle::Image;
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

QString buttonPositionToString(ButtonPosition p) {
  return p == ButtonPosition::Toolbar ? QStringLiteral("toolbar") : QStringLiteral("row");
}

ButtonPosition stringToButtonPosition(const QString &s) {
  return s == QStringLiteral("toolbar") ? ButtonPosition::Toolbar : ButtonPosition::Row;
}

QString buttonActionTypeToString(ButtonActionType t) {
  switch (t) {
    case ButtonActionType::Confirm:
      return QStringLiteral("confirm");
    case ButtonActionType::Dialog:
      return QStringLiteral("dialog");
    case ButtonActionType::Link:
      return QStringLiteral("link");
    default:
      return QStringLiteral("ajax");
  }
}

ButtonActionType stringToButtonActionType(const QString &s) {
  if (s == QStringLiteral("confirm")) return ButtonActionType::Confirm;
  if (s == QStringLiteral("dialog")) return ButtonActionType::Dialog;
  if (s == QStringLiteral("link")) return ButtonActionType::Link;
  return ButtonActionType::Ajax;
}

// ════════════════════════════════════════════════════════════
//  TagItem
// ════════════════════════════════════════════════════════════

QJsonObject TagItem::toJson() const {
  QJsonObject obj;
  obj["value"] = value;
  obj["text"] = text;
  obj["color"] = color;
  return obj;
}

TagItem TagItem::fromJson(const QJsonObject &obj) {
  TagItem t;
  t.value = obj.value("value").toString();
  t.text = obj.value("text").toString();
  t.color = obj.value("color").toString();
  return t;
}

QList<TagItem> defaultTagItems() {
  QList<TagItem> items;
  TagItem off;
  off.value = QStringLiteral("0");
  off.text = QStringLiteral("关闭");
  off.color = QStringLiteral("info");
  items.append(off);
  TagItem on;
  on.value = QStringLiteral("1");
  on.text = QStringLiteral("开启");
  on.color = QStringLiteral("success");
  items.append(on);
  return items;
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
  if (editStyle == EditStyle::Money) {
    obj["precision"] = precision;
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
  if (!displayType.isEmpty()) {
    obj["displayType"] = displayType;
    if (displayType == QStringLiteral("tag")) {
      QJsonArray arr;
      for (const auto &t : tagItems) arr.append(t.toJson());
      obj["tagItems"] = arr;
    }
    if (displayType == QStringLiteral("boolean")) {
      obj["boolTrueText"] = boolTrueText;
      obj["boolFalseText"] = boolFalseText;
    }
  }
  if (!defaultValue.isEmpty()) obj["defaultValue"] = defaultValue;
  if (!defaultSort.isEmpty()) obj["defaultSort"] = defaultSort;
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
  c.displayType = obj.value("displayType").toString();
  // tagItems 数组读取（displayType == "tag" 时使用）
  // 兼容旧格式：若 tagItems 为空但存在 tagTrueText/tagFalseText，则转换
  const QJsonArray tagArr = obj.value("tagItems").toArray();
  for (const auto &v : tagArr) {
    c.tagItems.append(TagItem::fromJson(v.toObject()));
  }
  if (c.displayType == QStringLiteral("tag") && c.tagItems.isEmpty()) {
    // 旧格式兼容：tagTrueText/tagFalseText → tagItems
    QString tt = obj.value("tagTrueText").toString();
    QString tc = obj.value("tagTrueColor").toString();
    QString ft = obj.value("tagFalseText").toString();
    QString fc = obj.value("tagFalseColor").toString();
    if (!tt.isEmpty() || !ft.isEmpty()) {
      TagItem falseItem;
      falseItem.value = QStringLiteral("0");
      falseItem.text = ft.isEmpty() ? QStringLiteral("关闭") : ft;
      falseItem.color = fc.isEmpty() ? QStringLiteral("info") : fc;
      c.tagItems.append(falseItem);
      TagItem trueItem;
      trueItem.value = QStringLiteral("1");
      trueItem.text = tt.isEmpty() ? QStringLiteral("开启") : tt;
      trueItem.color = tc.isEmpty() ? QStringLiteral("success") : tc;
      c.tagItems.append(trueItem);
    } else {
      c.tagItems = defaultTagItems();
    }
  }
  c.boolTrueText = obj.value("boolTrueText").toString();
  c.boolFalseText = obj.value("boolFalseText").toString();
  // 通用配置
  c.defaultValue = obj.value("defaultValue").toString();
  c.defaultSort = obj.value("defaultSort").toString();
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
//  DialogFieldConfig
// ════════════════════════════════════════════════════════════

QJsonObject DialogFieldConfig::toJson() const {
  QJsonObject obj;
  obj["fieldName"] = fieldName;
  obj["label"] = label;
  obj["editStyle"] = editStyleToString(editStyle);
  obj["required"] = required;
  // 样式特定配置（仅对应 editStyle 时输出）
  if (editStyle == EditStyle::Select) {
    obj["selectUrl"] = selectUrl;
    obj["selectValueField"] = selectValueField;
    obj["selectLabelField"] = selectLabelField;
  }
  if (editStyle == EditStyle::Text) {
    obj["placeholder"] = placeholder;
    obj["maxlength"] = maxlength;
  }
  if (editStyle == EditStyle::TextArea) {
    obj["placeholder"] = placeholder;
    obj["textareaRows"] = textareaRows;
  }
  if (editStyle == EditStyle::Int || editStyle == EditStyle::Float) {
    obj["minValue"] = minValue;
    obj["maxValue"] = maxValue;
  }
  if (editStyle == EditStyle::Float) {
    obj["precision"] = precision;
  }
  if (editStyle == EditStyle::Date) {
    obj["dateFormat"] = dateFormat;
  }
  return obj;
}

DialogFieldConfig DialogFieldConfig::fromJson(const QJsonObject &obj) {
  DialogFieldConfig f;
  f.fieldName = obj.value("fieldName").toString();
  f.label = obj.value("label").toString();
  f.editStyle = stringToEditStyle(obj.value("editStyle").toString(QStringLiteral("text")));
  f.required = obj.value("required").toBool(false);
  f.placeholder = obj.value("placeholder").toString();
  f.maxlength = obj.value("maxlength").toInt(0);
  f.textareaRows = obj.value("textareaRows").toInt(3);
  f.minValue = obj.value("minValue").toDouble(0);
  f.maxValue = obj.value("maxValue").toDouble(0);
  f.precision = obj.value("precision").toInt(2);
  f.dateFormat = obj.value("dateFormat").toString();
  f.selectUrl = obj.value("selectUrl").toString();
  f.selectValueField = obj.value("selectValueField").toString();
  f.selectLabelField = obj.value("selectLabelField").toString();
  return f;
}

// ════════════════════════════════════════════════════════════
//  ButtonConfig
// ════════════════════════════════════════════════════════════

QJsonObject ButtonConfig::toJson() const {
  QJsonObject obj;
  obj["label"] = label;
  obj["icon"] = icon;
  obj["position"] = buttonPositionToString(position);
  obj["buttonType"] = buttonType;
  obj["actionType"] = buttonActionTypeToString(actionType);
  obj["actionKey"] = actionKey;
  // Ajax / Confirm 行为专用
  if (!apiName.isEmpty()) obj["apiName"] = apiName;
  if (!confirmText.isEmpty()) obj["confirmText"] = confirmText;
  // Dialog 行为专用
  if (!dialogTitle.isEmpty()) obj["dialogTitle"] = dialogTitle;
  if (!dialogApi.isEmpty()) obj["dialogApi"] = dialogApi;
  if (!dialogFields.isEmpty()) {
    QJsonArray arr;
    for (const auto &f : dialogFields) arr.append(f.toJson());
    obj["dialogFields"] = arr;
  }
  // Link 行为专用
  if (!linkPath.isEmpty()) obj["linkPath"] = linkPath;
  return obj;
}

ButtonConfig ButtonConfig::fromJson(const QJsonObject &obj) {
  ButtonConfig b;
  b.label = obj.value("label").toString();
  b.icon = obj.value("icon").toString();
  b.position = stringToButtonPosition(obj.value("position").toString(QStringLiteral("row")));
  b.buttonType = obj.value("buttonType").toString();
  b.actionType = stringToButtonActionType(obj.value("actionType").toString(QStringLiteral("ajax")));
  b.actionKey = obj.value("actionKey").toString();
  b.apiName = obj.value("apiName").toString();
  b.confirmText = obj.value("confirmText").toString();
  b.dialogTitle = obj.value("dialogTitle").toString();
  b.dialogApi = obj.value("dialogApi").toString();
  const QJsonArray dArr = obj.value("dialogFields").toArray();
  for (const auto &v : dArr) {
    b.dialogFields.append(DialogFieldConfig::fromJson(v.toObject()));
  }
  b.linkPath = obj.value("linkPath").toString();
  return b;
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

  QJsonArray btnArr;
  for (const auto &b : buttons) btnArr.append(b.toJson());
  root["buttons"] = btnArr;

  return root;
}

QString JsonVueConfig::toJsonString() const {
  // 生成 JSON5 格式（键名无引号、字符串单引号、允许尾随逗号、键按字母序排列）
  // 复用各结构体的 toJson() 方法，消除两套序列化逻辑的同步问题
  return jsonObjectToJson5(toJsonObject(), 0) + "\n";
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

  const QJsonArray btnArr = obj.value("buttons").toArray();
  for (const auto &v : btnArr) {
    cfg.buttons.append(ButtonConfig::fromJson(v.toObject()));
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
