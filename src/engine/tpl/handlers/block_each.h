/**
 * @file block_each.h
 * @brief 循环块处理器 — 处理 ${each items}...${/each} 语法
 *
 * 遍历 JSON 数组，为每个元素渲染循环体内容。
 * 支持通过 ${this} / ${.} 引用当前元素，或直接引用元素属性名。
 */

#pragma once

#include "tpl_block.h"

/**
 * @class BlockEach
 * @brief 循环块处理器
 *
 * 处理模板中的 ${each items}...${/each} 循环语法。
 * 是 TplFactory 的三种处理器之一（ExprType::Each）。
 *
 * 工作流程：
 * 1. 提取数组变量名（去掉 "each " 前缀）
 * 2. 查找匹配的 ${/each} 结束标记
 * 3. 提取循环体内容
 * 4. 通过 TplEngine::resolvePath 获取数组数据
 * 5. 遍历数组，为每个元素创建独立上下文并递归渲染循环体
 *
 * 上下文机制：
 * - 对象元素：将元素的每个属性合并到当前上下文中
 * - 基本类型：同时存入 "." 键（与 ${.} 语法配合）和变量名键下
 *
 * 示例：
 * @code
 * 模板：${each users}${name}: ${email}\n${/each}
 * 数据：{"users": [{"name":"张三","email":"z@test.com"}]}
 * 输出：张三: z@test.com
 * @endcode
 */
class BlockEach : public TplBlock {
public:
  /**
   * @brief 构造函数
   * @param engine 模板引擎引用，用于调用 resolvePath、renderBlock 和 setError
   */
  explicit BlockEach(const TplEngine &engine) : m_engine(engine) {}

  /**
   * @brief 处理 ${each varName}...${/each} 循环块
   *
   * 从表达式中提取变量名，获取对应的 JSON 数组，
   * 遍历数组元素并为每个元素递归渲染循环体。
   *
   * @param block   完整模板片段
   * @param pos     当前解析位置（进入时指向循环体开头，退出时指向 ${/each}
   * 之后）
   * @param expr    ${...} 内的表达式内容（如 "each users"）
   * @param context 变量上下文
   * @param result  渲染结果输出（追加模式）
   * @return true  处理成功
   * @return false 出错（如找不到闭合标签、变量不是数组）
   */
  bool handle(const QString &block, int &pos, const QString &expr,
              const QJsonObject &context, QString &result) const override;

private:
  /// 模板引擎引用，用于 resolvePath / renderBlock / setError
  const TplEngine &m_engine;
};