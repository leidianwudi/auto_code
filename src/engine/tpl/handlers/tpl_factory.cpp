/**
 * @file tpl_factory.cpp
 * @brief 模板块处理器 — 工厂实现
 */

#include "tpl_factory.h"

#include "../../ac_language.h"
#include "block_each.h"
#include "block_expression.h"
#include "block_if.h"

// ── 类型检测 ──

TplFactory::BlockType TplFactory::detectType(const QString &expr) {
  if (expr.startsWith(QString::fromLatin1(AcTemplate::kEachPrefix))) return BlockType::Each;
  if (expr.startsWith(QString::fromLatin1(AcTemplate::kIfPrefix))) return BlockType::If;
  // ${else if condition} 也走 If 分支
  if (expr.startsWith(QString::fromLatin1(AcTemplate::kElse) + QLatin1Char(' ') +
                      QString::fromLatin1(AcTemplate::kIfPrefix)))
    return BlockType::If;
  return BlockType::Expression;
}

// ── 创建处理器 ──

std::unique_ptr<TplBlock> TplFactory::createHandler(const QString &expr) const {
  switch (detectType(expr)) {
    case BlockType::Each:
      return std::make_unique<BlockEach>(m_engine);
    case BlockType::If:
      return std::make_unique<BlockIf>(m_engine);
    default:
      return std::make_unique<BlockExpression>(m_engine);
  }
}