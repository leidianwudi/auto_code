/**
 * @file fun_factory.cpp
 * @brief 函数工厂实现
 */

#include "fun_factory.h"
#include "fun_base.h"
#include "fun_db.h"
#include "fun_str.h"

// ============================================================================
// 构造函数 — 注册内置函数
// ============================================================================

FunFactory::FunFactory() {
  // 注册支持的函数处理器
  registerHandler(std::make_unique<FunStr>());
  registerHandler(std::make_unique<FunDb>());
}

// ============================================================================
// registerHandler — 注册函数处理器
// ============================================================================

void FunFactory::registerHandler(std::unique_ptr<FunBase> handler) {
  if (!handler)
    return;
  const QString key = handler->name();
  m_handlers[key] = std::move(handler);
}

// ============================================================================
// call — 按名称查找并执行函数
// ============================================================================

QJsonValue FunFactory::call(const QString &funcName,
                            const QJsonArray &args) const {
  auto it = m_handlers.find(funcName);
  if (it != m_handlers.end())
    return it->second->execute(args);
  return QJsonValue();
}

// ============================================================================
// contains — 检查函数是否已注册
// ============================================================================

bool FunFactory::contains(const QString &funcName) const {
  return m_handlers.find(funcName) != m_handlers.end();
}