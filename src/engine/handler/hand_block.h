/**
 * @file hand_block.h
 * @brief 模板块处理器 — 基类接口与工厂
 *
 * 定义：
 * - HandBlock 抽象基类：所有处理器的公共接口
 * - HandlerFactory：根据表达式前缀自动选择并创建对应的处理器
 *
 * 具体处理器定义在各自的头文件中：
 * - HandEachBlock   -> hand_each_block.h
 * - HandIfBlock     -> hand_if_block.h
 * - HandExpression  -> hand_expression.h
 */

#pragma once

#include <QJsonObject>
#include <QString>
#include <memory>

class TemplateEngine;

/**
 * @brief 查找匹配的闭合标签（支持嵌套）
 *
 * 从 startPos 开始扫描 block，计数嵌套的 openTag 和 closeTag，
 * 返回深度归零时的位置（即匹配的闭合标签位置）。
 */
int findMatchingClose(const QString &block, int startPos,
                      const QString &openPrefix, const QString &closeTag);

/**
 * @class HandBlock
 * @brief 模板块处理器抽象基类
 *
 * 所有表达式处理器的公共接口。
 * 子类实现 handle 方法，完成特定类型的表达式解析和渲染。
 */
class HandBlock {
public:
  virtual ~HandBlock() = default;

  /**
   * @brief 处理一个模板表达式
   * @param block 完整模板片段
   * @param pos 当前解析位置（会被修改为表达式结束之后的位置）
   * @param expr ${...} 内的表达式内容
   * @param context 变量上下文
   * @param result 渲染结果输出
   * @return true 表示处理成功，false 表示出错
   */
  virtual bool handle(const QString &block, int &pos, const QString &expr,
                      const QJsonObject &context, QString &result) const = 0;
};

/**
 * @enum ExprType
 * @brief 表达式类型枚举
 *
 * 用于 HandlerFactory 内部分发：根据表达式前缀匹配到对应类型，
 * 再通过 switch 创建对应的 HandBlock 实例。
 */
enum class ExprType {
  Each,      // ${each items}...${/each} 循环块
  If,        // ${if cond}...${else}...${/if} 条件块
  Expression // ${expression} 普通表达式（兜底）
};

/**
 * @class HandlerFactory
 * @brief 处理器工厂
 *
 * 根据表达式前缀自动选择并创建合适的处理器：
 * - "each " 开头 => HandEachBlock (ExprType::Each)
 * - "if "   开头 => HandIfBlock   (ExprType::If)
 * - 其他          => HandExpression(ExprType::Expression)
 */
class HandlerFactory {
public:
  explicit HandlerFactory(const TemplateEngine &engine) : m_engine(engine) {}

  std::unique_ptr<HandBlock> createHandler(const QString &expr) const;

private:
  /**
   * @brief 根据表达式前缀检测其类型
   * @param expr ${...} 内的表达式内容（已 trim）
   * @return 对应的 ExprType 枚举值
   */
  static ExprType detectType(const QString &expr);

  const TemplateEngine &m_engine;
};