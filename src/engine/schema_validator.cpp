/**
 * @file schema_validator.cpp
 * @brief SchemaValidator 实现
 */

#include "schema_validator.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// load — 加载 schema 定义文件
bool SchemaValidator::load(const QString &filePath) {
  // 打开文件
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly))
    return false;

  // 解析 JSON
  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
  if (err.error != QJsonParseError::NoError)
    return false;

  // 根节点必须是对象
  if (!doc.isObject())
    return false;

  QJsonObject root = doc.object();
  m_classes.clear();

  // 遍历每个类定义（root 的 key 为类名）
  for (auto it = root.begin(); it != root.end(); ++it) {
    QString className = it.key();
    QJsonObject classObj = it.value().toObject();
    QJsonObject props = classObj.value(QStringLiteral("properties")).toObject();

    ClassDef def;
    // 遍历该类的每个属性
    for (auto pit = props.begin(); pit != props.end(); ++pit) {
      QString propName = pit.key();
      QJsonObject propDef = pit.value().toObject();

      PropertyDef pd;
      pd.type = propDef.value(QStringLiteral("type")).toString();
      pd.items = propDef.value(QStringLiteral("items")).toString();
      pd.className = propDef.value(QStringLiteral("class")).toString();

      def.properties[propName] = pd;
    }

    m_classes[className] = def;
  }

  return true;
}

// validate — 校验入口
QString SchemaValidator::validate(const QString &className,
                                  const QJsonObject &data) const {
  auto it = m_classes.find(className);
  if (it == m_classes.end())
    return QStringLiteral("Schema class '%1' not defined").arg(className);

  return validateObject(it.value(), data, className);
}

// validateObject — 递归校验对象
QString SchemaValidator::validateObject(const ClassDef &def,
                                        const QJsonObject &obj,
                                        const QString &path) const {
  // 检查是否有未定义的属性
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    const QString &key = it.key();
    if (!def.properties.contains(key))
      return QStringLiteral("'%1.%2' is not a valid property").arg(path, key);
  }

  // 检查每个属性的类型
  for (auto it = def.properties.begin(); it != def.properties.end(); ++it) {
    const QString &propName = it.key();
    const PropertyDef &pd = it.value();

    if (!obj.contains(propName))
      continue; // 可选属性，跳过

    QJsonValue val = obj.value(propName);
    QString childPath = path + QStringLiteral(".") + propName;

    if (pd.type == QStringLiteral("int")) {
      if (!val.isDouble())
        return QStringLiteral("'%1' should be int").arg(childPath);
    } else if (pd.type == QStringLiteral("string")) {
      if (!val.isString())
        return QStringLiteral("'%1' should be string").arg(childPath);
    } else if (pd.type == QStringLiteral("double")) {
      if (!val.isDouble())
        return QStringLiteral("'%1' should be double").arg(childPath);
    } else if (pd.type == QStringLiteral("bool")) {
      if (!val.isBool())
        return QStringLiteral("'%1' should be bool").arg(childPath);
    } else if (pd.type == QStringLiteral("array")) {
      if (!val.isArray())
        return QStringLiteral("'%1' should be array").arg(childPath);

      if (!pd.items.isEmpty()) {
        // 数组元素为指定类，逐个校验
        QJsonArray arr = val.toArray();
        auto itemIt = m_classes.find(pd.items);
        if (itemIt == m_classes.end())
          return QStringLiteral("'%1' item class '%2' not defined")
              .arg(childPath, pd.items);

        for (int i = 0; i < arr.size(); ++i) {
          if (!arr[i].isObject())
            return QStringLiteral("'%1[%2]' should be object")
                .arg(childPath)
                .arg(i);

          QString err =
              validateObject(itemIt.value(), arr[i].toObject(),
                             childPath + QStringLiteral("[%1]").arg(i));
          if (!err.isEmpty())
            return err;
        }
      }
    } else if (pd.type == QStringLiteral("object")) {
      if (!val.isObject())
        return QStringLiteral("'%1' should be object").arg(childPath);

      if (!pd.className.isEmpty()) {
        auto clsIt = m_classes.find(pd.className);
        if (clsIt == m_classes.end())
          return QStringLiteral("'%1' class '%2' not defined")
              .arg(childPath, pd.className);

        QString err = validateObject(clsIt.value(), val.toObject(), childPath);
        if (!err.isEmpty())
          return err;
      }
    }
  }

  return {};
}

// classNames — 所有已注册类名
QStringList SchemaValidator::classNames() const { return m_classes.keys(); }

// hasClass — 判断类是否已注册
bool SchemaValidator::hasClass(const QString &className) const {
  return m_classes.contains(className);
}