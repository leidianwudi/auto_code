/**
 * @file button_types.h
 * @brief jsonvue / jsontable 共享的按钮与对话框字段类型
 *
 * 从 json_vue_model.h 抽取的共享定义（jsontable 完整替代 jsonvue 的过程中，
 * 两套配置需要同形的操作按钮/对话框字段/编辑样式类型）：
 *   - EditStyle          编辑界面样式枚举（DialogFieldConfig 依赖）
 *   - ButtonPosition / ButtonActionType / ButtonConfig / DialogFieldConfig
 *   - 按钮/对话框字段相关 JSON 键名（JsonVueKey 重开命名空间）与 HTTP 方法常量
 *     （JsonVueHttp 整体迁入）
 *
 * json_vue_model.h include 本头文件回导全部名字；键名与枚举值保持原样，
 * 序列化实现仍位于 json_vue_model.cpp（同一编译目标，链接可见）。
 */

#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

/// HTTP 方法常量（接口配置 dataMethod 使用；自 json_vue_model.h 整体迁入）
namespace JsonVueHttp {
inline constexpr const char *kGet = "GET";
inline constexpr const char *kPost = "POST";
inline constexpr const char *kPut = "PUT";
inline constexpr const char *kDelete = "DELETE";
}  // namespace JsonVueHttp

/// 操作按钮相关 JSON 键名（JsonVueKey 重开命名空间；自 json_vue_model.h 迁入）
namespace JsonVueKey {
// 操作按钮（ButtonConfig / DialogFieldConfig）
inline constexpr const char *kLabel = "label";
inline constexpr const char *kFieldName = "fieldName";
inline constexpr const char *kIcon = "icon";
inline constexpr const char *kPosition = "position";
inline constexpr const char *kButtonType = "buttonType";
inline constexpr const char *kActionType = "actionType";
inline constexpr const char *kActionKey = "actionKey";
inline constexpr const char *kApiName = "apiName";
inline constexpr const char *kConfirmText = "confirmText";
inline constexpr const char *kDialogTitle = "dialogTitle";
inline constexpr const char *kDialogApi = "dialogApi";
inline constexpr const char *kDialogFields = "dialogFields";
inline constexpr const char *kLinkPath = "linkPath";
}  // namespace JsonVueKey

/// 枚举序列化字符串值常量（写入 .jsonvue / .jsontable / 生成代码时使用；
/// 自 json_vue_model.h 整体迁入，经 include 回导名字不变）
namespace JsonVueStyle {
inline constexpr const char *kText = "text";
inline constexpr const char *kSwitch = "switch";
inline constexpr const char *kInt = "int";
inline constexpr const char *kFloat = "float";
inline constexpr const char *kMoney = "money";
inline constexpr const char *kDate = "date";
inline constexpr const char *kDatetime = "datetime";
inline constexpr const char *kMonth = "month";
inline constexpr const char *kYear = "year";
inline constexpr const char *kDaterange = "daterange";
inline constexpr const char *kRange = "range";
inline constexpr const char *kTag = "tag";
inline constexpr const char *kBoolean = "boolean";
inline constexpr const char *kImage = "image";
inline constexpr const char *kSelect = "select";
inline constexpr const char *kTextarea = "textarea";
inline constexpr const char *kToolbar = "toolbar";
inline constexpr const char *kRow = "row";
inline constexpr const char *kAjax = "ajax";
inline constexpr const char *kConfirm = "confirm";
inline constexpr const char *kDialog = "dialog";
inline constexpr const char *kLink = "link";
/// 兼容旧名称（"time" → Date 查询输入框）
inline constexpr const char *kTime = "time";
}  // namespace JsonVueStyle

/// 编辑界面样式（编辑页控件类型；自 json_vue_model.h 迁入）
enum class EditStyle {
  Text = 0,     ///< 纯文本
  Int = 1,      ///< 整数
  Float = 2,    ///< 小数
  Money = 3,    ///< 金额（数字输入 + 小数位）
  Date = 4,     ///< 日期
  Tag = 5,      ///< 标签（下拉选择 tagItems）
  Boolean = 6,  ///< 布尔（下拉选择真假文字）
  Image = 7,    ///< 图片（URL 输入）
  Select = 8,   ///< 下拉框（远程数据源）
  TextArea = 9  ///< 多行文本
};

/// 按钮位置
enum class ButtonPosition {
  Row = 0,     ///< 行操作列（与编辑/删除同行）
  Toolbar = 1  ///< 顶部工具栏（与新增/删除同行）
};

/// 按钮行为类型
enum class ButtonActionType {
  Ajax = 0,     ///< 直接调 API（如禁用用户）
  Confirm = 1,  ///< 二次确认后调 API（如重置密码）
  Dialog = 2,   ///< 打开对话框填表单再提交（如修改密码）
  Link = 3      ///< 跳转页面
};

/// 编辑样式 / 按钮枚举 ↔ 字符串转换（实现见 button_types.cpp）
QString editStyleToString(EditStyle style);
EditStyle stringToEditStyle(const QString &s);
QString buttonPositionToString(ButtonPosition p);
ButtonPosition stringToButtonPosition(const QString &s);
QString buttonActionTypeToString(ButtonActionType t);
ButtonActionType stringToButtonActionType(const QString &s);

/**
 * @struct DialogFieldConfig
 * @brief 自定义对话框内的表单字段配置（轻量版 ColumnConfig，复用 EditStyle）
 */
struct DialogFieldConfig {
  QString fieldName;                      ///< 字段名（提交给 API 的 key）
  QString label;                          ///< 显示标签
  EditStyle editStyle = EditStyle::Text;  ///< 编辑样式（复用 EditStyle 枚举）
  bool required = false;                  ///< 是否必填
  QString placeholder;                    ///< 占位提示（text/textarea）
  int maxlength = 0;                      ///< 最大长度（text，0=不限）
  int textareaRows = 3;                   ///< 行数（textarea）
  double minValue = 0;                    ///< 最小值（int/float）
  double maxValue = 0;                    ///< 最大值（int/float）
  int precision = 2;                      ///< 小数位数（float）
  QString dateFormat;                     ///< 日期格式（date）
  QString selectUrl;                      ///< 下拉框数据源 URL（select）
  QString selectSourceFile;               ///< 引用的 .jsonsource 文件路径（select）
  QString selectSourceId;                 ///< 引用的 .jsonsource 数据源 id（select）
  QString selectValueField;               ///< 下拉框 Value 字段名（select）
  QString selectLabelField;               ///< 下拉框 Label 字段名（select）
  bool selectPaged = false;               ///< 是否查询分页加载（select）
  QString selectPageKey = QStringLiteral("page");      ///< 页码参数名（select，默认 "page"）
  QString selectPageSizeKey = QStringLiteral("pageSize");  ///< 页大小参数名（select，默认 "pageSize"）
  int selectPageSize = 20;                ///< 默认页大小（select）
  QString selectSearchTitle;              ///< 查询标题（select，查询分页时搜索框提示）
  QString selectSearchField;              ///< 字段名（select，查询分页时搜索参数 key）
  QString selectMethod = QString::fromLatin1(JsonVueHttp::kPost);  ///< 查询请求方式（GET/POST）

  /// 序列化为 JSON
  QJsonObject toJson() const;
  /// 从 JSON 反序列化
  static DialogFieldConfig fromJson(const QJsonObject &obj);
};

/// 逐字段相等比较（jsontable 表级 QVector<ButtonConfig> 比较依赖）
inline bool operator==(const DialogFieldConfig &a, const DialogFieldConfig &b) {
  return a.fieldName == b.fieldName && a.label == b.label && a.editStyle == b.editStyle &&
         a.required == b.required && a.placeholder == b.placeholder &&
         a.maxlength == b.maxlength && a.textareaRows == b.textareaRows &&
         a.minValue == b.minValue && a.maxValue == b.maxValue && a.precision == b.precision &&
         a.dateFormat == b.dateFormat && a.selectUrl == b.selectUrl &&
         a.selectSourceFile == b.selectSourceFile && a.selectSourceId == b.selectSourceId &&
         a.selectValueField == b.selectValueField && a.selectLabelField == b.selectLabelField &&
         a.selectPaged == b.selectPaged && a.selectPageKey == b.selectPageKey &&
         a.selectPageSizeKey == b.selectPageSizeKey && a.selectPageSize == b.selectPageSize &&
         a.selectSearchTitle == b.selectSearchTitle &&
         a.selectSearchField == b.selectSearchField && a.selectMethod == b.selectMethod;
}

/**
 * @struct ButtonConfig
 * @brief 自定义操作按钮配置
 *
 * 支持在顶部工具栏或行操作列添加自定义按钮，按钮行为支持：
 * - Ajax：直接调 API
 * - Confirm：二次确认后调 API
 * - Dialog：打开对话框填表单再提交
 * - Link：跳转页面
 */
struct ButtonConfig {
  // ── 外观 ──
  QString label;            ///< 按钮文字（如"修改密码"）
  QString icon;             ///< 图标名（如"vi-mingcute:key-line"，可为空）
  ButtonPosition position;  ///< 按钮位置（行/工具栏）
  QString buttonType;       ///< 按钮样式（""/primary/success/danger/warning）

  // ── 行为 ──
  ButtonActionType actionType;  ///< 行为类型
  QString actionKey;            ///< 动作唯一标识（生成的处理函数用，如"resetPwd"）

  // Ajax / Confirm 行为专用
  QString apiName;      ///< API 函数名（如"resetUserPasswordApi"）
  QString confirmText;  ///< Confirm: 确认提示文字（如"确定重置该用户密码？"）

  // Dialog 行为专用
  QString dialogTitle;                      ///< 对话框标题
  QString dialogApi;                        ///< 对话框提交 API 函数名
  QVector<DialogFieldConfig> dialogFields;  ///< 对话框表单字段

  // Link 行为专用
  QString linkPath;  ///< 跳转路径（如"/user/log?id={id}"，{id} 用行数据替换）

  /// 序列化为 JSON
  QJsonObject toJson() const;
  /// 从 JSON 反序列化
  static ButtonConfig fromJson(const QJsonObject &obj);
};

/// 逐字段相等比较（jsontable 表级 QVector<ButtonConfig> 比较依赖）
inline bool operator==(const ButtonConfig &a, const ButtonConfig &b) {
  return a.label == b.label && a.icon == b.icon && a.position == b.position &&
         a.buttonType == b.buttonType && a.actionType == b.actionType &&
         a.actionKey == b.actionKey && a.apiName == b.apiName &&
         a.confirmText == b.confirmText && a.dialogTitle == b.dialogTitle &&
         a.dialogApi == b.dialogApi && a.dialogFields == b.dialogFields &&
         a.linkPath == b.linkPath;
}
