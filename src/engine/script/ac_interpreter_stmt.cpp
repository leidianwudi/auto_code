/**
 * @file ac_interpreter_stmt.cpp
 * @brief 解释器语句执行实现文件
 */

#include <QDir>
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

QJsonValue AcInterpreter::execCallBody(const QVector<ParamDef> &params, const QJsonValue &callArgs,
                                       const Block &body, const QJsonObject *thisObj,
                                       const QString &funcName) {
  // 递归深度上限：防止无界递归/误用导致解释器长时间无响应（类似 JS 的调用栈溢出）
  constexpr int kMaxCallDepth = 512;
  if (m_callDepth >= kMaxCallDepth) {
    setError(QStringLiteral("调用栈溢出（递归过深，超过 %1 层）：函数 %2")
                 .arg(kMaxCallDepth)
                 .arg(funcName),
             body.stmts.isEmpty() ? 0 : body.stmts.first().line);
    return QJsonValue();
  }
  ++m_callDepth;
  QString frameFile;
  int frameLine = 0;
  if (m_debugger) {
    frameLine = body.stmts.isEmpty() ? 0 : body.stmts.first().line;
    frameFile = body.stmts.isEmpty() ? QString() : body.stmts.first().filePath;
    m_callStack.append(AcDebugFrame{funcName, frameFile, frameLine});
  }

  QString oldFuncName = m_currentFuncName;
  m_currentFuncName = funcName;  // 记录当前函数名，供变量定位列展示

  QJsonArray argsArr = callArgs.toArray();
  QJsonObject oldThis = m_currentThis;

  bool savedReturned = m_hasReturned;
  QJsonValue savedReturnValue = m_returnValue;
  m_hasReturned = false;
  m_returnValue = QJsonValue();

  if (thisObj) {
    m_currentThis = *thisObj;
    m_modifiedThis = *thisObj;
  }

  pushScope();

  for (int i = 0; i < params.size(); ++i) {
    declareVar(params[i].name, i < argsArr.size() ? argsArr[i] : QJsonValue());
    recordVarLoc(params[i].name, frameFile, frameLine);
  }

  execBlock(body);

  popScope();

  QJsonValue result = m_hasReturned ? m_returnValue : QJsonValue();

  m_hasReturned = savedReturned;
  m_returnValue = savedReturnValue;

  m_currentThis = oldThis;
  m_currentFuncName = oldFuncName;

  if (m_debugger) m_callStack.removeLast();
  --m_callDepth;

  return result;
}

QJsonValue AcInterpreter::execMethod(const MethodDef &method, const QJsonObject &thisObj,
                                     const QJsonValue &callArgs) {
  // 用「类名.方法名」作为函数名，供调用栈与变量位置列展示
  QString qualified = method.name;
  const QString cls = thisObj.value(QString::fromLatin1(AcRuntime::kClassKey)).toString();
  if (!cls.isEmpty()) qualified = QStringLiteral("%1.%2").arg(cls, method.name);
  return execCallBody(method.params, callArgs, method.body, &thisObj, qualified);
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
  return execCallBody(func.params, callArgs, func.body, nullptr, func.name);
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

void AcInterpreter::assignToIndex(const QJsonValue &objVal, const QJsonValue &idxVal,
                                  const QJsonValue &newVal, const Expr &objectExpr) {
  if (objVal.isObject()) {
    QJsonObject obj = objVal.toObject();
    QString key = idxVal.isString() ? idxVal.toString() : QString::number(idxVal.toDouble());
    if (obj.contains(key)) {
      releaseIfInstanceWithDestruct(obj.value(key));
    }
    obj[key] = newVal;
    writeBackVar(objectExpr, obj);
  } else if (objVal.isArray()) {
    QJsonArray arr = objVal.toArray();
    int idx = safeJsonToInt(idxVal);
    if (idx >= 0 && idx < arr.size()) {
      releaseIfInstanceWithDestruct(arr[idx]);
      arr.replace(idx, newVal);
    } else if (idx == arr.size()) {
      arr.append(newVal);
    }
    writeBackVar(objectExpr, QJsonValue(arr));
  }
}

void AcInterpreter::assignToProperty(const QJsonValue &objVal, const QString &prop,
                                     QJsonValue newVal, const Expr &objectExpr, CompoundOp op) {
  if (!objVal.isObject()) return;
  QJsonObject obj = objVal.toObject();
  if (op != CompoundOp::kNone) {
    QJsonValue oldVal = obj.value(prop);
    newVal = applyCompoundOp(oldVal, newVal, op, 0);
    if (!m_error.isEmpty()) return;
  }
  if (obj.contains(prop)) {
    releaseIfInstanceWithDestruct(obj.value(prop));
  }
  obj[prop] = newVal;
  writeBackVar(objectExpr, obj);
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
        setError(err, stmt.call.className.line);
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

      if (stmt.assign.isDeclaration) {
        declareVar(stmt.assign.name, val);
        recordVarLoc(stmt.assign.name, stmt.filePath, stmt.line);
      } else {
        setVar(stmt.assign.name, val);
      }

      if (!stmt.assign.hasTypeAnnotation) {
        recordInferredType(stmt.assign.name, val);
      }
      break;
    }

    case Block::Stmt::kIndexAssign: {
      QJsonValue objVal = evalExpr(stmt.indexAssign.objectExpr);
      QJsonValue idxVal = evalExpr(stmt.indexAssign.indexExpr);
      QJsonValue newVal = evalExpr(stmt.indexAssign.value);
      retainIfInstance(newVal);
      assignToIndex(objVal, idxVal, newVal, stmt.indexAssign.objectExpr);
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
      assignToProperty(objVal, stmt.propAssign.prop, newVal, stmt.propAssign.objectExpr,
                       stmt.propAssign.compoundOp);
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
          // 循环体内 return 后必须立即退出循环，
          // 否则后续轮次的 return 会覆盖正确的返回值
          if (m_hasBreak || m_hasReturned) {
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
          declareVar(stmt.forStmt.varName, v);
          recordVarLoc(stmt.forStmt.varName, stmt.filePath, stmt.line);
          execBlock(stmt.forStmt.body);
          popScope();
          if (!m_error.isEmpty()) return;
          // 循环体内 return 后必须立即退出循环，
          // 否则后续轮次的 return 会覆盖正确的返回值
          if (m_hasBreak || m_hasReturned) {
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
        // 循环体内 return 后必须立即退出循环，
        // 否则后续轮次的 return 会覆盖正确的返回值
        if (m_hasBreak || m_hasReturned) {
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
          // case 内 return 后必须立即停止遍历后续 case，
          // 否则后续 case 的 return 会覆盖正确的返回值
          if (m_hasBreak || m_hasReturned) {
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
      declareVar(stmt.usingStmt.varName, val);
      recordVarLoc(stmt.usingStmt.varName, stmt.filePath, stmt.line);
      recordInferredType(stmt.usingStmt.varName, val);
      if (!m_usingStack.isEmpty()) {
        m_usingStack.last().append(stmt.usingStmt.varName);
      }
      break;
    }

    case Block::Stmt::kBlock: {
      pushScope();
      execBlock(stmt.blockBody);
      popScope();
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
    if (m_cancelFlag && m_cancelFlag->load()) {
      m_error = QStringLiteral("执行已取消");
      return;
    }
    // 声明类语句（class/function/interface/enum/import）不参与断点/单步暂停，
    // 避免单步调试时在声明行上显示"错位"的高亮
    const bool isDeclStmt =
        (stmt.kind == Block::Stmt::kClassDef || stmt.kind == Block::Stmt::kFuncDef ||
         stmt.kind == Block::Stmt::kInterfaceDef || stmt.kind == Block::Stmt::kEnumDef ||
         stmt.kind == Block::Stmt::kImport);
    // 顶层脚本帧：实时更新其行号为当前执行语句行，使调用栈展示当前所在位置
    if (m_debugger && m_callDepth == 1 && !m_callStack.isEmpty() &&
        (m_scriptFile.isEmpty() || stmt.filePath == m_scriptFile)) {
      m_callStack[0].line = stmt.line;
    }
    // 调试：命中断点/单步时暂停（阻塞等待 GUI 指令），返回 false 表示用户停止
    if (m_debugger && !isDeclStmt) {
      bool cont =
          m_debugger->onStatement(stmt.filePath, stmt.line, m_callDepth,
                                  [this](QVector<AcDebugFrame> &stack, QList<AcDebugVar> &vars) {
                                    buildDebugSnapshot(stack, vars);
                                  });
      if (!cont) {
        m_error = QStringLiteral("执行已取消");
        return;
      }
    }
    execStmt(stmt);
    if (!m_error.isEmpty()) return;
    if (m_hasReturned || m_hasBreak || m_hasContinue) return;
  }
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

// ═════════════════════════════════════════════════════════════════════════════
//  调试快照
// ═════════════════════════════════════════════════════════════════════════════

void AcInterpreter::buildDebugSnapshot(QVector<AcDebugFrame> &stack,
                                       QList<AcDebugVar> &vars) const {
  // 调用栈（从外层到内层）
  stack = m_callStack;

  // 变量：所有作用域（从内到外展示）+ 静态变量 + this
  for (int i = m_scopeStack.size() - 1; i >= 0; --i) {
    const QString scopeName =
        QStringLiteral("%1").arg(i == 0 ? QStringLiteral("全局") : QStringLiteral("局部"));
    const auto &scope = m_scopeStack[i];
    // 变量位置表（与作用域同步维护）；越界时视为空表
    QHash<QString, QPair<QString, int>> loc;
    if (i < m_varLocStack.size()) loc = m_varLocStack[i];
    for (auto it = scope.cbegin(); it != scope.cend(); ++it) {
      QString filePath;
      int line = 0;
      auto lit = loc.constFind(it.key());
      if (lit != loc.cend()) {
        filePath = lit->first;
        line = lit->second;
      }
      vars.append(AcDebugVar{scopeName, it.key(), it.value(), filePath, line,
                             i == 0 ? QString() : m_currentFuncName});
    }
  }

  // 静态变量
  for (auto it = m_staticVars.cbegin(); it != m_staticVars.cend(); ++it) {
    const QJsonObject &sv = it.value();
    for (auto sit = sv.cbegin(); sit != sv.cend(); ++sit) {
      vars.append(AcDebugVar{QStringLiteral("静态.%1").arg(it.key()), sit.key(), sit.value(),
                             QString(), 0, it.key()});
    }
  }

  // this 对象的属性
  if (!m_currentThis.isEmpty()) {
    for (auto it = m_currentThis.cbegin(); it != m_currentThis.cend(); ++it) {
      vars.append(AcDebugVar{QStringLiteral("this"), it.key(), it.value(), QString(), 0,
                             m_currentFuncName});
    }
  }
}