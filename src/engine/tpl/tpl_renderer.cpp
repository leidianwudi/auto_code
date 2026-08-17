/**
 * @file tpl_renderer.cpp
 * @brief 模板渲染器实现
 *
 * 遍历 AST 树，输出最终字符串。
 *
 * 表达式求值策略：
 *   1. 内置函数：printLog(text)、fileExists(path)
 *   2. 循环变量：${this}、${.}
 *   3. 算术表达式：含四则运算符的表达式
 *   4. 普通变量路径：通过 TplEngine::resolvePath 解析
 */

#include "tpl_renderer.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonValue>
#include <cmath>
#include <optional>

#include "../ac_language.h"
#include "tpl_engine.h"

namespace TplRenderer {

namespace {

/// @brief 检测表达式是否包含算术运算符（顶层，非括号内）
bool checkArithmetic(const QString &expr) {
  int parenDepth = 0;
  for (int i = 0; i < expr.length(); ++i) {
    QChar ch = expr[i];
    if (ch == '(')
      ++parenDepth;
    else if (ch == ')')
      --parenDepth;
    else if ((ch == '+' || ch == '-' || ch == '*' || ch == '/') && parenDepth == 0) {
      // 开头的 +/- 视为一元运算符
      if ((ch == '+' || ch == '-') &&
          (i == 0 || expr[i - 1] == '(' || expr[i - 1] == '+' || expr[i - 1] == '-' ||
           expr[i - 1] == '*' || expr[i - 1] == '/'))
        continue;
      return true;
    }
  }
  return false;
}

/// @brief 去掉字符串字面量的外层引号
QString stripQuotes(const QString &s) {
  if (s.length() >= 2 &&
      ((s.startsWith('\'') && s.endsWith('\'')) || (s.startsWith('"') && s.endsWith('"')))) {
    return s.mid(1, s.length() - 2);
  }
  return s;
}

/// @brief 解析函数调用表达式，提取参数字符串
std::optional<QString> parseFuncArg(const QString &expr, const QString &funcName) {
  QString prefix = funcName + QLatin1Char('(');
  if (!expr.startsWith(prefix) || !expr.endsWith(QLatin1Char(')'))) return std::nullopt;
  return expr.mid(prefix.length(), expr.length() - prefix.length() - 1).trimmed();
}

/// @brief 将 QJsonValue 转为字符串（用于输出）
QString valueToString(const QJsonValue &v) {
  if (v.isString()) return v.toString();
  if (v.isDouble()) {
    double d = v.toDouble();
    if (d == std::floor(d)) {
      return QString::number(static_cast<qint64>(d));
    }
    return QString::number(d);
  }
  if (v.isBool())
    return v.toBool() ? QString::fromLatin1(AcKeyword::kTrue)
                      : QString::fromLatin1(AcKeyword::kFalse);
  return {};
}

/// @brief 解析函数调用的字符串参数
QString resolveStringArg(const QString &raw, const QJsonObject &context, const TplEngine &engine) {
  QString stripped = stripQuotes(raw);
  if (stripped != raw) return stripped;  // 原值带引号，是字面量
  QJsonValue v = engine.resolvePath(raw, context);
  if (!v.isNull() && !v.isUndefined()) return v.toString();
  return raw;
}

/// @brief 算术表达式递归下降求值
QJsonValue evalAddSub(const QString &expr, int &pos, const QJsonObject &context,
                      const TplEngine &engine);
QJsonValue evalMulDiv(const QString &expr, int &pos, const QJsonObject &context,
                      const TplEngine &engine);

QJsonValue evalPrimary(const QString &expr, int &pos, const QJsonObject &context,
                       const TplEngine &engine) {
  while (pos < expr.length() && expr[pos].isSpace()) ++pos;
  if (pos >= expr.length()) return QJsonValue();
  QChar ch = expr[pos];
  if (ch == '+') return ++pos, evalPrimary(expr, pos, context, engine);
  if (ch == '-') return ++pos, QJsonValue(-evalPrimary(expr, pos, context, engine).toDouble());
  if (ch == '(') {
    ++pos;
    QJsonValue result = evalAddSub(expr, pos, context, engine);
    while (pos < expr.length() && expr[pos].isSpace()) ++pos;
    if (pos < expr.length() && expr[pos] == ')') ++pos;
    return result;
  }
  if (ch == '"' || ch == '\'') {
    QChar quote = ch;
    ++pos;
    int start = pos;
    while (pos < expr.length() && expr[pos] != quote) ++pos;
    QString str = expr.mid(start, pos - start);
    if (pos < expr.length()) ++pos;
    return QJsonValue(str);
  }
  if (ch.isDigit() || ch == '.') {
    int start = pos;
    while (pos < expr.length() && (expr[pos].isDigit() || expr[pos] == '.')) ++pos;
    bool ok = false;
    double num = expr.mid(start, pos - start).toDouble(&ok);
    return ok ? QJsonValue(num) : QJsonValue();
  }
  if (ch.isLetter() || ch == '_') {
    int start = pos;
    while (pos < expr.length() &&
           (expr[pos].isLetterOrNumber() || expr[pos] == '.' || expr[pos] == '_'))
      ++pos;
    return engine.resolvePath(expr.mid(start, pos - start), context);
  }
  return QJsonValue();
}

QJsonValue evalMulDiv(const QString &expr, int &pos, const QJsonObject &context,
                      const TplEngine &engine) {
  QJsonValue left = evalPrimary(expr, pos, context, engine);
  while (pos < expr.length()) {
    while (pos < expr.length() && expr[pos].isSpace()) ++pos;
    if (pos >= expr.length()) break;
    QChar ch = expr[pos];
    if (ch == '*') {
      ++pos;
      QJsonValue right = evalPrimary(expr, pos, context, engine);
      left = QJsonValue(left.toDouble() * right.toDouble());
    } else if (ch == '/') {
      ++pos;
      QJsonValue right = evalPrimary(expr, pos, context, engine);
      if (right.toDouble() == 0) {
        const_cast<TplEngine &>(engine).setError(QStringLiteral("Division by zero"));
        return QJsonValue();
      }
      left = QJsonValue(left.toDouble() / right.toDouble());
    } else {
      break;
    }
  }
  return left;
}

QJsonValue evalAddSub(const QString &expr, int &pos, const QJsonObject &context,
                      const TplEngine &engine) {
  QJsonValue left = evalMulDiv(expr, pos, context, engine);
  while (pos < expr.length()) {
    while (pos < expr.length() && expr[pos].isSpace()) ++pos;
    if (pos >= expr.length()) break;
    QChar ch = expr[pos];
    if (ch == '+') {
      ++pos;
      QJsonValue right = evalMulDiv(expr, pos, context, engine);
      if (left.isString() || right.isString()) {
        left = QJsonValue(valueToString(left) + valueToString(right));
      } else {
        left = QJsonValue(left.toDouble() + right.toDouble());
      }
    } else if (ch == '-') {
      ++pos;
      QJsonValue right = evalMulDiv(expr, pos, context, engine);
      left = QJsonValue(left.toDouble() - right.toDouble());
    } else {
      break;
    }
  }
  return left;
}

/// @brief 算术表达式求值入口
QJsonValue evalArithmetic(const QString &expr, const QJsonObject &context,
                          const TplEngine &engine) {
  int pos = 0;
  return evalAddSub(expr, pos, context, engine);
}

/// @brief 求值变量表达式（${expression}）
///
/// 按以下顺序尝试：
///   1. 内置函数 printLog(text)
///   2. 内置函数 fileExists(path)
///   3. 循环变量 ${this} 或 ${.}
///   4. 算术表达式（含四则运算符）
///   5. 普通变量路径
QString evalVariable(const QString &expr, const QJsonObject &context, const TplEngine &engine) {
  // 1. printLog(text)
  if (auto arg = parseFuncArg(expr, QString::fromLatin1(AcBuiltin::kPrintLog))) {
    QString resolved = resolveStringArg(*arg, context, engine);
    auto cb = engine.logCallback();
    if (cb) cb(resolved, false);
    return {};
  }

  // 2. fileExists(path)
  if (auto arg = parseFuncArg(expr, QString::fromLatin1(AcBuiltin::kFileExists))) {
    QString resolved = resolveStringArg(*arg, context, engine);
    bool exists = QFileInfo::exists(resolved);
    return exists ? QString::fromLatin1(AcKeyword::kTrue) : QString::fromLatin1(AcKeyword::kFalse);
  }

  // 3. 循环变量 ${this} 或 ${.}
  if (expr == QString::fromLatin1(AcKeyword::kThis) ||
      expr == QString::fromLatin1(AcTemplate::kCurrentItem)) {
    QJsonValue v = context.value(QString::fromLatin1(AcTemplate::kCurrentItem));
    return valueToString(v);
  }

  // 4. 算术表达式
  if (checkArithmetic(expr)) {
    return valueToString(evalArithmetic(expr, context, engine));
  }

  // 5. 普通变量路径
  return valueToString(engine.resolvePath(expr, context));
}

/// @brief 判断 JSON 值是否为 truthy
bool isTruthy(const QJsonValue &val) {
  switch (val.type()) {
    case QJsonValue::Bool:
      return val.toBool();
    case QJsonValue::String:
      return !val.toString().isEmpty();
    case QJsonValue::Double: {
      double d = val.toDouble();
      return d != 0.0 && !std::isnan(d);
    }
    case QJsonValue::Array:
      return !val.toArray().isEmpty();
    case QJsonValue::Object:
      return true;
    default:
      return false;  // Null / Undefined
  }
}

/// @brief 将表达式求值为 QJsonValue（支持字面量、内置函数和变量路径）
QJsonValue evalExprToJson(const QString &expr, const QJsonObject &context,
                          const TplEngine &engine) {
  // 1. 字符串字面量：'abc' / "abc"
  //    否则 == 比较右侧 "number" 会被当变量路径解析返回 null，比较永远失败
  QString trimmed = expr.trimmed();
  if (trimmed.length() >= 2 &&
      ((trimmed.startsWith(QChar('"')) && trimmed.endsWith(QChar('"'))) ||
       (trimmed.startsWith(QChar('\'')) && trimmed.endsWith(QChar('\''))))) {
    return QJsonValue(trimmed.mid(1, trimmed.length() - 2));
  }
  // 2. 数字字面量：123 / 3.14
  {
    bool ok = false;
    double num = trimmed.toDouble(&ok);
    if (ok) return QJsonValue(num);
  }
  // 3. 内置函数 fileExists(path)
  if (auto arg = parseFuncArg(expr, QString::fromLatin1(AcBuiltin::kFileExists))) {
    QString resolved = resolveStringArg(*arg, context, engine);
    return QJsonValue(QFileInfo::exists(resolved));
  }
  // 4. 内置函数 printLog(text)
  if (auto arg = parseFuncArg(expr, QString::fromLatin1(AcBuiltin::kPrintLog))) {
    QString resolved = resolveStringArg(*arg, context, engine);
    auto cb = engine.logCallback();
    if (cb) cb(resolved, false);
    return QJsonValue(QJsonValue::Null);
  }
  // 5. 变量路径
  return engine.resolvePath(expr, context);
}

/// @brief 比较两个 JSON 值是否相等（按类型严格比较）
static bool valuesEqual(const QJsonValue &l, const QJsonValue &r) {
  if (l.type() != r.type()) {
    // 数值与字符串不宽松互比（模板场景均为同类型比较）
    return false;
  }
  switch (l.type()) {
    case QJsonValue::Bool:
      return l.toBool() == r.toBool();
    case QJsonValue::Double:
      return l.toDouble() == r.toDouble();
    case QJsonValue::String:
      return l.toString() == r.toString();
    default:
      return l == r;
  }
}

/// @brief 在表达式顶层（引号、括号外）查找 == / != 比较运算符位置
/// @return 位置索引；未找到返回 -1。isNeq 标记是否为 !=
static int findTopLevelCompare(const QString &expr, bool &isNeq) {
  bool inSQuote = false, inDQuote = false;
  int parenDepth = 0;
  for (int i = 0; i + 1 < expr.length(); ++i) {
    const QChar c = expr[i];
    if (c == QChar('"') && !inSQuote) {
      inDQuote = !inDQuote;
    } else if (c == QChar('\'') && !inDQuote) {
      inSQuote = !inSQuote;
    } else if (!inSQuote && !inDQuote) {
      if (c == QChar('(')) {
        ++parenDepth;
      } else if (c == QChar(')')) {
        --parenDepth;
      } else if (parenDepth == 0 && c == QChar('=') && expr[i + 1] == QChar('=')) {
        isNeq = false;
        return i;
      } else if (parenDepth == 0 && c == QChar('!') && expr[i + 1] == QChar('=')) {
        isNeq = true;
        return i;
      }
    }
  }
  return -1;
}

/// @brief 解析条件表达式（支持 ! 取反、== / != 比较）
bool evalCondition(QString expr, const QJsonObject &context, const TplEngine &engine) {
  expr = expr.trimmed();
  // == / != 比较（先于 ! 前缀处理，避免 field.a != "x" 被误剥）
  bool isNeq = false;
  const int cmpPos = findTopLevelCompare(expr, isNeq);
  if (cmpPos > 0) {
    const QString lhs = expr.left(cmpPos).trimmed();
    const QString rhs = expr.mid(cmpPos + 2).trimmed();
    const QJsonValue l = evalExprToJson(lhs, context, engine);
    const QJsonValue r = evalExprToJson(rhs, context, engine);
    const bool equal = valuesEqual(l, r);
    return isNeq ? !equal : equal;
  }
  bool negate = false;
  if (expr.startsWith(QLatin1Char('!'))) {
    negate = true;
    expr = expr.mid(1).trimmed();
  }
  QJsonValue condVal = evalExprToJson(expr, context, engine);
  bool truthy = isTruthy(condVal);
#ifdef AC_DEBUG
  // 调试日志：输出条件求值结果
  if (auto cb = engine.logCallback()) {
    QString valStr;
    if (condVal.isBool())
      valStr = condVal.toBool() ? QString::fromLatin1(AcKeyword::kTrue)
                                : QString::fromLatin1(AcKeyword::kFalse);
    else if (condVal.isString())
      valStr = QStringLiteral("\"") + condVal.toString() + QStringLiteral("\"");
    else if (condVal.isDouble())
      valStr = QString::number(condVal.toDouble());
    else if (condVal.isNull())
      valStr = QString::fromLatin1(AcKeyword::kNull);
    else
      valStr = QStringLiteral("other");
    cb(QStringLiteral("DEBUG: condition='%1' value=%2 truthy=%3 negate=%4 result=%5")
           .arg(expr)
           .arg(valStr)
           .arg(truthy)
           .arg(negate)
           .arg(negate ? !truthy : truthy),
       false);
  }
#endif
  return negate ? !truthy : truthy;
}

/// @brief 递归渲染节点列表
QString renderNodes(const QList<QSharedPointer<TplAst::AstNode>> &nodes, const QJsonObject &context,
                    const TplEngine &engine);

/// @brief 渲染单个 If 节点
QString renderIf(const TplAst::IfNode *node, const QJsonObject &context, const TplEngine &engine) {
  for (const auto &branch : node->branches) {
    bool match;
    if (branch.isElse) {
      match = true;  // ${else} 无条件匹配
    } else {
      match = evalCondition(branch.condition, context, engine);
    }
    if (match) {
      return renderNodes(branch.body, context, engine);
    }
  }
  return {};  // 所有分支都不匹配
}

/// @brief 渲染单个 Each 节点
QString renderEach(const TplAst::EachNode *node, const QJsonObject &context,
                   const TplEngine &engine) {
  QJsonValue arrVal = engine.resolvePath(node->arrayName, context);
  if (!arrVal.isArray()) {
    const_cast<TplEngine &>(engine).setError(
        QStringLiteral("'%1' is not an array").arg(node->arrayName));
    return {};
  }

  QString result;
  QJsonArray arr = arrVal.toArray();
  int idx = 0;
  for (const QJsonValue &item : arr) {
    QJsonObject itemContext = context;
    // 注入循环元信息：${item名}_index（从 0 起）、${item名}_last（是否末项）
    // 模板可用 ${if !item_last},${/if} 实现末项无分隔符等排版
    itemContext[node->itemName + QStringLiteral("_index")] = idx;
    itemContext[node->itemName + QStringLiteral("_last")] = (idx == arr.size() - 1);
    if (item.isObject()) {
      QJsonObject itemObj = item.toObject();
      if (node->explicitNaming) {
        itemContext[node->itemName] = itemObj;
      } else {
        // 隐式命名：把对象所有属性合并到顶层
        for (auto it = itemObj.begin(); it != itemObj.end(); ++it) {
          itemContext[it.key()] = it.value();
        }
      }
    } else {
      // 基本类型值
      itemContext[QString::fromLatin1(AcTemplate::kCurrentItem)] = item;
      itemContext[node->itemName] = item;
    }
    result += renderNodes(node->body, itemContext, engine);
    ++idx;
  }
  return result;
}

QString renderNodes(const QList<QSharedPointer<TplAst::AstNode>> &nodes, const QJsonObject &context,
                    const TplEngine &engine) {
  QString result;
  for (const auto &node : nodes) {
    switch (node->type) {
      case TplAst::NodeType::Text: {
        auto *textNode = static_cast<TplAst::TextNode *>(node.data());
        result += textNode->text;
        break;
      }
      case TplAst::NodeType::Variable: {
        auto *varNode = static_cast<TplAst::VariableNode *>(node.data());
        result += evalVariable(varNode->expr, context, engine);
        break;
      }
      case TplAst::NodeType::If: {
        auto *ifNode = static_cast<TplAst::IfNode *>(node.data());
        result += renderIf(ifNode, context, engine);
        break;
      }
      case TplAst::NodeType::Each: {
        auto *eachNode = static_cast<TplAst::EachNode *>(node.data());
        result += renderEach(eachNode, context, engine);
        break;
      }
    }
  }
  return result;
}

}  // namespace

QString render(const QList<QSharedPointer<TplAst::AstNode>> &nodes, const QJsonObject &context,
               const TplEngine &engine) {
  return renderNodes(nodes, context, engine);
}

}  // namespace TplRenderer
