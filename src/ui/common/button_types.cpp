/**
 * @file button_types.cpp
 * @brief 共享按钮/对话框字段类型的序列化与枚举转换实现
 *
 * 实现自 json_vue_model.cpp 迁入（键名与行为零变化），使不链接 json_vue
 * 模块的编译目标（如 auto_code_tests 中的 json_table_model）也能解析
 * ButtonConfig/DialogFieldConfig。
 */

#include "button_types.h"

#include <QJsonArray>

// 完整 JsonVueKey 键词汇（kSelectUrl/kPlaceholder/kDateFormat 等对话框字段键）
// 定义在 json_vue_model.h；include 头文件即可（纯 QtCore 模型头，无额外链接依赖）
#include "../json_vue/json_vue_model.h"

// ════════════════════════════════════════════════════════════
//  枚举 ↔ 字符串转换（自 json_vue_model.cpp 迁入）
// ════════════════════════════════════════════════════════════

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
    // 引用的 .jsonsource 数据源
    if (!selectSourceFile.isEmpty()) obj[JsonVueKey::kSelectSourceFile] = selectSourceFile;
    if (!selectSourceId.isEmpty()) obj[JsonVueKey::kSelectSourceId] = selectSourceId;
    // 查询分页配置（仅启用分页时输出）
    if (selectPaged) {
      obj[JsonVueKey::kSelectPaged] = true;
      obj[JsonVueKey::kSelectPageKey] = selectPageKey;
      obj[JsonVueKey::kSelectPageSizeKey] = selectPageSizeKey;
      obj[JsonVueKey::kSelectPageSize] = selectPageSize;
      obj[JsonVueKey::kSelectSearchTitle] = selectSearchTitle;
      obj[JsonVueKey::kSelectSearchField] = selectSearchField;
      obj[JsonVueKey::kSelectMethod] = selectMethod;
    }
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
  f.selectSourceFile = obj.value(JsonVueKey::kSelectSourceFile).toString();
  f.selectSourceId = obj.value(JsonVueKey::kSelectSourceId).toString();
  f.selectPaged = obj.value(JsonVueKey::kSelectPaged).toBool(false);
  f.selectPageKey = obj.value(JsonVueKey::kSelectPageKey).toString(QStringLiteral("page"));
  f.selectPageSizeKey =
      obj.value(JsonVueKey::kSelectPageSizeKey).toString(QStringLiteral("pageSize"));
  f.selectPageSize = obj.value(JsonVueKey::kSelectPageSize).toInt(20);
  f.selectSearchTitle = obj.value(JsonVueKey::kSelectSearchTitle).toString();
  f.selectSearchField = obj.value(JsonVueKey::kSelectSearchField).toString();
  f.selectMethod =
      obj.value(JsonVueKey::kSelectMethod).toString(QString::fromLatin1(JsonVueHttp::kPost));
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
