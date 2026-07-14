/**
 * @file block_if.cpp
 * @brief 条件块处理器实现
 */

#include "block_if.h"

#include "../../ac_language.h"
#include "../tpl_engine.h"


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
    default:  // Null / Undefined
      return false;
  }
}

/**
 * @brief 处理 ${if condition}...${else}...${/if} 条件块
 *
 * 模板语法：
 *   ${if condition}
 *     ...条件成立时渲染的内容...
 *   ${/if}
 *
 * 或带 else 分支：
 *   ${if condition}
 *     ...条件成立时渲染的内容...
 *   ${else}
 *     ...条件不成立时渲染的内容...
 *   ${/if}
 *
 * 条件判断规则与 JavaScript 一致：
 * - 非空字符串、非零数字、非空数组、非空对象 => 成立
 * - 空字符串、0、空数组、null、undefined、false => 不成立
 */
bool BlockIf::handle(const QString &block, int &pos, const QString &expr,
                     const QJsonObject &context, QString &result) const {
  // 提取条件表达式（去掉 "if " 前缀）
  QString condition = expr.mid(3).trimmed();

  // 解析条件值
  QJsonValue condVal = m_engine.resolvePath(condition, context);

  // 查找 ${else} 和 ${/if}
  int closePos = TplBlock::findMatchingClose(block, pos, QString::fromLatin1(AcTemplate::kIfPrefix),
                                             QString::fromLatin1(AcTemplate::kIfClose));
  if (closePos == -1) {
    const_cast<TplEngine &>(m_engine).setError(QStringLiteral("Unclosed ${if ") + condition +
                                               QStringLiteral("}"));
    return false;
  }

  // 在 ${if} 和 ${/if} 之间查找 ${else}
  int elsePos = block.indexOf(QString::fromLatin1(AcTemplate::kElse), pos);

  if (isTruthy(condVal)) {
    // 条件成立：渲染 then 部分
    QString thenBody;
    if (elsePos != -1 && elsePos < closePos)
      thenBody = block.mid(pos, elsePos - pos);
    else
      thenBody = block.mid(pos, closePos - pos);
    result += m_engine.renderBlock(thenBody, context);
  } else if (elsePos != -1 && elsePos < closePos) {
    // 条件不成立且有 else：渲染 else 部分
    QString elseBody = block.mid(elsePos + 7, closePos - elsePos - 7);
    result += m_engine.renderBlock(elseBody, context);
  }

  pos = closePos + 6;  // 跳过 ${/if}
  return true;
}