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
  return style == ListStyle::Switch ? QString::fromLatin1(JsonVueStyle::kSwitch)
                                    : QString::fromLatin1(JsonVueStyle::kText);
}

ListStyle stringToListStyle(const QString &s) {
  return s == JsonVueStyle::kSwitch ? ListStyle::Switch : ListStyle::Text;
}

QString editStyleToString(EditStyle style) {
  switch (style) {
    case EditStyle::Int:
      return QString::fromLatin1(JsonVueStyle::kInt);
    case EditStyle::Float:
      return QString::fromLatin1(JsonVueStyle::kFloat);
    case EditStyle::Money:
      return QString::fromLatin1(JsonVueStyle::kMoney);
    case EditStyle::Date:
      return QString::fromLatin1(JsonVueStyle::kDate);
    case EditStyle::Tag:
      return QString::fromLatin1(JsonVueStyle::kTag);
    case EditStyle::Boolean:
      return QString::fromLatin1(JsonVueStyle::kBoolean);
    case EditStyle::Image:
      return QString::fromLatin1(JsonVueStyle::kImage);
    case EditStyle::Select:
      return QString::fromLatin1(JsonVueStyle::kSelect);
    case EditStyle::TextArea:
      return QString::fromLatin1(JsonVueStyle::kTextarea);
    default:
      return QString::fromLatin1(JsonVueStyle::kText);
  }
}

EditStyle stringToEditStyle(const QString &s) {
  if (s == JsonVueStyle::kInt) return EditStyle::Int;
  if (s == JsonVueStyle::kFloat) return EditStyle::Float;
  if (s == JsonVueStyle::kMoney) return EditStyle::Money;
  if (s == JsonVueStyle::kDate) return EditStyle::Date;
  if (s == JsonVueStyle::kTag) return EditStyle::Tag;
  if (s == JsonVueStyle::kBoolean) return EditStyle::Boolean;
  if (s == JsonVueStyle::kImage) return EditStyle::Image;
  // 兼容旧名称 "combobox"
  if (s == JsonVueStyle::kSelect || s == QStringLiteral("combobox")) return EditStyle::Select;
  if (s == JsonVueStyle::kTextarea) return EditStyle::TextArea;
  return EditStyle::Text;
}

QString queryRelationToString(QueryRelation r) {
  if (r == QueryRelation::Range) return QString::fromLatin1(JsonVueStyle::kRange);
  return QStringLiteral("=");
}

QueryRelation stringToQueryRelation(const QString &s) {
  if (s == JsonVueStyle::kRange) return QueryRelation::Range;
  return QueryRelation::Equal;
}

QString queryInputStyleToString(QueryInputStyle s) {
  switch (s) {
    case QueryInputStyle::Date:
      return QString::fromLatin1(JsonVueStyle::kDate);
    case QueryInputStyle::Select:
      return QString::fromLatin1(JsonVueStyle::kSelect);
    default:
      return QString::fromLatin1(JsonVueStyle::kText);
  }
}

QueryInputStyle stringToQueryInputStyle(const QString &s) {
  // 兼容旧名称 "time"
  if (s == JsonVueStyle::kDate || s == JsonVueStyle::kTime) return QueryInputStyle::Date;
  if (s == JsonVueStyle::kSelect) return QueryInputStyle::Select;
  return QueryInputStyle::Text;
}

QString buttonPositionToString(ButtonPosition p) {
  return p == ButtonPosition::Toolbar ? QString::fromLatin1(JsonVueStyle::kToolbar)
                                      : QString::fromLatin1(JsonVueStyle::kRow);
}

ButtonPosition stringToButtonPosition(const QString &s) {
  return s == JsonVueStyle::kToolbar ? ButtonPosition::Toolbar : ButtonPosition::Row;
}

QString buttonActionTypeToString(ButtonActionType t) {
  switch (t) {
    case ButtonActionType::Confirm:
      return QString::fromLatin1(JsonVueStyle::kConfirm);
    case ButtonActionType::Dialog:
      return QString::fromLatin1(JsonVueStyle::kDialog);
    case ButtonActionType::Link:
      return QString::fromLatin1(JsonVueStyle::kLink);
    default:
      return QString::fromLatin1(JsonVueStyle::kAjax);
  }
}

ButtonActionType stringToButtonActionType(const QString &s) {
  if (s == JsonVueStyle::kConfirm) return ButtonActionType::Confirm;
  if (s == JsonVueStyle::kDialog) return ButtonActionType::Dialog;
  if (s == JsonVueStyle::kLink) return ButtonActionType::Link;
  return ButtonActionType::Ajax;
}

// ════════════════════════════════════════════════════════════
//  TagItem
// ════════════════════════════════════════════════════════════

QJsonObject TagItem::toJson() const {
  QJsonObject obj;
  obj[JsonVueKey::kTagValue] = value;
  obj[JsonVueKey::kTagText] = text;
  obj[JsonVueKey::kTagColor] = color;
  return obj;
}

TagItem TagItem::fromJson(const QJsonObject &obj) {
  TagItem t;
  t.value = obj.value(JsonVueKey::kTagValue).toString();
  t.text = obj.value(JsonVueKey::kTagText).toString();
  t.color = obj.value(JsonVueKey::kTagColor).toString();
  return t;
}

QList<TagItem> defaultTagItems() {
  QList<TagItem> items;
  TagItem off;
  off.value = QStringLiteral("0");
  off.text = QStringLiteral("关闭");
  off.color = QString::fromLatin1(JsonVueColor::kInfo);
  items.append(off);
  TagItem on;
  on.value = QStringLiteral("1");
  on.text = QStringLiteral("开启");
  on.color = QString::fromLatin1(JsonVueColor::kSuccess);
  items.append(on);
  return items;
}

// ════════════════════════════════════════════════════════════
//  JsonVueMeta
// ════════════════════════════════════════════════════════════

QJsonObject JsonVueMeta::toJson() const {
  QJsonObject obj;
  obj[JsonVueKey::kDataMethod] = dataMethod;
  obj[JsonVueKey::kDataUrl] = dataUrl;
  obj[JsonVueKey::kQueryApi] = queryApi;
  obj[JsonVueKey::kDeleteApi] = deleteApi;
  obj[JsonVueKey::kNoDelete] = noDelete;
  obj[JsonVueKey::kUpdateApi] = updateApi;
  obj[JsonVueKey::kNoEdit] = noEdit;
  obj[JsonVueKey::kNoDetail] = noDetail;
  obj[JsonVueKey::kDescription] = description;
  return obj;
}

JsonVueMeta JsonVueMeta::fromJson(const QJsonObject &obj) {
  JsonVueMeta m;
  m.dataMethod = obj.value(JsonVueKey::kDataMethod).toString(QStringLiteral("GET"));
  m.dataUrl = obj.value(JsonVueKey::kDataUrl).toString();
  m.queryApi = obj.value(JsonVueKey::kQueryApi).toString();
  m.deleteApi = obj.value(JsonVueKey::kDeleteApi).toString();
  m.noDelete = obj.value(JsonVueKey::kNoDelete).toBool(false);
  m.updateApi = obj.value(JsonVueKey::kUpdateApi).toString();
  m.noEdit = obj.value(JsonVueKey::kNoEdit).toBool(false);
  m.noDetail = obj.value(JsonVueKey::kNoDetail).toBool(false);
  m.description = obj.value(JsonVueKey::kDescription).toString();
  return m;
}

// ════════════════════════════════════════════════════════════
//  ColumnConfig
// ════════════════════════════════════════════════════════════

QJsonObject ColumnConfig::toJson() const {
  QJsonObject obj;
  obj[JsonVueKey::kDataName] = dataName;
  obj[JsonVueKey::kQueryVisible] = queryVisible;
  obj[JsonVueKey::kQueryName] = queryName;
  obj[JsonVueKey::kQueryStyle] = listStyleToString(queryStyle);
  obj[JsonVueKey::kSwitchEditable] = switchEditable;
  obj[JsonVueKey::kEditVisible] = editVisible;
  obj[JsonVueKey::kEditName] = editName;
  obj[JsonVueKey::kEditStyle] = editStyleToString(editStyle);
  obj[JsonVueKey::kEditEditable] = editEditable;
  // 下拉框配置（仅 Select 时序列化）
  if (editStyle == EditStyle::Select) {
    obj[JsonVueKey::kSelectUrl] = selectUrl;
    obj[JsonVueKey::kSelectValueField] = selectValueField;
    obj[JsonVueKey::kSelectLabelField] = selectLabelField;
  }
  // 样式特定配置
  if (editStyle == EditStyle::Text) {
    obj[JsonVueKey::kPlaceholder] = placeholder;
    obj[JsonVueKey::kMaxlength] = maxlength;
  }
  if (editStyle == EditStyle::Int) {
    obj[JsonVueKey::kMinValue] = minValue;
    obj[JsonVueKey::kMaxValue] = maxValue;
  }
  if (editStyle == EditStyle::Float) {
    obj[JsonVueKey::kPrecision] = precision;
    obj[JsonVueKey::kMinValue] = minValue;
    obj[JsonVueKey::kMaxValue] = maxValue;
  }
  if (editStyle == EditStyle::Money) {
    obj[JsonVueKey::kPrecision] = precision;
  }
  if (editStyle == EditStyle::Date) {
    obj[JsonVueKey::kDateFormat] = dateFormat;
  }
  if (editStyle == EditStyle::TextArea) {
    obj[JsonVueKey::kPlaceholder] = placeholder;
    obj[JsonVueKey::kTextareaRows] = textareaRows;
  }
  // 通用配置（所有样式都输出）
  obj[JsonVueKey::kRequired] = required;
  obj[JsonVueKey::kColumnWidth] = columnWidth;
  if (!columnFixed.isEmpty()) obj[JsonVueKey::kColumnFixed] = columnFixed;
  if (!formatter.isEmpty()) obj[JsonVueKey::kFormatter] = formatter;
  if (formSpan != 24) obj[JsonVueKey::kFormSpan] = formSpan;
  if (!displayType.isEmpty()) {
    obj[JsonVueKey::kDisplayType] = displayType;
    if (displayType == JsonVueStyle::kTag) {
      QJsonArray arr;
      for (const auto &t : tagItems) arr.append(t.toJson());
      obj[JsonVueKey::kTagItems] = arr;
    }
    if (displayType == JsonVueStyle::kBoolean) {
      obj[JsonVueKey::kBoolTrueText] = boolTrueText;
      obj[JsonVueKey::kBoolFalseText] = boolFalseText;
    }
  }
  if (!defaultValue.isEmpty()) obj[JsonVueKey::kDefaultValue] = defaultValue;
  if (!defaultSort.isEmpty()) obj[JsonVueKey::kDefaultSort] = defaultSort;
  return obj;
}

ColumnConfig ColumnConfig::fromJson(const QJsonObject &obj) {
  ColumnConfig c;
  c.dataName = obj.value(JsonVueKey::kDataName).toString();
  c.queryVisible = obj.value(JsonVueKey::kQueryVisible).toBool(true);
  c.queryName = obj.value(JsonVueKey::kQueryName).toString();
  c.queryStyle =
      stringToListStyle(obj.value(JsonVueKey::kQueryStyle).toString(JsonVueStyle::kText));
  c.switchEditable = obj.value(JsonVueKey::kSwitchEditable).toBool(false);
  c.editVisible = obj.value(JsonVueKey::kEditVisible).toBool(true);
  c.editName = obj.value(JsonVueKey::kEditName).toString();
  c.editStyle = stringToEditStyle(obj.value(JsonVueKey::kEditStyle).toString(JsonVueStyle::kText));
  c.editEditable = obj.value(JsonVueKey::kEditEditable).toBool(true);
  // 兼容旧字段名 comboboxUrl/comboboxValueField/comboboxLabelField
  c.selectUrl = obj.value(JsonVueKey::kSelectUrl)
                    .toString(obj.value(QStringLiteral("comboboxUrl")).toString());
  c.selectValueField = obj.value(JsonVueKey::kSelectValueField)
                           .toString(obj.value(QStringLiteral("comboboxValueField")).toString());
  c.selectLabelField = obj.value(JsonVueKey::kSelectLabelField)
                           .toString(obj.value(QStringLiteral("comboboxLabelField")).toString());
  // 样式特定配置
  c.placeholder = obj.value(JsonVueKey::kPlaceholder).toString();
  c.maxlength = obj.value(JsonVueKey::kMaxlength).toInt(0);
  c.minValue = obj.value(JsonVueKey::kMinValue).toDouble(0);
  c.maxValue = obj.value(JsonVueKey::kMaxValue).toDouble(0);
  c.precision = obj.value(JsonVueKey::kPrecision).toInt(2);
  c.dateFormat = obj.value(JsonVueKey::kDateFormat).toString();
  c.textareaRows = obj.value(JsonVueKey::kTextareaRows).toInt(3);
  // 通用配置
  c.required = obj.value(JsonVueKey::kRequired).toBool(false);
  c.columnWidth = obj.value(JsonVueKey::kColumnWidth).toInt(0);
  c.columnFixed = obj.value(JsonVueKey::kColumnFixed).toString();
  c.formatter = obj.value(JsonVueKey::kFormatter).toString();
  c.formSpan = obj.value(JsonVueKey::kFormSpan).toInt(24);
  c.displayType = obj.value(JsonVueKey::kDisplayType).toString();
  // tagItems 数组读取（displayType == "tag" 时使用）
  // 兼容旧格式：若 tagItems 为空但存在 tagTrueText/tagFalseText，则转换
  const QJsonArray tagArr = obj.value(JsonVueKey::kTagItems).toArray();
  for (const auto &v : tagArr) {
    c.tagItems.append(TagItem::fromJson(v.toObject()));
  }
  if (c.displayType == JsonVueStyle::kTag && c.tagItems.isEmpty()) {
    // 旧格式兼容：tagTrueText/tagFalseText → tagItems
    QString tt = obj.value(QStringLiteral("tagTrueText")).toString();
    QString tc = obj.value(QStringLiteral("tagTrueColor")).toString();
    QString ft = obj.value(QStringLiteral("tagFalseText")).toString();
    QString fc = obj.value(QStringLiteral("tagFalseColor")).toString();
    if (!tt.isEmpty() || !ft.isEmpty()) {
      TagItem falseItem;
      falseItem.value = QStringLiteral("0");
      falseItem.text = ft.isEmpty() ? QStringLiteral("关闭") : ft;
      falseItem.color = fc.isEmpty() ? QString::fromLatin1(JsonVueColor::kInfo) : fc;
      c.tagItems.append(falseItem);
      TagItem trueItem;
      trueItem.value = QStringLiteral("1");
      trueItem.text = tt.isEmpty() ? QStringLiteral("开启") : tt;
      trueItem.color = tc.isEmpty() ? QString::fromLatin1(JsonVueColor::kSuccess) : tc;
      c.tagItems.append(trueItem);
    } else {
      c.tagItems = defaultTagItems();
    }
  }
  c.boolTrueText = obj.value(JsonVueKey::kBoolTrueText).toString();
  if (c.boolTrueText.isEmpty() && c.displayType == JsonVueStyle::kBoolean)
    c.boolTrueText = QStringLiteral("是");
  c.boolFalseText = obj.value(JsonVueKey::kBoolFalseText).toString();
  if (c.boolFalseText.isEmpty() && c.displayType == JsonVueStyle::kBoolean)
    c.boolFalseText = QStringLiteral("否");
  // 通用配置
  c.defaultValue = obj.value(JsonVueKey::kDefaultValue).toString();
  c.defaultSort = obj.value(JsonVueKey::kDefaultSort).toString();
  return c;
}

// ════════════════════════════════════════════════════════════
//  QueryFieldConfig
// ════════════════════════════════════════════════════════════

QJsonObject QueryFieldConfig::toJson() const {
  QJsonObject obj;
  obj[JsonVueKey::kDisplayName] = displayName;
  obj[JsonVueKey::kDataName] = dataName;
  obj[JsonVueKey::kInputStyle] = queryInputStyleToString(inputStyle);
  obj[JsonVueKey::kRelation] = queryRelationToString(relation);
  // 下拉框配置（仅 Select 时序列化）
  if (inputStyle == QueryInputStyle::Select) {
    obj[JsonVueKey::kSelectUrl] = selectUrl;
    obj[JsonVueKey::kSelectValueField] = selectValueField;
    obj[JsonVueKey::kSelectLabelField] = selectLabelField;
  }
  if (inputStyle == QueryInputStyle::Text) {
    obj[JsonVueKey::kPlaceholder] = placeholder;
  }
  if (inputStyle == QueryInputStyle::Date) {
    obj[JsonVueKey::kDateFormat] = dateFormat;
  }
  return obj;
}

QueryFieldConfig QueryFieldConfig::fromJson(const QJsonObject &obj) {
  QueryFieldConfig q;
  q.displayName = obj.value(JsonVueKey::kDisplayName).toString();
  q.dataName = obj.value(JsonVueKey::kDataName).toString();
  q.inputStyle =
      stringToQueryInputStyle(obj.value(JsonVueKey::kInputStyle).toString(JsonVueStyle::kText));
  q.relation =
      stringToQueryRelation(obj.value(JsonVueKey::kRelation).toString(QStringLiteral("=")));
  q.selectUrl = obj.value(JsonVueKey::kSelectUrl).toString();
  q.selectValueField = obj.value(JsonVueKey::kSelectValueField).toString();
  q.selectLabelField = obj.value(JsonVueKey::kSelectLabelField).toString();
  q.placeholder = obj.value(JsonVueKey::kPlaceholder).toString();
  q.dateFormat = obj.value(JsonVueKey::kDateFormat).toString();
  return q;
}

// ════════════════════════════════════════════════════════════
//  DialogFieldConfig
// ════════════════════════════════════════════════════════════

QJsonObject DialogFieldConfig::toJson() const {
  QJsonObject obj;
  obj[JsonVueKey::kFieldName] = fieldName;
  obj[JsonVueKey::kLabel] = label;
  obj[JsonVueKey::kEditStyle] = editStyleToString(editStyle);
  obj[JsonVueKey::kRequired] = required;
  // 样式特定配置（仅对应 editStyle 时输出）
  if (editStyle == EditStyle::Select) {
    obj[JsonVueKey::kSelectUrl] = selectUrl;
    obj[JsonVueKey::kSelectValueField] = selectValueField;
    obj[JsonVueKey::kSelectLabelField] = selectLabelField;
  }
  if (editStyle == EditStyle::Text) {
    obj[JsonVueKey::kPlaceholder] = placeholder;
    obj[JsonVueKey::kMaxlength] = maxlength;
  }
  if (editStyle == EditStyle::TextArea) {
    obj[JsonVueKey::kPlaceholder] = placeholder;
    obj[JsonVueKey::kTextareaRows] = textareaRows;
  }
  if (editStyle == EditStyle::Int || editStyle == EditStyle::Float) {
    obj[JsonVueKey::kMinValue] = minValue;
    obj[JsonVueKey::kMaxValue] = maxValue;
  }
  if (editStyle == EditStyle::Float) {
    obj[JsonVueKey::kPrecision] = precision;
  }
  if (editStyle == EditStyle::Date) {
    obj[JsonVueKey::kDateFormat] = dateFormat;
  }
  return obj;
}

DialogFieldConfig DialogFieldConfig::fromJson(const QJsonObject &obj) {
  DialogFieldConfig f;
  f.fieldName = obj.value(JsonVueKey::kFieldName).toString();
  f.label = obj.value(JsonVueKey::kLabel).toString();
  f.editStyle = stringToEditStyle(obj.value(JsonVueKey::kEditStyle).toString(JsonVueStyle::kText));
  f.required = obj.value(JsonVueKey::kRequired).toBool(false);
  f.placeholder = obj.value(JsonVueKey::kPlaceholder).toString();
  f.maxlength = obj.value(JsonVueKey::kMaxlength).toInt(0);
  f.textareaRows = obj.value(JsonVueKey::kTextareaRows).toInt(3);
  f.minValue = obj.value(JsonVueKey::kMinValue).toDouble(0);
  f.maxValue = obj.value(JsonVueKey::kMaxValue).toDouble(0);
  f.precision = obj.value(JsonVueKey::kPrecision).toInt(2);
  f.dateFormat = obj.value(JsonVueKey::kDateFormat).toString();
  f.selectUrl = obj.value(JsonVueKey::kSelectUrl).toString();
  f.selectValueField = obj.value(JsonVueKey::kSelectValueField).toString();
  f.selectLabelField = obj.value(JsonVueKey::kSelectLabelField).toString();
  return f;
}

// ════════════════════════════════════════════════════════════
//  ButtonConfig
// ════════════════════════════════════════════════════════════

QJsonObject ButtonConfig::toJson() const {
  QJsonObject obj;
  obj[JsonVueKey::kLabel] = label;
  obj[JsonVueKey::kIcon] = icon;
  obj[JsonVueKey::kPosition] = buttonPositionToString(position);
  obj[JsonVueKey::kButtonType] = buttonType;
  obj[JsonVueKey::kActionType] = buttonActionTypeToString(actionType);
  obj[JsonVueKey::kActionKey] = actionKey;
  // Ajax / Confirm 行为专用
  if (!apiName.isEmpty()) obj[JsonVueKey::kApiName] = apiName;
  if (!confirmText.isEmpty()) obj[JsonVueKey::kConfirmText] = confirmText;
  // Dialog 行为专用
  if (!dialogTitle.isEmpty()) obj[JsonVueKey::kDialogTitle] = dialogTitle;
  if (!dialogApi.isEmpty()) obj[JsonVueKey::kDialogApi] = dialogApi;
  if (!dialogFields.isEmpty()) {
    QJsonArray arr;
    for (const auto &f : dialogFields) arr.append(f.toJson());
    obj[JsonVueKey::kDialogFields] = arr;
  }
  // Link 行为专用
  if (!linkPath.isEmpty()) obj[JsonVueKey::kLinkPath] = linkPath;
  return obj;
}

ButtonConfig ButtonConfig::fromJson(const QJsonObject &obj) {
  ButtonConfig b;
  b.label = obj.value(JsonVueKey::kLabel).toString();
  b.icon = obj.value(JsonVueKey::kIcon).toString();
  b.position =
      stringToButtonPosition(obj.value(JsonVueKey::kPosition).toString(JsonVueStyle::kRow));
  b.buttonType = obj.value(JsonVueKey::kButtonType).toString();
  b.actionType =
      stringToButtonActionType(obj.value(JsonVueKey::kActionType).toString(JsonVueStyle::kAjax));
  b.actionKey = obj.value(JsonVueKey::kActionKey).toString();
  b.apiName = obj.value(JsonVueKey::kApiName).toString();
  b.confirmText = obj.value(JsonVueKey::kConfirmText).toString();
  b.dialogTitle = obj.value(JsonVueKey::kDialogTitle).toString();
  b.dialogApi = obj.value(JsonVueKey::kDialogApi).toString();
  const QJsonArray dArr = obj.value(JsonVueKey::kDialogFields).toArray();
  for (const auto &v : dArr) {
    b.dialogFields.append(DialogFieldConfig::fromJson(v.toObject()));
  }
  b.linkPath = obj.value(JsonVueKey::kLinkPath).toString();
  return b;
}

// ════════════════════════════════════════════════════════════
//  JsonVueConfig
// ════════════════════════════════════════════════════════════

QJsonObject JsonVueConfig::toJsonObject() const {
  QJsonObject root;
  root[JsonVueKey::kMeta] = meta.toJson();

  QJsonArray colArr;
  for (const auto &c : columns) colArr.append(c.toJson());
  root[JsonVueKey::kColumns] = colArr;

  QJsonArray qArr;
  for (const auto &q : queryFields) qArr.append(q.toJson());
  root[JsonVueKey::kQueryFields] = qArr;

  QJsonArray btnArr;
  for (const auto &b : buttons) btnArr.append(b.toJson());
  root[JsonVueKey::kButtons] = btnArr;

  return root;
}

QString JsonVueConfig::toJsonString() const {
  // 生成 JSON5 格式（键名无引号、字符串单引号、允许尾随逗号、键按字母序排列）
  // 复用各结构体的 toJson() 方法，消除两套序列化逻辑的同步问题
  return jsonObjectToJson5(toJsonObject(), 0) + "\n";
}

QString JsonVueConfig::toJsonString(const QJsonObject &root) {
  // 直接序列化调用方构造好的完整对象（可能含可视化未表达的保留字段），
  // 不再二次经 toJsonObject()，保证字段/键不被丢弃
  return jsonObjectToJson5(root, 0) + "\n";
}

JsonVueConfig JsonVueConfig::fromJson(const QJsonObject &obj) {
  JsonVueConfig cfg;
  cfg.meta = JsonVueMeta::fromJson(obj.value(JsonVueKey::kMeta).toObject());

  const QJsonArray colArr = obj.value(JsonVueKey::kColumns).toArray();
  for (const auto &v : colArr) {
    cfg.columns.append(ColumnConfig::fromJson(v.toObject()));
  }

  const QJsonArray qArr = obj.value(JsonVueKey::kQueryFields).toArray();
  for (const auto &v : qArr) {
    cfg.queryFields.append(QueryFieldConfig::fromJson(v.toObject()));
  }

  const QJsonArray btnArr = obj.value(JsonVueKey::kButtons).toArray();
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
