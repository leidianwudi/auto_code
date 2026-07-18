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
#include "ac_object_manager.h"

// ═════════════════════════════════════════════════════════════════════════════
//  变量操作
// ═════════════════════════════════════════════════════════════════════════════

QJsonValue AcInterpreter::resolveVar(const QString &name) const {
  if (name == QString::fromLatin1(AcKeyword::kThis)) return QJsonValue(m_currentThis);
  if (name == QString::fromLatin1(AcKeyword::kSuper)) return QJsonValue(m_currentThis);
  for (int i = m_scopeStack.size() - 1; i >= 0; --i) {
    auto it = m_scopeStack[i].find(name);
    if (it != m_scopeStack[i].end()) return it.value();
  }
  return QJsonValue();
}

void AcInterpreter::setVar(const QString &name, const QJsonValue &val) {
  for (int i = m_scopeStack.size() - 1; i >= 0; --i) {
    if (m_scopeStack[i].contains(name)) {
      const QJsonValue &old = m_scopeStack[i][name];
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

void AcInterpreter::pushScope() {
  m_scopeStack.append(QHash<QString, QJsonValue>());
  m_usingStack.append(QVector<QString>());
}

void AcInterpreter::popScope() {
  if (m_scopeStack.isEmpty()) return;

  if (!m_usingStack.isEmpty()) {
    auto usingVars = m_usingStack.takeLast();
    for (int i = usingVars.size() - 1; i >= 0; --i) {
      QJsonValue val = resolveVar(usingVars[i]);
      if (val.isObject()) {
        QJsonObject obj = val.toObject();
        QString className = obj.value(QString::fromLatin1(AcRuntime::kClassKey)).toString();
        if (!className.isEmpty() && m_classes.contains(className)) {
          const ClassDef &cd = m_classes[className];
          QString disposeName = QString::fromLatin1(AcKeyword::kDispose);
          if (cd.isNative) {
            FunMgr::ins().call(className, disposeName, {QJsonValue(obj)});
            FunMgr::takeError();
          } else {
            const MethodDef *disposeMethod = findMethod(className, disposeName);
            if (disposeMethod) {
              execMethod(*disposeMethod, obj, QJsonValue());
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
  collectCycles();
}

bool AcInterpreter::containsVar(const QString &name) const {
  for (int i = m_scopeStack.size() - 1; i >= 0; --i) {
    if (m_scopeStack[i].contains(name)) return true;
  }
  return false;
}

bool AcInterpreter::isTruthy(const QJsonValue &cond) {
  if (cond.isBool()) return cond.toBool();
  if (cond.isString()) return !cond.toString().isEmpty();
  if (cond.isDouble()) return cond.toDouble() != 0.0;
  if (cond.isNull() || cond.isUndefined()) return false;
  return true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  引用计数辅助
// ═════════════════════════════════════════════════════════════════════════════

void AcInterpreter::retainIfInstance(const QJsonValue &val) {
  if (!AcObjectManager::isManagedInstance(val)) return;
  m_objMgr.retain(AcObjectManager::getObjId(val));
}

void AcInterpreter::releaseIfInstance(const QJsonValue &val) {
  if (!AcObjectManager::isManagedInstance(val)) return;
  m_objMgr.release(AcObjectManager::getObjId(val));
}

void AcInterpreter::releaseIfInstanceWithDestruct(const QJsonValue &val) {
  if (!AcObjectManager::isManagedInstance(val)) return;
  m_objMgr.release(AcObjectManager::getObjId(val));
  QVector<AcObjectManager::DestructInfo> pending = m_objMgr.takePendingDestructs();
  for (const auto &info : pending) {
    processDestructInfo(info);
  }
}

void AcInterpreter::processDestructInfo(const AcObjectManager::DestructInfo &info) {
  QJsonObject obj = info.instance;
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    if (it.key() == QString::fromLatin1(AcRuntime::kClassKey) ||
        it.key() == QString::fromLatin1(AcRuntime::kObjId))
      continue;
    releaseDeep(it.value());
  }
  if (FunMgr::ins().contains(info.className, QString::fromLatin1(AcRuntime::kDestructor))) {
    QJsonArray args;
    args.append(QJsonValue(info.instance));
    FunMgr::ins().call(info.className, QString::fromLatin1(AcRuntime::kDestructor), args);
  } else if (m_classes.contains(info.className) && !m_classes[info.className].isNative) {
    for (const auto &method : m_classes[info.className].methods) {
      if (method.name == QString::fromLatin1(AcRuntime::kDestructor)) {
        execMethod(method, info.instance, QJsonValue(QJsonArray()));
        break;
      }
    }
  }
}

void AcInterpreter::traverseNested(const QJsonValue &val,
                                   const std::function<void(const QJsonValue &)> &onChild) {
  if (val.isArray()) {
    for (const QJsonValue &item : val.toArray()) onChild(item);
  } else if (val.isObject()) {
    QJsonObject obj = val.toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      if (it.key() == QString::fromLatin1(AcRuntime::kClassKey) ||
          it.key() == QString::fromLatin1(AcRuntime::kObjId))
        continue;
      onChild(it.value());
    }
  }
}

void AcInterpreter::releaseDeep(const QJsonValue &val) {
  if (AcObjectManager::isManagedInstance(val)) {
    releaseIfInstanceWithDestruct(val);
    return;
  }
  traverseNested(val, [this](const QJsonValue &child) { releaseDeep(child); });
}

// ═════════════════════════════════════════════════════════════════════════════
//  标记-清扫（处理循环引用）
// ═════════════════════════════════════════════════════════════════════════════

void AcInterpreter::markFromValue(const QJsonValue &val) {
  if (AcObjectManager::isManagedInstance(val)) {
    QString objId = AcObjectManager::getObjId(val);
    if (m_objMgr.isMarked(objId) || !m_objMgr.contains(objId)) return;
    m_objMgr.mark(objId);
    QJsonObject obj = m_objMgr.getObject(objId);
    traverseNested(QJsonValue(obj), [this](const QJsonValue &child) { markFromValue(child); });
    return;
  }
  traverseNested(val, [this](const QJsonValue &child) { markFromValue(child); });
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
    for (auto pit = it.value().begin(); pit != it.value().end(); ++pit) {
      markFromValue(pit.value());
    }
  }
  if (!m_currentThis.isEmpty()) {
    for (auto it = m_currentThis.begin(); it != m_currentThis.end(); ++it) {
      if (it.key() == QString::fromLatin1(AcRuntime::kClassKey) ||
          it.key() == QString::fromLatin1(AcRuntime::kObjId))
        continue;
      markFromValue(it.value());
    }
  }
  if (!m_modifiedThis.isEmpty()) {
    for (auto it = m_modifiedThis.begin(); it != m_modifiedThis.end(); ++it) {
      if (it.key() == QString::fromLatin1(AcRuntime::kClassKey) ||
          it.key() == QString::fromLatin1(AcRuntime::kObjId))
        continue;
      markFromValue(it.value());
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

QJsonValue AcInterpreter::execute(const Block &program, QString &error) {
  m_error.clear();
  m_scopeStack.clear();
  m_usingStack.clear();
  m_classes.clear();
  m_functions.clear();
  m_currentThis = QJsonObject();
  m_hasReturned = false;
  m_returnValue = QJsonValue();
  m_generatedFiles.clear();
  m_funcExprCounter = 0;

  pushScope();

  for (const auto &name : AcClass::kAll) {
    ClassDef nativeClass;
    nativeClass.name = name;
    nativeClass.isNative = true;
    m_classes[name] = nativeClass;
  }

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
          setVar(varName, member.value);
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
    return QJsonValue();
  }

  while (!m_scopeStack.isEmpty()) {
    popScope();
  }

  return m_hasReturned ? m_returnValue : QJsonValue();
}