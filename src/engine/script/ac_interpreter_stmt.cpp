/**
 * @file ac_interpreter_stmt.cpp
 * @brief 解释器语句执行实现文件
 */

#include <QDir>
#include <QJsonDocument>
#include <cmath>

#include "../ac_language.h"
#include "../ac_value_str.h"
#include "../function/fun_builtin.h"
#include "../function/fun_mgr.h"
#include "../tpl/tpl_engine.h"
#include "ac_builtin_eval.h"
#include "ac_interpreter.h"
#include "ac_object_manager.h"
#include "src/core/json/ac_json_value.h"

// ═════════════════════════════════════════════════════════════════════════════
//  类方法执行
// ═════════════════════════════════════════════════════════════════════════════

accore::AcJsonValue AcInterpreter::execCallBody(const QVector<ParamDef> &params,
                                                const accore::AcJsonValue &callArgs,
                                                const Block &body,
                                                const accore::AcJsonValue *thisObj,
                                                const QString &funcName) {
  // 递归深度上限：防止无界递归/误用导致解释器长时间无响应（类似 JS 的调用栈溢出）
  constexpr int kMaxCallDepth = 512;
  if (m_callDepth >= kMaxCallDepth) {
    setError(QStringLiteral("调用栈溢出（递归过深，超过 %1 层）：函数 %2")
                 .arg(kMaxCallDepth)
                 .arg(funcName),
             body.stmts.isEmpty() ? 0 : body.stmts.first().loc.line);
    return accore::AcJsonValue();
  }
  ++m_callDepth;
  QString frameFile;
  int frameLine = 0;
  if (m_debugger) {
    frameLine = body.stmts.isEmpty() ? 0 : body.stmts.first().loc.line;
    frameFile = body.stmts.isEmpty() ? QString() : body.stmts.first().filePath;
    m_callStack.append(AcDebugFrame{funcName, frameFile, frameLine});
  }

  QString oldFuncName = m_currentFuncName;
  m_currentFuncName = funcName;  // 记录当前函数名，供变量定位列展示

  const accore::AcJsonValue &argsArr = callArgs;
  accore::AcJsonValue oldThis = m_currentThis;

  bool savedReturned = m_hasReturned;
  accore::AcJsonValue savedReturnValue = m_returnValue;
  m_hasReturned = false;
  m_returnValue = accore::AcJsonValue();

  if (thisObj) {
    m_currentThis = *thisObj;
    m_modifiedThis = *thisObj;
  }

  pushScope();

  for (int i = 0; i < params.size(); ++i) {
    // 缺省实参时优先使用声明的默认值字面量（param: Type = 字面量），否则为 null
    // （AST 的 ParamDef.defaultValue 保持 QJsonValue，经 fromQJsonValue 转入运行时值）
    if (i < argsArr.size()) {
      declareVar(params[i].name, argsArr.at(i));
    } else if (params[i].defaultValue.isUndefined()) {
      declareVar(params[i].name, accore::AcJsonValue());
    } else {
      declareVar(params[i].name, accore::AcJsonValue::fromQJsonValue(params[i].defaultValue));
    }
    recordVarLoc(params[i].name, frameFile, frameLine);
  }

  execBlock(body);

  popScope();

  accore::AcJsonValue result = m_hasReturned ? m_returnValue : accore::AcJsonValue();

  m_hasReturned = savedReturned;
  m_returnValue = savedReturnValue;

  m_currentThis = oldThis;
  m_currentFuncName = oldFuncName;

  if (m_debugger) m_callStack.removeLast();
  --m_callDepth;

  return result;
}

accore::AcJsonValue AcInterpreter::execMethod(const MethodDef &method,
                                              const accore::AcJsonValue &thisObj,
                                              const accore::AcJsonValue &callArgs) {
  // 用「类名.方法名」作为函数名，供调用栈与变量位置列展示
  QString qualified = method.name;
  const QString cls = thisObj.instanceClass();
  if (!cls.isEmpty()) qualified = QStringLiteral("%1.%2").arg(cls, method.name);
  return execCallBody(method.params, callArgs, method.body, &thisObj, qualified);
}

void AcInterpreter::initStaticVars(const ClassDef &cd) {
  if (m_staticInited.contains(cd.name)) return;
  m_staticInited.insert(cd.name);

  if (!cd.baseClass.isEmpty() && m_classes.contains(cd.baseClass)) {
    initStaticVars(m_classes[cd.baseClass]);
  }

  accore::AcJsonValue sv = accore::AcJsonValue::makeObject();
  if (!cd.baseClass.isEmpty() && m_staticVars.contains(cd.baseClass)) {
    sv = m_staticVars[cd.baseClass];
  }
  for (const auto &prop : cd.properties) {
    if (prop.isStatic) {
      if (prop.value) {
        accore::AcJsonValue v = evalExpr(*prop.value);
        retainIfInstance(v);
        sv.set(prop.key, v);
      } else {
        sv.set(prop.key, accore::AcJsonValue());
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

accore::AcJsonValue AcInterpreter::createBaseInstance(const QString &baseClassName) {
  if (!m_classes.contains(baseClassName)) return accore::AcJsonValue::makeInstance(baseClassName);
  const ClassDef &cd = m_classes[baseClassName];
  accore::AcJsonValue instance = accore::AcJsonValue::makeInstance(baseClassName);
  if (!cd.baseClass.isEmpty()) {
    instance = createBaseInstance(cd.baseClass);
  }
  for (const auto &prop : cd.properties) {
    if (prop.isStatic) continue;  // 静态属性不属于实例，不嵌入实例对象
    if (prop.value) {
      accore::AcJsonValue v = evalExpr(*prop.value);
      if (!m_error.isEmpty()) return instance;
      retainIfInstance(v);
      instance.set(prop.key, v);
    } else {
      instance.set(prop.key, accore::AcJsonValue());
    }
  }
  return instance;
}

// ═════════════════════════════════════════════════════════════════════════════
//  顶层函数执行
// ═════════════════════════════════════════════════════════════════════════════

accore::AcJsonValue AcInterpreter::execUserFunction(const MethodDef &func,
                                                    const accore::AcJsonValue &callArgs) {
  return execCallBody(func.params, callArgs, func.body, nullptr, func.name);
}

// ═════════════════════════════════════════════════════════════════════════════
//  语句执行辅助
// ═════════════════════════════════════════════════════════════════════════════

void AcInterpreter::writeBackVar(const Expr &objectExpr, const accore::AcJsonValue &val) {
  if (objectExpr.kind == Expr::kIdent) {
    setVar(objectExpr.ident, val);
  } else if (objectExpr.kind == Expr::kThis) {
    m_currentThis = val;
    m_modifiedThis = val;
  } else if (objectExpr.kind == Expr::kPropAccess) {
    accore::AcJsonValue parentVal = resolveVar(objectExpr.ident);
    if (parentVal.isObject()) {
      accore::AcJsonValue parentObj = parentVal;
      parentObj.set(objectExpr.prop, val);
      setVar(objectExpr.ident, parentObj);
    }
  }
}

void AcInterpreter::execStaticAssign(const QString &className, const QString &propName,
                                     const accore::AcJsonValue &val) {
  if (m_staticVars.contains(className)) {
    accore::AcJsonValue sv = m_staticVars[className];
    if (sv.has(propName)) {
      releaseIfInstanceWithDestruct(sv.value(propName));
    }
    retainIfInstance(val);
    sv.set(propName, val);
    m_staticVars[className] = sv;
  }
}

void AcInterpreter::execThisAssign(const QString &propName, const accore::AcJsonValue &val) {
  accore::AcJsonValue old = m_currentThis.value(propName);
  releaseIfInstanceWithDestruct(old);
  retainIfInstance(val);
  m_currentThis.set(propName, val);
  m_modifiedThis.set(propName, val);
}

void AcInterpreter::assignToIndex(const accore::AcJsonValue &objVal,
                                  const accore::AcJsonValue &idxVal,
                                  const accore::AcJsonValue &newVal, const Expr &objectExpr) {
  if (objVal.isObject()) {
    accore::AcJsonValue obj = objVal;
    QString key = idxVal.isString() ? idxVal.toString() : QString::number(idxVal.toDouble());
    if (obj.has(key)) {
      releaseIfInstanceWithDestruct(obj.value(key));
    }
    obj.set(key, newVal);
    writeBackVar(objectExpr, obj);
  } else if (objVal.isArray()) {
    accore::AcJsonValue arr = objVal;
    // safeJsonToInt 保持 QJsonValue 签名（ac_language.h），经 toQJsonValue 转换
    int idx = safeJsonToInt(idxVal.toQJsonValue());
    if (idx >= 0 && idx < arr.size()) {
      releaseIfInstanceWithDestruct(arr.at(idx));
      arr.replace(idx, newVal);
    } else if (idx == arr.size()) {
      arr.append(newVal);
    }
    writeBackVar(objectExpr, arr);
  } else {
    // 对齐 JS TypeError 语义：对 null/标量做索引赋值属于非法操作，显式报错而非静默跳过
    setError(QStringLiteral("cannot index-assign on value"), objectExpr.loc.line);
  }
}

void AcInterpreter::assignToProperty(const accore::AcJsonValue &objVal, const QString &prop,
                                     accore::AcJsonValue newVal, const Expr &objectExpr,
                                     CompoundOp op) {
  if (!objVal.isObject()) {
    // 对齐 JS TypeError 语义：对 null/标量设置属性属于非法操作，显式报错
    setError(QStringLiteral("cannot set property '%1' on value").arg(prop), objectExpr.loc.line);
    return;
  }
  accore::AcJsonValue obj = objVal;
  if (op != CompoundOp::kNone) {
    accore::AcJsonValue oldVal = obj.value(prop);
    // 复合运算报错带上宿主表达式行号（此前传 0 丢失行号）
    newVal = applyCompoundOp(oldVal, newVal, op, objectExpr.loc.line);
    if (!m_error.isEmpty()) return;
  }
  if (obj.has(prop)) {
    releaseIfInstanceWithDestruct(obj.value(prop));
  }
  obj.set(prop, newVal);
  writeBackVar(objectExpr, obj);
}

// ═════════════════════════════════════════════════════════════════════════════
//  语句执行
// ═════════════════════════════════════════════════════════════════════════════

void AcInterpreter::execStmt(const Block::Stmt &stmt) {
  switch (stmt.kind) {
    case Block::Stmt::kCall: {
      accore::AcJsonValue cls = evalExpr(stmt.call.className);
      accore::AcJsonValue func = evalExpr(stmt.call.funcName);
      accore::AcJsonValue args = evalExpr(stmt.call.args);
      if (!m_error.isEmpty()) return;
      QString clsName = cls.toString();
      QString funcName = func.toString();

      // FunMgr 已迁移到 accore 签名：直接传值，无边界转换
      FunMgr::ins().call(clsName, funcName, args);
      QString err = FunMgr::takeError();
      if (!err.isEmpty()) {
        setError(err, stmt.call.className.loc.line);
      }
      break;
    }

    case Block::Stmt::kAssign: {
      accore::AcJsonValue val = evalExpr(stmt.assign.value);
      if (!m_error.isEmpty()) return;

      if (stmt.assign.compoundOp != CompoundOp::kNone) {
        accore::AcJsonValue currentVal;
        if (stmt.assign.isStatic) {
          currentVal = m_staticVars[stmt.assign.staticClassName].value(stmt.assign.name);
        } else if (!stmt.assign.thisProp.isEmpty()) {
          currentVal = m_currentThis.value(stmt.assign.thisProp);
        } else {
          currentVal = resolveVar(stmt.assign.name);
        }
        val = applyCompoundOp(currentVal, val, stmt.assign.compoundOp, stmt.assign.value.loc.line);
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
        declareVar(stmt.assign.name, val, stmt.assign.isConst);
        recordVarLoc(stmt.assign.name, stmt.filePath, stmt.loc.line);
      } else {
        setVar(stmt.assign.name, val);
      }

      if (!stmt.assign.hasTypeAnnotation) {
        recordInferredType(stmt.assign.name, val);
      }
      break;
    }

    case Block::Stmt::kIndexAssign: {
      accore::AcJsonValue objVal = evalExpr(stmt.indexAssign.objectExpr);
      if (!m_error.isEmpty()) return;
      accore::AcJsonValue idxVal = evalExpr(stmt.indexAssign.indexExpr);
      if (!m_error.isEmpty()) return;
      accore::AcJsonValue newVal = evalExpr(stmt.indexAssign.value);
      if (!m_error.isEmpty()) return;
      retainIfInstance(newVal);
      assignToIndex(objVal, idxVal, newVal, stmt.indexAssign.objectExpr);
      break;
    }

    case Block::Stmt::kPropAssign: {
      if (stmt.propAssign.objectExpr.kind == Expr::kIdent &&
          m_classes.contains(stmt.propAssign.objectExpr.ident)) {
        accore::AcJsonValue newVal = evalExpr(stmt.propAssign.value);
        if (!m_error.isEmpty()) return;
        QString className = stmt.propAssign.objectExpr.ident;
        if (!m_staticInited.contains(className)) {
          initStaticVars(m_classes[className]);
        }
        execStaticAssign(className, stmt.propAssign.prop, newVal);
        break;
      }
      accore::AcJsonValue objVal = evalExpr(stmt.propAssign.objectExpr);
      if (!m_error.isEmpty()) return;
      accore::AcJsonValue newVal = evalExpr(stmt.propAssign.value);
      if (!m_error.isEmpty()) return;
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
        accore::AcJsonValue iterVal = evalExpr(stmt.forStmt.arrayExpr);
        accore::AcJsonValue arr;
        if (iterVal.isString()) {
          arr = accore::AcJsonValue::makeArray();
          QString s = iterVal.toString();
          for (int i = 0; i < s.length(); ++i) arr.append(accore::AcJsonValue(QString(s[i])));
        } else if (iterVal.isObject()) {
          arr = accore::AcJsonValue::makeArray();
          // 键保序：按对象插入序迭代成员键（QJsonObject 迭代为字母序，此为迁移核心收益）
          for (const auto &mem : iterVal.members()) {
            arr.append(accore::AcJsonValue(mem.key));
          }
        } else {
          arr = iterVal;
          if (arr.isEmpty() && iterVal.isArray()) break;
        }
        for (const accore::AcJsonValue &v : arr.items()) {
          pushScope();
          declareVar(stmt.forStmt.varName, v);
          recordVarLoc(stmt.forStmt.varName, stmt.filePath, stmt.loc.line);
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
      if (stmt.whileStmt.isDoWhile) {
        // do { body } while (cond); — 至少执行一次循环体，再判断条件
        while (true) {
          execBlock(stmt.whileStmt.body);
          if (!m_error.isEmpty()) {
            popScope();
            return;
          }
          if (m_hasBreak || m_hasReturned) {
            m_hasBreak = false;
            break;
          }
          m_hasContinue = false;
          if (!isTruthy(evalExpr(stmt.whileStmt.condition))) break;
        }
      } else {
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
      }
      popScope();
      break;
    }

    case Block::Stmt::kSwitch: {
      accore::AcJsonValue switchVal = evalExpr(stmt.switchStmt.expr);
      bool matched = false;
      bool fellThrough = false;
      for (const auto &sc : stmt.switchStmt.cases) {
        if (!matched && !fellThrough) {
          if (sc.isDefault) {
            matched = true;
          } else {
            accore::AcJsonValue caseVal = evalExpr(sc.value);
            if (!m_error.isEmpty()) return;
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
        // AST 枚举成员值保持 QJsonValue，经 fromQJsonValue 转入运行时值
        setVar(varName, accore::AcJsonValue::fromQJsonValue(member.value));
      }
      break;
    }

    case Block::Stmt::kFuncDef:
      m_functions[stmt.funcDef.name] = stmt.funcDef;
      break;

    case Block::Stmt::kUsing: {
      accore::AcJsonValue val = evalExpr(*stmt.usingStmt.value);
      if (!m_error.isEmpty()) return;
      retainIfInstance(val);
      declareVar(stmt.usingStmt.varName, val);
      recordVarLoc(stmt.usingStmt.varName, stmt.filePath, stmt.loc.line);
      recordInferredType(stmt.usingStmt.varName, val);
      if (!m_usingStack.isEmpty()) {
        m_usingStack.last().append(stmt.usingStmt.varName);
      }
      break;
    }

    case Block::Stmt::kThrow: {
      // 抛出错误：求值表达式后经 m_error 通道传播；try/catch 在块边界拦截。
      // 抛出值即错误消息本身，不追加 "at line N"（用户主动抛出，非引擎内部错误）
      accore::AcJsonValue v = evalExpr(stmt.returnValue);
      if (!m_error.isEmpty()) return;
      m_error = v.isString() ? v.toString() : AcValueStr::toString(v);
      return;
    }

    case Block::Stmt::kTry: {
      execTryStmt(stmt.tryStmt);
      return;
    }

    case Block::Stmt::kBlock: {
      pushScope();
      execBlock(stmt.blockBody);
      popScope();
      break;
    }

    case Block::Stmt::kReturn: {
      // 先求值后置标志：求值出错时保留 m_error 通道，不误置 m_hasReturned
      accore::AcJsonValue rv = evalExpr(stmt.returnValue);
      if (!m_error.isEmpty()) break;
      m_hasReturned = true;
      m_returnValue = rv;
      break;
    }

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
      m_callStack[0].line = stmt.loc.line;
    }
    // 调试：命中断点/单步时暂停（阻塞等待 GUI 指令），返回 false 表示用户停止。
    // 先用无锁原子标志门控：普通执行（调试器指针常驻但未进入调试会话）直接跳过，
    // 否则每条语句都要构造快照 lambda + 加锁，Debug 构建下脚本执行会慢数倍
    if (m_debugger && !isDeclStmt && m_debugger->isDebugging()) {
      bool cont =
          m_debugger->onStatement(stmt.filePath, stmt.loc.line, m_callDepth,
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

void AcInterpreter::execTryStmt(const TryStmt &ts) {
  execBlock(ts.tryBody);
  const bool tryFailed = !m_error.isEmpty();
  QString caughtErr;
  if (tryFailed) caughtErr = takeError();

  bool handled = false;
  if (tryFailed && ts.hasCatch) {
    // catch：错误消息绑定为字符串变量，仅 catch 块内可见
    pushScope();
    if (!ts.catchVar.isEmpty()) {
      declareVar(ts.catchVar, accore::AcJsonValue(caughtErr));
      recordVarLoc(ts.catchVar, m_scriptFile, 0);
    }
    execBlock(ts.catchBody);
    popScope();
    handled = m_error.isEmpty();  // catch 自身出错则继续向上传播
  }

  const bool propagate = tryFailed && !handled;
  if (propagate) m_error = caughtErr;  // 恢复错误状态，finally 执行后继续向上传播

  if (ts.hasFinally) {
    execBlock(ts.finallyBody);
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
      // AcDebugVar 字段保持 QJsonValue，经 toQJsonValue 转换
      vars.append(AcDebugVar{scopeName, it.key(), it.value().toQJsonValue(), filePath, line,
                             i == 0 ? QString() : m_currentFuncName});
    }
  }

  // 静态变量（成员按插入序迭代；AcDebugVar 字段保持 QJsonValue）
  for (auto it = m_staticVars.cbegin(); it != m_staticVars.cend(); ++it) {
    const accore::AcJsonValue &sv = it.value();
    for (const auto &mem : sv.members()) {
      vars.append(AcDebugVar{QStringLiteral("静态.%1").arg(it.key()), mem.key,
                             mem.value.toQJsonValue(), QString(), 0, it.key()});
    }
  }

  // this 对象的属性
  if (!m_currentThis.isEmpty()) {
    for (const auto &mem : m_currentThis.members()) {
      vars.append(AcDebugVar{QStringLiteral("this"), mem.key, mem.value.toQJsonValue(), QString(),
                             0, m_currentFuncName});
    }
  }
}
