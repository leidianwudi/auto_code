/**
 * @file ac_interpreter_stmt.cpp
 * @brief 解释器语句执行实现文件
 */

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
#include "ac_interpreter.h"
#include "ac_object_manager.h"

// ═════════════════════════════════════════════════════════════════════════════
//  类方法执行
// ═════════════════════════════════════════════════════════════════════════════

QJsonValue AcInterpreter::execMethod(const MethodDef &method, const QJsonObject &thisObj,
                                     const QJsonValue &callArgs) {
  QJsonArray argsArr = callArgs.toArray();
  QJsonObject oldThis = m_currentThis;

  bool savedReturned = m_hasReturned;
  QJsonValue savedReturnValue = m_returnValue;
  m_hasReturned = false;
  m_returnValue = QJsonValue();

  m_currentThis = thisObj;
  m_modifiedThis = thisObj;

  pushScope();

  for (int i = 0; i < method.params.size(); ++i) {
    setVar(method.params[i].name, i < argsArr.size() ? argsArr[i] : QJsonValue());
  }

  execBlock(method.body);

  popScope();

  QJsonValue result = m_hasReturned ? m_returnValue : QJsonValue();

  m_hasReturned = savedReturned;
  m_returnValue = savedReturnValue;

  m_currentThis = oldThis;

  return result;
}

void AcInterpreter::initStaticVars(const ClassDef &cd) {
  if (m_staticInited.contains(cd.name)) return;
  m_staticInited.insert(cd.name);

  if (!cd.baseClass.isEmpty() && m_classes.contains(cd.baseClass)) {
    initStaticVars(m_classes[cd.baseClass]);
  }

  QJsonObject sv;
  if (!cd.baseClass.isEmpty() && m_staticVars.contains(cd.baseClass)) {
    sv = m_staticVars[cd.baseClass];
  }
  for (const auto &prop : cd.properties) {
    if (prop.isStatic) {
      if (prop.value) {
        QJsonValue v = evalExpr(*prop.value);
        retainIfInstance(v);
        sv[prop.key] = v;
      } else {
        sv[prop.key] = QJsonValue();
      }
    }
  }
  m_staticVars[cd.name] = sv;
}

const MethodDef *AcInterpreter::findMethod(const QString &className,
                                           const QString &methodName) const {
  if (!m_classes.contains(className)) return nullptr;
  const ClassDef &cd = m_classes[className];
  for (const auto &m : cd.methods) {
    if (m.name == methodName) return &m;
  }
  if (!cd.baseClass.isEmpty()) {
    return findMethod(cd.baseClass, methodName);
  }
  return nullptr;
}

QJsonObject AcInterpreter::createBaseInstance(const QString &baseClassName) {
  if (!m_classes.contains(baseClassName)) return QJsonObject();
  const ClassDef &cd = m_classes[baseClassName];
  QJsonObject instance;
  if (!cd.baseClass.isEmpty()) {
    instance = createBaseInstance(cd.baseClass);
  }
  for (const auto &prop : cd.properties) {
    if (prop.value) {
      QJsonValue v = evalExpr(*prop.value);
      retainIfInstance(v);
      instance[prop.key] = v;
    } else {
      instance[prop.key] = QJsonValue();
    }
  }
  return instance;
}

// ═════════════════════════════════════════════════════════════════════════════
//  顶层函数执行
// ═════════════════════════════════════════════════════════════════════════════

QJsonValue AcInterpreter::execUserFunction(const MethodDef &func, const QJsonValue &callArgs) {
  QJsonArray argsArr = callArgs.toArray();

  bool savedReturned = m_hasReturned;
  QJsonValue savedReturnValue = m_returnValue;
  m_hasReturned = false;
  m_returnValue = QJsonValue();

  pushScope();

  for (int i = 0; i < func.params.size(); ++i) {
    setVar(func.params[i].name, i < argsArr.size() ? argsArr[i] : QJsonValue());
  }

  execBlock(func.body);

  QJsonValue result = m_hasReturned ? m_returnValue : QJsonValue();

  popScope();

  m_hasReturned = savedReturned;
  m_returnValue = savedReturnValue;

  return result;
}

// ═════════════════════════════════════════════════════════════════════════════
//  语句执行辅助
// ═════════════════════════════════════════════════════════════════════════════

void AcInterpreter::writeBackVar(const Expr &objectExpr, const QJsonValue &val) {
  if (objectExpr.kind == Expr::kIdent) {
    setVar(objectExpr.ident, val);
  } else if (objectExpr.kind == Expr::kThis) {
    m_currentThis = val.toObject();
    m_modifiedThis = val.toObject();
  } else if (objectExpr.kind == Expr::kPropAccess) {
    QJsonValue parentVal = resolveVar(objectExpr.ident);
    if (parentVal.isObject()) {
      QJsonObject parentObj = parentVal.toObject();
      parentObj[objectExpr.prop] = val;
      setVar(objectExpr.ident, parentObj);
    }
  }
}

void AcInterpreter::execStaticAssign(const QString &className, const QString &propName,
                                     const QJsonValue &val) {
  if (m_staticVars.contains(className)) {
    QJsonObject sv = m_staticVars[className];
    if (sv.contains(propName)) {
      releaseIfInstanceWithDestruct(sv.value(propName));
    }
    retainIfInstance(val);
    sv[propName] = val;
    m_staticVars[className] = sv;
  }
}

void AcInterpreter::execThisAssign(const QString &propName, const QJsonValue &val) {
  QJsonValue old = m_currentThis.value(propName);
  releaseIfInstanceWithDestruct(old);
  retainIfInstance(val);
  m_currentThis[propName] = val;
  m_modifiedThis[propName] = val;
}

// ═════════════════════════════════════════════════════════════════════════════
//  语句执行
// ═════════════════════════════════════════════════════════════════════════════

void AcInterpreter::execStmt(const Block::Stmt &stmt) {
  switch (stmt.kind) {
    case Block::Stmt::kCall: {
      QJsonValue cls = evalExpr(stmt.call.className);
      QJsonValue func = evalExpr(stmt.call.funcName);
      QJsonValue args = evalExpr(stmt.call.args);
      if (!m_error.isEmpty()) return;
      QString clsName = cls.toString();
      QString funcName = func.toString();
      QJsonArray argsArr = args.toArray();

      FunMgr::ins().call(clsName, funcName, argsArr);
      QString err = FunMgr::takeError();
      if (!err.isEmpty()) {
        m_error = QStringLiteral("%1 at line %2").arg(err).arg(stmt.call.className.line);
      }
      break;
    }

    case Block::Stmt::kAssign: {
      QJsonValue val = evalExpr(stmt.assign.value);
      if (!m_error.isEmpty()) return;

      if (stmt.assign.compoundOp != CompoundOp::kNone) {
        QJsonValue currentVal;
        if (stmt.assign.isStatic) {
          currentVal = m_staticVars[stmt.assign.staticClassName].value(stmt.assign.name);
        } else if (!stmt.assign.thisProp.isEmpty()) {
          currentVal = m_currentThis.value(stmt.assign.thisProp);
        } else {
          currentVal = resolveVar(stmt.assign.name);
        }
        val = applyCompoundOp(currentVal, val, stmt.assign.compoundOp, stmt.assign.value.line);
        if (!m_error.isEmpty()) return;
      }

      if (stmt.assign.isStatic) {
        execStaticAssign(stmt.assign.staticClassName, stmt.assign.name, val);
        break;
      }

      if (!stmt.assign.thisProp.isEmpty()) {
        execThisAssign(stmt.assign.thisProp, val);
        break;
      }

      setVar(stmt.assign.name, val);

      if (!stmt.assign.hasTypeAnnotation) {
        m_inferredTypes[stmt.assign.name] = inferTypeName(val);
      }
      break;
    }

    case Block::Stmt::kIndexAssign: {
      QJsonValue objVal = evalExpr(stmt.indexAssign.objectExpr);
      QJsonValue idxVal = evalExpr(stmt.indexAssign.indexExpr);
      QJsonValue newVal = evalExpr(stmt.indexAssign.value);
      retainIfInstance(newVal);

      if (objVal.isObject()) {
        QJsonObject obj = objVal.toObject();
        QString key = idxVal.isString() ? idxVal.toString() : QString::number(idxVal.toDouble());
        if (obj.contains(key)) {
          releaseIfInstanceWithDestruct(obj.value(key));
        }
        obj[key] = newVal;
        writeBackVar(stmt.indexAssign.objectExpr, obj);
      } else if (objVal.isArray()) {
        QJsonArray arr = objVal.toArray();
        int idx = idxVal.toInt();
        if (idx >= 0 && idx < arr.size()) {
          releaseIfInstanceWithDestruct(arr[idx]);
          arr.replace(idx, newVal);
        } else if (idx == arr.size()) {
          arr.append(newVal);
        }
        writeBackVar(stmt.indexAssign.objectExpr, QJsonValue(arr));
      }
      break;
    }

    case Block::Stmt::kPropAssign: {
      if (stmt.propAssign.objectExpr.kind == Expr::kIdent &&
          m_classes.contains(stmt.propAssign.objectExpr.ident)) {
        QJsonValue newVal = evalExpr(stmt.propAssign.value);
        QString className = stmt.propAssign.objectExpr.ident;
        if (!m_staticInited.contains(className)) {
          initStaticVars(m_classes[className]);
        }
        execStaticAssign(className, stmt.propAssign.prop, newVal);
        break;
      }
      QJsonValue objVal = evalExpr(stmt.propAssign.objectExpr);
      QJsonValue newVal = evalExpr(stmt.propAssign.value);
      retainIfInstance(newVal);
      if (objVal.isObject()) {
        QJsonObject obj = objVal.toObject();
        if (stmt.propAssign.compoundOp != CompoundOp::kNone) {
          QJsonValue oldVal = obj.value(stmt.propAssign.prop);
          newVal = applyCompoundOp(oldVal, newVal, stmt.propAssign.compoundOp, 0);
          if (!m_error.isEmpty()) return;
        }
        if (obj.contains(stmt.propAssign.prop)) {
          releaseIfInstanceWithDestruct(obj.value(stmt.propAssign.prop));
        }
        obj[stmt.propAssign.prop] = newVal;
        writeBackVar(stmt.propAssign.objectExpr, obj);
      }
      break;
    }

    case Block::Stmt::kFor: {
      if (stmt.forStmt.isStandard) {
        pushScope();
        execBlock(stmt.forStmt.initBlock);
        if (!m_error.isEmpty()) {
          popScope();
          return;
        }
        while (isTruthy(evalExpr(stmt.forStmt.condition))) {
          execBlock(stmt.forStmt.body);
          if (!m_error.isEmpty()) {
            popScope();
            return;
          }
          if (m_hasBreak) {
            m_hasBreak = false;
            break;
          }
          m_hasContinue = false;
          evalExpr(stmt.forStmt.updateExpr);
          if (!m_error.isEmpty()) {
            popScope();
            return;
          }
        }
        popScope();
      } else {
        QJsonValue iterVal = evalExpr(stmt.forStmt.arrayExpr);
        QJsonArray arr;
        if (iterVal.isString()) {
          QString s = iterVal.toString();
          for (int i = 0; i < s.length(); ++i) arr.append(QJsonValue(QString(s[i])));
        } else if (iterVal.isObject()) {
          QJsonObject obj = iterVal.toObject();
          for (auto it = obj.begin(); it != obj.end(); ++it) {
            arr.append(QJsonValue(it.key()));
          }
        } else {
          arr = iterVal.toArray();
          if (arr.isEmpty() && iterVal.isArray()) break;
        }
        for (const QJsonValue &v : arr) {
          pushScope();
          setVar(stmt.forStmt.varName, v);
          execBlock(stmt.forStmt.body);
          popScope();
          if (!m_error.isEmpty()) return;
          if (m_hasBreak) {
            m_hasBreak = false;
            break;
          }
          m_hasContinue = false;
        }
      }
      break;
    }

    case Block::Stmt::kIf:
      execIfStmt(stmt.ifStmt);
      break;

    case Block::Stmt::kWhile: {
      pushScope();
      while (isTruthy(evalExpr(stmt.whileStmt.condition))) {
        execBlock(stmt.whileStmt.body);
        if (!m_error.isEmpty()) {
          popScope();
          return;
        }
        if (m_hasBreak) {
          m_hasBreak = false;
          break;
        }
        m_hasContinue = false;
      }
      popScope();
      break;
    }

    case Block::Stmt::kSwitch: {
      QJsonValue switchVal = evalExpr(stmt.switchStmt.expr);
      bool matched = false;
      bool fellThrough = false;
      for (const auto &sc : stmt.switchStmt.cases) {
        if (!matched && !fellThrough) {
          if (sc.isDefault) {
            matched = true;
          } else {
            QJsonValue caseVal = evalExpr(sc.value);
            if (compareValues(switchVal, caseVal) == 0) {
              matched = true;
            }
          }
        }
        if (matched || fellThrough) {
          pushScope();
          execBlock(sc.body);
          popScope();
          if (!m_error.isEmpty()) return;
          if (m_hasBreak) {
            m_hasBreak = false;
            matched = false;
            fellThrough = false;
            break;
          }
          fellThrough = true;
          matched = false;
        }
      }
      break;
    }

    case Block::Stmt::kBreak:
      m_hasBreak = true;
      return;

    case Block::Stmt::kContinue:
      m_hasContinue = true;
      return;

    case Block::Stmt::kExpr:
      evalExpr(stmt.exprStmt);
      break;

    case Block::Stmt::kClassDef:
      m_classes[stmt.classDef.name] = stmt.classDef;
      initStaticVars(stmt.classDef);
      break;

    case Block::Stmt::kInterfaceDef:
      break;

    case Block::Stmt::kEnumDef: {
      for (const auto &member : stmt.enumDef.members) {
        QString varName = QStringLiteral("%1.%2").arg(stmt.enumDef.name, member.name);
        setVar(varName, member.value);
      }
      break;
    }

    case Block::Stmt::kFuncDef:
      m_functions[stmt.funcDef.name] = stmt.funcDef;
      break;

    case Block::Stmt::kUsing: {
      QJsonValue val = evalExpr(*stmt.usingStmt.value);
      if (!m_error.isEmpty()) return;
      retainIfInstance(val);
      setVar(stmt.usingStmt.varName, val);
      m_inferredTypes[stmt.usingStmt.varName] = inferTypeName(val);
      if (!m_usingStack.isEmpty()) {
        m_usingStack.last().append(stmt.usingStmt.varName);
      }
      break;
    }

    case Block::Stmt::kReturn:
      m_hasReturned = true;
      m_returnValue = evalExpr(stmt.returnValue);
      break;

    case Block::Stmt::kImport:
      break;
  }
}

void AcInterpreter::execBlock(const Block &block) {
  for (int i = 0; i < block.stmts.size(); ++i) {
    const auto &stmt = block.stmts[i];
    execStmt(stmt);
    if (!m_error.isEmpty()) return;
    if (m_hasReturned || m_hasBreak || m_hasContinue) return;
  }
}

void AcInterpreter::execBlockWithThis(const Block &block, const QJsonObject &thisObj,
                                      QJsonValue &returnVal) {
  QJsonObject oldThis = m_currentThis;
  m_currentThis = thisObj;
  execBlock(block);
  if (m_hasReturned) returnVal = m_returnValue;
  m_currentThis = oldThis;
}

void AcInterpreter::execIfStmt(const IfStmt &ifStmt) {
  if (isTruthy(evalExpr(ifStmt.condition))) {
    pushScope();
    execBlock(ifStmt.thenBlock);
    popScope();
  } else if (ifStmt.elseIfBranch) {
    execIfStmt(*ifStmt.elseIfBranch);
  } else if (ifStmt.hasElse) {
    pushScope();
    execBlock(ifStmt.elseBlock);
    popScope();
  }
}