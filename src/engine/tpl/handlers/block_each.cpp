/**
 * @file block_each.cpp
 * @brief 循环块处理器实现
 *
 * 处理模板中的 ${each items}...${/each} 循环语法。
 * 支持遍历 JSON 数组，为每个元素渲染循环体内容。
 */

#include "block_each.h"

#include "../../ac_language.h"
#include "../tpl_engine.h"


/**
 * @brief 处理 ${each varName}...${/each} 循环块
 *
 * 语法格式：
 *   ${each items}
 *     ...循环体（可引用当前元素的属性）...
 *   ${/each}
 *
 * 工作流程：
 * 1. 从表达式中提取变量名（去掉 "each " 前缀）
 * 2. 查找匹配的 ${/each} 结束标记
 * 3. 提取循环体内容
 * 4. 通过 resolvePath 获取数组数据
 * 5. 遍历数组，为每个元素创建独立上下文并渲染循环体
 *
 * 示例：
 * 模板：${each users}${name}: ${email}\n${/each}
 * 数据：{"users": [{"name":"张三","email":"z@test.com"},
 * {"name":"李四","email":"l@test.com"}]} 输出：张三: z@test.com\n李四:
 * l@test.com\n
 */
bool BlockEach::handle(const QString &block, int &pos, const QString &expr,
                       const QJsonObject &context, QString &result) const {
  // 提取变量名（去掉 "each " 前缀）
  QString varName = expr.mid(5).trimmed();

  // 查找匹配的闭合标签 ${/each}
  int closePos =
      TplBlock::findMatchingClose(block, pos, QString::fromLatin1(AcTemplate::kEachPrefix),
                                  QString::fromLatin1(AcTemplate::kEachClose));
  if (closePos == -1) {
    const_cast<TplEngine &>(m_engine).setError(QStringLiteral("Unclosed ${each ") + varName +
                                               QStringLiteral("}"));
    return false;
  }

  // 提取循环体内容
  QString body = block.mid(pos, closePos - pos);
  pos = closePos + QString::fromLatin1(AcTemplate::kEachClose).length();

  // 获取数组数据
  QJsonValue arrVal = m_engine.resolvePath(varName, context);
  if (!arrVal.isArray()) {
    const_cast<TplEngine &>(m_engine).setError(QStringLiteral("'%1' is not an array").arg(varName));
    return false;
  }

  QJsonArray arr = arrVal.toArray();
  for (const QJsonValue &item : arr) {
    // 为每个元素创建独立的上下文
    QJsonObject itemContext = context;
    if (item.isObject()) {
      QJsonObject itemObj = item.toObject();
      for (auto it = itemObj.begin(); it != itemObj.end(); ++it) itemContext[it.key()] = it.value();
    } else {
      // 基本类型值放在 "." 键下（与 ${.} 语法配合）
      itemContext[QString::fromLatin1(AcTemplate::kCurrentItem)] = item;
      itemContext[varName] = item;
    }
    result += m_engine.renderBlock(body, itemContext);
  }

  return true;
}