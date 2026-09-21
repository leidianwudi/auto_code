/**
 * @file ac_vm.h
 * @brief 字节码虚拟机 — 执行 AcModule 的栈式 VM
 *
 * 运行时语义与 AcInterpreter 1:1 对齐（作用域链、引用计数、标记清扫、
 * this/静态成员、函数调用、try/catch 错误通道），值类型同为 accore::AcJsonValue。
 *
 * v1 范围说明：
 * - 内置方法：JSON.parse/stringify、String 方法（经 FunMgr "str" 域）、
 *   Array 常用方法（VM 内实现，map/filter/forEach/reduce 等通过 kCallValue 执行回调）、
 *   Object 内置 keys/values/has/size
 * - 调试钩子：与解释器相同的语句级 onStatement（非调试会话零开销）
 */

#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <atomic>
#include <functional>

#include "ac_debugger.h"
#include "ac_object_manager.h"
#include "ac_opcode.h"
#include "src/core/json/ac_json_value.h"

class AcVm {
public:
  using LogCallback = std::function<void(const QString &text, bool isError)>;

  /// @brief 执行模块（顶层单元）
  /// @return 脚本返回值（m_hasReturned 语义与解释器一致：无 return 返回 Null）
  accore::AcJsonValue run(AcModule &module, QString &error);

  // ── 运行环境（与解释器同构） ──
  void setLogCallback(const LogCallback &cb) { m_logCallback = cb; }
  void setScriptDir(const QString &dir) { m_scriptDir = dir; }
  void setScriptFile(const QString &path) { m_scriptFile = path; }
  void setRootDir(const QString &dir) { m_rootDir = dir; }
  void setCancelFlag(std::atomic<bool> *flag) { m_cancelFlag = flag; }
  void setDebugger(AcDebugger *dbg) { m_debugger = dbg; }
  /// 指令级跟踪（临时调试用）：每条指令执行后打印 opcode+pc
  void setTrace(bool on) { m_trace = on; }

  QStringList generatedFiles() const { return m_generatedFiles; }

private:
  // ── 指令执行 ──
  /// 运行指定函数单元（递归深度防护），结果压栈
  void callUnit(int unitIdx, int argc, const QString &funcName);
  /// 执行单元字节码（帧已建立）；返回时结果在 m_retSlot
  void executeUnit(int unitIdx);
  void dispatch(const AcInstr &ins, int unitIdx);

  // ── 变量/作用域（复刻解释器） ──
  void setError(const QString &msg, int line);
  accore::AcJsonValue resolveVar(const QString &name) const;
  void setVar(const QString &name, const accore::AcJsonValue &val);
  void declareVar(const QString &name, const accore::AcJsonValue &val, bool isConst = false);
  void pushScope();
  void popScope();
  bool containsVar(const QString &name) const;
  void adjustScope(int absDepth);
  int currentScope() const { return m_scopeStack.size(); }

  // ── 引用计数 / GC（复刻解释器） ──
  void retainIfInstance(const accore::AcJsonValue &val);
  void releaseIfInstance(const accore::AcJsonValue &val);
  void releaseIfInstanceWithDestruct(const accore::AcJsonValue &val);
  void releaseDeep(const accore::AcJsonValue &val);
  void traverseNested(const accore::AcJsonValue &val,
                      const std::function<void(const accore::AcJsonValue &)> &onChild);
  void markFromValue(const accore::AcJsonValue &val);
  void collectCycles();
  void processDestructInfo(const AcObjectManager::DestructInfo &info);

  // ── 类支持 ──
  void initStatic(const QString &className);
  const MethodDef *findMethod(const QString &className, const QString &methodName) const;
  void execConstructor(const QString &className, accore::AcJsonValue &instance, int argc,
                       const QVector<accore::AcJsonValue> &args);  // 构造器 + 属性初始化
  accore::AcJsonValue makeInstanceObject(const QString &className);

  // ── 调用语义 ──
  void callFunc(const QString &name, int argc, int line);       // kCallFunc
  void callMethod(const QString &method, int argc, int32_t flags, int line);  // kCallMethod
  void callStaticMethod(const QString &cls, const QString &method, int argc, int line);
  void callFuncRefValue(const accore::AcJsonValue &funcRef, int argc, int line);
  void callFunMgrIfOwned(const QString &cls, const QString &func, const accore::AcJsonValue &args,
                         int line);

  // ── 内置方法 ──
  void callJSONMethod(const QString &method, int argc, int line);
  void callStringMethod(const QString &str, const QString &method, int argc, int line);
  void callArrayMethod(accore::AcJsonValue &arr, const QString &method, int argc, int line,
                       accore::AcJsonValue *modifiedOut);
  void callObjectBuiltin(accore::AcJsonValue &obj, const QString &method, int argc, int line);

  // ── 运算 ──
  accore::AcJsonValue binOp(AcOpcode op, const accore::AcJsonValue &l,
                            const accore::AcJsonValue &r, int line);
  accore::AcJsonValue applyCompoundOp(const accore::AcJsonValue &cur,
                                      const accore::AcJsonValue &delta, int op, int line);

  // ── 错误处理（try/catch） ──
  struct ActiveTry {
    int tryIdx = -1;
    int catchAddr = -1;
    int finallyAddr = -1;
  };
  void handleError(int unitIdx, int pc);

  // ── 返回槽（callUnit 的结果传递） ──
  accore::AcJsonValue m_retSlot;

  // ── 运行时状态 ──
  AcModule *m_module = nullptr;
  QVector<accore::AcJsonValue> m_vstack;  ///< 值栈
  QVector<QHash<QString, accore::AcJsonValue>> m_scopeStack;
  QVector<QSet<QString>> m_constVars;
  QVector<QVector<QString>> m_usingStack;
  QHash<QString, accore::AcJsonValue> m_staticVars;
  QSet<QString> m_staticInited;
  QHash<QString, ClassDef> m_classes;
  accore::AcJsonValue m_currentThis;
  accore::AcJsonValue m_modifiedThis;
  AcObjectManager m_objMgr;
  int m_objectsAtLastGc = 0;
  QString m_error;
  QStringList m_generatedFiles;
  QString m_scriptDir;
  QString m_scriptFile;
  QString m_rootDir;
  std::atomic<bool> *m_cancelFlag = nullptr;
  AcDebugger *m_debugger = nullptr;
  LogCallback m_logCallback;
  bool m_trace = false;  ///< 指令级跟踪（临时调试）

  // ── 帧与迭代器 ──
  struct Frame {
    int unitIdx = -1;
    int pc = 0;
    int baseScope = 0;       ///< 进入时作用域深度（返回时恢复）
    QString funcName;        ///< 调试展示
    bool returned = false;
    accore::AcJsonValue retVal;  ///< kRet 写入的帧返回值
  };
  QVector<Frame> m_frames;
  struct IterFrame {
    accore::AcJsonValue arr;
    int idx = 0;
    accore::AcJsonValue cur;  ///< 当前元素
  };
  QVector<IterFrame> m_iters;
  QVector<ActiveTry> m_tryStack;
  QString m_pendingCatchErr;  ///< catch 绑定的错误文本
  QString m_pendingPropagateErr;  ///< finally 待传播的错误
  bool m_hadErrorInTry = false;
  int m_callDepth = 0;
  int m_objectsAtLastGc0 = 0;
};