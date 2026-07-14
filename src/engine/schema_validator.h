/**
 * @file schema_validator.h
 * @brief JSON 数据 Schema 校验器
 *
 * 从 JSON 文件加载类定义，校验 JSON 数据是否符合指定类的属性约束。
 * 支持的基本类型：int / string / double / bool
 * 支持的复合类型：array（元素为指定类名） / object（嵌套类名）
 */

#pragma once

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

/**
 * @class SchemaValidator
 * @brief 基于类定义的 JSON 数据校验器
 *
 * 使用方式：
 *   1. SchemaValidator v;
 *   2. v.load("schema/user.json");
 *   3. QString err = v.validate("User", data);
 *      if (!err.isEmpty()) { ... }
 */
class SchemaValidator {
public:
  SchemaValidator() = default;

  /**
   * @brief 加载 schema 定义文件
   * @param filePath schema JSON 文件路径
   * @return true 加载成功
   */
  bool load(const QString &filePath);

  /**
   * @brief 校验 JSON 数据是否符合指定类定义
   * @param className 类名
   * @param data 待校验的 JSON 数据
   * @return 空字符串表示通过，否则返回错误描述
   */
  QString validate(const QString &className, const QJsonObject &data) const;

  /**
   * @brief 获取所有已注册的类名
   */
  QStringList classNames() const;

  /**
   * @brief 判断指定类是否已注册
   */
  bool hasClass(const QString &className) const;

private:
  struct PropertyDef {
    QString type;       // int / string / double / bool / array / object
    QString items;      // type=array 时，元素类名
    QString className;  // type=object 时，嵌套类名
  };

  struct ClassDef {
    QMap<QString, PropertyDef> properties;
  };

  QString validateObject(const ClassDef &def, const QJsonObject &obj, const QString &path) const;

  QMap<QString, ClassDef> m_classes;
};