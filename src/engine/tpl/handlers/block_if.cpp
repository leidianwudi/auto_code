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
 * 或带 else if 分支：
 *   ${if condition1}
 *     ...条件1成立时渲染的内容...
 *   ${else if condition2}
 *     ...条件2成立时渲染的内容...
 *   ${else}
 *     ...以上条件都不成立时渲染的内容...
 *   ${/if}
 *
 * 条件判断规则与 JavaScript 一致：
 * - 非空字符串、非零数字、非空数组、非空对象 => 成立
 * - 空字符串、0、空数组、null、undefined、false => 不成立
 */
bool BlockIf::handle(const QString &block, int &pos, const QString &expr,
                     const QJsonObject &context, QString &result) const {
  // 提取条件表达式（去掉 "if " 或 "else if " 前缀）
  QString condition;
  if (expr.startsWith(QString::fromLatin1(AcTemplate::kElse) + QLatin1Char(' ') +
                      QString::fromLatin1(AcTemplate::kIfPrefix))) {
    condition = expr.mid(8).trimmed();  // 去掉 "else if "
  } else {
    condition = expr.mid(3).trimmed();  // 去掉 "if "
  }

  // 支持 ! 取反：${if !fileExists(path)} 表示"不存在"
  bool negate = false;
  if (condition.startsWith(QLatin1Char('!'))) {
    negate = true;
    condition = condition.mid(1).trimmed();
  }

  // 解析条件值
  QJsonValue condVal = m_engine.resolvePath(condition, context);
  bool truthy = isTruthy(condVal);
  if (negate) truthy = !truthy;

  // 查找 ${else} / ${else if} 和 ${/if}
  int closePos = TplBlock::findMatchingClose(block, pos, QString::fromLatin1(AcTemplate::kIfPrefix),
                                             QString::fromLatin1(AcTemplate::kIfClose));
  if (closePos == -1) {
    const_cast<TplEngine &>(m_engine).setError(QStringLiteral("Unclosed ${if ") + condition +
                                               QStringLiteral("}"));
    return false;
  }

  // 在 ${if} 和 ${/if} 之间查找 ${else} 或 ${else if}（支持嵌套，避免误找子 if 内的 else）
  bool isElseIf = false;
  int elsePos = TplBlock::findElsePos(block, pos, &isElseIf);

  if (truthy) {
    // 条件成立：渲染 then 部分
    QString thenBody;
    if (elsePos != -1 && elsePos < closePos)
      thenBody = block.mid(pos, elsePos - pos);
    else
      thenBody = block.mid(pos, closePos - pos);
    result += m_engine.renderBlock(thenBody, context);
  } else if (elsePos != -1 && elsePos < closePos) {
    if (isElseIf) {
      // ${else if condition}：提取 else if 部分，递归渲染
      // else if 标签格式：${else if condition}
      // 找到 ${else if condition} 的结束位置 }
      int elseIfExprStart = elsePos + 2;  // 跳过 ${
      int elseIfExprEnd = block.indexOf(QLatin1Char('}'), elseIfExprStart);
      if (elseIfExprEnd == -1) {
        const_cast<TplEngine &>(m_engine).setError(QStringLiteral("Malformed ${else if}"));
        return false;
      }
      // else if 部分从 } 之后到 ${/if} 之前
      // 这部分可能还包含更多 ${else if} 或 ${else}，交给递归的 renderBlock 处理
      // 构造一个虚拟的 ${if condition}...${/if} 块，让 renderBlock 递归处理
      QString elseIfExpr = block.mid(elseIfExprStart, elseIfExprEnd - elseIfExprStart).trimmed();
      // elseIfExpr 格式为 "else if condition"，提取 "if condition" 部分
      QString ifExpr = elseIfExpr.mid(5).trimmed();  // 去掉 "else "，保留 "if condition"
      // 构造虚拟块：${ifExpr}...body...${/if}
      QString elseIfBody = block.mid(elseIfExprEnd + 1, closePos - elseIfExprEnd - 1);
      QString virtualBlock = QString::fromLatin1(AcTemplate::kExprOpen) + ifExpr + QChar('}') +
                             elseIfBody + QString::fromLatin1(AcTemplate::kIfClose);
      result += m_engine.renderBlock(virtualBlock, context);
    } else {
      // ${else}（不带 if）：渲染 else 部分
      int elseTagEnd = elsePos + QString::fromLatin1(AcTemplate::kElse).length();
      QString elseBody = block.mid(elseTagEnd, closePos - elseTagEnd);
      result += m_engine.renderBlock(elseBody, context);
    }
  }

  pos = closePos + 6;  // 跳过 ${/if}
  return true;
}