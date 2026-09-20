/**
 * @file ac_value_str.h
 * @brief AC 值转字符串 — 将运行时值转为可读字符串
 *
 * 递归处理类实例、普通 JSON 对象、数组、类/函数引用等，
 * 供 printLog/printError 和 + 运算符拼接共同使用。
 *
 * 显示格式：
 * - 类实例：&lt;ClassName&gt;{"prop":value,...}（类名在实例专用字段，无内部键过滤）
 * - 类引用：&lt;Class ClassName&gt;
 * - 函数引用：function(name)
 * - 普通对象：{"key":value,...}
 */

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <cmath>

#include "ac_language.h"
#include "src/core/json/ac_json_value.h"

namespace AcValueStr {

/// @brief 将 QJsonValue 转为可读字符串（递归）
inline QString toString(const QJsonValue &v);

/// @brief 将 QJsonObject 转为可读字符串
inline QString objectToString(const QJsonObject &obj) {
  QStringList parts;
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    parts.append(QStringLiteral("\"%1\":%2").arg(it.key()).arg(toString(it.value())));
  }
  return QStringLiteral("{%1}").arg(parts.join(QStringLiteral(",")));
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
      return v.toBool() ? QString::fromLatin1(AcKeyword::kTrue)
                        : QString::fromLatin1(AcKeyword::kFalse);
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
      return QString::fromLatin1(AcKeyword::kUndefined);
    case QJsonValue::Array:
      return arrayToString(v.toArray());
    case QJsonValue::Object:
      return objectToString(v.toObject());
    default:
      return QString::fromLatin1(AcKeyword::kUndefined);
  }
}

// ═════════════════════════════════════════════════════════════
// accore 原生重载 — 语义与 QJsonValue 版本一致，供解释器/print 免转换使用
// ═════════════════════════════════════════════════════════════

/// @brief 将 accore 值转为可读字符串（递归）
inline QString toString(const accore::AcJsonValue &v);

/// @brief 将 accore 对象/实例转为可读字符串
inline QString objectToString(const accore::AcJsonValue &obj) {
  const bool isInst = obj.isInstance();
  QStringList parts;
  // 实例的类名/objId 在专用字段，members 即纯属性
  for (const auto &m : obj.members()) {
    parts.append(QStringLiteral("\"%1\":%2").arg(m.key).arg(toString(m.value)));
  }
  const QString content = parts.join(QStringLiteral(","));
  if (isInst) {
    return QStringLiteral("<%1>{%2}").arg(obj.instanceClass()).arg(content);
  }
  return QStringLiteral("{%1}").arg(content);
}

/// @brief 将 accore 数组转为可读字符串
inline QString arrayToString(const accore::AcJsonValue &arr) {
  QStringList parts;
  for (const accore::AcJsonValue &v : arr.items()) {
    parts.append(toString(v));
  }
  return QStringLiteral("[%1]").arg(parts.join(QStringLiteral(",")));
}

/// @brief 将 accore 值转为可读字符串（递归）
inline QString toString(const accore::AcJsonValue &v) {
  switch (v.type()) {
    case accore::AcJsonValue::Type::String:
      return QStringLiteral("\"%1\"").arg(v.toString().replace('"', "\\\""));
    case accore::AcJsonValue::Type::Bool:
      return v.toBool() ? QString::fromLatin1(AcKeyword::kTrue)
                        : QString::fromLatin1(AcKeyword::kFalse);
    case accore::AcJsonValue::Type::Number: {
      double d = v.toDouble();
      if (d == std::floor(d)) {
        return QString::number(static_cast<qint64>(d));
      }
      return QString::number(d);
    }
    case accore::AcJsonValue::Type::Null:
      return QStringLiteral("null");
    case accore::AcJsonValue::Type::Array:
      return arrayToString(v);
    case accore::AcJsonValue::Type::Object:
    case accore::AcJsonValue::Type::Instance:
      return objectToString(v);
    case accore::AcJsonValue::Type::ClassRef:
      return QStringLiteral("<Class %1>").arg(v.instanceClass());
    case accore::AcJsonValue::Type::FuncRef:
      return QStringLiteral("function(%1)").arg(
          v.funcRefName().isEmpty() ? QStringLiteral("...") : v.funcRefName());
  }
  return QString::fromLatin1(AcKeyword::kUndefined);
}

}  // namespace AcValueStr
