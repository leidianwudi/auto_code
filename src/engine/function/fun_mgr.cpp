/**
 * @file fun_mgr.cpp
 * @brief 函数管理器实现
 */

#include "fun_mgr.h"

#include "fun_builtin.h"
#include "fun_db.h"
#include "fun_file.h"
#include "fun_json.h"
#include "fun_str.h"

// ============================================================================
// 单例
// ============================================================================

QString FunMgr::s_lastError;

FunMgr::~FunMgr() = default;

// ============================================================================
// init — 全局初始化：注册所有内置函数
// ============================================================================

void FunMgr::init() {
  FunStr::init();
  FunDb::init();
  FunFile::init();
  FunJson::init();
  FunBuiltin::init();
}

// ============================================================================
// cleanup — 全局清理：释放全局资源
// ============================================================================

void FunMgr::cleanup() { FunDb::cleanup(); }

// ============================================================================
// registerFuncs — 注册一个类的所有函数
// ============================================================================

void FunMgr::registerFuncs(const QString &className, const std::map<QString, FunPtr> &funcs) {
  auto &target = m_registry[className];
  // 纯参数方法 → 包装为忽略 this 的实例方法签名，统一调用路径
  for (const auto &[name, fn] : funcs) {
    target[name] = [fn](const QJsonValue &, const QJsonArray &args) { return fn(args); };
  }
}

void FunMgr::registerFuncs(const QString &className, const std::map<QString, FunPtrVoid> &funcs) {
  auto &target = m_registry[className];
  for (const auto &[name, fn] : funcs) {
    target[name] = [fn](const QJsonValue &, const QJsonArray &) { return fn(); };
  }
}

void FunMgr::registerFuncsWithThis(const QString &className,
                                   const std::map<QString, FunPtrThis> &funcs) {
  auto &target = m_registry[className];
  // 实例方法直接以原始签名注册（显式接收 thisObj）
  for (const auto &[name, fn] : funcs) target[name] = fn;
}

// ============================================================================
// call — 二级查找并执行
// ============================================================================

QJsonValue FunMgr::call(const QString &className, const QString &funcName, const QJsonArray &args) {
  return call(className, funcName, QJsonValue(), args);
}

QJsonValue FunMgr::call(const QString &className, const QString &funcName,
                        const QJsonValue &thisObj, const QJsonArray &args) {
  auto clsIt = m_registry.find(className);
  if (clsIt == m_registry.end()) return QJsonValue();

  const auto &funcs = clsIt->second;
  auto funcIt = funcs.find(funcName);
  if (funcIt == funcs.end()) return QJsonValue();

  return funcIt->second(thisObj, args);
}

// ============================================================================
// contains — 检查类是否已注册
// ============================================================================

bool FunMgr::contains(const QString &className) const {
  return m_registry.find(className) != m_registry.end();
}

// ============================================================================
// contains — 检查某类的某函数是否已注册
// ============================================================================

bool FunMgr::contains(const QString &className, const QString &funcName) const {
  auto clsIt = m_registry.find(className);
  if (clsIt == m_registry.end()) return false;
  return clsIt->second.find(funcName) != clsIt->second.end();
}

// ============================================================================
// setError / takeError — 函数执行错误报告
// ============================================================================

void FunMgr::setError(const QString &msg) { s_lastError = msg; }

QString FunMgr::takeError() {
  QString e = s_lastError;
  s_lastError.clear();
  return e;
}