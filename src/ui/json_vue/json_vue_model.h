/**
 * @file json_vue_model.h
 * @brief .jsonvue 文件数据模型
 *
 * 定义 Vue3 后台管理界面配置的数据结构。
 * 一个 .jsonvue 文件包含：
 *   - meta: 接口配置（生成数据URL、查询/删除/修改接口等）
 *   - columns: 列配置数组（HTTP 返回的列名 + 查询/编辑界面配置）
 *   - queryFields: 查询字段数组（动态添加的查询列配置）
 */

#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVector>

// ════════════════════════════════════════════════════════════
//  .jsonvue 序列化键名 / 枚举字符串值常量
//  model（toJson/fromJson）与 UI 层共用，避免硬编码字符串，
//  防止读写键名不一致导致数据丢失。改名只需改此一处。
// ════════════════════════════════════════════════════════════

/// .jsonvue 文件的 JSON 键名常量
namespace JsonVueKey {
// 顶层结构
inline constexpr const char *kMeta = "meta";
inline constexpr const char *kColumns = "columns";
inline constexpr const char *kQueryFields = "queryFields";
inline constexpr const char *kButtons = "buttons";

// 接口配置（JsonVueMeta）
inline constexpr const char *kDataMethod = "dataMethod";
inline constexpr const char *kDataUrl = "dataUrl";
inline constexpr const char *kQueryApi = "queryApi";
inline constexpr const char *kDeleteApi = "deleteApi";
inline constexpr const char *kNoDelete = "noDelete";
inline constexpr const char *kUpdateApi = "updateApi";
inline constexpr const char *kNoEdit = "noEdit";
inline constexpr const char *kNoDetail = "noDetail";
inline constexpr const char *kDescription = "description";

// 列 / 查询字段 / 对话框字段 通用键
inline constexpr const char *kDataName = "dataName";
inline constexpr const char *kDisplayName = "displayName";
inline constexpr const char *kQueryVisible = "queryVisible";
inline constexpr const char *kQueryName = "queryName";
inline constexpr const char *kQueryStyle = "queryStyle";
inline constexpr const char *kSwitchEditable = "switchEditable";
inline constexpr const char *kEditVisible = "editVisible";
inline constexpr const char *kEditName = "editName";
inline constexpr const char *kEditStyle = "editStyle";
inline constexpr const char *kEditEditable = "editEditable";
inline constexpr const char *kInputStyle = "inputStyle";
inline constexpr const char *kRelation = "relation";
inline constexpr const char *kSelectUrl = "selectUrl";
inline constexpr const char *kSelectValueField = "selectValueField";
inline constexpr const char *kSelectLabelField = "selectLabelField";
inline constexpr const char *kSelectPaged = "selectPaged";            ///< 下拉框是否查询分页加载
inline constexpr const char *kSelectPageKey = "selectPageKey";        ///< 页码参数名（默认 page）
inline constexpr const char *kSelectPageSizeKey = "selectPageSizeKey";  ///< 页大小参数名（默认 pageSize）
inline constexpr const char *kSelectPageSize = "selectPageSize";      ///< 默认页大小
inline constexpr const char *kSelectSearchTitle = "selectSearchTitle";  ///< 查询标题（搜索框提示）
inline constexpr const char *kSelectSearchField = "selectSearchField";  ///< 字段名（搜索参数key）
inline constexpr const char *kSelectMethod = "selectMethod";            ///< 查询请求方式（GET/POST）
inline constexpr const char *kPlaceholder = "placeholder";
inline constexpr const char *kMaxlength = "maxlength";
inline constexpr const char *kMinValue = "minValue";
inline constexpr const char *kMaxValue = "maxValue";
inline constexpr const char *kPrecision = "precision";
inline constexpr const char *kDateFormat = "dateFormat";
inline constexpr const char *kTextareaRows = "textareaRows";
inline constexpr const char *kRequired = "required";
inline constexpr const char *kColumnWidth = "columnWidth";
inline constexpr const char *kColumnFixed = "columnFixed";
inline constexpr const char *kFormatter = "formatter";
inline constexpr const char *kFormSpan = "formSpan";
inline constexpr const char *kDisplayType = "displayType";
inline constexpr const char *kTagItems = "tagItems";
inline constexpr const char *kBoolTrueText = "boolTrueText";
inline constexpr const char *kBoolFalseText = "boolFalseText";
inline constexpr const char *kDefaultValue = "defaultValue";
inline constexpr const char *kDefaultSort = "defaultSort";

// TagItem
inline constexpr const char *kTagValue = "value";
inline constexpr const char *kTagText = "text";
inline constexpr const char *kTagColor = "color";

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

/// 枚举序列化字符串值常量（写入 .jsonvue / 生成代码时使用）
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

/// 标签 / 按钮配色名称常量（Element Plus 主题色）
namespace JsonVueColor {
inline constexpr const char *kPrimary = "primary";
inline constexpr const char *kSuccess = "success";
inline constexpr const char *kWarning = "warning";
inline constexpr const char *kInfo = "info";
inline constexpr const char *kDanger = "danger";
}  // namespace JsonVueColor

/// HTTP 方法常量（接口配置 dataMethod 使用）
namespace JsonVueHttp {
inline constexpr const char *kGet = "GET";
inline constexpr const char *kPost = "POST";
inline constexpr const char *kPut = "PUT";
inline constexpr const char *kDelete = "DELETE";
}  // namespace JsonVueHttp

/**
 * @struct TagItem
 * @brief 标签映射项（displayType == "tag" 时使用）
 *
 * 将数据值映射为显示文字和颜色，支持任意多个值。
 */
struct TagItem {
  QString value;  ///< 数据值（如 "0" / "1"）
  QString text;   ///< 显示文字（如 "关闭" / "开启"）
  QString color;  ///< 颜色（success/primary/warning/info/danger）

  /// 序列化为 JSON
  QJsonObject toJson() const;
  /// 从 JSON 反序列化
  static TagItem fromJson(const QJsonObject &obj);
};

/**
 * @struct JsonVueMeta
 * @brief 接口元数据配置
 */
struct JsonVueMeta {
  /// 生成数据 URL 的 HTTP 方法 ("GET" / "POST" 等)
  QString dataMethod = QString::fromLatin1(JsonVueHttp::kPost);
  /// 生成数据 URL（相对路径，会拼接 baseUrl）
  QString dataUrl;
  /// 查询接口名称，例如 "getConfigGListApi"
  QString queryApi;
  /// 删除接口名称，例如 "delConfigGListApi"
  QString deleteApi;
  /// 不可删除（打勾后不能删除数据）
  bool noDelete = false;
  /// 修改接口名称，例如 "updConfigGListApi"
  QString updateApi;
  /// 不可编辑（打勾后不能编辑数据）
  bool noEdit = false;
  /// 不可查看详情（打勾后不生成详情按钮）
  bool noDetail = false;
  /// 脚本说明（接口配置的说明文字）
  QString description;

  /// 序列化为 JSON
  QJsonObject toJson() const;
  /// 从 JSON 反序列化
  static JsonVueMeta fromJson(const QJsonObject &obj);
};

/// 列表界面样式（保留向后兼容，新设计改用 displayType 字段）
enum class ListStyle {
  Text = 0,   ///< txt 样式（文本）
  Switch = 1  ///< 开关样式
};

/// 编辑界面样式（编辑页控件类型）
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

/// 查询关系（普通 / 范围）
enum class QueryRelation {
  Equal = 0,  ///< 普通（单个输入框）
  Range = 1   ///< 范围（生成两个输入控件，如起始/结束）
};

/// 查询输入框样式
enum class QueryInputStyle {
  Text = 0,   ///< txt 输入框
  Date = 1,   ///< 日期输入框
  Select = 2  ///< 下拉选择框
};

/**
 * @struct ColumnConfig
 * @brief 列配置（对应 HTTP 返回的每个字段）
 */
struct ColumnConfig {
  /// 数据列名（HTTP 返回的列名，如 id、sort、key）
  QString dataName;
  /// 查询界面是否显示
  bool queryVisible = true;
  /// 查询界面列名
  QString queryName;
  /// 查询界面样式（txt/开关）
  ListStyle queryStyle = ListStyle::Text;
  /// 开关样式时是否可编辑
  bool switchEditable = false;
  /// 编辑界面是否显示
  bool editVisible = true;
  /// 编辑界面列名
  QString editName;
  /// 编辑界面样式
  EditStyle editStyle = EditStyle::Text;
  /// 编辑界面是否可编辑
  bool editEditable = true;
  /// 下拉框数据源 URL（仅 editStyle == Select 时使用）
  QString selectUrl;
  /// 下拉框 Value 字段名（实际值，仅 editStyle == Select 时使用）
  QString selectValueField;
  /// 下拉框 Label 字段名（显示文本，仅 editStyle == Select 时使用）
  QString selectLabelField;
  /// 是否查询分页加载（仅 editStyle == Select 时使用）
  bool selectPaged = false;
  /// 页码参数名（查询分页时使用，默认 "page"）
  QString selectPageKey;
  /// 页大小参数名（查询分页时使用，默认 "pageSize"）
  QString selectPageSizeKey;
  /// 默认页大小（查询分页时使用，默认 20）
  int selectPageSize = 20;
  /// 查询标题（查询分页时搜索框提示文字）
  QString selectSearchTitle;
  /// 字段名（查询分页时搜索参数 key）
  QString selectSearchField;
  /// 查询请求方式（GET/POST，查询分页时使用）
  QString selectMethod = QString::fromLatin1(JsonVueHttp::kPost);

  // ── 样式特定配置（仅对应 editStyle 时使用）──
  QString placeholder;   ///< text/textarea: 占位提示文字
  int maxlength = 0;     ///< text: 最大输入长度（0=不限）
  double minValue = 0;   ///< int/float: 最小值
  double maxValue = 0;   ///< int/float: 最大值
  int precision = 2;     ///< float: 小数位数
  QString dateFormat;    ///< date: 日期格式（datetime/date/month/year/daterange）
  int textareaRows = 3;  ///< textarea: 行数

  // ── 表单验证（3-1）──
  bool required = false;  ///< 编辑表单是否必填

  // ── 表格列设置（3-2）──
  int columnWidth = 0;  ///< 表格列宽（0=自动）
  QString columnFixed;  ///< 固定列（""/"left"/"right"）

  // ── 表格列格式化（3-3）──
  QString formatter;  ///< 格式化类型（""/"date"/"status"/"currency"）

  // ── 表单布局（3-4）──
  int formSpan = 24;  ///< 表单项占比（24=整行，12=半行，8=三分之一）

  // ── 表格列显示样式（3-5）──
  /// 表格列渲染方式（""/"text"/"money"/"tag"/"boolean"/"image"）
  /// text/""  - 纯文本（默认）
  /// money    - 金额格式化（千分位，如 1,234.56）
  /// tag      - 用 ElTag 显示（根据 tagItems 映射值→文字+颜色）
  /// boolean  - 布尔值转文字（用 boolTrueText/boolFalseText）
  /// image    - 用 ElImage 显示缩略图
  QString displayType;
  /// 标签映射数组（仅 displayType == "tag" 时使用）
  /// 默认值：[{value:"0",text:"关闭",color:"info"},{value:"1",text:"开启",color:"success"}]
  QList<TagItem> tagItems;
  QString boolTrueText;   ///< boolean: true 时显示的文字（如"显示"）
  QString boolFalseText;  ///< boolean: false 时显示的文字（如"隐藏"）

  // ── 通用配置（3-6）──
  QString defaultValue;  ///< 新增记录时的默认值（如 "1" / "0" / ""）
  QString defaultSort;   ///< 表格默认排序方向（""/"asc"/"desc"，仅一列设置有效）

  /// 序列化为 JSON
  QJsonObject toJson() const;
  /// 从 JSON 反序列化
  static ColumnConfig fromJson(const QJsonObject &obj);
};

/**
 * @struct QueryFieldConfig
 * @brief 查询字段配置（动态添加的查询列）
 */
struct QueryFieldConfig {
  /// 显示的列名
  QString displayName;
  /// 数据列名（从 columns 列表中选择）
  QString dataName;
  /// 输入框样式（txt/时间/下拉选择）
  QueryInputStyle inputStyle = QueryInputStyle::Text;
  /// 查询关系
  QueryRelation relation = QueryRelation::Equal;
  /// 下拉框数据源 URL（仅 inputStyle == Select 时使用）
  QString selectUrl;
  /// 下拉框 Value 字段名（实际值，仅 inputStyle == Select 时使用）
  QString selectValueField;
  /// 下拉框 Label 字段名（显示文本，仅 inputStyle == Select 时使用）
  QString selectLabelField;
  /// 是否查询分页加载（仅 inputStyle == Select 时使用）
  bool selectPaged = false;
  /// 页码参数名（查询分页时使用，默认 "page"）
  QString selectPageKey;
  /// 页大小参数名（查询分页时使用，默认 "pageSize"）
  QString selectPageSizeKey;
  /// 默认页大小（查询分页时使用，默认 20）
  int selectPageSize = 20;
  /// 查询标题（查询分页时搜索框提示文字）
  QString selectSearchTitle;
  /// 字段名（查询分页时搜索参数 key）
  QString selectSearchField;
  /// 查询请求方式（GET/POST，查询分页时使用）
  QString selectMethod = QString::fromLatin1(JsonVueHttp::kPost);

  // ── 样式特定配置 ──
  QString placeholder;  ///< text: 占位提示文字
  QString dateFormat;   ///< date: 日期格式（datetime/date/month/year/daterange）

  /// 序列化为 JSON
  QJsonObject toJson() const;
  /// 从 JSON 反序列化
  static QueryFieldConfig fromJson(const QJsonObject &obj);
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
  QString selectValueField;               ///< 下拉框 Value 字段名（select）
  QString selectLabelField;               ///< 下拉框 Label 字段名（select）
  bool selectPaged = false;               ///< 是否查询分页加载（select）
  QString selectPageKey;                  ///< 页码参数名（select，默认 "page"）
  QString selectPageSizeKey;              ///< 页大小参数名（select，默认 "pageSize"）
  int selectPageSize = 20;                ///< 默认页大小（select）
  QString selectSearchTitle;              ///< 查询标题（select，查询分页时搜索框提示）
  QString selectSearchField;              ///< 字段名（select，查询分页时搜索参数 key）
  QString selectMethod = QString::fromLatin1(JsonVueHttp::kPost);  ///< 查询请求方式（GET/POST）

  /// 序列化为 JSON
  QJsonObject toJson() const;
  /// 从 JSON 反序列化
  static DialogFieldConfig fromJson(const QJsonObject &obj);
};

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

/**
 * @class JsonVueConfig
 * @brief .jsonvue 文件完整配置
 */
class JsonVueConfig {
public:
  /// 元数据（接口配置）
  JsonVueMeta meta;
  /// 列配置数组
  QVector<ColumnConfig> columns;
  /// 查询字段配置数组
  QVector<QueryFieldConfig> queryFields;
  /// 自定义操作按钮配置数组
  QVector<ButtonConfig> buttons;

  /// 判断是否为空配置
  bool isEmpty() const { return meta.dataUrl.isEmpty() && columns.isEmpty(); }

  /// 序列化为 JSON 文档字符串（紧凑格式）
  QString toJsonString() const;

  /// 将给定的完整 JSON 对象序列化为 JSON 文档字符串（紧凑格式）
  /// 供可视化编辑“保真合并”后的结果直接序列化，避免经 toJsonObject() 二次往返丢失字段
  static QString toJsonString(const QJsonObject &root);

  /// 从 JSON 字符串反序列化
  static JsonVueConfig fromJsonString(const QString &jsonStr, QString *error = nullptr);

  /// 从 JSON 对象反序列化
  static JsonVueConfig fromJson(const QJsonObject &obj);

  /// 转为 JSON 对象
  QJsonObject toJsonObject() const;
};

/// 枚举与字符串的转换辅助函数
QString listStyleToString(ListStyle style);
ListStyle stringToListStyle(const QString &s);
QString editStyleToString(EditStyle style);
EditStyle stringToEditStyle(const QString &s);
QString queryRelationToString(QueryRelation r);
QueryRelation stringToQueryRelation(const QString &s);
QString queryInputStyleToString(QueryInputStyle s);
QueryInputStyle stringToQueryInputStyle(const QString &s);
QString buttonPositionToString(ButtonPosition p);
ButtonPosition stringToButtonPosition(const QString &s);
QString buttonActionTypeToString(ButtonActionType t);
ButtonActionType stringToButtonActionType(const QString &s);

/// 获取默认的 tagItems（关闭/开启）
QList<TagItem> defaultTagItems();
