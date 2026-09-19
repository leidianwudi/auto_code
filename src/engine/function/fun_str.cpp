/**
 * @file fun_str.cpp
 * @brief 字符串函数实现
 */

#include "fun_str.h"

#include <QString>

#include "../ac_language.h"
#include "fun_args.h"
#include "fun_mgr.h"

void FunStr::init() {
  FunMgr::ins().registerFuncs(QString::fromLatin1(AcCallStr::kClassName),
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
  if (!FunArgs::requireString(args, 0,
                              QStringLiteral("str::toLowerCase() requires a string argument")))
    return QJsonValue();
  return QJsonValue(args[0].toString().toLower());
}

// ============================================================================
// toUpperCase — 转大写
// ============================================================================

QJsonValue FunStr::toUpperCase(const QJsonArray &args) {
  if (!FunArgs::requireString(args, 0,
                              QStringLiteral("str::toUpperCase() requires a string argument")))
    return QJsonValue();
  return QJsonValue(args[0].toString().toUpper());
}

// ============================================================================
// trim — 去首尾空白
// ============================================================================

QJsonValue FunStr::trim(const QJsonArray &args) {
  if (!FunArgs::requireString(args, 0, QStringLiteral("str::trim() requires a string argument")))
    return QJsonValue();
  return QJsonValue(args[0].toString().trimmed());
}

// ============================================================================
// capitalize — 首字母大写
// ============================================================================

QJsonValue FunStr::capitalize(const QJsonArray &args) {
  if (!FunArgs::requireString(args, 0,
                              QStringLiteral("str::capitalize() requires a string argument")))
    return QJsonValue();
  QString str = args[0].toString();
  if (!str.isEmpty()) str = str[0].toUpper() + str.mid(1);
  return QJsonValue(str);
}

// ============================================================================
// substring — 截子串
// ============================================================================

QJsonValue FunStr::substring(const QJsonArray &args) {
  const QString subErr = QStringLiteral("str::substring() requires a string and a start position");
  if (!FunArgs::requireCount(args, 2, subErr) || !FunArgs::requireString(args, 0, subErr) ||
      !FunArgs::requireNumber(args, 1, subErr))
    return QJsonValue();

  const QString str = args[0].toString();
  const int start = safeJsonToInt(args[1]);

  if (args.size() >= 3 && args[2].isDouble()) {
    const int length = safeJsonToInt(args[2]);
    return QJsonValue(str.mid(start, length));
  }

  return QJsonValue(str.mid(start));
}

// ============================================================================
// replace — 替换
// ============================================================================

QJsonValue FunStr::replace(const QJsonArray &args) {
  const QString repErr =
      QStringLiteral("str::replace() requires 3 arguments: string, before, after");
  if (!FunArgs::requireCount(args, 3, repErr) || !FunArgs::requireString(args, 0, repErr) ||
      !FunArgs::requireString(args, 1, repErr) || !FunArgs::requireString(args, 2, repErr))
    return QJsonValue();

  QString str = args[0].toString();
  const QString before = args[1].toString();
  const QString after = args[2].toString();

  return QJsonValue(str.replace(before, after));
}