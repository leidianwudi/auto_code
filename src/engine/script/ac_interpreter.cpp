/**
 * @file ac_interpreter.cpp
 * @brief 解释执行器 — 辅助函数、变量操作、引用计数、执行入口
 */

#include "ac_interpreter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#include "../ac_language.h"
#include "../function/fun_builtin.h"
#include "../function/fun_mgr.h"
#include "../tpl/tpl_engine.h"
#include "ac_builtin_eval.h"
#include "ac_builtin_loader.h"
#include "ac_object_manager.h"
#include "src/core/json/ac_json_value.h"

// ═════════════════════════════════════════════════════════════════════════════
//  变量操作
// ═════════════════════════════════════════════════════════════════════════════

accore::AcJsonValue AcInterpreter::resolveVar(const QString &name) const {
  if (name == QString::fromLatin1(AcKeyword::kThis)) return m_currentThis;
  if (name == QString::fromLatin1(AcKeyword::kSuper)) return m_currentThis;
  for (int i = m_scopeStack.size() - 1; i >= 0; --i) {
    auto it = m_scopeStack[i].find(name);
    if (it != m_scopeStack[i].end()) return it.value();
  }
  return accore::AcJsonValue();
}

void AcInterpreter::setVar(const QString &name, const accore::AcJsonValue &val) {
  for (int i = m_scopeStack.size() - 1; i >= 0; --i) {
    if (m_scopeStack[i].contains(name)) {
      // const 常量：定义所在作用域内禁止再赋值
      if (i < m_constVars.size() && m_constVars[i].contains(name)) {
        setError(QStringLiteral("cannot assign to const '%1'").arg(name), 0);
        return;
      }
      const accore::AcJsonValue &old = m_scopeStack[i][name];
      // 自赋值检测走 accore 原生接口，避免每次赋值 4 次 AcJsonValue→QJsonObject 深拷贝
      if (AcObjectManager::isManagedInstance(old) && AcObjectManager::isManagedInstance(val) &&
          AcObjectManager::getObjId(old) == AcObjectManager::getObjId(val)) {
        m_scopeStack[i][name] = val;
        return;
      }
      releaseIfInstanceWithDestruct(old);
      m_scopeStack[i][name] = val;
      retainIfInstance(val);
      return;
    }
  }
  retainIfInstance(val);
  m_scopeStack.last()[name] = val;
}

void AcInterpreter::declareVar(const QString &name, const accore::AcJsonValue &val, bool isConst) {
  // let/const 声明只在最新（最内层）作用域内创建变量，绝不覆盖外层同名变量
  retainIfInstance(val);
  m_scopeStack.last()[name] = val;
  if (isConst) m_constVars.last().insert(name);
}

void AcInterpreter::pushScope() {
  m_scopeStack.append(QHash<QString, accore::AcJsonValue>());
  m_constVars.append(QSet<QString>());
  m_usingStack.append(QVector<QString>());
  m_varLocStack.append(QHash<QString, QPair<QString, int>>());
}

void AcInterpreter::popScope() {
  if (m_scopeStack.isEmpty()) return;

  if (!m_usingStack.isEmpty()) {
    auto usingVars = m_usingStack.takeLast();
    for (int i = usingVars.size() - 1; i >= 0; --i) {
      accore::AcJsonValue val = resolveVar(usingVars[i]);
      if (val.isInstance()) {
        QString className = val.instanceClass();
        if (!className.isEmpty() && m_classes.contains(className)) {
          const ClassDef &cd = m_classes[className];
          QString disposeName = QString::fromLatin1(AcKeyword::kDispose);
          if (cd.isNative) {
            // 原生类 dispose：显式传递实例 thisObj（实例方法经 registerFuncsWithThis 注册）
            FunMgr::ins().call(className, disposeName, val, accore::AcJsonValue::makeArray());
            FunMgr::takeError();
          } else {
            const MethodDef *disposeMethod = findMethod(className, disposeName);
            if (disposeMethod) {
              execMethod(*disposeMethod, val, accore::AcJsonValue());
            }
          }
        }
      }
    }
  }

  auto scope = m_scopeStack.takeLast();
  m_constVars.removeLast();
  for (auto it = scope.begin(); it != scope.end(); ++it) {
    releaseDeep(it.value());
  }
  if (!m_varLocStack.isEmpty()) m_varLocStack.takeLast();

  // 环回收节流：不在每次退出作用域时都做全堆 mark-sweep——
  // for/for-in 循环逐轮 push/pop 作用域，若每轮都全堆回收，整体复杂度近似 O(n²)。
  // 改为两类触发时机：
  //   1) 托管对象数自上次回收后增长超过阈值（分配驱动，循环体内不反弹）
  //   2) 作用域栈清空（脚本/函数执行边界，兜底保证最终回收）
  // 引用计数的确定性析构（releaseDeep → releaseIfInstanceWithDestruct）不受影响，
  // 延迟的只是循环引用垃圾的清扫时机。
  constexpr int kGcGrowthThreshold = 64;
  const int objCount = m_objMgr.objectCount();
  if (m_scopeStack.isEmpty() || objCount - m_objectsAtLastGc >= kGcGrowthThreshold) {
    collectCycles();
    m_objectsAtLastGc = m_objMgr.objectCount();
  }
}

void AcInterpreter::recordVarLoc(const QString &name, const QString &filePath, int line) {
  // 仅调试时记录，避免常规执行开销；作用域为空时忽略
  if (m_debugger && !m_varLocStack.isEmpty()) {
    m_varLocStack.last()[name] = QPair<QString, int>(filePath, line);
  }
}

bool AcInterpreter::containsVar(const QString &name) const {
  for (int i = m_scopeStack.size() - 1; i >= 0; --i) {
    if (m_scopeStack[i].contains(name)) return true;
  }
  return false;
}

bool AcInterpreter::isTruthy(const accore::AcJsonValue &cond) {
  if (cond.isBool()) return cond.toBool();
  if (cond.isString()) return !cond.toString().isEmpty();
  if (cond.isDouble()) return cond.toDouble() != 0.0;
  if (cond.isNull()) return false;
  return true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  引用计数辅助
// ═════════════════════════════════════════════════════════════════════════════

void AcInterpreter::retainIfInstance(const accore::AcJsonValue &val) {
  if (!AcObjectManager::isManagedInstance(val)) return;
  m_objMgr.retain(AcObjectManager::getObjId(val));
}

void AcInterpreter::releaseIfInstance(const accore::AcJsonValue &val) {
  if (!AcObjectManager::isManagedInstance(val)) return;
  m_objMgr.release(AcObjectManager::getObjId(val));
}

void AcInterpreter::releaseIfInstanceWithDestruct(const accore::AcJsonValue &val) {
  if (!AcObjectManager::isManagedInstance(val)) return;
  m_objMgr.release(AcObjectManager::getObjId(val));
  QVector<AcObjectManager::DestructInfo> pending = m_objMgr.takePendingDestructs();
  for (const auto &info : pending) {
    processDestructInfo(info);
  }
}

void AcInterpreter::processDestructInfo(const AcObjectManager::DestructInfo &info) {
  const accore::AcJsonValue &obj = info.instance;
  // 实例的属性即全部成员（类名/objId 在专用字段，无需再过滤内部键）
  for (const auto &m : obj.members()) {
    releaseDeep(m.value);
  }
  if (FunMgr::ins().contains(info.className, QString::fromLatin1(AcRuntime::kDestructor))) {
    // 原生类析构：显式传递实例 thisObj（实例方法经 registerFuncsWithThis 注册）
    FunMgr::ins().call(info.className, QString::fromLatin1(AcRuntime::kDestructor), obj,
                       accore::AcJsonValue::makeArray());
  } else if (m_classes.contains(info.className) && !m_classes[info.className].isNative) {
    const MethodDef *dtor = findMethod(info.className, QString::fromLatin1(AcRuntime::kDestructor));
    if (dtor) {
      execMethod(*dtor, obj, accore::AcJsonValue::makeArray());
    }
  }
}

void AcInterpreter::traverseNested(
    const accore::AcJsonValue &val,
    const std::function<void(const accore::AcJsonValue &)> &onChild) {
  if (val.isArray()) {
    for (const accore::AcJsonValue &item : val.items()) onChild(item);
  } else if (val.isObject()) {
    // 实例的类名/objId 在专用字段，members 即纯属性，无需过滤内部键
    for (const auto &m : val.members()) onChild(m.value);
  }
}

void AcInterpreter::releaseDeep(const accore::AcJsonValue &val) {
  if (AcObjectManager::isManagedInstance(val)) {
    releaseIfInstanceWithDestruct(val);
    return;
  }
  traverseNested(val, [this](const accore::AcJsonValue &child) { releaseDeep(child); });
}

// ═════════════════════════════════════════════════════════════════════════════
//  标记-清扫（处理循环引用）
// ═════════════════════════════════════════════════════════════════════════════

void AcInterpreter::markFromValue(const accore::AcJsonValue &val) {
  if (AcObjectManager::isManagedInstance(val)) {
    QString objId = AcObjectManager::getObjId(val);
    if (m_objMgr.isMarked(objId) || !m_objMgr.contains(objId)) return;
    m_objMgr.mark(objId);
    // accore 原生形态遍历，避免 GC 标记阶段对每个实例做一次深拷贝转换
    accore::AcJsonValue obj = m_objMgr.getAcObject(objId);
    traverseNested(obj, [this](const accore::AcJsonValue &child) { markFromValue(child); });
    return;
  }
  traverseNested(val, [this](const accore::AcJsonValue &child) { markFromValue(child); });
}

void AcInterpreter::collectCycles() {
  auto &mgr = m_objMgr;
  if (mgr.objectCount() == 0) return;

  mgr.clearMarks();

  for (const auto &scope : m_scopeStack) {
    for (auto it = scope.begin(); it != scope.end(); ++it) {
      markFromValue(it.value());
    }
  }
  for (auto it = m_staticVars.begin(); it != m_staticVars.end(); ++it) {
    for (const auto &m : it.value().members()) {
      markFromValue(m.value);
    }
  }
  if (!m_currentThis.isEmpty()) {
    for (const auto &m : m_currentThis.members()) {
      markFromValue(m.value);
    }
  }
  if (!m_modifiedThis.isEmpty()) {
    for (const auto &m : m_modifiedThis.members()) {
      markFromValue(m.value);
    }
  }

  QVector<AcObjectManager::DestructInfo> cycles = mgr.collectUnmarked();
  for (const auto &info : cycles) {
    processDestructInfo(info);
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  执行入口
// ═════════════════════════════════════════════════════════════════════════════

accore::AcJsonValue AcInterpreter::execute(const Block &program, QString &error) {
  m_error.clear();
  m_scopeStack.clear();
  m_usingStack.clear();
  m_varLocStack.clear();
  m_classes.clear();
  m_functions.clear();
  m_currentThis = accore::AcJsonValue();
  m_hasReturned = false;
  m_returnValue = accore::AcJsonValue();
  m_generatedFiles.clear();
  m_funcExprCounter = 0;

  // 顶层脚本帧：供调试器调用栈展示（始终指向入口脚本，而非被注入的 import 模块）
  if (m_debugger) {
    ++m_callDepth;
    int topLine = 0;
    QString topFile = m_scriptFile;
    for (const auto &s : program.stmts) {
      if (m_scriptFile.isEmpty() || s.filePath == m_scriptFile) {
        topLine = s.loc.line;
        break;
      }
    }
    m_callStack.append(AcDebugFrame{QString(), topFile, topLine});
  }

  pushScope();

  AcBuiltinLoader::registerNativeClasses(m_classes);

  FunMgr::init();
  FunBuiltin::setContext({m_scriptDir, m_rootDir, m_logCallback, &m_generatedFiles, 0});
  m_objectsAtLastGc = 0;  // 环回收节流基线随每次执行重置

  for (const auto &stmt : program.stmts) {
    switch (stmt.kind) {
      case Block::Stmt::kFuncDef:
        m_functions[stmt.funcDef.name] = stmt.funcDef;
        break;
      case Block::Stmt::kClassDef:
        m_classes[stmt.classDef.name] = stmt.classDef;
        break;
      case Block::Stmt::kEnumDef:
        for (const auto &member : stmt.enumDef.members) {
          QString varName = QStringLiteral("%1.%2").arg(stmt.enumDef.name, member.name);
          setVar(varName, accore::AcJsonValue::fromQJsonValue(member.value));
        }
        break;
      default:
        break;
    }
  }

  execBlock(program);
  if (!m_error.isEmpty()) {
    error = m_error;
    m_objMgr.cleanup();
    if (m_debugger) {
      m_callStack.removeLast();
      --m_callDepth;
    }
    return accore::AcJsonValue();
  }

  while (!m_scopeStack.isEmpty()) {
    popScope();
  }

  if (m_debugger) {
    m_callStack.removeLast();
    --m_callDepth;
  }

  return m_hasReturned ? m_returnValue : accore::AcJsonValue();
}
