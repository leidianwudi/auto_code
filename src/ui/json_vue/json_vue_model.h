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

#include "../common/button_types.h"  // EditStyle/ButtonConfig/DialogFieldConfig 等共享类型回导

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
inline constexpr const char *kSelectSourceFile =
    "selectSourceFile";                                           ///< 引用的 .jsonsource 文件路径
inline constexpr const char *kSelectSourceId = "selectSourceId";  ///< 引用的数据源 id
inline constexpr const char *kSelectPaged = "selectPaged";        ///< 下拉框是否查询分页加载
inline constexpr const char *kSelectPageKey = "selectPageKey";    ///< 页码参数名（默认 page）
inline constexpr const char *kSelectPageSizeKey =
    "selectPageSizeKey";                                          ///< 页大小参数名（默认 pageSize）
inline constexpr const char *kSelectPageSize = "selectPageSize";  ///< 默认页大小
inline constexpr const char *kSelectSearchTitle = "selectSearchTitle";  ///< 查询标题（搜索框提示）
inline constexpr const char *kSelectSearchField = "selectSearchField";  ///< 字段名（搜索参数key）
inline constexpr const char *kSelectMethod = "selectMethod";  ///< 查询请求方式（GET/POST）
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
inline constexpr const char *kBoolSourceFile = "boolSourceFile";  ///< boolean 引用的静态数据源文件
inline constexpr const char *kBoolSourceId = "boolSourceId";      ///< boolean 引用的静态数据源 id
// 字段取值域（方案 B：字段层声明一次选项/数据源，列渲染/编辑控件/查询筛选三处共用）
inline constexpr const char *kDomainType = "domainType";  ///< 取值域类型（enum/static/remote，空=未声明）
inline constexpr const char *kDomainSourceFile =
    "domainSourceFile";  ///< enum/static 引用的数据源文件（全局枚举固定基名 global_enum.jsonsource）
inline constexpr const char *kDomainSourceId = "domainSourceId";  ///< enum/static 引用的数据源 id
inline constexpr const char *kDomainUrl = "domainUrl";  ///< remote 手动数据源 URL
inline constexpr const char *kDomainInherit =
    "domainInherit";  ///< 查询筛选是否沿用同名列取值域（仅 QueryFieldConfig，缺省 true）
inline constexpr const char *kUploadSourceFile =
    "uploadSourceFile";  ///< image 引用的 .jsonupload 文件路径
inline constexpr const char *kUploadSourceId = "uploadSourceId";  ///< image 引用的上传预设 id
// 图片缩略图尺寸（字段级覆盖；0=未配置 → 回退上传预设尺寸 → 模板默认）
inline constexpr const char *kListThumbWidth = "listThumbWidth";    ///< 列表缩略图宽 px；0=跟随预设（默认 80）
inline constexpr const char *kListThumbHeight = "listThumbHeight";  ///< 列表缩略图高 px；0=跟随预设（默认 40）
inline constexpr const char *kEditThumbWidth = "editThumbWidth";    ///< 编辑/详情卡片宽 px；0=跟随预设（默认 160）
inline constexpr const char *kEditThumbHeight = "editThumbHeight";  ///< 编辑/详情卡片高 px；0=跟随预设（默认 80）
inline constexpr const char *kDefaultValue = "defaultValue";
inline constexpr const char *kDefaultSort = "defaultSort";

// TagItem
inline constexpr const char *kTagValue = "value";
inline constexpr const char *kTagText = "text";
inline constexpr const char *kTagColor = "color";

// 操作按钮相关键（kLabel..kLinkPath）与 HTTP 方法常量已迁至
// src/ui/common/button_types.h（本文件经 include 回导，名字不变）
}  // namespace JsonVueKey

/// 枚举序列化字符串值常量（JsonVueStyle）已迁至 src/ui/common/button_types.h

/// 字段取值域类型常量（ColumnConfig::domainType 的取值）
namespace JsonVueDomain {
inline constexpr const char *kEnum = "enum";      ///< 枚举（恰好 2 项的静态源，含全局枚举）
inline constexpr const char *kStatic = "static";  ///< 静态源（任意项数的静态源）
inline constexpr const char *kRemote = "remote";  ///< 远程源（手动 URL + 下拉参数）
}  // namespace JsonVueDomain

/// 标签 / 按钮配色名称常量（Element Plus 主题色）
namespace JsonVueColor {
inline constexpr const char *kPrimary = "primary";
inline constexpr const char *kSuccess = "success";
inline constexpr const char *kWarning = "warning";
inline constexpr const char *kInfo = "info";
inline constexpr const char *kDanger = "danger";
}  // namespace JsonVueColor

/// HTTP 方法常量（JsonVueHttp）已迁至 src/ui/common/button_types.h

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

/// 编辑界面样式（EditStyle）已迁至 src/ui/common/button_types.h

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
  /// 引用的 .jsonsource 文件路径（可为空，表示未引用 jsonsource，直接使用 selectUrl）
  QString selectSourceFile;
  /// 引用的 .jsonsource 数据源 id（selectSourceFile 非空时有效）
  QString selectSourceId;
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
  int formSpan = 12;  ///< 表单项占比（24=整行，12=半行，8=三分之一）

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
  /// boolean 引用的静态数据源（恰好 2 项选项的静态源，如 0:禁用/1:启用）。
  /// 引用时真假文字从数据源读取并锁定不可改；空 = 手动输入。
  /// 注：方案 B 后仅为旧格式兼容字段，新配置统一写 domain* 取值域键
  QString boolSourceFile;
  QString boolSourceId;

  // ── 字段取值域（方案 B：列渲染/编辑控件/查询筛选三处共用，声明一次）──
  /// 取值域类型（JsonVueDomain 常量；空 = 未声明，生成/编辑侧按旧键回退）
  QString domainType;
  /// enum/static 引用的数据源文件基名（全局枚举固定写 global_enum.jsonsource）
  QString domainSourceFile;
  /// enum/static 引用的数据源 id
  QString domainSourceId;
  /// remote 手动数据源 URL（下拉参数沿用 selectValueField 等键）
  QString domainUrl;
  /// image 编辑样式引用的上传预设（.jsonupload 文件 + 预设 id）。
  /// 引用时编辑页生成上传组件（el-upload + axios 上传），路径随表单 JSON 提交；
  /// 空 = 保持旧行为（URL 手工输入）
  QString uploadSourceFile;
  QString uploadSourceId;
  /// 图片缩略图尺寸（字段级覆盖；0=未配置 → 回退上传预设尺寸 → 模板默认）。
  /// 与同名上传预设键解耦：同一预设被多个字段引用时可有不同显示尺寸
  int listThumbWidth = 0;   ///< 列表页缩略图宽（成对 >0 时生效）
  int listThumbHeight = 0;  ///< 列表页缩略图高
  int editThumbWidth = 0;   ///< 编辑/详情卡片宽（成对 >0 时生效）
  int editThumbHeight = 0;  ///< 编辑/详情卡片高

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
  /// 引用的 .jsonsource 文件路径（可为空）
  QString selectSourceFile;
  /// 引用的 .jsonsource 数据源 id（selectSourceFile 非空时有效）
  QString selectSourceId;
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
  /// 查询筛选是否沿用同名列的取值域（方案 B，缺省 true）。
  /// true 时生成侧从同名列的取值域实时推导（含布尔枚举列，自动带「全部」空选项）；
  /// false 时使用本字段自身的 selectUrl/selectSourceFile 等覆盖键
  bool domainInherit = true;

  // ── 样式特定配置 ──
  QString placeholder;  ///< text: 占位提示文字
  QString dateFormat;   ///< date: 日期格式（datetime/date/month/year/daterange）

  /// 序列化为 JSON
  QJsonObject toJson() const;
  /// 从 JSON 反序列化
  static QueryFieldConfig fromJson(const QJsonObject &obj);
};

/// ButtonPosition / ButtonActionType / DialogFieldConfig / ButtonConfig
/// 已迁至 src/ui/common/button_types.h（本文件经 include 回导，名字不变）

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
/// （editStyle/buttonPosition/buttonActionType 六个转换函数已迁至
///   src/ui/common/button_types.h，经 include 回导名字不变）
QString listStyleToString(ListStyle style);
ListStyle stringToListStyle(const QString &s);
QString queryRelationToString(QueryRelation r);
QueryRelation stringToQueryRelation(const QString &s);
QString queryInputStyleToString(QueryInputStyle s);
QueryInputStyle stringToQueryInputStyle(const QString &s);

/// 获取默认的 tagItems（关闭/开启）
QList<TagItem> defaultTagItems();
