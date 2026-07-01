/**
 * @file fun_const.cpp
 * @brief 函数名称常量实现
 */

#include "fun_const.h"

QStringList FunConst::stringMethods() {
  return {
      QString::fromLatin1(kToLowerCase), QString::fromLatin1(kToUpperCase),
      QString::fromLatin1(kTrim),        QString::fromLatin1(kCapitalize),
      QString::fromLatin1(kSubstring),   QString::fromLatin1(kReplace),
  };
}

QStringList FunConst::dbMethods() {
  return {
      QString::fromLatin1(kConnect),
      QString::fromLatin1(kDisconnect),
      QString::fromLatin1(kTableSchema),
      QString::fromLatin1(kQuery),
  };
}

QStringList FunConst::fileMethods() {
  return {
      QString::fromLatin1(kRead),
      QString::fromLatin1(kWrite),
  };
}