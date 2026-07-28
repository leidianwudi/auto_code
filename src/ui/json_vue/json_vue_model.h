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
#include <QString>
#include <QVector>

/**
 * @struct JsonVueMeta
 * @brief 接口元数据配置
 */
struct JsonVueMeta {
  /// 生成数据 URL 的 HTTP 方法 ("GET" / "POST" 等)
  QString dataMethod = QStringLiteral("GET");
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

  /// 序列化为 JSON
  QJsonObject toJson() const;
  /// 从 JSON 反序列化
  static JsonVueMeta fromJson(const QJsonObject &obj);
};

/// 列表界面样式
enum class ListStyle {
  Text = 0,    ///< txt 样式（文本）
  Switch = 1   ///< 开关样式
};

/// 编辑界面样式
enum class EditStyle {
  Text = 0,      ///< 文本输入
  Int = 1,       ///< 整型输入
  Float = 2,     ///< 浮点型输入
  Date = 3,      ///< 日期输入
  ComboBox = 4,  ///< 下拉列表输入
  TextArea = 5   ///< 多行文本输入
};

/// 查询关系
enum class QueryRelation {
  Equal = 0,           ///< 等于
  Like = 1,            ///< 模糊
  GreaterEqual = 2,    ///< 大于等于
  LessEqual = 3,       ///< 小于等于
  Greater = 4,         ///< 大于
  Less = 5             ///< 小于
};

/// 查询输入框样式
enum class QueryInputStyle {
  Text = 0,   ///< txt 输入框
  Time = 1    ///< 时间输入框
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
  /// 输入框样式（txt/时间）
  QueryInputStyle inputStyle = QueryInputStyle::Text;
  /// 查询关系
  QueryRelation relation = QueryRelation::Equal;

  /// 序列化为 JSON
  QJsonObject toJson() const;
  /// 从 JSON 反序列化
  static QueryFieldConfig fromJson(const QJsonObject &obj);
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

  /// 判断是否为空配置
  bool isEmpty() const { return meta.dataUrl.isEmpty() && columns.isEmpty(); }

  /// 序列化为 JSON 文档字符串（紧凑格式）
  QString toJsonString() const;

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
