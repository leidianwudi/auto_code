/**
 * @file ac_builtin_eval.cpp
 * @brief 内置类型方法求值实现 — 字符串方法和数组方法
 */

#include "ac_builtin_eval.h"

#include "ac_interpreter.h"


// ═════════════════════════════════════════════════════════════════════════════
//  字符串内置方法
// ═════════════════════════════════════════════════════════════════════════════

QJsonValue AcBuiltinEval::evalStringMethod(AcInterpreter &interp, const QString &obj,
                                           const QString &method, const QVector<Expr *> &args,
                                           int line, QString &error) {
  auto evalArg = [&](int idx) -> QJsonValue { return interp.evalExpr(*args[idx]); };

  if (method == QStringLiteral("toLowerCase")) return QJsonValue(obj.toLower());
  if (method == QStringLiteral("toUpperCase")) return QJsonValue(obj.toUpper());
  if (method == QStringLiteral("trim")) return QJsonValue(obj.trimmed());

  if (method == QStringLiteral("includes")) {
    if (args.isEmpty()) {
      error = QStringLiteral("string.includes() requires 1 argument at line %1").arg(line);
      return QJsonValue();
    }
    return QJsonValue(obj.contains(evalArg(0).toString()));
  }
  if (method == QStringLiteral("startsWith")) {
    if (args.isEmpty()) {
      error = QStringLiteral("string.startsWith() requires 1 argument at line %1").arg(line);
      return QJsonValue();
    }
    return QJsonValue(obj.startsWith(evalArg(0).toString()));
  }
  if (method == QStringLiteral("endsWith")) {
    if (args.isEmpty()) {
      error = QStringLiteral("string.endsWith() requires 1 argument at line %1").arg(line);
      return QJsonValue();
    }
    return QJsonValue(obj.endsWith(evalArg(0).toString()));
  }
  if (method == QStringLiteral("indexOf")) {
    if (args.isEmpty()) {
      error = QStringLiteral("string.indexOf() requires 1 argument at line %1").arg(line);
      return QJsonValue();
    }
    return QJsonValue(obj.indexOf(evalArg(0).toString()));
  }
  if (method == QStringLiteral("lastIndexOf")) {
    if (args.isEmpty()) {
      error = QStringLiteral("string.lastIndexOf() requires 1 argument at line %1").arg(line);
      return QJsonValue();
    }
    return QJsonValue(obj.lastIndexOf(evalArg(0).toString()));
  }
  if (method == QStringLiteral("split")) {
    if (args.isEmpty()) {
      error = QStringLiteral("string.split() requires 1 argument at line %1").arg(line);
      return QJsonValue();
    }
    QStringList parts = obj.split(evalArg(0).toString());
    QJsonArray arr;
    for (const QString &p : parts) arr.append(QJsonValue(p));
    return QJsonValue(arr);
  }
  if (method == QStringLiteral("replace")) {
    if (args.size() < 2) {
      error = QStringLiteral("string.replace() requires 2 arguments at line %1").arg(line);
      return QJsonValue();
    }
    QString result = obj;
    result.replace(evalArg(0).toString(), evalArg(1).toString());
    return QJsonValue(result);
  }
  if (method == QStringLiteral("substring") || method == QStringLiteral("slice")) {
    if (args.isEmpty()) {
      error = QStringLiteral("string.%1() requires at least 1 argument at line %2")
                  .arg(method)
                  .arg(line);
      return QJsonValue();
    }
    int start = evalArg(0).toInt();
    if (args.size() >= 2) {
      int end = evalArg(1).toInt();
      return QJsonValue(obj.mid(start, end - start));
    }
    return QJsonValue(obj.mid(start));
  }
  if (method == QStringLiteral("charAt")) {
    if (args.isEmpty()) {
      error = QStringLiteral("string.charAt() requires 1 argument at line %1").arg(line);
      return QJsonValue();
    }
    int idx = evalArg(0).toInt();
    if (idx >= 0 && idx < obj.size()) return QJsonValue(QString(obj[idx]));
    return QJsonValue(QString());
  }
  if (method == QStringLiteral("repeat")) {
    if (args.isEmpty()) {
      error = QStringLiteral("string.repeat() requires 1 argument at line %1").arg(line);
      return QJsonValue();
    }
    return QJsonValue(QString(obj).repeated(evalArg(0).toInt()));
  }
  if (method == QStringLiteral("padStart")) {
    if (args.size() < 2) {
      error = QStringLiteral("string.padStart() requires 2 arguments at line %1").arg(line);
      return QJsonValue();
    }
    int len = evalArg(0).toInt();
    QString fill = evalArg(1).toString();
    QString result = obj;
    while (result.length() < len) result = fill + result;
    return QJsonValue(result.left(len));
  }
  if (method == QStringLiteral("padEnd")) {
    if (args.size() < 2) {
      error = QStringLiteral("string.padEnd() requires 2 arguments at line %1").arg(line);
      return QJsonValue();
    }
    int len = evalArg(0).toInt();
    QString fill = evalArg(1).toString();
    QString result = obj;
    while (result.length() < len) result = result + fill;
    return QJsonValue(result.left(len));
  }
  if (method == QStringLiteral("length")) return QJsonValue(obj.length());

  error = QStringLiteral("string has no method '%1' at line %2").arg(method).arg(line);
  return QJsonValue();
}

// ═════════════════════════════════════════════════════════════════════════════
//  数组内置方法
// ═════════════════════════════════════════════════════════════════════════════

QJsonValue AcBuiltinEval::evalArrayMethod(AcInterpreter &interp, const QJsonArray &arr,
                                          const QString &method, const QVector<Expr *> &args,
                                          int line, QJsonValue &modifiedArr, QString &error) {
  auto evalArg = [&](int idx) -> QJsonValue { return interp.evalExpr(*args[idx]); };

  if (method == QStringLiteral("push") || method == QStringLiteral("append")) {
    if (args.isEmpty()) {
      error = QStringLiteral("array.%1() requires 1 argument at line %2").arg(method).arg(line);
      return QJsonValue();
    }
    QJsonArray newArr = arr;
    newArr.append(evalArg(0));
    modifiedArr = QJsonValue(newArr);
    return QJsonValue(newArr.size());
  }
  if (method == QStringLiteral("pop")) {
    if (arr.isEmpty()) return QJsonValue();
    QJsonArray newArr = arr;
    QJsonValue last = newArr.last();
    newArr.removeLast();
    modifiedArr = QJsonValue(newArr);
    return last;
  }
  if (method == QStringLiteral("shift")) {
    if (arr.isEmpty()) return QJsonValue();
    QJsonArray newArr = arr;
    QJsonValue first = newArr.first();
    newArr.removeFirst();
    modifiedArr = QJsonValue(newArr);
    return first;
  }
  if (method == QStringLiteral("unshift")) {
    if (args.isEmpty()) {
      error = QStringLiteral("array.unshift() requires 1 argument at line %1").arg(line);
      return QJsonValue();
    }
    QJsonArray newArr = arr;
    newArr.insert(0, evalArg(0));
    modifiedArr = QJsonValue(newArr);
    return QJsonValue(newArr.size());
  }
  if (method == QStringLiteral("join")) {
    QString sep = QStringLiteral(",");
    if (!args.isEmpty()) {
      QJsonValue sepVal = evalArg(0);
      if (sepVal.isString()) sep = sepVal.toString();
    }
    QStringList parts;
    for (const QJsonValue &v : arr) {
      parts.append(v.isString() ? v.toString() : QString::number(v.toDouble()));
    }
    return QJsonValue(parts.join(sep));
  }
  if (method == QStringLiteral("indexOf")) {
    if (args.isEmpty()) {
      error = QStringLiteral("array.indexOf() requires 1 argument at line %1").arg(line);
      return QJsonValue();
    }
    QJsonValue target = evalArg(0);
    for (int i = 0; i < arr.size(); ++i) {
      if (AcInterpreter::compareValues(arr[i], target) == 0) return QJsonValue(i);
    }
    return QJsonValue(-1);
  }
  if (method == QStringLiteral("includes")) {
    if (args.isEmpty()) {
      error = QStringLiteral("array.includes() requires 1 argument at line %1").arg(line);
      return QJsonValue();
    }
    QJsonValue target = evalArg(0);
    for (const QJsonValue &v : arr) {
      if (AcInterpreter::compareValues(v, target) == 0) return QJsonValue(true);
    }
    return QJsonValue(false);
  }
  if (method == QStringLiteral("slice")) {
    int start = 0;
    int end = arr.size();
    if (!args.isEmpty()) start = evalArg(0).toInt();
    if (args.size() >= 2) end = evalArg(1).toInt();
    QJsonArray result;
    for (int i = start; i < end && i < arr.size(); ++i) result.append(arr[i]);
    return QJsonValue(result);
  }
  if (method == QStringLiteral("concat")) {
    QJsonArray result = arr;
    for (auto *argExpr : args) {
      QJsonValue val = interp.evalExpr(*argExpr);
      if (val.isArray()) {
        for (const QJsonValue &v : val.toArray()) result.append(v);
      } else {
        result.append(val);
      }
    }
    return QJsonValue(result);
  }
  if (method == QStringLiteral("reverse")) {
    QJsonArray newArr;
    for (int i = arr.size() - 1; i >= 0; --i) newArr.append(arr[i]);
    modifiedArr = QJsonValue(newArr);
    return QJsonValue(newArr);
  }
  if (method == QStringLiteral("splice")) {
    if (args.isEmpty()) {
      error = QStringLiteral("array.splice() requires at least 1 argument at line %1").arg(line);
      return QJsonValue();
    }
    int start = evalArg(0).toInt();
    int deleteCount = arr.size() - start;
    if (args.size() >= 2) deleteCount = evalArg(1).toInt();
    QJsonArray newArr = arr;
    QJsonArray removed;
    for (int i = 0; i < deleteCount && start < newArr.size(); ++i) {
      removed.append(newArr.takeAt(start));
    }
    for (int i = 2; i < args.size(); ++i) {
      newArr.insert(start + i - 2, interp.evalExpr(*args[i]));
    }
    modifiedArr = QJsonValue(newArr);
    return QJsonValue(removed);
  }
  if (method == QStringLiteral("map") || method == QStringLiteral("filter") ||
      method == QStringLiteral("forEach")) {
    error = QStringLiteral("array.%1() with callback is not supported yet at line %2")
                .arg(method)
                .arg(line);
    return QJsonValue();
  }

  error = QStringLiteral("array has no method '%1' at line %2").arg(method).arg(line);
  return QJsonValue();
}