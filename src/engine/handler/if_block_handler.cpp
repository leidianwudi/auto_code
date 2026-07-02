/**
 * @file if_block_handler.cpp
 * @brief 条件块处理器实现
 */

#include "if_block_handler.h"
#include "../template_engine.h"

/**
 * @brief 判断 JSON 值是否为 truthy（JavaScript 风格）
 *
 * 用于 ${if condition} 条件判断。与 JavaScript 的 Boolean() 转换规则一致：
 *
 * | 类型        | 值                          | 结果  |
 * |------------|-----------------------------|-------|
 * | bool       | true                        | true  |
 * | bool       | false                       | false |
 * | string     | 非空字符串                  | true  |
 * | string     | "" (空)                     | false |
 * | double     | 非0数字 (含负数、小数)      | true  |
 * | double     | 0                           | false |
 * | array      | 非空数组                    | true  |
 * | array      | [] (空)                     | false |
 * | object     | {} (任意对象)               | true  |
 * | null       | —                           | false |
 *
 * @param val 待判断的 JSON 值
 * @return true 表示条件成立，false 表示条件不成立
 */
static bool isTruthy(const QJsonValue &val) {
  if (val.isBool())
    return val.toBool();
  if (val.isString())
    return !val.toString().isEmpty();
  if (val.isDouble())
    return val.toDouble() != 0;
  if (val.isArray())
    return !val.toArray().isEmpty();
  if (val.isObject())
    return true;
  return false; // null / undefined
}

bool IfBlockHandler::handle(const QString &block, int &pos, const QString &expr,
                            const QJsonObject &context, QString &result) const {
  QString condition = expr.mid(3).trimmed();

  // 支持 ! 取反
  bool negate = false;
  if (condition.startsWith('!')) {
    negate = true;
    condition = condition.mid(1).trimmed();
  }

  int ifStart = pos;
  int elsePos = block.indexOf(QStringLiteral("${else}"), pos);
  int ifEnd = block.indexOf(QStringLiteral("${/if}"), pos);

  if (ifEnd == -1) {
    m_engine.setError(QStringLiteral("Unclosed ${if %1}").arg(condition));
    return false;
  }

  QString thenBlock;
  QString elseBlock;

  if (elsePos != -1 && elsePos < ifEnd) {
    thenBlock = block.mid(ifStart, elsePos - ifStart);
    elseBlock = block.mid(elsePos + 7, ifEnd - elsePos - 7);
  } else {
    thenBlock = block.mid(ifStart, ifEnd - ifStart);
  }

  pos = ifEnd + 6;

  QJsonValue val = m_engine.resolvePath(condition, context);
  // 如果 resolvePath 找不到，尝试作为表达式求值（如 fileExists(path)）
  if (val.isNull() || val.isUndefined()) {
    QString exprResult =
        m_engine.renderBlock(QStringLiteral("${%1}").arg(condition), context);
    if (exprResult == QStringLiteral("true"))
      val = QJsonValue(true);
    else if (exprResult == QStringLiteral("false"))
      val = QJsonValue(false);
  }
  bool truthy = isTruthy(val);
  if (negate)
    truthy = !truthy;
  if (truthy) {
    result += m_engine.renderBlock(thenBlock, context);
  } else {
    result += m_engine.renderBlock(elseBlock, context);
  }
  return true;
}