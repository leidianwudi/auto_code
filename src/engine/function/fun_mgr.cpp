/**
 * @file fun_mgr.cpp
 * @brief 函数管理器实现
 */

#include "fun_mgr.h"

#include <shared_mutex>

#include "fun_builtin.h"
#include "fun_db.h"
#include "fun_file.h"
#include "fun_json.h"
#include "fun_str.h"

// ============================================================================
// 单例
// ============================================================================

/// 当前线程的最后一次函数执行错误（thread_local：并行解释器各自独立，
/// 同线程内 setError → takeError 的时序由调用约定保证）
namespace {
thread_local QString t_lastError;
}  // namespace

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
  std::unique_lock lock(m_registryMutex);
  auto &target = m_registry[className];
  // 纯参数方法 → 包装为忽略 this 的实例方法签名，统一调用路径
  for (const auto &[name, fn] : funcs) {
    target[name] = [fn](const accore::AcJsonValue &, const accore::AcJsonValue &args) {
      return fn(args);
    };
  }
}

void FunMgr::registerFuncs(const QString &className, const std::map<QString, FunPtrVoid> &funcs) {
  std::unique_lock lock(m_registryMutex);
  auto &target = m_registry[className];
  for (const auto &[name, fn] : funcs) {
    target[name] = [fn](const accore::AcJsonValue &, const accore::AcJsonValue &) { return fn(); };
  }
}

void FunMgr::registerFuncsWithThis(const QString &className,
                                   const std::map<QString, FunPtrThis> &funcs) {
  std::unique_lock lock(m_registryMutex);
  auto &target = m_registry[className];
  // 实例方法直接以原始签名注册（显式接收 thisObj）
  for (const auto &[name, fn] : funcs) target[name] = fn;
}

// ============================================================================
// call — 二级查找并执行
// ============================================================================

accore::AcJsonValue FunMgr::call(const QString &className, const QString &funcName,
                                 const accore::AcJsonValue &args) {
  return call(className, funcName, accore::AcJsonValue(), args);
}

accore::AcJsonValue FunMgr::call(const QString &className, const QString &funcName,
                                 const accore::AcJsonValue &thisObj,
                                 const accore::AcJsonValue &args) {
  // 共享锁内快照函数对象，执行放锁外：内置函数可能重入 call/register
  // （同线程重复加共享锁在写者等待时会死锁），也可能长时间执行阻塞注册
  FunPtrThis fn;
  {
    std::shared_lock lock(m_registryMutex);
    auto clsIt = m_registry.find(className);
    if (clsIt == m_registry.end()) return accore::AcJsonValue();
    auto funcIt = clsIt->second.find(funcName);
    if (funcIt == clsIt->second.end()) return accore::AcJsonValue();
    fn = funcIt->second;
  }
  return fn(thisObj, args);
}

// ============================================================================
// contains — 检查类是否已注册
// ============================================================================

bool FunMgr::contains(const QString &className) const {
  std::shared_lock lock(m_registryMutex);
  return m_registry.find(className) != m_registry.end();
}

// ============================================================================
// contains — 检查某类的某函数是否已注册
// ============================================================================

bool FunMgr::contains(const QString &className, const QString &funcName) const {
  std::shared_lock lock(m_registryMutex);
  auto clsIt = m_registry.find(className);
  if (clsIt == m_registry.end()) return false;
  return clsIt->second.find(funcName) != clsIt->second.end();
}

// ============================================================================
// setError / takeError — 函数执行错误报告（thread_local，线程内独立）
// ============================================================================

void FunMgr::setError(const QString &msg) { t_lastError = msg; }

QString FunMgr::takeError() {
  QString e = std::move(t_lastError);
  t_lastError.clear();
  return e;
}
