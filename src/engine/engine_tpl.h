/**
 * @file engine_tpl.h
 * @brief 模板引擎头文件
 *
 * 实现一个简单的模板引擎，支持：
 * - 变量替换: ${variableName} 或 ${obj.property}
 * - 循环: ${each items}...${/each}
 * - 条件判断: ${if condition}...${else}...${/if}
 * - 算术运算: ${a + b * c}
 *
 * 具体的处理逻辑已提取到对应的 TplBlock 子类中。
 * TemplateEngine 作为协调者：提供 render 入口 + 共享工具方法。
 */

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <functional>

#include "tpl_hand/tpl_block.h"

class ValidatorJson;

/**
 * @class TemplateEngine
 * @brief 模板引擎类
 *
 * 核心职责（精简后）：
 * 1. render() — 对外渲染入口
 * 2. renderBlock() — 递归扫描 ${...} 并交给 HandlerFactory
 * 3. resolvePath() — 嵌套属性解析（供 handler 共享调用）
 *
 * 算术表达式求值已移至 TplExpression 内部实现。
 * C++ 函数调用使用 FunMgr::call(类名, 函数名, 参数)。
 */
class TemplateEngine {
public:
  /**
   * @brief 默认构造函数
   */
  TemplateEngine();

  /// 日志回调：模板中 ${print(...)} 的输出通过此回调通知 UI
  using LogCallback = std::function<void(const QString &text, bool isError)>;
  void setLogCallback(LogCallback cb) { m_logCallback = std::move(cb); }
  LogCallback logCallback() const { return m_logCallback; }

  /**
   * @brief 设置 Schema 校验器（启用后渲染前自动校验数据）
   * @param className 数据对应的类名
   * @param validator Schema 校验器指针（生命周期由调用者管理）
   */
  static void setSchema(const QString &className,
                        const ValidatorJson *validator);

  /**
   * @brief 清除 Schema 校验（关闭数据校验）
   */
  static void clearSchema();

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
   * @brief 设置错误信息（供 TplBlock 调用）
   * @param msg 错误信息
   */
  void setError(const QString &msg) const { m_lastError = msg; }

  // ====================================================================
  // 共享工具方法（供 TplBlock 子类调用）
  // ====================================================================

  /**
   * @brief 解析嵌套属性路径
   * @param path 属性路径，如 "user.name.email"
   * @param context JSON 上下文
   * @return 解析后的值，未找到返回 Null
   */
  QJsonValue resolvePath(const QString &path, const QJsonObject &context) const;

  /**
   * @brief 递归渲染模板块（供 TplBlock 子类调用）
   */
  QString renderBlock(const QString &block, const QJsonObject &context) const;

private:
  /// Handler 工厂
  mutable HandlerFactory m_handlerFactory;

  /// 最后一次错误信息
  mutable QString m_lastError;

  /// 日志回调
  LogCallback m_logCallback;

  /// Schema 校验器（nullptr 表示不校验）
  static const ValidatorJson *sm_validator;
  /// 当前 Schema 类名
  static QString sm_schemaClass;
};