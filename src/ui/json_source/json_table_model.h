/**
 * @file json_table_model.h
 * @brief .jsontable 文件数据模型（统一 API 配置）
 *
 * .jsontable 是新框架（Go + pure-admin）双端代码生成的事实源：
 * 一份配置同时描述 MySQL 表结构（mysqlType/unsigned/nullable/default/
 * comment/index，建表与生成同源）与字段前端角色（list/search/form/required，
 * gva 式字段级配置），供表设计器编辑、DDL 同步建表与双端模板渲染共用，
 * 字段只配一遍。
 *
 * 顶层结构（plan 3.1）：
 *   meta（project + db 连接四件套/port/goBasePath/webBasePath）
 *   + tables[]（tableName/modelName/columns[]/indexes[]）。
 *
 * 未知键保真：进阶节点（i18n/globalEnumCols/joinTable 等）v1 不结构化，
 * 表级整体存入 JsonTableTable::extra；meta 下未知键存入 JsonTableConfig::extra；
 * 序列化时原样写回，向前兼容未来扩展键。
 */

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

// ════════════════════════════════════════════════════════════
//  .jsontable 序列化键名常量
//  model（toJson/fromJson）与 UI 层共用，避免硬编码字符串，
//  防止读写键名不一致导致数据丢失。
// ════════════════════════════════════════════════════════════

/// .jsontable 文件的 JSON 键名常量
namespace JsonTableKey {
// meta 键
inline constexpr const char *kMeta = "meta";
inline constexpr const char *kProject = "project";
inline constexpr const char *kDb = "db";
inline constexpr const char *kHost = "host";
inline constexpr const char *kPort = "port";
inline constexpr const char *kUser = "user";
inline constexpr const char *kPassword = "password";
inline constexpr const char *kDatabase = "database";
inline constexpr const char *kGoBasePath = "goBasePath";    ///< Go 项目根（父目录分组）
inline constexpr const char *kWebBasePath = "webBasePath";  ///< pure-admin 项目根
// tables 键
inline constexpr const char *kTables = "tables";
inline constexpr const char *kTableName = "tableName";
inline constexpr const char *kModelName = "modelName";
inline constexpr const char *kTableComment = "tableComment";
inline constexpr const char *kManualUpdateTime = "manualUpdateTime";
inline constexpr const char *kColumns = "columns";
inline constexpr const char *kIndexes = "indexes";
// 列键（kName 同时用于列名与索引名）
inline constexpr const char *kName = "name";
inline constexpr const char *kMysqlType = "mysqlType";
inline constexpr const char *kUnsigned = "unsigned";
inline constexpr const char *kNullable = "nullable";
inline constexpr const char *kIsPrimary = "isPrimary";
inline constexpr const char *kIsAutoInc = "isAutoInc";
inline constexpr const char *kDefault = "default";  ///< 空串/缺省/null=无默认值
inline constexpr const char *kComment = "comment";
// 列前端角色（gva 式，字段级）
inline constexpr const char *kList = "list";      ///< 列表页显示
inline constexpr const char *kSearch = "search";  ///< 查询项
inline constexpr const char *kForm =
    "form";  ///< 表单角色：none/input/textarea/select/date/number/image/upload/boolean
inline constexpr const char *kRequired = "required";  ///< 表单必填
// 索引键
inline constexpr const char *kCols = "cols";
inline constexpr const char *kUnique = "unique";
}  // namespace JsonTableKey

/**
 * @struct JsonTableColumn
 * @brief .jsontable 中的一列：MySQL 元数据 + 前端角色（建表与生成同源）
 */
struct JsonTableColumn {
  QString name;                                  ///< 列名
  QString mysqlType = QStringLiteral("bigint");  ///< MySQL 类型，如 bigint/varchar(64)
  bool isUnsigned = false;  ///< 无符号（JSON 键 "unsigned"；与 isPrimary 同风格命名）
  bool nullable = false;    ///< 允许 NULL
  bool isPrimary = false;   ///< 主键
  bool isAutoInc = false;   ///< 自增
  QString defaultValue;     ///< 默认值（JSON 键 "default"；空串=无默认值）
  QString comment;          ///< 列注释
  // ── 前端角色 ──
  bool list = true;     ///< 列表页显示
  bool search = false;  ///< 查询项
  QString form = QStringLiteral(
      "input");           ///< 表单角色：none/input/textarea/select/date/number/image/upload/boolean
  bool required = false;  ///< 表单必填

  /// 序列化为 JSON 对象
  QJsonObject toJsonObject() const;
  /// 从 JSON 对象反序列化
  static JsonTableColumn fromJsonObject(const QJsonObject &obj);
  /// 逐字段相等比较
  bool operator==(const JsonTableColumn &other) const;
};

/**
 * @struct JsonTableIndex
 * @brief .jsontable 中的一条表索引
 */
struct JsonTableIndex {
  QString name;         ///< 索引名（如 uk_name）
  QStringList cols;     ///< 索引列名列表
  bool unique = false;  ///< 唯一索引

  /// 序列化为 JSON 对象
  QJsonObject toJsonObject() const;
  /// 从 JSON 对象反序列化
  static JsonTableIndex fromJsonObject(const QJsonObject &obj);
  /// 逐字段相等比较
  bool operator==(const JsonTableIndex &other) const;
};

/**
 * @struct JsonTableTable
 * @brief .jsontable 中的一张表
 *
 * extra 为未知键保真存储：i18n/globalEnumCols/joinTable 等进阶节点
 * v1 不结构化，反序列化时整体保留（剔除已知键），序列化时原样写回。
 */
struct JsonTableTable {
  QString tableName;               ///< 物理表名
  QString modelName;               ///< 模型名（生成代码用的标识）
  QString tableComment;            ///< 表注释
  bool manualUpdateTime = false;   ///< 手动维护更新时间（不走 DB ON UPDATE）
  QVector<JsonTableColumn> columns;  ///< 列定义（顺序即建表顺序）
  QVector<JsonTableIndex> indexes;   ///< 索引定义
  QJsonObject extra;               ///< 未知键保真（进阶节点/未来扩展键原样写回）

  /// 序列化为 JSON 对象（extra 铺底 + 已知键覆盖）
  QJsonObject toJsonObject() const;
  /// 从 JSON 对象反序列化（已知键外的内容全部进 extra）
  static JsonTableTable fromJsonObject(const QJsonObject &obj);
  /// 逐字段相等比较（含 extra）
  bool operator==(const JsonTableTable &other) const;
};

/**
 * @class JsonTableConfig
 * @brief .jsontable 文件完整配置
 */
class JsonTableConfig {
public:
  QString metaProject;           ///< meta.project 项目名
  QString dbHost;                ///< meta.db.host
  int dbPort = 3306;             ///< meta.db.port（缺省 3306）
  QString dbUser;                ///< meta.db.user
  QString dbPassword;            ///< meta.db.password
  QString dbDatabase;            ///< meta.db.database
  QString goBasePath;            ///< meta.goBasePath（Go 项目根）
  QString webBasePath;           ///< meta.webBasePath（pure-admin 项目根）
  QVector<JsonTableTable> tables;  ///< 表定义数组
  QJsonObject extra;             ///< meta 下未知键保真（剔除已知键后原样写回）

  /// 序列化为 JSON 对象（meta/extra 铺底 + 已知键覆盖）
  QJsonObject toJsonObject() const;

  /// 序列化为 JSON 文档字符串（紧凑格式）
  QString toJsonString() const;
  /// 将给定的完整 JSON 对象序列化为 JSON 文档字符串（紧凑格式）
  static QString toJsonString(const QJsonObject &root);

  /// 从 JSON 字符串反序列化（失败时 error 非空）
  static JsonTableConfig fromJsonString(const QString &jsonStr, QString *error = nullptr);
  /// 从 JSON 对象反序列化
  static JsonTableConfig fromJson(const QJsonObject &obj);
};
