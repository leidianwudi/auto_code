/**
 * @file fun_base.h
 * @brief 函数基类 — 所有可执行函数的公共接口
 *
 * FunLink 责任链中的每个处理器都继承自 FunBase。
 * 通过 name() 标识自己能处理的函数名，由 FunLink 按链路由。
 */

#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <QString>

/**
 * @class FunBase
 * @brief 函数对象抽象基类
 *
 * 子类必须实现：
 * - name()     — 返回函数名称，供 FunLink 路由匹配
 * - execute()  — 执行函数逻辑，接收 QJsonArray 参数，返回 QJsonValue
 */
class FunBase {
public:
  virtual ~FunBase() = default;

  /**
   * @brief 返回本处理器支持的函数名称
   * @return 函数名（如 "str"、"db"）
   */
  virtual QString name() const = 0;

  /**
   * @brief 执行函数
   * @param args 参数数组
   * @return 执行结果（JSON 值）
   */
  virtual QJsonValue execute(const QJsonArray &args) = 0;
};