/**
 * @file fun_mgr.cpp
 * @brief 函数管理器实现
 */

#include "fun_mgr.h"
#include "fun_db.h"
#include "fun_file.h"
#include "fun_str.h"

// ============================================================================
// 单例
// ============================================================================

FunMgr &FunMgr::ins() {
  static FunMgr s_inst;
  return s_inst;
}

FunMgr::~FunMgr() = default;

// ============================================================================
// init — 全局初始化：注册所有内置函数
// ============================================================================

void FunMgr::init() {
  FunStr::init();
  FunDb::init();
  FunFile::init();
}

// ============================================================================
// cleanup — 全局清理：释放全局资源
// ============================================================================

void FunMgr::cleanup() { FunDb::cleanup(); }

// ============================================================================
// registerFuncs — 注册一个类的所有函数
// ============================================================================

void FunMgr::registerFuncs(const QString &className,
                           const std::map<QString, FunPtr> &funcs) {
  m_registry[className] = funcs;
}

// ============================================================================
// call — 二级查找并执行
// ============================================================================

QJsonValue FunMgr::call(const QString &className, const QString &funcName,
                        const QJsonArray &args) {
  auto clsIt = m_registry.find(className);
  if (clsIt == m_registry.end())
    return QJsonValue();

  const auto &funcs = clsIt->second;
  auto funcIt = funcs.find(funcName);
  if (funcIt == funcs.end())
    return QJsonValue();

  return funcIt->second(args);
}

// ============================================================================
// contains — 检查类是否已注册
// ============================================================================

bool FunMgr::contains(const QString &className) const {
  return m_registry.find(className) != m_registry.end();
}