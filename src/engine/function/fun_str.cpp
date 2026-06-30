/**
 * @file fun_str.cpp
 * @brief 字符串函数实现
 */

#include "fun_str.h"

QJsonValue FunStr::execute(const QJsonArray &args) {
  if (args.size() < 2 || !args[1].isString())
    return QJsonValue();

  const QString cmd = args[0].toString();
  QString str = args[1].toString();

  if (cmd == QStringLiteral("toLowerCase")) {
    str = str.toLower();
  } else if (cmd == QStringLiteral("toUpperCase")) {
    str = str.toUpper();
  } else if (cmd == QStringLiteral("trim")) {
    str = str.trimmed();
  } else if (cmd == QStringLiteral("capitalize")) {
    if (!str.isEmpty())
      str = str[0].toUpper() + str.mid(1);
  } else {
    return QJsonValue();
  }

  return QJsonValue(str);
}