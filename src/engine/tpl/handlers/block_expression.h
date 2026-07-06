/**
 * @file block_expression.h
 * @brief 表达式处理器 -- 处理普通 ${expression} 表达式求值
 *
 * 这是模板引擎中最复杂的处理器，承担三大职责：
 * 1. 循环变量替换：${this} / ${.} -- 取循环当前元素
 * 2. 算术表达式求值：内置递归下降解析器（eval* 系列）
 * 3. 变量路径解析：通过 TplEngine::resolvePath() 获取值
 *
 * 算术表达式支持的语法：
 * - 四则运算：+, -, *, /
 * - 括号分组：(a + b) * c
 * - 一元运算符：+x, -x
 * - 数字字面量：123, 3.14, .5
 * - 变量引用：price * 1.13
 * - 嵌套属性：user.age + 1
 */

#pragma once

#include "tpl_block.h"

/**
 * @class BlockExpression
 * @brief 表达式处理器（兜底处理器）
 *
 * TplFactory 的默认处理器，处理所有非 "each " / "if "
 * 开头的表达式。是三个 handler 中唯一包含算术求值逻辑的类。
 *
 * 设计为自包含：算术解析器（4 个私有方法）完全封装在内部，
 * 不依赖外部计算库，TplEngine 只提供 resolvePath 共享工具。
 */
class BlockExpression : public TplBlock {
public:
  /**
   * @brief 构造函数
   * @param engine 模板引擎引用，用于调用 resolvePath 和 setError
   */
  explicit BlockExpression(const TplEngine &engine) : m_engine(engine) {}

  /**
   * @brief 处理普通表达式（分发入口）
   *
   * 根据表达式内容自动选择处理策略：
   * | 表达式示例          | 策略     | 说明                         |
   * |--------------------|----------|------------------------------|
   * | ${this} 或 ${.}     | 循环变量 | 从 context 的 "." 键取值      |
   * | ${price * 1.13}     | 算术求值 | 内置递归下降解析器计算         |
   * | ${name}             | 变量路径 | 通过 resolvePath 解析         |
   * | ${user.email}       | 嵌套属性 | 支持 .toLowerCase 等方法后缀  |
   *
   * @param block 完整模板片段（本处理器不使用）
   * @param pos 当前解析位置（本处理器不修改）
   * @param expr ${...} 内的表达式内容
   * @param context 变量上下文
   * @param result 渲染结果输出（追加模式）
   * @return 始终返回 true -- 表达式求值不会导致模板解析失败
   */
  bool handle(const QString &block, int &pos, const QString &expr,
              const QJsonObject &context, QString &result) const override;

private:
  // ====================================================================
  // 算术表达式递归下降解析器（内部私有方法）
  //
  // 采用经典递归下降设计，按运算符优先级分层：
  //
  //   evalExpression (入口)
  //     调用 evalAddSub   (+, -)          最低优先级
  //       调用 evalMulDiv (*, /)          中等优先级
  //         调用 evalPrimary              最高优先级
  //           处理: 一元(+x,-x) / 括号 / 数字 / 变量
  // ====================================================================

  /**
   * @brief 算术表达式求值入口
   *
   * 初始化位置指针后委托给 evalAddSub 开始自顶向下解析。
   *
   * @param expr 表达式字符串（如 "price * 1.13 + tax"）
   * @param context 变量上下文（用于解析表达式中引用的变量名）
   * @return 计算结果（double），解析失败时返回 0
   */
  double evalExpression(const QString &expr, const QJsonObject &context) const;

  /**
   * @brief 解析加减法表达式（优先级最低）
   *
   * 左结合的加法/减法运算。先解析左侧操作数（通过 evalMulDiv），
   * 然后循环匹配后续的 + / - 运算符。
   *
   * 示例：a + b - c + d 解析为 (((a + b) - c) + d)
   *
   * @param expr 表达式字符串
   * @param pos 当前解析位置（输入输出参数，随解析推进）
   * @param context 变量上下文
   * @return 加减法子表达式的计算结果
   */
  double evalAddSub(const QString &expr, int &pos,
                    const QJsonObject &context) const;

  /**
   * @brief 解析乘除法表达式（优先级中等）
   *
   * 左结合的乘法/除法运算。先解析左侧操作数（通过 evalPrimary），
   * 然后循环匹配后续的 * / 运算符。
   *
   * @param expr 表达式字符串
   * @param pos 当前解析位置（输入输出参数，随解析推进）
   * @param context 变量上下文
   * @return 乘除法子表达式的计算结果
   */
  double evalMulDiv(const QString &expr, int &pos,
                    const QJsonObject &context) const;

  /**
   * @brief 解析基本元素（优先级最高）
   *
   * 处理单个"原子"元素，包括：
   * - 一元运算符：+x, -x
   * - 括号表达式：(subExpr)
   * - 数字字面量：123, 3.14, .5
   * - 变量引用：name, user.age（通过 resolvePath 取值）
   *
   * @param expr 表达式字符串
   * @param pos 当前解析位置（输入输出参数，随解析推进）
   * @param context 变量上下文
   * @return 基本元素的计算结果
   */
  double evalPrimary(const QString &expr, int &pos,
                     const QJsonObject &context) const;

  const TplEngine &m_engine;
};