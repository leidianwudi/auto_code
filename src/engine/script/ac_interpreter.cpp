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
      const accore::AcJsonValue &old = m_scopeStack[i][name];
      if (AcObjectManager::isManagedInstance(old.toQJsonValue()) &&
          AcObjectManager::isManagedInstance(val.toQJsonValue()) &&
          AcObjectManager::getObjId(old.toQJsonValue()) ==
              AcObjectManager::getObjId(val.toQJsonValue())) {
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

void AcInterpreter::declareVar(const QString &name, const accore::AcJsonValue &val) {
  // let 声明只在最新（最内层）作用域内创建变量，绝不覆盖外层同名变量
  retainIfInstance(val);
  m_scopeStack.last()[name] = val;
}

void AcInterpreter::pushScope() {
  m_scopeStack.append(QHash<QString, accore::AcJsonValue>());
  m_usingStack.append(QVector<QString>());
  m_varLocStack.append(QHash<QString, QPair<QString, int>>());
}

void AcInterpreter::popScope() {
  if (m_scopeStack.isEmpty()) return;

  if (!m_usingStack.isEmpty()) {
    auto usingVars = m_usingStack.takeLast();
    for (int i = usingVars.size() - 1; i >= 0; --i) {
      accore::AcJsonValue val = resolveVar(usingVars[i]);
      if (val.isObject()) {
        QString className = val.value(QString::fromLatin1(AcRuntime::kClassKey)).toString();
        if (!className.isEmpty() && m_classes.contains(className)) {
          const ClassDef &cd = m_classes[className];
          QString disposeName = QString::fromLatin1(AcKeyword::kDispose);
          if (cd.isNative) {
            // 原生类 dispose：显式传递实例 thisObj（实例方法经 registerFuncsWithThis 注册）
            FunMgr::ins().call(className, disposeName, val.toQJsonValue(), QJsonArray());
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
  for (auto it = scope.begin(); it != scope.end(); ++it) {
    releaseDeep(it.value());
  }
  if (!m_varLocStack.isEmpty()) m_varLocStack.takeLast();
  collectCycles();
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
  if (!AcObjectManager::isManagedInstance(val.toQJsonValue())) return;
  m_objMgr.retain(AcObjectManager::getObjId(val.toQJsonValue()));
}

void AcInterpreter::releaseIfInstance(const accore::AcJsonValue &val) {
  if (!AcObjectManager::isManagedInstance(val.toQJsonValue())) return;
  m_objMgr.release(AcObjectManager::getObjId(val.toQJsonValue()));
}

void AcInterpreter::releaseIfInstanceWithDestruct(const accore::AcJsonValue &val) {
  if (!AcObjectManager::isManagedInstance(val.toQJsonValue())) return;
  m_objMgr.release(AcObjectManager::getObjId(val.toQJsonValue()));
  QVector<AcObjectManager::DestructInfo> pending = m_objMgr.takePendingDestructs();
  for (const auto &info : pending) {
    processDestructInfo(info);
  }
}

void AcInterpreter::processDestructInfo(const AcObjectManager::DestructInfo &info) {
  accore::AcJsonValue obj = accore::AcJsonValue::fromQJsonValue(info.instance);
  for (const auto &m : obj.members()) {
    if (m.key == QString::fromLatin1(AcRuntime::kClassKey) ||
        m.key == QString::fromLatin1(AcRuntime::kObjId))
      continue;
    releaseDeep(m.value);
  }
  if (FunMgr::ins().contains(info.className, QString::fromLatin1(AcRuntime::kDestructor))) {
    // 原生类析构：显式传递实例 thisObj（实例方法经 registerFuncsWithThis 注册）
    FunMgr::ins().call(info.className, QString::fromLatin1(AcRuntime::kDestructor),
                       QJsonValue(info.instance), QJsonArray());
  } else if (m_classes.contains(info.className) && !m_classes[info.className].isNative) {
    const MethodDef *dtor = findMethod(info.className, QString::fromLatin1(AcRuntime::kDestructor));
    if (dtor) {
      execMethod(*dtor, accore::AcJsonValue::fromQJsonValue(info.instance),
                 accore::AcJsonValue::makeArray());
    }
  }
}

void AcInterpreter::traverseNested(
    const accore::AcJsonValue &val,
    const std::function<void(const accore::AcJsonValue &)> &onChild) {
  if (val.isArray()) {
    for (const accore::AcJsonValue &item : val.items()) onChild(item);
  } else if (val.isObject()) {
    for (const auto &m : val.members()) {
      if (m.key == QString::fromLatin1(AcRuntime::kClassKey) ||
          m.key == QString::fromLatin1(AcRuntime::kObjId))
        continue;
      onChild(m.value);
    }
  }
}

void AcInterpreter::releaseDeep(const accore::AcJsonValue &val) {
  if (AcObjectManager::isManagedInstance(val.toQJsonValue())) {
    releaseIfInstanceWithDestruct(val);
    return;
  }
  traverseNested(val, [this](const accore::AcJsonValue &child) { releaseDeep(child); });
}

// ═════════════════════════════════════════════════════════════════════════════
//  标记-清扫（处理循环引用）
// ═════════════════════════════════════════════════════════════════════════════

void AcInterpreter::markFromValue(const accore::AcJsonValue &val) {
  if (AcObjectManager::isManagedInstance(val.toQJsonValue())) {
    QString objId = AcObjectManager::getObjId(val.toQJsonValue());
    if (m_objMgr.isMarked(objId) || !m_objMgr.contains(objId)) return;
    m_objMgr.mark(objId);
    accore::AcJsonValue obj = accore::AcJsonValue::fromQJsonValue(m_objMgr.getObject(objId));
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
      if (m.key == QString::fromLatin1(AcRuntime::kClassKey) ||
          m.key == QString::fromLatin1(AcRuntime::kObjId))
        continue;
      markFromValue(m.value);
    }
  }
  if (!m_modifiedThis.isEmpty()) {
    for (const auto &m : m_modifiedThis.members()) {
      if (m.key == QString::fromLatin1(AcRuntime::kClassKey) ||
          m.key == QString::fromLatin1(AcRuntime::kObjId))
        continue;
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
        topLine = s.line;
        break;
      }
    }
    m_callStack.append(AcDebugFrame{QString(), topFile, topLine});
  }

  pushScope();

  AcBuiltinLoader::registerNativeClasses(m_classes);

  FunMgr::init();
  FunBuiltin::setContext({m_scriptDir, m_rootDir, m_logCallback, &m_generatedFiles, 0});

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
