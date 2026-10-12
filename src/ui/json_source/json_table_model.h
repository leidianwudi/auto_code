/**
 * @file json_table_model.h
 * @brief .jsontable 文件数据模型（统一 API 配置）
 *
 * .jsontable 是新框架（Go + pure-admin）双端代码生成的事实源：
 * 一份配置同时描述 MySQL 表结构（mysqlType/unsigned/nullable/default/
 * comment/index，建表与生成同源）、联表声明（joinTable，多对一/一对多）
 * 与查询读模型（selCols 列表列 + selColsEq/Like/Gap/Sort 查询条件，列引用
 * 支持联表路径）及编辑表单角色（form/required，gva 式字段级配置），
 * 供表设计器编辑、DDL 同步建表与双端模板渲染共用，字段只配一遍。
 *
 * 顶层结构（plan 3.1 + 联表/读模型 v3）：
 *   meta（project + db 连接四件套/port/goBasePath/webBasePath）
 *   + tables[]（tableName/modelName/columns[]/indexes[]/joinTable[]/
 *   selCols[]/selColsEq/Like/Gap/Sort/enums[]/globalEnumCols[]/i18n）。
 *
 * 未知键保真：进阶节点未结构化部分表级整体存入 JsonTableTable::extra；
 * meta 下未知键存入 JsonTableConfig::extra；序列化时原样写回，
 * 向前兼容未来扩展键。
 */

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QPair>
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
// 列前端角色（gva 式，字段级）：列表/搜索/表单角色全部退役——页面呈现由
// 表级三视图接管（selCols 列表 / selColsEq/Like/Gap 查询 / editCols 编辑）
// 索引键
inline constexpr const char *kCols = "cols";
inline constexpr const char *kUnique = "unique";
// 表级进阶键（v2 结构化；此前走 extra 未知键保真）
inline constexpr const char *kJoinTable = "joinTable";  ///< 联表声明（多对一列表联查/一对多详情聚合）
inline constexpr const char *kJoinEnClass = "joinEnClass";  ///< join 目标实体/模型名（=目标表 modelName）
inline constexpr const char *kJoinEnClassPro =
    "joinEnClassPro";                 ///< 联查引用名：SQL 别名前缀 + 前端 ref 前缀（旧 .json 同名）
inline constexpr const char *kJoinType = "joinType";  ///< ManyToOne（列表联查）/ OneToMany（详情聚合）
inline constexpr const char *kJoinFk = "joinFk";      ///< 本表外键列（∈ columns.name）
inline constexpr const char *kJoinPk = "joinPk";      ///< 目标表主键列（缺省 id）
inline constexpr const char *kIsJoinRelations =
    "isJoinRelations";  ///< true=有效声明；false=该条目不生效（旧 .json 同名）
inline constexpr const char *kSelCols = "selCols";      ///< 列表读模型（[{ref,label,width}]，ref 支持联表路径）
inline constexpr const char *kSelColsEq = "selColsEq";  ///< 精确查询列（[{ref,visible}]，支持联表路径）
inline constexpr const char *kSelColsGap = "selColsGap";        ///< 范围查询列（展开 _min/_max）
inline constexpr const char *kSelColsLike = "selColsLike";      ///< 模糊查询列（LIKE %x%）
inline constexpr const char *kSelColsSort = "selColsSort";      ///< 默认排序（[["id","DESC"]]）
inline constexpr const char *kEditCols = "editCols";  ///< 编辑视图（[{ref,label,visible,widget,required}]）
inline constexpr const char *kRef = "ref";  ///< 视图列引用（本表列名 或 joinEnClassPro.column）
inline constexpr const char *kVisible = "visible";  ///< 前端显示（true=渲染；false=仅后端生成，UI 不渲染）
inline constexpr const char *kWidth = "width";      ///< 列表列宽 px（0/缺省 = 自适应）
inline constexpr const char *kWidget = "widget";    ///< 编辑控件类型（空=按 mysqlType 推导）
inline constexpr const char *kRequired = "required";  ///< 编辑视图必填（→ InsertParams binding）
inline constexpr const char *kEnums = "enums";                  ///< 表内枚举
inline constexpr const char *kGlobalEnumCols = "globalEnumCols";  ///< 引用全局枚举的列名
inline constexpr const char *kI18n = "i18n";                    ///< 多语言翻译表配置
// 枚举键（kName 复用列名常量；kComment 复用列注释常量）
inline constexpr const char *kColumn = "column";  ///< 枚举对应列名
inline constexpr const char *kItems = "items";    ///< 枚举选项数组
inline constexpr const char *kKey = "key";        ///< 枚举成员名（缺省按序号兜底）
inline constexpr const char *kLabel = "label";    ///< 枚举项说明
inline constexpr const char *kValue = "value";    ///< 枚举值
inline constexpr const char *kValueType = "valueType";  ///< ""=string / "number"
// i18n 键
inline constexpr const char *kI18nTable = "table";        ///< 翻译表表名（空=未启用）
inline constexpr const char *kExtKey = "extKey";          ///< 翻译表外键列
inline constexpr const char *kLangKey = "langKey";        ///< 语言码列
inline constexpr const char *kFields = "fields";          ///< 参与多语言的文本列
inline constexpr const char *kDefaultLang = "defaultLang";  ///< 默认语言（缺省 zh）
inline constexpr const char *kLangFrom = "langFrom";      ///< 语言码来源 body/header
}  // namespace JsonTableKey

/**
 * @struct JsonTableColumn
 * @brief .jsontable 中的一列：纯 MySQL 元数据（建表与生成同源）
 *
 * 页面呈现（标题/显隐/控件）全部由表级三视图接管：selCols（列表）/
 * selColsEq/Like/Gap（查询）/ editCols（编辑）；历史 list/search/form/
 * required 列角色均已退役。
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

  /// 序列化为 JSON 对象
  QJsonObject toJsonObject() const;
  /// 从 JSON 对象反序列化
  static JsonTableColumn fromJsonObject(const QJsonObject &obj);
  /// 逐字段相等比较
  bool operator==(const JsonTableColumn &other) const;
};

/**
 * @struct JsonTableJoin
 * @brief 联表声明（旧 .json joinTable 同名参数族）
 *
 * ManyToOne（多对一列表联查）：本表持外键 joinFk → 目标表 joinPk，
 * 列表 SELECT 目标列平铺为 joinEnClassPro_column 别名；OneToMany
 * （一对多详情聚合）v1 仅声明不生成。
 */
struct JsonTableJoin {
  QString joinEnClass;     ///< join 目标实体/模型名（= 目标表 modelName，同文件 tables[] 定位）
  QString joinEnClassPro;  ///< 联查引用名（SQL 别名前缀 + 前端 ref 前缀，如 user）
  QString joinType = QStringLiteral("ManyToOne");  ///< ManyToOne / OneToMany
  QString joinFk;                                  ///< 本表外键列（∈ columns.name）
  QString joinPk = QStringLiteral("id");           ///< 目标表主键列（缺省 id）
  bool isJoinRelations = true;                     ///< true=有效声明；false=条目不生效

  QJsonObject toJsonObject() const;
  static JsonTableJoin fromJsonObject(const QJsonObject &obj);
  bool operator==(const JsonTableJoin &other) const;
};

/**
 * @struct JsonTableSelCol
 * @brief 列表视图的一列（selCols 数组元素）
 */
struct JsonTableSelCol {
  QString ref;    ///< 列引用：本表列名 或 joinEnClassPro.column（联表路径）
  QString label;  ///< 显示名（空 = 生成侧回退列注释/列名）
  int width = 0;  ///< 列宽 px（0/缺省 = 自适应）

  QJsonObject toJsonObject() const;
  static JsonTableSelCol fromJsonObject(const QJsonObject &obj);
  bool operator==(const JsonTableSelCol &other) const;
};

/**
 * @struct JsonTableQueryCol
 * @brief 查询视图的一列（selColsEq/Like/Gap 数组元素）
 *
 * visible=false = 后端条件照常生成（API 能力保留），前端不渲染输入框。
 * 查询输入框标题统一取：列表 selCols.label（同 ref 时）> 列注释 > ref。
 */
struct JsonTableQueryCol {
  QString ref;         ///< 列引用：本表列名 或 joinEnClassPro.column
  bool visible = true;  ///< 前端是否渲染查询输入框

  QJsonObject toJsonObject() const;
  static JsonTableQueryCol fromJsonObject(const QJsonObject &obj);
  bool operator==(const JsonTableQueryCol &other) const;
};

/**
 * @struct JsonTableEditCol
 * @brief 编辑视图的一列（editCols 数组元素；仅物理列，联表列只读进不来）
 *
 * 显式列出的列 = 进编辑表单（未列出 = 不进，原 form:"none" 语义被取代）；
 * widget 空 = 按 mysqlType 推导（varchar→input、int/decimal→number、
 * datetime→date、tinyint(1)→boolean）；未配置 editCols 时缺省全部可写列
 * 进表单、控件按类型推导（零配置等价旧行为）。
 */
struct JsonTableEditCol {
  QString ref;          ///< 列引用（仅本表物理列名）
  QString label;        ///< 编辑标题（空 = 回退列注释/列名）
  bool visible = true;  ///< 前端是否渲染表单项（false=参数仍生成，仅 UI 不渲染）
  QString widget;       ///< 控件类型：input/textarea/select/date/number/image/upload/boolean
  bool required = false;  ///< 必填（→ InsertParams binding:"required"）

  QJsonObject toJsonObject() const;
  static JsonTableEditCol fromJsonObject(const QJsonObject &obj);
  bool operator==(const JsonTableEditCol &other) const;
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
 * @struct JsonTableEnumOption
 * @brief 表内枚举的一个选项（形状与 .jsonglobalenum 的 options 一致）
 */
struct JsonTableEnumOption {
  QString key;        ///< 枚举成员名（snake_case，生成 Go const 转帕斯卡；空=生成侧按序号兜底）
  QString label;      ///< 说明（Go 注释与前端标签文字）
  QString value;      ///< 枚举值（valueType=number 时数字字面量，其余字符串）
  QString valueType;  ///< ""=string / "number"

  QJsonObject toJsonObject() const;
  static JsonTableEnumOption fromJsonObject(const QJsonObject &obj);
  bool operator==(const JsonTableEnumOption &other) const;
};

/**
 * @struct JsonTableEnum
 * @brief 表内枚举（某列的封闭值域：Go const + Map 与前端标签/下拉同源）
 */
struct JsonTableEnum {
  QString column;  ///< 枚举对应的列名
  QString comment;  ///< 说明（缺省回退列注释）
  QVector<JsonTableEnumOption> options;  ///< 选项列表

  QJsonObject toJsonObject() const;
  static JsonTableEnum fromJsonObject(const QJsonObject &obj);
  bool operator==(const JsonTableEnum &other) const;
};

/**
 * @struct JsonTableI18n
 * @brief 多语言翻译表配置（主表 + 翻译表联合查询/upsert/级联删除）
 */
struct JsonTableI18n {
  QString table;                ///< 翻译表表名（空 = 未启用）
  QString extKey;               ///< 翻译表中关联主表主键的外键列（如 ext_id）
  QString langKey;              ///< 语言码列（如 lang）
  QStringList fields;           ///< 参与多语言的翻译表文本列（如 ["name","intro"]）
  QString defaultLang = QStringLiteral("zh");  ///< 默认语言（回退目标）
  QString langFrom = QStringLiteral("body");   ///< 语言码来源：body / header

  bool isEnabled() const { return !table.isEmpty(); }
  QJsonObject toJsonObject() const;
  static JsonTableI18n fromJsonObject(const QJsonObject &obj);
  bool operator==(const JsonTableI18n &other) const;
};

/**
 * @struct JsonTableTable
 * @brief .jsontable 中的一张表
 *
 * 读模型三层：物理列（columns，DDL + 编辑表单来源）、联表声明
 * （joinTable，多对一/一对多关系）、查询读模型（selCols 列表列 +
 * selColsEq/Like/Gap/Sort 查询条件，列引用支持联表路径）。
 * extra 为未知键保真存储：未结构化的进阶节点反序列化时整体保留
 * （剔除已知键），序列化时原样写回。
 */
struct JsonTableTable {
  QString tableName;               ///< 物理表名
  QString modelName;               ///< 模型名（生成代码用的标识）
  QString tableComment;            ///< 表注释
  bool manualUpdateTime = false;   ///< 手动维护更新时间（不走 DB ON UPDATE）
  QVector<JsonTableColumn> columns;  ///< 列定义（顺序即建表顺序）
  QVector<JsonTableIndex> indexes;   ///< 索引定义
  // ── 表级进阶键（v2 结构化，此前走 extra）──
  QVector<JsonTableJoin> joinTable;  ///< 联表声明（多对一列表联查/一对多详情聚合）
  QVector<JsonTableSelCol> selCols;  ///< 列表视图（ref 支持联表路径 joinEnClassPro.column）
  QVector<JsonTableQueryCol> selColsEq;   ///< 查询视图：精确（visible=false 仅后端）
  QVector<JsonTableQueryCol> selColsGap;  ///< 查询视图：范围（每列展开 _min/_max）
  QVector<JsonTableQueryCol> selColsLike; ///< 查询视图：模糊（LIKE %x%）
  QVector<JsonTableEditCol> editCols;  ///< 编辑视图（显式列出=进表单；空=全列按类型推导）
  QVector<QPair<QString, QString>> selColsSort;  ///< 默认排序（列名+ASC/DESC，首项为默认排序）
  QVector<JsonTableEnum> enums;    ///< 表内枚举（与 globalEnumCols 二选一配置来源）
  QStringList globalEnumCols;      ///< 引用全局枚举的列名（列名 = .jsonglobalenum 枚举 name）
  JsonTableI18n i18n;              ///< 多语言翻译表配置（table 空 = 未启用）
  QJsonObject extra;               ///< 未知键保真（未结构化节点/未来扩展键原样写回）

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
