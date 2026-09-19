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

accore::AcJsonValue FunStr::toLowerCase(const accore::AcJsonValue &args) {
  if (!FunArgs::requireString(args, 0,
                              QStringLiteral("str::toLowerCase() requires a string argument")))
    return accore::AcJsonValue();
  return accore::AcJsonValue(args.at(0).toString().toLower());
}

// ============================================================================
// toUpperCase — 转大写
// ============================================================================

accore::AcJsonValue FunStr::toUpperCase(const accore::AcJsonValue &args) {
  if (!FunArgs::requireString(args, 0,
                              QStringLiteral("str::toUpperCase() requires a string argument")))
    return accore::AcJsonValue();
  return accore::AcJsonValue(args.at(0).toString().toUpper());
}

// ============================================================================
// trim — 去首尾空白
// ============================================================================

accore::AcJsonValue FunStr::trim(const accore::AcJsonValue &args) {
  if (!FunArgs::requireString(args, 0, QStringLiteral("str::trim() requires a string argument")))
    return accore::AcJsonValue();
  return accore::AcJsonValue(args.at(0).toString().trimmed());
}

// ============================================================================
// capitalize — 首字母大写
// ============================================================================

accore::AcJsonValue FunStr::capitalize(const accore::AcJsonValue &args) {
  if (!FunArgs::requireString(args, 0,
                              QStringLiteral("str::capitalize() requires a string argument")))
    return accore::AcJsonValue();
  QString str = args.at(0).toString();
  if (!str.isEmpty()) str = str[0].toUpper() + str.mid(1);
  return accore::AcJsonValue(str);
}

// ============================================================================
// substring — 截子串
// ============================================================================

accore::AcJsonValue FunStr::substring(const accore::AcJsonValue &args) {
  const QString subErr = QStringLiteral("str::substring() requires a string and a start position");
  if (!FunArgs::requireCount(args, 2, subErr) || !FunArgs::requireString(args, 0, subErr) ||
      !FunArgs::requireNumber(args, 1, subErr))
    return accore::AcJsonValue();

  const QString str = args.at(0).toString();
  const int start = safeJsonToInt(args.at(1));

  if (args.size() >= 3 && args.at(2).isNumber()) {
    const int length = safeJsonToInt(args.at(2));
    return accore::AcJsonValue(str.mid(start, length));
  }

  return accore::AcJsonValue(str.mid(start));
}

// ============================================================================
// replace — 替换
// ============================================================================

accore::AcJsonValue FunStr::replace(const accore::AcJsonValue &args) {
  const QString repErr =
      QStringLiteral("str::replace() requires 3 arguments: string, before, after");
  if (!FunArgs::requireCount(args, 3, repErr) || !FunArgs::requireString(args, 0, repErr) ||
      !FunArgs::requireString(args, 1, repErr) || !FunArgs::requireString(args, 2, repErr))
    return accore::AcJsonValue();

  QString str = args.at(0).toString();
  const QString before = args.at(1).toString();
  const QString after = args.at(2).toString();

  return accore::AcJsonValue(str.replace(before, after));
}
