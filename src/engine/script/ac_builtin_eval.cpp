/**
 * @file ac_builtin_eval.cpp
 * @brief 内置类型方法求值实现 — 字符串方法和数组方法
 */

#include "ac_builtin_eval.h"

#include "../ac_language.h"
#include "ac_interpreter.h"
#include "src/core/json/ac_json_value.h"

// 迁移期适配：evalExpr / safeJsonToInt / compareValues 等 Qt 侧接口尚未迁移，
// 本文件在边界处经 fromQJsonValue / toQJsonValue 互转（待上游迁移后移除）
using accore::AcJsonValue;

// ═════════════════════════════════════════════════════════════════════════════
//  字符串内置方法
// ═════════════════════════════════════════════════════════════════════════════

AcJsonValue AcBuiltinEval::evalStringMethod(AcInterpreter &interp, const QString &obj,
                                            const QString &method,
                                            const std::vector<std::unique_ptr<Expr>> &args,
                                            int line, QString &error) {
  auto evalArg = [&](int idx) -> AcJsonValue { return interp.evalExpr(*args[idx]); };

  if (method == QStringLiteral("toLowerCase")) return AcJsonValue(obj.toLower());
  if (method == QStringLiteral("toUpperCase")) return AcJsonValue(obj.toUpper());
  if (method == QStringLiteral("trim")) return AcJsonValue(obj.trimmed());

  if (method == QStringLiteral("includes")) {
    if (args.empty()) {
      error = QStringLiteral("string.includes() requires 1 argument at line %1").arg(line);
      return AcJsonValue();
    }
    return AcJsonValue(obj.contains(evalArg(0).toString()));
  }
  if (method == QStringLiteral("startsWith")) {
    if (args.empty()) {
      error = QStringLiteral("string.startsWith() requires 1 argument at line %1").arg(line);
      return AcJsonValue();
    }
    return AcJsonValue(obj.startsWith(evalArg(0).toString()));
  }
  if (method == QStringLiteral("endsWith")) {
    if (args.empty()) {
      error = QStringLiteral("string.endsWith() requires 1 argument at line %1").arg(line);
      return AcJsonValue();
    }
    return AcJsonValue(obj.endsWith(evalArg(0).toString()));
  }
  if (method == QStringLiteral("indexOf")) {
    if (args.empty()) {
      error = QStringLiteral("string.indexOf() requires 1 argument at line %1").arg(line);
      return AcJsonValue();
    }
    // JS 语义：from 可选，默认 0；负值按 0 处理
    int from = 0;
    if (args.size() >= 2) {
      from = safeJsonToInt(evalArg(1));
      if (from < 0) from = 0;
    }
    return AcJsonValue(int(obj.indexOf(evalArg(0).toString(), from)));
  }
  if (method == QStringLiteral("lastIndexOf")) {
    if (args.empty()) {
      error = QStringLiteral("string.lastIndexOf() requires 1 argument at line %1").arg(line);
      return AcJsonValue();
    }
    // JS 语义：from 可选，缺省从末尾开始反向搜索（Qt 用 -1 表示）
    int from = -1;
    if (args.size() >= 2) {
      from = safeJsonToInt(evalArg(1));
    }
    return AcJsonValue(int(obj.lastIndexOf(evalArg(0).toString(), from)));
  }
  if (method == QStringLiteral("split")) {
    if (args.empty()) {
      error = QStringLiteral("string.split() requires 1 argument at line %1").arg(line);
      return AcJsonValue();
    }
    QStringList parts = obj.split(evalArg(0).toString());
    AcJsonValue arr = AcJsonValue::makeArray();
    for (const QString &p : parts) arr.append(AcJsonValue(p));
    return arr;
  }
  if (method == QStringLiteral("replace")) {
    if (args.size() < 2) {
      error = QStringLiteral("string.replace() requires 2 arguments at line %1").arg(line);
      return AcJsonValue();
    }
    QString result = obj;
    result.replace(evalArg(0).toString(), evalArg(1).toString());
    return AcJsonValue(result);
  }
  if (method == QStringLiteral("substring") || method == QStringLiteral("slice")) {
    if (args.empty()) {
      error = QStringLiteral("string.%1() requires at least 1 argument at line %2")
                  .arg(method)
                  .arg(line);
      return AcJsonValue();
    }
    int start = safeJsonToInt(evalArg(0));
    if (args.size() >= 2) {
      int end = safeJsonToInt(evalArg(1));
      return AcJsonValue(obj.mid(start, end - start));
    }
    return AcJsonValue(obj.mid(start));
  }
  if (method == QStringLiteral("charAt")) {
    if (args.empty()) {
      error = QStringLiteral("string.charAt() requires 1 argument at line %1").arg(line);
      return AcJsonValue();
    }
    int idx = safeJsonToInt(evalArg(0));
    if (idx >= 0 && idx < obj.size()) return AcJsonValue(QString(obj[idx]));
    return AcJsonValue(QString());
  }
  if (method == QStringLiteral("repeat")) {
    if (args.empty()) {
      error = QStringLiteral("string.repeat() requires 1 argument at line %1").arg(line);
      return AcJsonValue();
    }
    return AcJsonValue(QString(obj).repeated(safeJsonToInt(evalArg(0))));
  }
  if (method == QStringLiteral("padStart")) {
    if (args.size() < 2) {
      error = QStringLiteral("string.padStart() requires 2 arguments at line %1").arg(line);
      return AcJsonValue();
    }
    int len = safeJsonToInt(evalArg(0));
    QString fill = evalArg(1).toString();
    QString result = obj;
    while (result.length() < len) result = fill + result;
    return AcJsonValue(result.left(len));
  }
  if (method == QStringLiteral("padEnd")) {
    if (args.size() < 2) {
      error = QStringLiteral("string.padEnd() requires 2 arguments at line %1").arg(line);
      return AcJsonValue();
    }
    int len = safeJsonToInt(evalArg(0));
    QString fill = evalArg(1).toString();
    QString result = obj;
    while (result.length() < len) result = result + fill;
    return AcJsonValue(result.left(len));
  }
  if (method == QStringLiteral("length")) return AcJsonValue(int(obj.length()));

  error = QStringLiteral("string has no method '%1' at line %2").arg(method).arg(line);
  return AcJsonValue();
}

// ═════════════════════════════════════════════════════════════════════════════
//  数组内置方法
// ═════════════════════════════════════════════════════════════════════════════

AcJsonValue AcBuiltinEval::evalArrayMethod(AcInterpreter &interp, const AcJsonValue &arr,
                                           const QString &method,
                                           const std::vector<std::unique_ptr<Expr>> &args, int line,
                                           AcJsonValue &modifiedArr, QString &error) {
  auto evalArg = [&](int idx) -> AcJsonValue { return interp.evalExpr(*args[idx]); };

  if (method == QStringLiteral("push") || method == QStringLiteral("append")) {
    if (args.empty()) {
      error = QStringLiteral("array.%1() requires 1 argument at line %2").arg(method).arg(line);
      return AcJsonValue();
    }
    AcJsonValue newArr = arr;
    newArr.append(evalArg(0));
    modifiedArr = newArr;
    return AcJsonValue(newArr.size());
  }
  if (method == QStringLiteral("pop")) {
    if (arr.isEmpty()) return AcJsonValue();
    AcJsonValue newArr = arr;
    AcJsonValue last = newArr.at(newArr.size() - 1);
    newArr.removeLast();
    modifiedArr = newArr;
    return last;
  }
  if (method == QStringLiteral("shift")) {
    if (arr.isEmpty()) return AcJsonValue();
    AcJsonValue first = arr.at(0);
    // AcJsonValue 无 removeFirst()：跳过首元素重建数组，保持原 removeFirst 语义
    AcJsonValue rest = AcJsonValue::makeArray();
    for (int i = 1; i < arr.size(); ++i) rest.append(arr.at(i));
    modifiedArr = rest;
    return first;
  }

  if (method == QStringLiteral("unshift")) {
    if (args.empty()) {
      error = QStringLiteral("array.unshift() requires 1 argument at line %1").arg(line);
      return AcJsonValue();
    }
    // AcJsonValue 无 insert()：先追加新元素再拼接原数组，保持原 insert(0, ...) 语义
    AcJsonValue newArr = AcJsonValue::makeArray();
    newArr.append(evalArg(0));
    for (int i = 0; i < arr.size(); ++i) newArr.append(arr.at(i));
    modifiedArr = newArr;
    return AcJsonValue(newArr.size());
  }
  if (method == QStringLiteral("join")) {
    QString sep = QStringLiteral(",");
    if (!args.empty()) {
      AcJsonValue sepVal = evalArg(0);
      if (sepVal.isString()) sep = sepVal.toString();
    }
    QStringList parts;
    for (const AcJsonValue &v : arr.items()) {
      parts.append(v.isString() ? v.toString() : QString::number(v.toDouble()));
    }
    return AcJsonValue(parts.join(sep));
  }
  if (method == QStringLiteral("indexOf")) {
    if (args.empty()) {
      error = QStringLiteral("array.indexOf() requires 1 argument at line %1").arg(line);
      return AcJsonValue();
    }
    AcJsonValue target = evalArg(0);
    // JS 语义：from 可选，默认 0；负值按 0 处理
    int from = 0;
    if (args.size() >= 2) {
      from = safeJsonToInt(evalArg(1));
      if (from < 0) from = 0;
    }
    for (int i = from; i < arr.size(); ++i) {
      if (AcInterpreter::compareValues(arr.at(i), target) == 0) return AcJsonValue(i);
    }
    return AcJsonValue(-1);
  }
  if (method == QStringLiteral("includes")) {
    if (args.empty()) {
      error = QStringLiteral("array.includes() requires 1 argument at line %1").arg(line);
      return AcJsonValue();
    }
    AcJsonValue target = evalArg(0);
    for (const AcJsonValue &v : arr.items()) {
      if (AcInterpreter::compareValues(v, target) == 0) return AcJsonValue(true);
    }
    return AcJsonValue(false);
  }
  if (method == QStringLiteral("slice")) {
    int start = 0;
    int end = arr.size();
    if (!args.empty()) start = safeJsonToInt(evalArg(0));
    if (args.size() >= 2) end = safeJsonToInt(evalArg(1));
    AcJsonValue result = AcJsonValue::makeArray();
    for (int i = start; i < end && i < arr.size(); ++i) result.append(arr.at(i));
    return result;
  }
  if (method == QStringLiteral("concat")) {
    AcJsonValue result = arr;
    for (const auto &argExpr : args) {
      AcJsonValue val = interp.evalExpr(*argExpr);
      if (val.isArray()) {
        for (const AcJsonValue &v : val.items()) result.append(v);
      } else {
        result.append(val);
      }
    }
    return result;
  }
  if (method == QStringLiteral("reverse")) {
    AcJsonValue newArr = AcJsonValue::makeArray();
    for (int i = arr.size() - 1; i >= 0; --i) newArr.append(arr.at(i));
    modifiedArr = newArr;
    return newArr;
  }
  if (method == QStringLiteral("splice")) {
    if (args.empty()) {
      error = QStringLiteral("array.splice() requires at least 1 argument at line %1").arg(line);
      return AcJsonValue();
    }
    int start = safeJsonToInt(evalArg(0));
    // 钳制 start 到合法范围，避免负数或越界位置访问数组元素
    if (start < 0) start = 0;
    if (start > arr.size()) start = arr.size();
    int deleteCount = arr.size() - start;
    if (args.size() >= 2) {
      deleteCount = safeJsonToInt(evalArg(1));
      // 负删除数无意义，按 0 处理（与 JS 语义一致）
      if (deleteCount < 0) deleteCount = 0;
    }
    // AcJsonValue 无 takeAt()/insert()：按原语义重建 —— removed 收集删除区段，
    // newArr = 保留前段 + 插入参数 + 保留后段
    AcJsonValue removed = AcJsonValue::makeArray();
    for (int i = 0; i < deleteCount && start + i < arr.size(); ++i) {
      removed.append(arr.at(start + i));
    }
    AcJsonValue newArr = AcJsonValue::makeArray();
    for (int i = 0; i < start; ++i) newArr.append(arr.at(i));
    for (int i = 2; i < args.size(); ++i) {
      newArr.append(interp.evalExpr(*args[i]));
    }
    for (int i = start + deleteCount; i < arr.size(); ++i) newArr.append(arr.at(i));
    modifiedArr = newArr;
    return removed;
  }
  if (method == QStringLiteral("map") || method == QStringLiteral("filter") ||
      method == QStringLiteral("forEach")) {
    error = QStringLiteral("array.%1() with callback is not supported yet at line %2")
                .arg(method)
                .arg(line);
    return AcJsonValue();
  }

  error = QStringLiteral("array has no method '%1' at line %2").arg(method).arg(line);
  return AcJsonValue();
}
