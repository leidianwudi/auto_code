/**
 * @file block_expression.cpp
 * @brief 表达式处理器实现（自包含：算术解析器 + 变量求值）
 *
 * 本文件是 BlockExpression 的完整实现，包含：
 * - checkArithmetic() -- 判断表达式是否含算术运算符
 * - handle()          -- 三策略分发（循环变量 / 算术 / 普通变量）
 * - evalExpression()  -- 算术解析入口
 * - evalAddSub()      -- 加减法（最低优先级）
 * - evalMulDiv()      -- 乘除法（中等优先级）
 * - evalPrimary()     -- 基本元素（最高优先级）
 */

#include "block_expression.h"
#include "../../ac_language.h"
#include "../tpl_engine.h"

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
// BlockExpression::handle() -- 三策略分发
// ====================================================================

bool BlockExpression::handle(const QString &block, int &pos,
                             const QString &expr, const QJsonObject &context,
                             QString &result) const {
  Q_UNUSED(block)
  Q_UNUSED(pos)

  // === 策略 0: 内置函数调用 print(text) ===
  if (expr.startsWith(QString::fromLatin1(AcBuiltin::kPrint) + QLatin1Char('(')) &&
      expr.endsWith(QLatin1Char(')'))) {
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
  if (expr.startsWith(QString::fromLatin1(AcBuiltin::kFileExists) + QLatin1Char('(')) &&
      expr.endsWith(QLatin1Char(')'))) {
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
  if (expr == QString::fromLatin1(AcKeyword::kThis) ||
      expr == QString::fromLatin1(AcTemplate::kCurrentItem)) {
    QJsonValue v = context.value(QString::fromLatin1(AcTemplate::kCurrentItem));
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
  // 兜底分支：通过 TplEngine::resolvePath 获取 JSON 值并转为字符串。
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

double BlockExpression::evalExpression(const QString &expr,
                                       const QJsonObject &context) const {
  int pos = 0;
  return evalAddSub(expr, pos, context);
}

double BlockExpression::evalAddSub(const QString &expr, int &pos,
                                   const QJsonObject &context) const {
  double left = evalMulDiv(expr, pos, context);
  while (pos < expr.length()) {
    while (pos < expr.length() && expr[pos].isSpace())
      ++pos;
    if (pos >= expr.length())
      break;
    QChar ch = expr[pos];
    if (ch == '+') {
      ++pos;
      left += evalMulDiv(expr, pos, context);
    } else if (ch == '-') {
      ++pos;
      left -= evalMulDiv(expr, pos, context);
    } else {
      break;
    }
  }
  return left;
}

double BlockExpression::evalMulDiv(const QString &expr, int &pos,
                                   const QJsonObject &context) const {
  double left = evalPrimary(expr, pos, context);
  while (pos < expr.length()) {
    while (pos < expr.length() && expr[pos].isSpace())
      ++pos;
    if (pos >= expr.length())
      break;
    QChar ch = expr[pos];
    if (ch == '*') {
      ++pos;
      left *= evalPrimary(expr, pos, context);
    } else if (ch == '/') {
      ++pos;
      double right = evalPrimary(expr, pos, context);
      if (right == 0) {
        m_engine.setError(QStringLiteral("Division by zero"));
        return 0;
      }
      left /= right;
    } else {
      break;
    }
  }
  return left;
}

double BlockExpression::evalPrimary(const QString &expr, int &pos,
                                    const QJsonObject &context) const {
  while (pos < expr.length() && expr[pos].isSpace())
    ++pos;
  if (pos >= expr.length())
    return 0;
  QChar ch = expr[pos];
  if (ch == '+')
    return ++pos, evalPrimary(expr, pos, context);
  if (ch == '-')
    return ++pos, -evalPrimary(expr, pos, context);
  if (ch == '(') {
    ++pos;
    double result = evalAddSub(expr, pos, context);
    while (pos < expr.length() && expr[pos].isSpace())
      ++pos;
    if (pos < expr.length() && expr[pos] == ')')
      ++pos;
    return result;
  }
  if (ch.isDigit() || ch == '.') {
    int start = pos;
    while (pos < expr.length() && (expr[pos].isDigit() || expr[pos] == '.'))
      ++pos;
    bool ok = false;
    double num = expr.mid(start, pos - start).toDouble(&ok);
    return ok ? num : 0;
  }
  if (ch.isLetter() || ch == '_') {
    int start = pos;
    while (pos < expr.length() && (expr[pos].isLetterOrNumber() ||
                                   expr[pos] == '.' || expr[pos] == '_'))
      ++pos;
    QJsonValue val =
        m_engine.resolvePath(expr.mid(start, pos - start), context);
    if (val.isDouble())
      return val.toDouble();
    return 0;
  }
  return 0;
}