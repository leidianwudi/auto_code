/**
 * @file template_engine.h
 * @brief 模板引擎头文件
 *
 * 实现一个简单的模板引擎，支持：
 * - 变量替换: ${variableName} 或 ${obj.property}
 * - 循环: ${each items}...${/each}
 * - 条件判断: ${if condition}...${else}...${/if}
 * - 算术运算: ${a + b * c}
 *
 * 具体的处理逻辑已提取到对应的 BlockHandler 子类中。
 * TemplateEngine 作为协调者：提供 render 入口 + 共享工具方法。
 */

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "function/fun_factory.h"
#include "handler/block_handler.h"

/**
 * @class TemplateEngine
 * @brief 模板引擎类
 *
 * 核心职责（精简后）：
 * 1. render() — 对外渲染入口
 * 2. renderBlock() — 递归扫描 ${...} 并交给 HandlerFactory
 * 3. resolvePath() — 嵌套属性解析（供 handler 共享调用）
 *
 * 算术表达式求值已移至 ExpressionHandler 内部实现。
 */
class TemplateEngine {
public:
  /**
   * @brief 默认构造函数
   */
  TemplateEngine();

  /**
   * @brief 渲染模板
   * @param tmpl 模板字符串，包含 ${...} 占位符
   * @param data JSON 对象，作为变量上下文
   * @return 渲染后的字符串，如果出错返回空字符串
   */
  QString render(const QString &tmpl, const QJsonObject &data) const;

  /**
   * @brief 获取最后一次渲染的错误信息
   * @return 错误信息字符串，无错误时为空
   */
  QString lastError() const { return m_lastError; }

  /**
   * @brief 设置错误信息（供 BlockHandler 调用）
   * @param msg 错误信息
   */
  void setError(const QString &msg) const { m_lastError = msg; }

  // ====================================================================
  // 共享工具方法（供 BlockHandler 子类调用）
  // ====================================================================

  /**
   * @brief 获取函数工厂引用（用于注册自定义函数）
   * @return FunFactory 引用
   */
  FunFactory &funFactory() { return m_funFactory; }

  /**
   * @brief 获取函数工厂 const 引用
   */
  const FunFactory &funFactory() const { return m_funFactory; }

  /**
   * @brief 解析嵌套属性路径
   * @param path 属性路径，如 "user.name.email" 或 "str.toLowerCase(Hello)"
   * @param context 变量上下文
   * @return 解析到的 JSON 值
   *
   * 优先检查路径首段是否为已注册的函数名（如 "str"、"db"），
   * 若是则交由 FunFactory 处理；
   * 否则按 JSON 对象嵌套属性解析。
   *
   * 内置字符串方法后缀（向下兼容）：
   * - .toLowerCase - 转小写
   * - .toUpperCase - 转大写
   * - .trim - 去除首尾空白
   * - .capitalize - 首字母大写
   */
  QJsonValue resolvePath(const QString &path, const QJsonObject &context) const;

  /**
   * @brief 递归渲染模板片段（供 handler 内部递归调用）
   * @param block 模板片段
   * @param context 变量上下文
   * @return 渲染后的字符串
   */
  QString renderBlock(const QString &block, const QJsonObject &context) const;

private:
  /**
   * @brief 最后一次渲染的错误信息
   *
   * 使用 mutable 允许在 const 方法中修改。
   * BlockHandler 可通过 setError() 设置。
   */
  mutable QString m_lastError;
  mutable HandlerFactory m_handlerFactory;
  FunFactory m_funFactory;
};