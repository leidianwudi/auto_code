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

std::unique_ptr<BlockHandler>
HandlerFactory::createHandler(const QString &expr) const {
  if (expr.startsWith(QStringLiteral("each ")))
    return std::make_unique<EachBlockHandler>(m_engine);
  if (expr.startsWith(QStringLiteral("if ")))
    return std::make_unique<IfBlockHandler>(m_engine);
  return std::make_unique<ExpressionHandler>(m_engine);
}