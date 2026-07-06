/**
 * @file fun_str.cpp
 * @brief 字符串函数实现
 */

#include "fun_str.h"
#include "fun_const.h"
#include "fun_mgr.h"

#include <QString>

void FunStr::init() {
  FunMgr::ins().registerFuncs(
      QString::fromLatin1(kAcCallStr),
      {
          {QString::fromLatin1(AcCallStr::kToLowerCase), toLowerCase},
          {QString::fromLatin1(AcCallStr::kToUpperCase), toUpperCase},
          {QString::fromLatin1(AcCallStr::kTrim), trim},
          {QString::fromLatin1(AcCallStr::kCapitalize), capitalize},
          {QString::fromLatin1(AcCallStr::kSubstring), substring},
          {QString::fromLatin1(AcCallStr::kReplace), replace},
      });
}

// ============================================================================
// toLowerCase — 转小写
// ============================================================================

QJsonValue FunStr::toLowerCase(const QJsonArray &args) {
  if (args.isEmpty() || !args[0].isString())
    return QJsonValue();
  return QJsonValue(args[0].toString().toLower());
}

// ============================================================================
// toUpperCase — 转大写
// ============================================================================

QJsonValue FunStr::toUpperCase(const QJsonArray &args) {
  if (args.isEmpty() || !args[0].isString())
    return QJsonValue();
  return QJsonValue(args[0].toString().toUpper());
}

// ============================================================================
// trim — 去首尾空白
// ============================================================================

QJsonValue FunStr::trim(const QJsonArray &args) {
  if (args.isEmpty() || !args[0].isString())
    return QJsonValue();
  return QJsonValue(args[0].toString().trimmed());
}

// ============================================================================
// capitalize — 首字母大写
// ============================================================================

QJsonValue FunStr::capitalize(const QJsonArray &args) {
  if (args.isEmpty() || !args[0].isString())
    return QJsonValue();
  QString str = args[0].toString();
  if (!str.isEmpty())
    str = str[0].toUpper() + str.mid(1);
  return QJsonValue(str);
}

// ============================================================================
// substring — 截子串
// ============================================================================

QJsonValue FunStr::substring(const QJsonArray &args) {
  if (args.size() < 2 || !args[0].isString() || !args[1].isDouble())
    return QJsonValue();

  const QString str = args[0].toString();
  const int start = static_cast<int>(args[1].toDouble());

  if (args.size() >= 3 && args[2].isDouble()) {
    const int length = static_cast<int>(args[2].toDouble());
    return QJsonValue(str.mid(start, length));
  }

  return QJsonValue(str.mid(start));
}

// ============================================================================
// replace — 替换
// ============================================================================

QJsonValue FunStr::replace(const QJsonArray &args) {
  if (args.size() < 3 || !args[0].isString() || !args[1].isString() ||
      !args[2].isString())
    return QJsonValue();

  QString str = args[0].toString();
  const QString before = args[1].toString();
  const QString after = args[2].toString();

  return QJsonValue(str.replace(before, after));
}