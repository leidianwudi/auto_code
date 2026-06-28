/**
 * @file expression_handler.cpp
 * @brief 表达式处理器实现（自包含：算术解析器 + 变量求值）
 */

#include "expression_handler.h"
#include "../template_engine.h"

namespace {

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

bool ExpressionHandler::handle(const QString &block, int &pos,
                               const QString &expr, const QJsonObject &context,
                               QString &result) const {
  Q_UNUSED(block)
  Q_UNUSED(pos)

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

  if (checkArithmetic(expr)) {
    double val = evalExpression(expr, context);
    result += (val == static_cast<int>(val))
                  ? QString::number(static_cast<int>(val))
                  : QString::number(val);
    return true;
  }

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

double ExpressionHandler::evalExpression(const QString &expr,
                                         const QJsonObject &context) const {
  int pos = 0;
  return evalAddSub(expr, pos, context);
}

double ExpressionHandler::evalAddSub(const QString &expr, int &pos,
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

double ExpressionHandler::evalMulDiv(const QString &expr, int &pos,
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

double ExpressionHandler::evalPrimary(const QString &expr, int &pos,
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
    if (val.isString()) {
      bool ok = false;
      double d = val.toString().toDouble(&ok);
      return ok ? d : 0;
    }
    return 0;
  }

  return 0;
}