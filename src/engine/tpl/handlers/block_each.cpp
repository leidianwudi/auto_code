/**
 * @file block_each.cpp
 * @brief 循环块处理器实现
 *
 * 处理模板中的 ${each items}...${/each} 循环语法，
 * 以及增强语法 ${each item in items}...${/each}。
 * 支持遍历 JSON 数组，为每个元素渲染循环体内容。
 */

#include "block_each.h"

#include <QRegularExpression>

#include "../../ac_language.h"
#include "../tpl_engine.h"

/**
 * @brief 处理 ${each items}...${/each} 或 ${each item in items}...${/each} 循环块
 *
 * 语法格式 1（隐式命名，向后兼容）：
 *   ${each items}
 *     ...循环体（直接引用元素属性名或用 ${.}/${this} 取当前元素）...
 *   ${/each}
 *
 * 语法格式 2（显式命名，推荐）：
 *   ${each item in items}
 *     ...循环体（通过 ${item} 引用当前元素）...
 *   ${/each}
 *
 * 工作流程：
 * 1. 从表达式中提取数组变量名（和可选的项变量名）
 * 2. 查找匹配的 ${/each} 结束标记
 * 3. 提取循环体内容
 * 4. 通过 resolvePath 获取数组数据
 * 5. 遍历数组，为每个元素创建独立上下文并渲染循环体
 *
 * 示例 1（隐式命名）：
 *   模板：${each users}${name}: ${email}\n${/each}
 *   数据：{"users": [{"name":"张三","email":"z@test.com"}]}
 *   输出：张三: z@test.com
 *
 * 示例 2（显式命名，推荐）：
 *   模板：${each user in users}${user.name}: ${user.email}\n${/each}
 *   数据：同上
 *   输出：张三: z@test.com
 */
bool BlockEach::handle(const QString &block, int &pos, const QString &expr,
                       const QJsonObject &context, QString &result) const {
  // 提取循环表达式（去掉 "each " 前缀）
  QString loopExpr = expr.mid(5).trimmed();

  // 判断是否为显式命名语法: "item in items"
  int inPos = loopExpr.indexOf(QString::fromLatin1(AcTemplate::kEachIn));

  QString itemName;   // 循环体内引用当前元素的名字（如 "user"）
  QString arrayName;  // 数组在上下文中的名字（如 "users"）

  if (inPos != -1) {
    // 显式命名: each item in items
    itemName = loopExpr.left(inPos).trimmed();
    arrayName = loopExpr.mid(inPos + QString::fromLatin1(AcTemplate::kEachIn).length()).trimmed();
  } else {
    // 隐式命名（向后兼容）: each items
    // itemName 与 arrayName 相同，循环体内可直接写 ${name} 等属性
    itemName = loopExpr;
    arrayName = loopExpr;
  }

  if (itemName.isEmpty() || arrayName.isEmpty()) {
    const_cast<TplEngine &>(m_engine).setError(QStringLiteral("invalid each syntax: ") + expr);
    return false;
  }

  // 查找匹配的闭合标签 ${/each}
  int closePos =
      TplBlock::findMatchingClose(block, pos, QString::fromLatin1(AcTemplate::kEachPrefix),
                                  QString::fromLatin1(AcTemplate::kEachClose));
  if (closePos == -1) {
    const_cast<TplEngine &>(m_engine).setError(QStringLiteral("Unclosed ${each ") + loopExpr +
                                               QStringLiteral("}"));
    return false;
  }

  // 提取循环体内容
  QString body = block.mid(pos, closePos - pos);
  pos = closePos + QString::fromLatin1(AcTemplate::kEachClose).length();

  // 循环体 trim：只移除标签行的缩进（前导空格/制表符）和尾部缩进
  // 所有空行、换行符都保留，让模板作者完全控制空行输出
  {
    // 头部：renderBlock 已通过 skipRestOfLine 跳过了 ${each ...} 标签行的换行符
    // body 从下一行开始，这里不需要跳过任何内容
    int start = 0;

    // 尾部：只移除末尾的空格/制表符（${/each} 行的缩进）
    // 保留末尾的换行符和纯空白行，让模板作者完全控制空行输出
    int end = body.length();
    while (end > start && (body[end - 1] == QChar(' ') || body[end - 1] == QChar('\t'))) {
      end--;  // 跳过末尾的空格/制表符
    }

    if (start > 0 || end < body.length()) {
      body = body.mid(start, end - start);
    }
  }

  // 获取数组数据
  QJsonValue arrVal = m_engine.resolvePath(arrayName, context);
  if (!arrVal.isArray()) {
    const_cast<TplEngine &>(m_engine).setError(
        QStringLiteral("'%1' is not an array").arg(arrayName));
    return false;
  }

  QJsonArray arr = arrVal.toArray();
  for (const QJsonValue &item : arr) {
    // 为每个元素创建独立的上下文
    QJsonObject itemContext = context;

    if (item.isObject()) {
      QJsonObject itemObj = item.toObject();

      // ── 显式命名（item in items）：将对象存入 itemName 键下 ──
      //   例如 each user in users → 可用 ${user.name} 引用
      if (inPos != -1) {
        itemContext[itemName] = itemObj;
      } else {
        // ── 隐式命名（items）：兼容模式，每个属性直接合并到顶层 ──
        for (auto it = itemObj.begin(); it != itemObj.end(); ++it) {
          itemContext[it.key()] = it.value();
        }
      }
    } else {
      // 基本类型值（字符串/数字/布尔）
      // 同时存入 "." 键（${.} / ${this}），以及 itemName 键下
      itemContext[QString::fromLatin1(AcTemplate::kCurrentItem)] = item;
      itemContext[itemName] = item;
    }

    result += m_engine.renderBlock(body, itemContext);
  }

  return true;
}