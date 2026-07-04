/**
 * @file tpl_block.cpp
 * @brief 模板块处理器 — 工厂实现 + 共享辅助函数
 */

#include "tpl_block.h"
#include "tpl_each_block.h"
#include "tpl_expression.h"
#include "tpl_if_block.h"

/**
 * @brief 查找匹配的闭合标签（支持嵌套）
 *
 * 从 startPos 开始扫描 block，计数嵌套的 openTag 和 closeTag，
 * 返回深度归零时的位置（即匹配的闭合标签位置）。
 *
 * @param block  完整的模板字符串
 * @param startPos 开始搜索的位置
 * @param openPrefix 开放标签前缀（如 "if "、"each "）
 * @param closeTag 闭合标签（如 "${/if}"、"${/each}"）
 * @return 匹配的闭合标签位置，未找到返回 -1
 */
int findMatchingClose(const QString &block, int startPos,
                      const QString &openPrefix, const QString &closeTag) {
  int depth = 1;
  int pos = startPos;
  int openLen = 2 + openPrefix.length(); // "${" + prefix
  int closeLen = closeTag.length();

  while (pos < block.length()) {
    int nextOpen = block.indexOf(QStringLiteral("${") + openPrefix, pos);
    int nextClose = block.indexOf(closeTag, pos);

    if (nextClose == -1)
      return -1; // 没有闭合标签

    // 如果下一个 open 在 close 之前，说明嵌套了一层
    if (nextOpen != -1 && nextOpen < nextClose) {
      ++depth;
      pos = nextOpen + openLen;
    } else {
      --depth;
      if (depth == 0)
        return nextClose;
      pos = nextClose + closeLen;
    }
  }
  return -1;
}

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

std::unique_ptr<TplBlock>
HandlerFactory::createHandler(const QString &expr) const {
  switch (detectType(expr)) {
  case ExprType::Each:
    return std::make_unique<TplEachBlock>(m_engine);
  case ExprType::If:
    return std::make_unique<TplIfBlock>(m_engine);
  case ExprType::Expression:
    return std::make_unique<TplExpression>(m_engine);
  }
  return std::make_unique<TplExpression>(m_engine);
}