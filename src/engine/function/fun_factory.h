/**
 * @file fun_factory.h
 * @brief 函数工厂 — 按名称注册/调用 C++ 函数
 *
 * 工厂模式：
 * - registerHandler() 注册 FunBase 子类（按 name() 索引）
 * - call(name, args) 直接通过 name 查找并执行
 * - 未注册的函数返回 Null
 */

#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <map>
#include <memory>

#include "fun_base.h"

/**
 * @class FunFactory
 * @brief 函数工厂，管理 FunBase 子类
 *
 * 用法示例：
 * @code
 *   FunFactory factory;
 *   factory.registerHandler(std::make_unique<FunStr>());
 *   factory.registerHandler(std::make_unique<FunDb>());
 *
 *   QJsonValue r1 = factory.call("str",
 *       QJsonArray{"toLowerCase", "Hello"});
 *   QJsonValue r2 = factory.call("db", dbConfig);
 * @endcode
 */
class FunFactory {
public:
  FunFactory();

  /**
   * @brief 注册一个函数处理器
   * @param handler 继承 FunBase 的处理器实例
   */
  void registerHandler(std::unique_ptr<FunBase> handler);

  /**
   * @brief 调用已注册的函数
   * @param funcName 函数名称（handler->name()）
   * @param args     参数数组
   * @return 执行结果；未注册时返回 Null
   */
  QJsonValue call(const QString &funcName, const QJsonArray &args) const;

  /**
   * @brief 检查函数是否已注册
   * @param funcName 函数名称
   * @return true 表示已注册
   */
  bool contains(const QString &funcName) const;

private:
  std::map<QString, std::unique_ptr<FunBase>> m_handlers;
};