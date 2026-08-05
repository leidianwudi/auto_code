/**
 * @file schema_validator.h
 * @brief JSON 数据 Schema 校验器（支持 JSON5 自定义格式）
 *
 * 从 JSON/JSON5 文件加载 schema 定义，校验 JSON 数据是否符合约束，
 * 并提供基于 schema 的智能提示（补全）能力。
 *
 * 支持的基本类型：int / string / double / bool
 * 支持的复合类型：array（元素为指定类名或基本类型） / object（嵌套类名）
 * 支持的约束：required（必填）、enum（枚举）、description（提示）、
 *            additionalProperties（任意键对象，用于排序配置等）
 *
 * ═══════════════════════════════════════════════════════════════
 *  Schema 文件格式（JSON5，支持注释/无引号键/单引号）
 * ═══════════════════════════════════════════════════════════════
 * {
 *   root: "AutoConfig",          // 根入口类名（校验整个文档的入口）
 *   definitions: {
 *     "AutoConfig": {
 *       properties: {
 *         tables: { type: "array", items: "TableConfig", description: "..." }
 *       },
 *       required: ["tables"]
 *     },
 *     "TableConfig": {
 *       properties: {
 *         modelName: { type: "string", description: "模块名" },
 *         selCols: { type: "array", items: "string", description: "..." },
 *         selColsSort: { type: "object", class: "SortConfig", description: "..." },
 *         joinType: { type: "string", enum: ["A", "B"], description: "..." }
 *       },
 *       required: ["modelName", "tableName"]
 *     },
 *     "SortConfig": {
 *       // 任意键对象：键不限，值只能是 ASC/DESC
 *       additionalProperties: { type: "string", enum: ["ASC", "DESC"] }
 *     }
 *   }
 * }
 *
 * 兼容旧格式：根对象直接以类名为键（无 root/definitions），
 * 此时根类取第一个类名，validateDocument 以它为根。
 */

#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @class SchemaValidator
 * @brief 基于类定义的 JSON 数据校验器 + 智能提示
 *
 * 使用方式：
 *   1. SchemaValidator v;
 *   2. v.load("schema/param.schema.json");
 *   3. QStringList errs = v.validateDocument(data);   // 校验整个文档
 *   4. QStringList comps = v.completions(text, pos);  // 位置智能提示
 */
class SchemaValidator {
public:
  SchemaValidator() = default;

  /**
   * @brief 加载 schema 定义文件（支持 JSON5 语法）
   * @param filePath schema 文件路径
   * @return true 加载成功
   */
  bool load(const QString &filePath);

  /**
   * @brief 校验 JSON 数据是否符合指定类定义
   * @param className 类名
   * @param data 待校验的 JSON 数据
   * @return 空字符串表示通过，否则返回第一个错误描述
   */
  QString validate(const QString &className, const QJsonObject &data) const;

  /**
   * @brief 校验整个文档是否符合根类（root）定义
   * @param data 待校验的 JSON 数据
   * @return 错误列表（空表示通过）
   */
  QVector<QString> validateDocument(const QJsonObject &data) const;

  /**
   * @brief 根据光标位置返回 schema 智能提示（属性名或枚举值）
   * @param text 当前 JSON5 文本
   * @param pos 光标位置（0-based）
   * @return 建议列表（属性名或枚举值）
   */
  QStringList completions(const QString &text, int pos) const;

  /**
   * @brief 获取所有已注册的类名
   */
  QStringList classNames() const;

  /**
   * @brief 判断指定类是否已注册
   */
  bool hasClass(const QString &className) const;

  /// 根类名（空表示未设置）
  QString rootClass() const { return m_rootClass; }

  /// 是否已设置根类
  bool hasRoot() const { return !m_rootClass.isEmpty(); }

  /**
   * @brief 根据 JSON 路径返回属性说明文本（供悬停提示）
   * @param jsonPath 形如 "tables.0.tableName"（数字段表示数组索引）
   * @return 格式化说明文本（类型 / 枚举 / 描述），找不到返回空串
   */
  QString propertyDescription(const QString &jsonPath) const;

  /**
   * @brief 根据 JSON 路径返回属性所在类名与属性名（供在 schema 文件中定位）
   * @param jsonPath 形如 "tables.0.tableName"
   * @param[out] className 属性所属的 schema 类名
   * @param[out] propName  叶子属性名
   * @return true 表示能在 schema 中解析到该属性
   */
  bool propertyContext(const QString &jsonPath, QString *className, QString *propName) const;

  /**
   * @brief 根据 JSON5 文本与光标位置返回该处属性的 JSON 路径
   *
   * 仅当光标位于某个属性键（key）上时返回路径（形如 "tables.0.tableName"，
   * 数组索引可省略，仅用于定位属性所属类）；光标位于值/结构字符上时返回空串。
   * @param text 当前 JSON5 文本
   * @param pos 光标位置（0-based）
   * @return 属性路径，非属性键位置返回空串
   */
  QString propertyPathAt(const QString &text, int pos) const;

private:
  struct PropertyDef {
    QString type;            // int / string / double / bool / array / object
    QString items;           // type=array 时，元素类名或基本类型名
    QString className;       // type=object 时，嵌套类名
    QStringList enumValues;  // 枚举值（string 类型）
    QString description;     // 属性说明（用于提示）
  };

  struct ClassDef {
    QMap<QString, PropertyDef> properties;
    QStringList required;  // 必填属性名
    // additionalProperties：任意键对象（键不限定，值类型由该定义约束）
    bool hasAdditionalProp = false;
    PropertyDef additionalProp;
  };

  void validateObject(const ClassDef &def, const QJsonObject &obj, const QString &path,
                      QVector<QString> *errors) const;

  /// 按属性类型校验单个值
  void validateValue(const PropertyDef &pd, const QJsonValue &val, const QString &childPath,
                     QVector<QString> *errors) const;

  /// 解析单个类定义（properties/required/additionalProperties）
  void parseClassDef(const QJsonObject &obj, ClassDef &def) const;

  /// 在类中查找属性定义；不存在返回 nullptr
  static const PropertyDef *propertyOf(const ClassDef &def, const QString &name);

  /// 判断是否为基本类型名（int/string/double/bool）
  static bool isPrimitiveType(const QString &type);

  /// 解析 JSON 路径到叶子属性；输出叶子所属类名与属性定义
  bool resolveProperty(const QString &jsonPath, QString *ownerClass, const PropertyDef **out) const;

  QMap<QString, ClassDef> m_classes;
  QString m_rootClass;  // 根入口类名
};