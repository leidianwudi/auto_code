/**
 * @file fun_link.h
 * @brief 函数责任链 — 将函数调用路由到对应的 FunBase 处理器
 *
 * 责任链模式：
 * - 通过 addHandler() 注册处理器
 * - execute(name, args) 遍历链上所有处理器，
 *   找到 name() 匹配的处理器后执行并返回结果
 *
 * 若不注册任何处理器，execute() 返回 Null 表示无法处理。
 */

#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <memory>
#include <vector>

class FunBase;

/**
 * @class FunLink
 * @brief 函数责任链
 *
 * 用法示例：
 * @code
 *   FunLink link;
 *   link.addHandler(std::make_unique<FunStr>());
 *   link.addHandler(std::make_unique<FunDb>());
 *
 *   // 路由到 FunStr
 *   QJsonValue r1 = link.execute("str",
 *       QJsonArray{"toLowerCase", "Hello"});
 *
 *   // 路由到 FunDb
 *   QJsonValue r2 = link.execute("db",
 *       QJsonArray{dbConfig});
 * @endcode
 */
class FunLink {
public:
  /**
   * @brief 默认构造函数
   */
  FunLink() = default;

  /**
   * @brief 注册一个函数处理器到责任链尾部
   * @param handler 处理器（继承 FunBase）
   */
  void addHandler(std::unique_ptr<FunBase> handler);

  /**
   * @brief 沿责任链查找匹配的处理器并执行
   * @param funcName 函数名称（即 handler->name() 的返回值）
   * @param args     参数数组
   * @return 执行结果；未找到处理器时返回 Null
   */
  QJsonValue execute(const QString &funcName, const QJsonArray &args);

private:
  /**
   * @brief 责任链容器
   */
  std::vector<std::unique_ptr<FunBase>> m_chain;
};