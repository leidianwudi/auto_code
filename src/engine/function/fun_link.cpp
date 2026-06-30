/**
 * @file fun_link.cpp
 * @brief 函数责任链实现
 */

#include "fun_link.h"
#include "fun_base.h"

void FunLink::addHandler(std::unique_ptr<FunBase> handler) {
  if (handler)
    m_chain.push_back(std::move(handler));
}

QJsonValue FunLink::execute(const QString &funcName, const QJsonArray &args) {
  for (auto &handler : m_chain) {
    if (handler->name() == funcName) {
      return handler->execute(args);
    }
  }
  // 责任链中无匹配的处理器
  return QJsonValue();
}