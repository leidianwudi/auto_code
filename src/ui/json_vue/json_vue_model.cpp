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
  if (!displayType.isEmpty()) {
    obj["displayType"] = displayType;
    if (displayType == QStringLiteral("tag")) {
      obj["tagTrueText"] = tagTrueText;
      obj["tagTrueColor"] = tagTrueColor;
      obj["tagFalseText"] = tagFalseText;
      obj["tagFalseColor"] = tagFalseColor;
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
  c.tagTrueText = obj.value("tagTrueText").toString();
  c.tagTrueColor = obj.value("tagTrueColor").toString();
  c.tagFalseText = obj.value("tagFalseText").toString();
  c.tagFalseColor = obj.value("tagFalseColor").toString();
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
    if (!col.displayType.isEmpty()) {
      out += QStringLiteral("      displayType: '%1',\n").arg(esc(col.displayType));
      if (col.displayType == QStringLiteral("tag")) {
        out += QStringLiteral("      tagTrueText: '%1',\n").arg(esc(col.tagTrueText));
        out += QStringLiteral("      tagTrueColor: '%1',\n").arg(esc(col.tagTrueColor));
        out += QStringLiteral("      tagFalseText: '%1',\n").arg(esc(col.tagFalseText));
        out += QStringLiteral("      tagFalseColor: '%1',\n").arg(esc(col.tagFalseColor));
      }
      if (col.displayType == QStringLiteral("boolean")) {
        out += QStringLiteral("      boolTrueText: '%1',\n").arg(esc(col.boolTrueText));
        out += QStringLiteral("      boolFalseText: '%1',\n").arg(esc(col.boolFalseText));
      }
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

  // buttons（按字母序，仅在非空时输出）
  if (!buttons.isEmpty()) {
    out += QStringLiteral("  buttons: [\n");
    for (const auto &b : buttons) {
      out += QStringLiteral("    {\n");
      out += QStringLiteral("      actionKey: '%1',\n").arg(esc(b.actionKey));
      out +=
          QStringLiteral("      actionType: '%1',\n").arg(buttonActionTypeToString(b.actionType));
      if (!b.apiName.isEmpty()) {
        out += QStringLiteral("      apiName: '%1',\n").arg(esc(b.apiName));
      }
      if (!b.buttonType.isEmpty()) {
        out += QStringLiteral("      buttonType: '%1',\n").arg(esc(b.buttonType));
      }
      if (!b.confirmText.isEmpty()) {
        out += QStringLiteral("      confirmText: '%1',\n").arg(esc(b.confirmText));
      }
      if (!b.dialogApi.isEmpty()) {
        out += QStringLiteral("      dialogApi: '%1',\n").arg(esc(b.dialogApi));
      }
      if (!b.dialogFields.isEmpty()) {
        out += QStringLiteral("      dialogFields: [\n");
        for (const auto &f : b.dialogFields) {
          out += QStringLiteral("        {\n");
          out += QStringLiteral("          editStyle: '%1',\n").arg(editStyleToString(f.editStyle));
          out += QStringLiteral("          fieldName: '%1',\n").arg(esc(f.fieldName));
          out += QStringLiteral("          label: '%1',\n").arg(esc(f.label));
          // 样式特定配置（按字母序）
          if (f.editStyle == EditStyle::Date && !f.dateFormat.isEmpty()) {
            out += QStringLiteral("          dateFormat: '%1',\n").arg(esc(f.dateFormat));
          }
          if (f.editStyle == EditStyle::Text && f.maxlength > 0) {
            out += QStringLiteral("          maxlength: %1,\n").arg(f.maxlength);
          }
          if ((f.editStyle == EditStyle::Int || f.editStyle == EditStyle::Float) &&
              f.maxValue != 0) {
            out += QStringLiteral("          maxValue: %1,\n").arg(f.maxValue);
          }
          if ((f.editStyle == EditStyle::Int || f.editStyle == EditStyle::Float) &&
              f.minValue != 0) {
            out += QStringLiteral("          minValue: %1,\n").arg(f.minValue);
          }
          if ((f.editStyle == EditStyle::Text || f.editStyle == EditStyle::TextArea) &&
              !f.placeholder.isEmpty()) {
            out += QStringLiteral("          placeholder: '%1',\n").arg(esc(f.placeholder));
          }
          if (f.editStyle == EditStyle::Float && f.precision != 2) {
            out += QStringLiteral("          precision: %1,\n").arg(f.precision);
          }
          if (f.required) {
            out += QStringLiteral("          required: true,\n");
          }
          if (f.editStyle == EditStyle::Select) {
            out +=
                QStringLiteral("          selectLabelField: '%1',\n").arg(esc(f.selectLabelField));
            out += QStringLiteral("          selectUrl: '%1',\n").arg(esc(f.selectUrl));
            out +=
                QStringLiteral("          selectValueField: '%1',\n").arg(esc(f.selectValueField));
          }
          if (f.editStyle == EditStyle::TextArea && f.textareaRows != 3) {
            out += QStringLiteral("          textareaRows: %1,\n").arg(f.textareaRows);
          }
          out += QStringLiteral("        },\n");
        }
        out += QStringLiteral("      ],\n");
      }
      if (!b.dialogTitle.isEmpty()) {
        out += QStringLiteral("      dialogTitle: '%1',\n").arg(esc(b.dialogTitle));
      }
      if (!b.icon.isEmpty()) {
        out += QStringLiteral("      icon: '%1',\n").arg(esc(b.icon));
      }
      out += QStringLiteral("      label: '%1',\n").arg(esc(b.label));
      if (!b.linkPath.isEmpty()) {
        out += QStringLiteral("      linkPath: '%1',\n").arg(esc(b.linkPath));
      }
      out += QStringLiteral("      position: '%1',\n").arg(buttonPositionToString(b.position));
      out += QStringLiteral("    },\n");
    }
    out += QStringLiteral("  ],\n");
  }

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
