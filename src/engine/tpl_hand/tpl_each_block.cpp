/**
 * @file tpl_each_block.cpp
 * @brief 循环块处理器实现
 *
 * 处理模板中的 ${each items}...${/each} 循环语法。
 * 支持遍历 JSON 数组，为每个元素渲染循环体内容。
 */

#include "tpl_each_block.h"
#include "../engine_tpl.h"

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
 *
 * @param block 完整模板片段（用于查找 ${/each} 结束标记）
 * @param pos 当前解析位置（输入输出参数）
 *             调用时指向 ${each ...} 之后的位置
 *             返回时指向 ${/each} 之后的位置
 * @param expr ${...} 内的表达式内容（如 "each users"）
 * @param context 变量上下文（包含当前作用域的所有变量）
 * @param result 渲染结果输出（追加模式，每次遍历都追加渲染内容）
 * @return true 表示处理成功；false 表示未找到 ${/each} 结束标记
 */
bool TplEachBlock::handle(const QString &block, int &pos, const QString &expr,
                          const QJsonObject &context, QString &result) const {

  // 步骤1: 提取循环变量名
  // 表达式格式为 "each xxx"，去掉前5个字符 "each " 得到变量名
  // 例如 "each users" => varName = "users"
  QString varName = expr.mid(5).trimmed();

  // 步骤2: 记录循环体起始位置（当前位置就是循环体开始的地方）
  int eachStart = pos;

  // 步骤3: 查找 ${/each} 结束标记
  // 从当前位置向后搜索，必须找到结束标记才能构成完整的循环块
  int eachEnd = findMatchingClose(block, pos, QStringLiteral("each "),
                                  QStringLiteral("${/each}"));
  if (eachEnd == -1) {
    // 未找到结束标记，设置错误信息并返回失败
    m_engine.setError(QStringLiteral("Unclosed ${each %1}").arg(varName));
    return false;
  }

  // 步骤4: 提取循环体内容（${each} 和 ${/each} 之间的部分）
  // 这个内容会被多次渲染（每个数组元素一次）
  QString body = block.mid(eachStart, eachEnd - eachStart);

  // 步骤5: 更新位置指针到 ${/each} 之后（跳过8个字符 "${/each}"）
  pos = eachEnd + 8;

  // 步骤6: 通过变量名获取对应的值
  // 例如 varName="users"，则从 context 中获取 users 对应的 JSON 值
  QJsonValue val = m_engine.resolvePath(varName, context);

  // 如果不是数组，直接返回成功（不输出任何内容）
  if (!val.isArray())
    return true;

  // 转换为数组对象进行遍历
  QJsonArray arr = val.toArray();

  // 步骤7: 遍历数组的每个元素
  for (int i = 0; i < arr.size(); ++i) {

    // 创建新的上下文（继承父级上下文的所有变量）
    QJsonObject itemContext = context;

    // 获取当前数组元素
    QJsonValue item = arr[i];

    if (item.isObject()) {
      // 情况A: 当前元素是对象（如 {"name":"张三","email":"z@test.com"}）
      // 将对象的所有属性合并到上下文中，这样循环体内可以直接用 ${name} 引用
      QJsonObject obj = item.toObject();
      for (auto it = obj.begin(); it != obj.end(); ++it)
        itemContext.insert(it.key(), it.value());

      // 设置 "." 为空值（表示当前元素是对象，不是基本类型）
      itemContext.insert(QStringLiteral("."), QJsonValue());
    } else {
      // 情况B: 当前元素是基本类型（如字符串、数字、布尔值）
      // 将当前元素存储在 "." 键中，循环体内可以用 ${this} 或 ${.} 引用
      // 例如数组 ["apple","banana"]，第一个元素的 ${.} 就是 "apple"
      itemContext.insert(QStringLiteral("."), item);
    }

    // 使用新上下文渲染循环体，并将结果追加到 result
    // renderBlock 会递归处理循环体中的所有表达式（包括嵌套的 ${if}、${each}
    // 等）
    result += m_engine.renderBlock(body, itemContext);
  }

  // 返回成功
  return true;
}