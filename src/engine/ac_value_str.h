/**
 * @file ac_value_str.h
 * @brief AC 值转字符串 — 将 QJsonValue 转为可读字符串
 *
 * 递归处理 AC 类实例、普通 JSON 对象、数组等，
 * 供 printLog/printError 和 + 运算符拼接共同使用。
 *
 * AC 类实例格式：&lt;ClassName&gt;{"prop":value,...}
 * 普通 JSON 对象格式：{"key":value,...}
 * 内部键 __class__、__objId__ 自动过滤。
 */

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <cmath>

#include "ac_language.h"

namespace AcValueStr {

/// @brief 将 QJsonValue 转为可读字符串（递归）
inline QString toString(const QJsonValue &v);

/// @brief 将 QJsonObject 转为可读字符串
inline QString objectToString(const QJsonObject &obj) {
  bool isInstance = obj.contains(QString::fromLatin1(AcRuntime::kObjId));
  bool isFuncRef =
      obj.value(QString::fromLatin1(AcRuntime::kClassKey)).toString() == QStringLiteral("__func__");

  QString className;
  if (isInstance) {
    className = obj.value(QString::fromLatin1(AcRuntime::kClassKey)).toString();
  }

  QStringList parts;
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    QString key = it.key();
    if (key == QString::fromLatin1(AcRuntime::kClassKey) ||
        key == QString::fromLatin1(AcRuntime::kObjId))
      continue;
    parts.append(QStringLiteral("\"%1\":%2").arg(key).arg(toString(it.value())));
  }

  QString content = parts.join(QStringLiteral(","));
  if (isInstance) {
    return QStringLiteral("<%1>{%2}").arg(className).arg(content);
  } else if (isFuncRef) {
    QString funcName = obj.value(QStringLiteral("__name__")).toString();
    return QStringLiteral("function(%1)")
        .arg(funcName.isEmpty() ? QStringLiteral("...") : funcName);
  }
  return QStringLiteral("{%1}").arg(content);
}

/// @brief 将 QJsonArray 转为可读字符串
inline QString arrayToString(const QJsonArray &arr) {
  QStringList parts;
  for (const QJsonValue &v : arr) {
    parts.append(toString(v));
  }
  return QStringLiteral("[%1]").arg(parts.join(QStringLiteral(",")));
}

inline QString toString(const QJsonValue &v) {
  switch (v.type()) {
    case QJsonValue::String:
      return QStringLiteral("\"%1\"").arg(v.toString().replace('"', "\\\""));
    case QJsonValue::Bool:
      return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    case QJsonValue::Double: {
      double d = v.toDouble();
      if (d == std::floor(d)) {
        return QString::number(static_cast<qint64>(d));
      }
      return QString::number(d);
    }
    case QJsonValue::Null:
      return QStringLiteral("null");
    case QJsonValue::Undefined:
      return QStringLiteral("undefined");
    case QJsonValue::Array:
      return arrayToString(v.toArray());
    case QJsonValue::Object:
      return objectToString(v.toObject());
    default:
      return QStringLiteral("undefined");
  }
}

}  // namespace AcValueStr
