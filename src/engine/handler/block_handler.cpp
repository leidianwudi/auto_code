/**
 * @file block_handler.cpp
 * @brief 模板块处理器 — 工厂实现 + 共享辅助函数
 */

#include "block_handler.h"
#include "each_block_handler.h"
#include "expression_handler.h"
#include "if_block_handler.h"

static QString jsonValueToString(const QJsonValue &val) {
  if (val.isString())
    return val.toString();
  if (val.isDouble()) {
    double d = val.toDouble();
    if (d == static_cast<int>(d))
      return QString::number(static_cast<int>(d));
    return QString::number(d);
  }
  if (val.isBool())
    return val.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  if (val.isNull())
    return QString();
  return val.toString();
}

/**
 * @brief 根据表达式前缀检测其类型
 *
 * 按优先级依次匹配：
 * 1. "each " 前缀（5 字符）=> 循环块
 * 2. "if "   前缀（3 字符）=> 条件块
 * 3. 其他              => 普通表达式（兜底）
 *
 * @param expr ${...} 内的表达式内容
 * @return 对应的 ExprType 枚举值
 */
ExprType HandlerFactory::detectType(const QString &expr) {
  if (expr.startsWith(QStringLiteral("each ")))
    return ExprType::Each;
  if (expr.startsWith(QStringLiteral("if ")))
    return ExprType::If;
  return ExprType::Expression;
}

std::unique_ptr<BlockHandler>
HandlerFactory::createHandler(const QString &expr) const {
  switch (detectType(expr)) {
  case ExprType::Each:
    return std::make_unique<EachBlockHandler>(m_engine);
  case ExprType::If:
    return std::make_unique<IfBlockHandler>(m_engine);
  case ExprType::Expression:
    return std::make_unique<ExpressionHandler>(m_engine);
  }
  return std::make_unique<ExpressionHandler>(m_engine);
}