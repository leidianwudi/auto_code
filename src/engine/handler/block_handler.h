/**
 * @file block_handler.h
 * @brief 模板块处理器 — 基类接口与工厂
 *
 * 定义：
 * - BlockHandler 抽象基类：所有处理器的公共接口
 * - HandlerFactory：根据表达式前缀自动选择并创建对应的处理器
 *
 * 具体处理器定义在各自的头文件中：
 * - EachBlockHandler   → each_block_handler.h
 * - IfBlockHandler     → if_block_handler.h
 * - ExpressionHandler  → expression_handler.h
 */

#pragma once

#include <QJsonObject>
#include <QString>
#include <memory>

class TemplateEngine;

/**
 * @class BlockHandler
 * @brief 模板块处理器抽象基类
 *
 * 所有表达式处理器的公共接口。
 * 子类实现 handle 方法，完成特定类型的表达式解析和渲染。
 */
class BlockHandler {
public:
  virtual ~BlockHandler() = default;

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
 * @class HandlerFactory
 * @brief 处理器工厂
 *
 * 根据表达式前缀自动选择并创建合适的处理器：
 * - "each " 开头 → EachBlockHandler
 * - "if "   开头 → IfBlockHandler
 * - 其他          → ExpressionHandler
 */
class HandlerFactory {
public:
  explicit HandlerFactory(const TemplateEngine &engine) : m_engine(engine) {}

  std::unique_ptr<BlockHandler> createHandler(const QString &expr) const;

private:
  const TemplateEngine &m_engine;
};