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
  if (isTruthy(val)) {
    result += m_engine.renderBlock(thenBlock, context);
  } else {
    result += m_engine.renderBlock(elseBlock, context);
  }
  return true;
}