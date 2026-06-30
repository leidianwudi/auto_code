/**
 * @file fun_const.cpp
 * @brief 函数名称常量实现
 */

#include "fun_const.h"

QStringList FunConst::stringMethods() {
  return {
      QString::fromLatin1(kToLowerCase),
      QString::fromLatin1(kToUpperCase),
      QString::fromLatin1(kTrim),
      QString::fromLatin1(kCapitalize),
  };
}