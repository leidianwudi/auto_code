/**
 * @file hand_expression.cpp
 * @brief 表达式处理器实现（自包含：算术解析器 + 变量求值）
 *
 * 本文件是 HandExpression 的完整实现，包含：
 * - checkArithmetic() -- 判断表达式是否含算术运算符
 * - handle()          -- 三策略分发（循环变量 / 算术 / 普通变量）
 * - evalExpression()  -- 算术解析入口
 * - evalAddSub()      -- 加减法（最低优先级）
 * - evalMulDiv()      -- 乘除法（中等优先级）
 * - evalPrimary()     -- 基本元素（最高优先级）
 */

#include "hand_expression.h"
#include "../engine_tpl.h"

#include <QFileInfo>

namespace {

/**
 * @brief 检测表达式是否包含算术运算符
 *
 * 扫描表达式字符串，判断是否包含顶层（非括号内）的 +、-、*、/ 运算符。
 * 用于 handle() 中决定走"算术求值"还是"变量路径解析"分支。
 *
 * 特殊处理：开头的 +/-( 视为一元运算符，不算作算术表达式。
 * 例如 "-name" 不触发算术模式，但 "a + b" 会。
 *
 * @param expr 待检测的表达式
 * @return true 表达式中含有效的算术运算符
 */
bool checkArithmetic(const QString &expr) {
  int parenDepth = 0;
  for (int i = 0; i < expr.length(); ++i) {
    QChar ch = expr[i];
    if (ch == '(')
      ++parenDepth;
    else if (ch == ')')
      --parenDepth;
    else if ((ch == '+' || ch == '-' || ch == '*' || ch == '/') &&
             parenDepth == 0) {
      if ((ch == '+' || ch == '-') &&
          (i == 0 || expr[i - 1] == '(' || expr[i - 1] == '+' ||
           expr[i - 1] == '-' || expr[i - 1] == '*' || expr[i - 1] == '/'))
        continue;
      return true;
    }
  }
  return false;
}

} // namespace

// ====================================================================
// HandExpression::handle() -- 三策略分发
// ====================================================================

bool HandExpression::handle(const QString &block, int &pos, const QString &expr,
                            const QJsonObject &context, QString &result) const {
  Q_UNUSED(block)
  Q_UNUSED(pos)

  // === 策略 0: 内置函数调用 print(text) ===
  if (expr.startsWith(QStringLiteral("print(")) &&
      expr.endsWith(QStringLiteral(")"))) {
    QString arg = expr.mid(6, expr.length() - 7).trimmed();
    // 去掉可能的外层引号
    if ((arg.startsWith('\'') && arg.endsWith('\'')) ||
        (arg.startsWith('"') && arg.endsWith('"')))
      arg = arg.mid(1, arg.length() - 2);
    // 如果参数不是字面量，尝试从 context 解析变量
    QJsonValue v = m_engine.resolvePath(arg, context);
    if (!v.isNull() && !v.isUndefined())
      arg = v.toString();
    auto cb = m_engine.logCallback();
    if (cb)
      cb(arg, false);
    return true;
  }

  // === 策略 0: 内置函数调用 fileExists(path) ===
  if (expr.startsWith(QStringLiteral("fileExists(")) &&
      expr.endsWith(QStringLiteral(")"))) {
    QString arg = expr.mid(11, expr.length() - 12).trimmed();
    // 去掉可能的外层引号
    if ((arg.startsWith('\'') && arg.endsWith('\'')) ||
        (arg.startsWith('"') && arg.endsWith('"')))
      arg = arg.mid(1, arg.length() - 2);
    // 如果参数不是字面量，尝试从 context 解析变量
    QJsonValue v = m_engine.resolvePath(arg, context);
    if (!v.isNull() && !v.isUndefined())
      arg = v.toString();
    bool exists = QFileInfo::exists(arg);
    result += exists ? QStringLiteral("true") : QStringLiteral("false");
    return true;
  }

  // === 策略 1: 循环变量 ${this} 或 ${.} ===
  // 在 each 循环体内，当前元素存储在 context["."] 中。
  // 用户可以用 ${this} 或 ${.} 引用它（两种写法等价）。
  if (expr == QStringLiteral("this") || expr == QStringLiteral(".")) {
    QJsonValue v = context.value(QStringLiteral("."));
    if (v.isString())
      return result += v.toString(), true;
    if (v.isDouble()) {
      double d = v.toDouble();
      result += (d == static_cast<int>(d))
                    ? QString::number(static_cast<int>(d))
                    : QString::number(d);
      return true;
    }
    if (v.isBool())
      return result +=
             (v.toBool() ? QStringLiteral("true") : QStringLiteral("false")),
             true;
    return true;
  }

  // === 策略 2: 算术表达式（含 +-*/） ===
  // 先用 checkArithmetic 快速判断是否需要走算术解析器，
  // 避免对纯变量名也做完整的递归下降解析（性能优化）。
  if (checkArithmetic(expr)) {
    double val = evalExpression(expr, context);
    result += (val == static_cast<int>(val))
                  ? QString::number(static_cast<int>(val))
                  : QString::number(val);
    return true;
  }

  // === 策略 3: 普通变量路径解析 ===
  // 兜底分支：通过 TemplateEngine::resolvePath 获取 JSON 值并转为字符串。
  // 支持嵌套属性（如 user.email）和方法后缀（如 .toLowerCase）。
  QJsonValue val = m_engine.resolvePath(expr, context);
  if (val.isString())
    result += val.toString();
  else if (val.isDouble()) {
    double d = val.toDouble();
    result += (d == static_cast<int>(d)) ? QString::number(static_cast<int>(d))
                                         : QString::number(d);
  } else if (val.isBool())
    result += val.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  return true;
}

// ====================================================================
// 算术表达式递归下降解析器实现
//
// 解析器采用经典的"运算符优先级"分层设计：
//   高优先级（先计算）：* / => evalMulDiv
//   低优先级（后计算）：+ - => evalAddSub
//
// 每层函数的职责：
//   evalAddSub:  处理 + 和 -，调用 evalMulDiv 获取操作数
//   evalMulDiv:  处理 * 和 /，调用 evalPrimary 获取操作数
//   evalPrimary: 处理原子元素（数字、变量、括号、一元符号）
// ====================================================================

double HandExpression::evalExpression(const QString &expr,
                                      const QJsonObject &context) const {
  int pos = 0;
  return evalAddSub(expr, pos, context);
}

/**
 * @brief 解析加减法表达式（优先级最低）
 *
 * 左结合的加法/减法运算。先解析左侧操作数（通过 evalMulDiv），
 * 然后循环匹配后续的 + / - 运算符。
 *
 * 在递归下降解析器中，本函数是**最外层**的运算符处理器：
 *   evalExpression() -> evalAddSub() -> evalMulDiv() -> evalPrimary()
 *
 * 由于加减法优先级最低，所以它负责"收集"所有连续的加减运算，
 * 而每个操作数的乘除法部分委托给 evalMulDiv 处理。
 *
 * 示例：
 *   "a + b - c + d" 解析为 (((a + b) - c) + d)
 *   "2 + 3 * 4" 中，本函数解析 "2"，然后 evalMulDiv 解析 "3 * 4"
 *
 * @param expr 表达式字符串（如 "price + tax - discount"）
 * @param pos 当前解析位置（输入输出参数，随解析推进）
 *             调用时指向加减法子表达式的起始位置
 *             返回时指向最后一个操作数之后的位置
 * @param context 变量上下文（用于解析表达式中引用的变量名）
 * @return 加减法子表达式的计算结果
 */
double HandExpression::evalAddSub(const QString &expr, int &pos,
                                  const QJsonObject &context) const {

  // 步骤1: 解析第一个操作数（可能是乘除法表达式、单个数字或变量）
  // 例如 "a + b - c" 中，先通过 evalMulDiv 解析出 left=a
  // 如果是 "2 + 3 * 4"，evalMulDiv 会先计算 3*4=12，然后返回 2+12=14
  double left = evalMulDiv(expr, pos, context);

  // 步骤2: 循环匹配后续的 + 或 - 运算符
  // 支持连续的加减运算，如 "a + b - c + d - e"
  while (pos < expr.length()) {

    // 跳过空格（处理 "2 + 3" 中的空格）
    while (pos < expr.length() && expr[pos].isSpace())
      ++pos;

    // 表达式已结束，退出循环
    if (pos >= expr.length())
      break;

    // 读取当前字符，判断是加法还是减法
    QChar ch = expr[pos];

    if (ch == '+') {
      // 加法：跳过 '+' 号，解析右操作数（包含可能的乘除法），累加到 left
      // 例如当前 left=2，遇到 "+ 3*4"，evalMulDiv 返回12，left 变为 14
      ++pos;
      left += evalMulDiv(expr, pos, context);
    } else if (ch == '-') {
      // 减法：跳过 '-' 号，解析右操作数（包含可能的乘除法），从 left 中减去
      ++pos;
      left -= evalMulDiv(expr, pos, context);
    } else {
      // 遇到非 +- 字符（如 ) ] , 等），说明加减法子表达式结束
      // 这通常意味着遇到了括号闭合、表达式结束或逗号分隔符
      break;
    }
  }

  // 返回加减法子表达式的最终结果
  return left;
}

/**
 * @brief 解析乘除法表达式（优先级中等）
 *
 * 左结合的乘法/除法运算。先解析左侧操作数（通过 evalPrimary），
 * 然后循环匹配后续的 * / 运算符。
 *
 * 运算符优先级：乘除法高于加减法，所以 evalAddSub 会调用本函数
 * 来获取完整的乘除法子表达式。
 *
 * 示例：
 *   "a * b / c" 解析为 ((a * b) / c)
 *   "2 * 3 + 4" 中，本函数只解析 "2 * 3"，+4 由上层 evalAddSub 处理
 *
 * @param expr 表达式字符串（如 "price * count + tax"）
 * @param pos 当前解析位置（输入输出参数，随解析推进）
 *             调用时指向乘除法子表达式的起始位置
 *             返回时指向最后一个操作数之后的位置
 * @param context 变量上下文（用于解析表达式中引用的变量名）
 * @return 乘除法子表达式的计算结果
 */
double HandExpression::evalMulDiv(const QString &expr, int &pos,
                                  const QJsonObject &context) const {

  // 步骤1: 解析第一个操作数（数字、变量、括号表达式或一元运算符）
  // 例如 "2 * 3" 中，先解析出 left=2
  double left = evalPrimary(expr, pos, context);

  // 步骤2: 循环匹配后续的 * 或 / 运算符
  // 支持连续的乘除运算，如 "a * b / c * d"
  while (pos < expr.length()) {

    // 跳过空格（处理 "2 * 3" 中的空格）
    while (pos < expr.length() && expr[pos].isSpace())
      ++pos;

    // 表达式已结束，退出循环
    if (pos >= expr.length())
      break;

    // 读取当前字符，判断是乘法还是除法
    QChar ch = expr[pos];

    if (ch == '*') {
      // 乘法：跳过 '*' 号，解析右操作数，累乘到 left
      ++pos;
      left *= evalPrimary(expr, pos, context);
    } else if (ch == '/') {
      // 除法：跳过 '/' 号，解析右操作数
      ++pos;
      double right = evalPrimary(expr, pos, context);

      // 除零保护：防止程序崩溃，设置错误信息并返回0
      if (right == 0) {
        m_engine.setError(QStringLiteral("Division by zero"));
        return 0;
      }

      // 执行除法运算
      left /= right;
    } else {
      // 遇到非 */ 字符（如 + - ) ] 等），说明乘除法子表达式结束
      // 上层调用者（evalAddSub）会继续处理后续的加减运算
      break;
    }
  }

  // 返回乘除法子表达式的最终结果
  return left;
}

double HandExpression::evalPrimary(const QString &expr, int &pos,
                                   const QJsonObject &context) const {
  while (pos < expr.length() && expr[pos].isSpace())
    ++pos;

  if (pos >= expr.length())
    return 0;

  QChar ch = expr[pos];

  // 一元正号：+x 直接取 x 的值（无实际效果）
  if (ch == '+')
    return ++pos, evalPrimary(expr, pos, context);

  // 一元负号：-x 取 x 的相反数
  if (ch == '-')
    return ++pos, -evalPrimary(expr, pos, context);

  // 括号表达式：(subExpr) 递归回到 evalAddSub 处理括号内内容
  if (ch == '(') {
    ++pos;
    double result = evalAddSub(expr, pos, context);
    while (pos < expr.length() && expr[pos].isSpace())
      ++pos;
    if (pos < expr.length() && expr[pos] == ')')
      ++pos;
    return result;
  }

  // 数字字面量：123, 3.14, .5 直接转为 double
  if (ch.isDigit() || ch == '.') {
    int start = pos;
    while (pos < expr.length() && (expr[pos].isDigit() || expr[pos] == '.'))
      ++pos;
    bool ok = false;
    double num = expr.mid(start, pos - start).toDouble(&ok);
    return ok ? num : 0;
  }

  // 变量引用：name, user.age 通过 resolvePath 获取值后转数字
  // 如果值是字符串且能转为数字则转换，否则返回 0
  if (ch.isLetter() || ch == '_') {
    int start = pos;
    while (pos < expr.length() && (expr[pos].isLetterOrNumber() ||
                                   expr[pos] == '.' || expr[pos] == '_'))
      ++pos;
    QJsonValue val =
        m_engine.resolvePath(expr.mid(start, pos - start), context);
    if (val.isDouble())
      return val.toDouble();
    if (val.isString()) {
      bool ok = false;
      double d = val.toString().toDouble(&ok);
      return ok ? d : 0;
    }
    return 0;
  }

  return 0;
}