/**
 * @file ac_compiler.cpp
 * @brief 字节码编译器实现 — AST → AcModule（深度作用域方案）
 */

#include "ac_compiler.h"

#include <QSet>

#include "../ac_language.h"
#include "src/core/json/ac_json_value.h"

// ═════════════════════════════════════════════════════════════════════════════
//  指令发射
// ═════════════════════════════════════════════════════════════════════════════

void AcCompiler::emitInstr(AcOpcode op, int32_t a, int32_t b, int32_t c, int line) {
  AcInstr ins;
  ins.op = op;
  ins.a = a;
  ins.b = b;
  ins.c = c;
  ins.line = line;
  m_cur->code.append(ins);
}

int AcCompiler::emitHere() { return int(m_cur->code.size()); }

int32_t AcCompiler::constIdx(const accore::AcJsonValue &v) {
  m_cur->constants.append(v);
  return int32_t(m_cur->constants.size() - 1);
}

int32_t AcCompiler::identOf(const QString &name) {
  for (int i = 0; i < m_module->idents.size(); ++i) {
    if (m_module->idents[i] == name) return i;
  }
  m_module->idents.append(name);
  return int32_t(m_module->idents.size() - 1);
}

void AcCompiler::fillSlots(int breakTarget, int continueTarget, int loopElseTarget) {
  if (m_jumpStack.isEmpty()) return;
  AcJumpSlots &jt = m_jumpStack.last();
  for (int i = 0; i < jt.breakSlots.size(); ++i)
    m_cur->code[jt.breakSlots[i]].a = breakTarget;
  for (int i = 0; i < jt.continueSlots.size(); ++i)
    m_cur->code[jt.continueSlots[i]].a = continueTarget;
  for (int i = 0; i < jt.ifFalseSlots.size(); ++i)
    m_cur->code[jt.ifFalseSlots[i]].a = loopElseTarget;
  for (int i = 0; i < jt.jmpSlots.size(); ++i)
    m_cur->code[jt.jmpSlots[i]].a = breakTarget;
}

// ═════════════════════════════════════════════════════════════════════════════
//  单元生成
// ═════════════════════════════════════════════════════════════════════════════

int AcCompiler::addFuncUnit(const QString &name, const MethodDef &md, bool isMethod) {
  AcFuncUnit unit;
  unit.name = name;
  unit.identId = identOf(name);
  unit.isMethod = isMethod;
  for (const auto &p : md.params) {
    unit.paramNames.append(p.name);
    unit.paramDefaults.append(
        p.defaultValue.isUndefined() ? accore::AcJsonValue()
                                     : accore::AcJsonValue::fromQJsonValue(p.defaultValue));
  }
  m_module->funcs.append(std::move(unit));
  const int idx = int(m_module->funcs.size() - 1);
  m_funcUnits.insert(name, idx);
  m_module->funcUnits.insert(name, idx);
  return idx;
}

void AcCompiler::collectTopLevel(const Block &program) {
  for (const auto &stmt : program.stmts) {
    if (stmt.kind == Block::Stmt::kFuncDef && !stmt.funcDef.isDeclaration) {
      addFuncUnit(stmt.funcDef.name, stmt.funcDef, /*isMethod=*/false);
    } else if (stmt.kind == Block::Stmt::kClassDef && !stmt.classDef.isNative) {
      const ClassDef &cd = stmt.classDef;
      for (const auto &m : cd.methods) {
        if (m.isDeclaration) continue;
        addFuncUnit(QStringLiteral("%1.%2").arg(cd.name, m.name), m, /*isMethod=*/true);
      }
    }
  }
}

bool AcCompiler::compile(const Block &program, AcModule &module) {
  m_module = &module;
  m_error.clear();
  m_jumpStack.clear();
  m_funcUnits.clear();
  m_classInitUnits.clear();
  m_classStaticUnits.clear();
  m_classPropNames.clear();
  m_classNames.clear();
  m_enumValues.clear();

  // 先收集顶层类名与枚举扁平成员名：函数体/方法与顶层语句编译时，
  // m_module->classes 尚未填充（类登记在其后才执行），不能依赖它做静态定位
  for (const auto &stmt : program.stmts) {
    if (stmt.kind == Block::Stmt::kClassDef) {
      m_classNames.insert(stmt.classDef.name);
    } else if (stmt.kind == Block::Stmt::kEnumDef) {
      for (const auto &member : stmt.enumDef.members) {
        m_enumValues.insert(
            QStringLiteral("%1.%2").arg(stmt.enumDef.name, member.name),
            accore::AcJsonValue::fromQJsonValue(member.value));
      }
    }
  }

  // 顶层脚本体 = 单元 0
  AcFuncUnit top;
  top.name = QStringLiteral("<main>");
  module.funcs.append(std::move(top));
  module.entry = 0;

  collectTopLevel(program);

  // 编译函数/方法体（深度 0 = 函数作用域）
  for (const auto &stmt : program.stmts) {
    if (stmt.kind == Block::Stmt::kFuncDef && !stmt.funcDef.isDeclaration) {
      const auto it = m_funcUnits.constFind(stmt.funcDef.name);
      if (it == m_funcUnits.constEnd() || it.value() >= module.funcs.size()) continue;
      m_cur = &module.funcs[it.value()];
      if (!compileBlock(stmt.funcDef.body, 0)) return false;
    } else if (stmt.kind == Block::Stmt::kClassDef && !stmt.classDef.isNative) {
      const ClassDef &cd = stmt.classDef;
      for (const auto &m : cd.methods) {
        if (m.isDeclaration) continue;
        const QString fname = QStringLiteral("%1.%2").arg(cd.name, m.name);
        const auto it = m_funcUnits.constFind(fname);
        if (it == m_funcUnits.constEnd() || it.value() >= module.funcs.size()) continue;
        m_cur = &module.funcs[it.value()];
        if (!compileBlock(m.body, 0)) return false;
      }
    }
  }

  // 类初始化单元（实例/静态）
  for (const auto &stmt : program.stmts) {
    if (stmt.kind != Block::Stmt::kClassDef || stmt.classDef.isNative) continue;
    const ClassDef &cd = stmt.classDef;
    module.classes.insert(cd.name, cd);
    const int initUnit = addClassInitUnit(cd, 0);
    const int staticUnit = addClassStaticUnit(cd, 0);
    m_classInitUnits.insert(cd.name, initUnit);
    m_classStaticUnits.insert(cd.name, staticUnit);
    module.funcUnits.insert(QStringLiteral("<init:%1>").arg(cd.name), initUnit);
    module.funcUnits.insert(QStringLiteral("<static:%1>").arg(cd.name), staticUnit);
    QStringList names;
    if (!cd.baseClass.isEmpty()) {
      const auto baseIt = module.classes.constFind(cd.baseClass);
      if (baseIt != module.classes.constEnd()) {
        for (const auto &p : baseIt.value().properties) {
          if (!p.isStatic) names.append(p.key);
        }
      }
    }
    for (const auto &p : cd.properties) {
      if (!p.isStatic) names.append(p.key);
    }
    m_classPropNames.insert(cd.name, names);
  }

  // 编译顶层脚本体
  m_cur = &module.funcs[0];
  if (!compileBlock(program, 0)) return false;

  return true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  类初始化单元
// ═════════════════════════════════════════════════════════════════════════════

/// 实例初始化单元：按属性序把初始值压栈（继承链基类在前；无值 → NIL）
int AcCompiler::addClassInitUnit(const ClassDef &cd, int depth) {
  AcFuncUnit unit;
  unit.name = QStringLiteral("<init:%1>").arg(cd.name);
  unit.isMethod = false;
  m_module->funcs.append(std::move(unit));
  const int idx = int(m_module->funcs.size() - 1);
  m_cur = &m_module->funcs[idx];

  auto appendProps = [&](const ClassDef &cls, int dep) {
    (void)dep;
    for (const auto &p : cls.properties) {
      if (p.isStatic) continue;
      if (p.value) compileExpr(*p.value);
      else emitInstr(AcOpcode::kNil);
    }
  };
  if (!cd.baseClass.isEmpty()) {
    const auto baseIt = m_module->classes.constFind(cd.baseClass);
    if (baseIt != m_module->classes.constEnd()) appendProps(baseIt.value(), depth);
  }
  appendProps(cd, depth);
  return idx;
}

/// 静态初始化单元：交替压 (key, value)；VM 执行后按序组装静态对象
int AcCompiler::addClassStaticUnit(const ClassDef &cd, int depth) {
  (void)depth;
  AcFuncUnit unit;
  unit.name = QStringLiteral("<static:%1>").arg(cd.name);
  unit.isMethod = false;
  m_module->funcs.append(std::move(unit));
  const int idx = int(m_module->funcs.size() - 1);
  m_cur = &m_module->funcs[idx];

  for (const auto &p : cd.properties) {
    if (!p.isStatic) continue;
    emitInstr(AcOpcode::kConst, constIdx(accore::AcJsonValue(p.key)));
    if (p.value) compileExpr(*p.value);
    else emitInstr(AcOpcode::kNil);
  }
  return idx;
}

// ═════════════════════════════════════════════════════════════════════════════
//  语句编译（深度方案）
// ═════════════════════════════════════════════════════════════════════════════

bool AcCompiler::compileBlock(const Block &block, int depth) {
  for (const auto &stmt : block.stmts) {
    // 语句前同步作用域 + 行号（debug 钩子；声明类语句同样带行号但 VM 跳过 UI 定位）
    emitInstr(AcOpcode::kOpStmt, stmt.loc.line, depth);
    if (!compileStmt(stmt, depth)) return false;
  }
  return true;
}

static const QSet<QString> &stmtKeywords() {
  static const QSet<QString> s = {QStringLiteral("let"),   QStringLiteral("class"),
                                  QStringLiteral("function"), QStringLiteral("if"),
                                  QStringLiteral("for"),   QStringLiteral("while"),
                                  QStringLiteral("return"), QStringLiteral("import"),
                                  QStringLiteral("export"), QStringLiteral("using"),
                                  QStringLiteral("try"),   QStringLiteral("switch")};
  return s;
}

bool AcCompiler::compileStmt(const Block::Stmt &stmt, int depth) {
  switch (stmt.kind) {
    case Block::Stmt::kAssign:
      compileAssignStmt(stmt, depth);
      return true;
    case Block::Stmt::kExpr: {
      const Expr &ex = stmt.exprStmt;
      // —— 语句级自增/自减：对象为标识符变量时必须把更新后的对象写回
      // （AcJsonValue 值拷贝 + 写时分离，COW 分离后变量不会自动观察到改动）
      const Expr *lv = nullptr;
      double delta = 0.0;
      bool isInc = false;
      bool post = false;
      if (ex.kind == Expr::kPostInc || ex.kind == Expr::kPreInc) {
        isInc = true;
        delta = 1.0;
        post = (ex.kind == Expr::kPostInc);
        lv = ex.operand.get();
      } else if (ex.kind == Expr::kPostDec || ex.kind == Expr::kPreDec) {
        isInc = true;
        delta = -1.0;
        post = (ex.kind == Expr::kPostDec);
        lv = ex.operand.get();
      }
      if (isInc && lv) {
        if (lv->kind == Expr::kPropAccess && !lv->propObject && !lv->ident.isEmpty()) {
          if (m_classNames.contains(lv->ident)) {
            // 静态自增：Counter.n++ → 读静态值 + 复合加
            emitInstr(AcOpcode::kLoadStatic, identOf(lv->ident), identOf(lv->prop));
            emitInstr(AcOpcode::kConst, constIdx(accore::AcJsonValue(delta)));
            emitInstr(AcOpcode::kCompoundStatic, identOf(lv->ident), identOf(lv->prop),
                 int32_t(CompoundOp::kAdd), stmt.loc.line);
            return true;
          }
          emitInstr(AcOpcode::kLoadName, identOf(lv->ident));
          emitInstr(AcOpcode::kIncProp, identOf(lv->prop), int32_t(delta), post ? 3 : 2,
               stmt.loc.line);
          emitInstr(AcOpcode::kStoreName, identOf(lv->ident));
          return true;
        }
        if (lv->kind == Expr::kIndexAccess && lv->left && lv->left->kind == Expr::kIdent) {
          emitInstr(AcOpcode::kLoadName, identOf(lv->left->ident));
          compileExpr(*lv->right);
          emitInstr(AcOpcode::kIncIndex, 0, int32_t(delta), post ? 3 : 2, stmt.loc.line);
          emitInstr(AcOpcode::kStoreName, identOf(lv->left->ident));
          return true;
        }
      }
      // —— 语句级数组变异方法调用（a.push(...) 等）：变异后数组需写回变量
      if (ex.kind == Expr::kMethodCall) {
        static const QSet<QString> kMutators = {QStringLiteral("push"), QStringLiteral("pop"),
                                                QStringLiteral("shift"), QStringLiteral("unshift"),
                                                QStringLiteral("sort"), QStringLiteral("fill")};
        const MethodCall &mc = ex.methodCall;
        if (!mc.object && !mc.isOptional &&
            mc.objName != QStringLiteral("JSON") &&
            mc.objName != QString::fromLatin1(AcKeyword::kSuper) &&
            kMutators.contains(mc.methodName)) {
          emitInstr(AcOpcode::kLoadName, identOf(mc.objName));
          for (const auto &arg : mc.args) compileExpr(*arg);
          emitInstr(AcOpcode::kCallMethod, identOf(mc.methodName), int32_t(mc.args.size()),
               /*flags bit4 变异写回*/ 16, stmt.loc.line);
          emitInstr(AcOpcode::kStoreName, identOf(mc.objName));
          return true;
        }
      }
      compileExpr(stmt.exprStmt);
      emitInstr(AcOpcode::kPop);
      return true;
    }
    case Block::Stmt::kCall: {
      // call("cls","fn",args) → FunMgr::call：参数 = 数组表达式 stmt.call.args
      compileExpr(stmt.call.className);
      compileExpr(stmt.call.funcName);
      compileExpr(stmt.call.args);
      emitInstr(AcOpcode::kCallFunc, 0, 0, /*c=1 call形式*/ 1, stmt.loc.line);
      return true;
    }
    case Block::Stmt::kIndexAssign: {
      const auto &ia = stmt.indexAssign;
      compileExpr(ia.objectExpr);
      compileExpr(ia.indexExpr);
      compileExpr(ia.value);
      emitInstr(AcOpcode::kSetIndex, 0, 0, 0, stmt.loc.line);
      // 对象为标识符变量：写回（与解释器 writeBackVar 一致）
      if (ia.objectExpr.kind == Expr::kIdent)
        emitInstr(AcOpcode::kStoreName, identOf(ia.objectExpr.ident));
      return true;
    }
    case Block::Stmt::kPropAssign: {
      const auto &pa = stmt.propAssign;
      // 静态属性赋值：ClassName.prop = value（类名集合在编译早期收集，
      // m_module->classes 此时尚未填充，不可用于方法体内的静态判定）
      if (pa.objectExpr.kind == Expr::kIdent && m_classNames.contains(pa.objectExpr.ident)) {
        compileExpr(pa.value);
        if (pa.compoundOp != CompoundOp::kNone)
          emitInstr(AcOpcode::kCompoundStatic, identOf(pa.objectExpr.ident), identOf(pa.prop),
               int32_t(pa.compoundOp));
        else
          emitInstr(AcOpcode::kStoreStatic, identOf(pa.objectExpr.ident), identOf(pa.prop));
        return true;
      }
      compileExpr(pa.objectExpr);
      if (pa.compoundOp != CompoundOp::kNone) {
        compileExpr(pa.value);
        emitInstr(AcOpcode::kCompoundProp, identOf(pa.prop), int32_t(pa.compoundOp));
        emitInstr(AcOpcode::kPop);  // 丢弃运算结果（语句级不返回）
      } else {
        compileExpr(pa.value);
        emitInstr(AcOpcode::kSetProp, identOf(pa.prop));
      }
      if (pa.objectExpr.kind == Expr::kIdent)
        emitInstr(AcOpcode::kStoreName, identOf(pa.objectExpr.ident));
      return true;
    }
    case Block::Stmt::kFor:
      compileForStmt(stmt, depth);
      return true;
    case Block::Stmt::kIf:
      compileIfStmt(stmt, depth);
      return true;
    case Block::Stmt::kWhile:
      compileWhileStmt(stmt, depth);
      return true;
    case Block::Stmt::kSwitch:
      compileSwitchStmt(stmt, depth);
      return true;
    case Block::Stmt::kTry:
      compileTryStmt(stmt, depth);
      return true;
    case Block::Stmt::kUsing: {
      const auto &us = stmt.usingStmt;
      compileExpr(*us.value);
      emitInstr(AcOpcode::kDeclareUsing, identOf(us.varName), 0, 0, stmt.loc.line);
      return true;
    }
    case Block::Stmt::kBreak: {
      emitInstr(AcOpcode::kJmp, 0);  // 占位：0 表示未回填
      if (!m_jumpStack.isEmpty()) m_jumpStack.last().breakSlots.append(int(m_cur->code.size()) - 1);
      return true;
    }
    case Block::Stmt::kContinue: {
      emitInstr(AcOpcode::kJmp, 0);
      if (!m_jumpStack.isEmpty())
        m_jumpStack.last().continueSlots.append(int(m_cur->code.size()) - 1);
      return true;
    }
    case Block::Stmt::kReturn: {
      compileExpr(stmt.returnValue);
      emitInstr(AcOpcode::kRet, 0, 0, 0, stmt.loc.line);
      return true;
    }
    case Block::Stmt::kBlock:
      return compileBlock(stmt.blockBody, depth + 1);
    case Block::Stmt::kClassDef:
      return true;  // 单元已收集
    case Block::Stmt::kInterfaceDef:
      return true;
    case Block::Stmt::kEnumDef: {
      for (const auto &member : stmt.enumDef.members) {
        const QString varName = QStringLiteral("%1.%2").arg(stmt.enumDef.name, member.name);
        // 枚举成员值由 parser 已按声明确定（未显式指定时自动递增）
        accore::AcJsonValue v = accore::AcJsonValue::fromQJsonValue(member.value);
        emitInstr(AcOpcode::kConst, constIdx(v));
        emitInstr(AcOpcode::kStoreName, identOf(varName));
      }
      return true;
    }
    case Block::Stmt::kFuncDef:
      return true;  // 已预注册
    case Block::Stmt::kImport:
      return true;  // 已由链接阶段处理
    case Block::Stmt::kThrow: {
      compileExpr(stmt.returnValue);
      emitInstr(AcOpcode::kThrowValue, 0, 0, 0, stmt.loc.line);
      return true;
    }
  }
  return false;
}

void AcCompiler::compileAssignStmt(const Block::Stmt &s, int depth) {
  (void)depth;
  const AssignStmt &as = s.assign;
  if (as.compoundOp != CompoundOp::kNone) {
    compileExpr(as.value);
    if (as.isStatic) {
      emitInstr(AcOpcode::kCompoundStatic, identOf(as.staticClassName), identOf(as.name),
           int32_t(as.compoundOp), s.loc.line);
    } else if (!as.thisProp.isEmpty()) {
      emitInstr(AcOpcode::kCompoundThisProp, identOf(as.thisProp), int32_t(as.compoundOp), 0,
           s.loc.line);
    } else {
      emitInstr(AcOpcode::kCompoundName, identOf(as.name), int32_t(as.compoundOp), 0, s.loc.line);
    }
    return;
  }
  compileExpr(as.value);
  if (as.isStatic) {
    emitInstr(AcOpcode::kStoreStatic, identOf(as.staticClassName), identOf(as.name));
  } else if (!as.thisProp.isEmpty()) {
    emitInstr(AcOpcode::kStoreThisProp, identOf(as.thisProp));
  } else if (as.isDeclaration) {
    emitInstr(AcOpcode::kDeclareName, identOf(as.name), as.isConst ? 1 : 0);
  } else {
    emitInstr(AcOpcode::kStoreName, identOf(as.name));
  }
}

void AcCompiler::compileForStmt(const Block::Stmt &s, int depth) {
  const ForStmt &fs = s.forStmt;
  if (fs.isStandard) {
    // 循环整体作用域 = depth+1；init/cond/body/update 都在其内
    AcJumpSlots jt;
    m_jumpStack.append(jt);
    if (!compileBlock(fs.initBlock, depth + 1)) return;
    const int condAddr = emitHere();
    compileExpr(fs.condition);
    emitInstr(AcOpcode::kJmpIfF, 0);
    m_jumpStack.last().ifFalseSlots.append(int(m_cur->code.size()) - 1);
    if (!compileBlock(fs.body, depth + 1)) return;
    const int updateAddr = emitHere();  // continue 目标
    compileExpr(fs.updateExpr);
    emitInstr(AcOpcode::kPop);
    emitInstr(AcOpcode::kJmp, condAddr);
    const int breakAddr = emitHere();
    const int continueTarget = updateAddr;
    fillSlots(breakAddr, continueTarget, breakAddr);
    m_jumpStack.pop_back();
    return;
  }
  // for-in：数组在此求值（循环 scope 外，depth）
  AcJumpSlots jt;
  m_jumpStack.append(jt);
  compileExpr(fs.arrayExpr);
  emitInstr(AcOpcode::kForInInit);
  const int loopCtl = emitHere();  // continue 目标
  emitInstr(AcOpcode::kForInNext, identOf(fs.varName), 0, 0, s.loc.line);
  const int nextSlotIdx = int(m_cur->code.size()) - 1;
  // 进入每轮作用域：虚拟 kOpStmt(depth+1) 让 VM push 一层
  emitInstr(AcOpcode::kOpStmt, 0, depth + 1);
  emitInstr(AcOpcode::kDeclareIterVar, identOf(fs.varName), 0, 0, s.loc.line);
  if (!compileBlock(fs.body, depth + 1)) return;
  emitInstr(AcOpcode::kJmp, loopCtl);
  const int noMore = emitHere();
  m_cur->code[nextSlotIdx].b = noMore;  // kForInNext 无元素跳这里
  // break 目标 = noMore（后续语句 kOpStmt(depth) 自动 pop 循环作用域）
  fillSlots(noMore, loopCtl);
  m_jumpStack.pop_back();
}

void AcCompiler::compileIfStmt(const Block::Stmt &s, int depth) {
  const IfStmt &is = s.ifStmt;
  compileExpr(is.condition);
  emitInstr(AcOpcode::kJmpIfF, 0);
  const int condIfF = int(m_cur->code.size()) - 1;
  compileBlock(is.thenBlock, depth + 1);
  emitInstr(AcOpcode::kJmp, 0);
  const int thenJmp = int(m_cur->code.size()) - 1;
  const int elseAddr = emitHere();
  m_cur->code[condIfF].a = elseAddr;
  if (is.elseIfBranch) {
    // else if 链：以嵌套 if 编译（共享深度）
    Block::Stmt nested;
    nested.kind = Block::Stmt::kIf;
    nested.ifStmt = *is.elseIfBranch;
    nested.loc = is.elseIfBranch->condition.loc;
    compileIfStmt(nested, depth);
  } else if (is.hasElse) {
    compileBlock(is.elseBlock, depth + 1);
  }
  const int endAddr = emitHere();
  m_cur->code[thenJmp].a = endAddr;
}

void AcCompiler::compileWhileStmt(const Block::Stmt &s, int depth) {
  const WhileStmt &ws = s.whileStmt;
  if (ws.isDoWhile) {
    AcJumpSlots jt;
    m_jumpStack.append(jt);
    const int bodyStart = emitHere();
    if (!compileBlock(ws.body, depth + 1)) return;
    const int condAddr = emitHere();  // continue 目标
    emitInstr(AcOpcode::kOpStmt, 0, depth + 1);  // 同步循环作用域
    compileExpr(ws.condition);
    emitInstr(AcOpcode::kJmpIfT, bodyStart);
    const int breakAddr = emitHere();
    fillSlots(breakAddr, condAddr);
    m_jumpStack.pop_back();
    return;
  }
  AcJumpSlots jt;
  m_jumpStack.append(jt);
  const int condAddr = emitHere();  // continue 目标
  emitInstr(AcOpcode::kOpStmt, 0, depth + 1);  // 同步循环作用域
  compileExpr(ws.condition);
  emitInstr(AcOpcode::kJmpIfF, 0);
  m_jumpStack.last().ifFalseSlots.append(int(m_cur->code.size()) - 1);
  if (!compileBlock(ws.body, depth + 1)) return;
  emitInstr(AcOpcode::kJmp, condAddr);
  const int breakAddr = emitHere();
  fillSlots(breakAddr, condAddr, breakAddr);
  m_jumpStack.pop_back();
}

void AcCompiler::compileSwitchStmt(const Block::Stmt &s, int depth) {
  const SwitchStmt &ss = s.switchStmt;
  // switch 语义：比较 switchVal 与各 case → 命中跳对应 body；default 兜底；无命中跳过。
  // fall-through：解释器在一个 case body 结束后（无 break）继续执行后续 case ——
  // 字节码按 case 顺序连续排布 body，命中跳入后自然流到后续 body。
  AcJumpSlots jt;
  m_jumpStack.append(jt);
  compileExpr(ss.expr);      // [switchVal]
  QVector<int> matchJumps;   // 每个非 default case 的 kJmpIfT 占位
  bool hasDefault = false;
  for (const auto &sc : ss.cases) {
    if (sc.isDefault) {
      hasDefault = true;
      continue;
    }
    emitInstr(AcOpcode::kDup);  // [switchVal, switchVal]
    compileExpr(sc.value);      // [switchVal, switchVal, caseVal]
    emitInstr(AcOpcode::kEq);   // [switchVal, bool]（kEq 消耗副本与 caseVal）
    emitInstr(AcOpcode::kJmpIfT, 0);
    matchJumps.append(int(m_cur->code.size()) - 1);
    // 未命中时（kJmpIfT 不跳）栈保持 [switchVal]，供下一轮比较
  }
  emitInstr(AcOpcode::kPop);  // 弹出剩余的 switchVal（命中检测结束）
  emitInstr(AcOpcode::kJmp, 0);  // 未命中 → default 或结束
  const int fallbackJmp = int(m_cur->code.size()) - 1;

  QVector<int> bodyAddrs;
  int defaultBodyAddr = -1;
  for (const auto &sc : ss.cases) {
    const int addr = emitHere();
    bodyAddrs.append(addr);
    if (sc.isDefault) defaultBodyAddr = addr;
    if (!compileBlock(sc.body, depth + 1)) return;
  }
  const int switchEnd = emitHere();
  // 回填
  for (int i = 0; i < matchJumps.size(); ++i) {
    int bodyIdx = 0;
    int nonDefault = -1;
    for (int j = 0; j < ss.cases.size(); ++j) {
      if (!ss.cases[j].isDefault) {
        ++nonDefault;
        if (nonDefault == i) {
          bodyIdx = j;
          break;
        }
      }
    }
    m_cur->code[matchJumps[i]].a = bodyAddrs[bodyIdx];
  }
  m_cur->code[fallbackJmp].a = hasDefault && defaultBodyAddr >= 0 ? defaultBodyAddr : switchEnd;
  // break 目标 = switchEnd（每条 case body 首部 kOpStmt(depth+1) 已同步作用域,
  // break 跳 switchEnd 后下一条语句 kOpStmt(depth) 自动 pop）
  fillSlots(switchEnd, switchEnd);
  m_jumpStack.pop_back();
}

void AcCompiler::compileTryStmt(const Block::Stmt &s, int depth) {
  const TryStmt &ts = s.tryStmt;
  const int tryIdx = int(m_cur->tryTable.size());
  AcTryEntry entry;
  entry.catchVar = ts.catchVar;
  m_cur->tryTable.append(entry);

  // 头：同步离开 try 的跳转占位（try 正常结束跳 finally/after）
  emitInstr(AcOpcode::kEnterTry, tryIdx);
  const int tryStart = emitHere();
  if (!compileBlock(ts.tryBody, depth + 1)) return;
  const int tryEnd = emitHere();
  emitInstr(AcOpcode::kLeaveTry, tryIdx);
  emitInstr(AcOpcode::kJmp, 0);            // 正常结束 → “离开”占位（finally/after）
  const int afterTryJmp = int(m_cur->code.size()) - 1;

  const int catchAddr = emitHere();
  emitInstr(AcOpcode::kOpStmt, 0, depth + 1);  // 同步 catch 作用域
  emitInstr(AcOpcode::kCatchBegin, tryIdx);
  if (!compileBlock(ts.catchBody, depth + 1)) return;
  emitInstr(AcOpcode::kCatchEnd, tryIdx);
  emitInstr(AcOpcode::kJmp, 0);            // catch 完成 → 离开占位
  const int afterCatchJmp = int(m_cur->code.size()) - 1;

  const int finallyAddr = emitHere();
  emitInstr(AcOpcode::kOpStmt, 0, depth + 1);  // 同步 finally 作用域
  emitInstr(AcOpcode::kFinallyBegin, tryIdx);
  if (!compileBlock(ts.finallyBody, depth + 1)) return;
  emitInstr(AcOpcode::kFinallyEnd, tryIdx);

  const int afterAddr = emitHere();
  m_cur->code[afterTryJmp].a = ts.hasFinally ? finallyAddr : afterAddr;
  m_cur->code[afterCatchJmp].a = ts.hasFinally ? finallyAddr : afterAddr;

  m_cur->tryTable[tryIdx].tryStart = tryStart;
  m_cur->tryTable[tryIdx].tryEnd = tryEnd;
  m_cur->tryTable[tryIdx].catchAddr = ts.hasCatch ? catchAddr : -1;
  m_cur->tryTable[tryIdx].finallyAddr = ts.hasFinally ? finallyAddr : -1;
}

// ═════════════════════════════════════════════════════════════════════════════
//  表达式编译
// ═════════════════════════════════════════════════════════════════════════════

void AcCompiler::compileExpr(const Expr &e) {
  switch (e.kind) {
    case Expr::kError:
      emitInstr(AcOpcode::kNil, 0, 0, 0, e.loc.line);  // 残缺占位：编译为空值（正常管线不会到达）
      return;
    case Expr::kNull:
    case Expr::kUndefined:
      emitInstr(AcOpcode::kNil, 0, 0, 0, e.loc.line);
      return;
    case Expr::kBool:
      emitInstr(e.boolVal ? AcOpcode::kTrue : AcOpcode::kFalse, 0, 0, 0, e.loc.line);
      return;
    case Expr::kNumber:
      emitInstr(AcOpcode::kConst, constIdx(accore::AcJsonValue(e.numVal)), 0, 0, e.loc.line);
      return;
    case Expr::kString:
      emitInstr(AcOpcode::kConst, constIdx(accore::AcJsonValue(e.strVal)), 0, 0, e.loc.line);
      return;
    case Expr::kThis:
      emitInstr(AcOpcode::kLoadThis, 0, 0, 0, e.loc.line);
      return;
    case Expr::kIdent:
      emitInstr(AcOpcode::kLoadName, identOf(e.ident), 0, 0, e.loc.line);
      return;
    case Expr::kPropAccess:
      compilePropChain(e);
      return;
    case Expr::kIndexAccess:
      compileExpr(*e.left);
      compileExpr(*e.right);
      emitInstr(AcOpcode::kGetIndex, 0, e.isOptional ? 1 : 0, 0, e.loc.line);
      return;
    case Expr::kObject: {
      QVector<int32_t> keyIdx;
      for (const auto &entry : e.objEntries) keyIdx.append(constIdx(accore::AcJsonValue(entry.key)));
      for (const auto &entry : e.objEntries) compileExpr(*entry.value);
      emitInstr(AcOpcode::kNewObject, int32_t(e.objEntries.size()),
           keyIdx.isEmpty() ? 0 : keyIdx.first(), 0, e.loc.line);
      return;
    }
    case Expr::kArray:
      for (const auto &item : e.arrItems) compileExpr(*item);
      emitInstr(AcOpcode::kNewArray, int32_t(e.arrItems.size()), 0, 0, e.loc.line);
      return;
    case Expr::kCoalesce: {
      compileExpr(*e.left);
      emitInstr(AcOpcode::kDup);
      emitInstr(AcOpcode::kNullJmpT, 0);
      const int njt = int(m_cur->code.size()) - 1;
      emitInstr(AcOpcode::kPop);
      compileExpr(*e.right);
      const int endAddr = emitHere();
      m_cur->code[njt].a = endAddr;
      return;
    }
    case Expr::kFuncCall: {
      for (const auto &arg : e.funcCall.args) compileExpr(*arg);
      emitInstr(AcOpcode::kCallFunc, identOf(e.funcCall.name), int32_t(e.funcCall.args.size()), 0,
           e.loc.line);
      return;
    }
    case Expr::kFuncExpr:
      compileFuncExprInner(e);
      return;
    case Expr::kMethodCall:
      compileMethodCall(e);
      return;
    case Expr::kNewInstance:
      compileNew(e);
      return;
    case Expr::kStaticAccess:
      compileStaticAccess(e);
      return;
    case Expr::kBinary:
      compileBinary(e);
      return;
    case Expr::kUnary:
      compileExpr(*e.operand);
      emitInstr(AcOpcode::kNot, 0, 0, 0, e.loc.line);
      return;
    case Expr::kPreInc:
      compileIncDec(e, +1.0, false);
      return;
    case Expr::kPreDec:
      compileIncDec(e, -1.0, false);
      return;
    case Expr::kPostInc:
      compileIncDec(e, +1.0, true);
      return;
    case Expr::kPostDec:
      compileIncDec(e, -1.0, true);
      return;
    case Expr::kAssign:
      compileAssignExpr(e);
      return;
    case Expr::kTernary: {
      compileExpr(*e.left);
      emitInstr(AcOpcode::kJmpIfF, 0);
      const int cIf = int(m_cur->code.size()) - 1;
      compileExpr(*e.right);
      emitInstr(AcOpcode::kJmp, 0);
      const int jEnd = int(m_cur->code.size()) - 1;
      const int falseAddr = emitHere();
      m_cur->code[cIf].a = falseAddr;
      compileExpr(*e.operand);
      const int endAddr = emitHere();
      m_cur->code[jEnd].a = endAddr;
      return;
    }
  }
}

void AcCompiler::compileFuncExprInner(const Expr &e) {
  static int lambdaSeq = 0;
  const QString name = QStringLiteral("__lambda_%1").arg(++lambdaSeq);
  AcFuncUnit unit;
  unit.name = name;
  unit.identId = identOf(name);
  unit.isMethod = false;
  for (const auto &p : e.funcExpr.params) {
    unit.paramNames.append(p.name);
    unit.paramDefaults.append(
        p.defaultValue.isUndefined() ? accore::AcJsonValue()
                                     : accore::AcJsonValue::fromQJsonValue(p.defaultValue));
  }
  // 记录当前单元下标后再追加单元：funcs 扩容会使既有指针失效（悬垂），
  // 后续必须按下标重新取指针恢复 m_cur
  const int outerIdx = int(m_cur - m_module->funcs.constData());
  m_module->funcs.append(std::move(unit));
  const int idx = int(m_module->funcs.size() - 1);
  m_module->funcUnits.insert(name, idx);
  m_cur = &m_module->funcs[idx];
  compileBlock(e.funcExpr.body, 0);
  m_cur = &m_module->funcs[outerIdx];
  emitInstr(AcOpcode::kFuncExpr, idx, 0, 0, e.loc.line);
}

void AcCompiler::compilePropChain(const Expr &e) {
  if (e.propObject) {
    compileExpr(*e.propObject);
  } else if (!e.ident.isEmpty()) {
    // 类名根的属性读取 → 静态成员（如 Counter.n）
    if (m_classNames.contains(e.ident)) {
      emitInstr(AcOpcode::kLoadStatic, identOf(e.ident), identOf(e.prop), 0, e.loc.line);
      return;
    }
    // 枚举成员（Color.Blue）：变异体上枚举以扁平名注册，编译期直接解析为常量
    const auto enIt = m_enumValues.constFind(QStringLiteral("%1.%2").arg(e.ident, e.prop));
    if (enIt != m_enumValues.constEnd()) {
      emitInstr(AcOpcode::kConst, constIdx(enIt.value()), 0, 0, e.loc.line);
      return;
    }
    emitInstr(AcOpcode::kLoadName, identOf(e.ident), 0, 0, e.loc.line);
  } else {
    emitInstr(AcOpcode::kLoadThis, 0, 0, 0, e.loc.line);
  }
  emitInstr(AcOpcode::kGetProp, identOf(e.prop), e.isOptional ? 1 : 0, 0, e.loc.line);
}

void AcCompiler::compileBinary(const Expr &e) {
  const Expr::BinaryOp op = e.binOp;
  if (op == Expr::kOr || op == Expr::kAnd) {
    if (op == Expr::kOr) {
      compileExpr(*e.left);
      emitInstr(AcOpcode::kDup);
      emitInstr(AcOpcode::kJmpIfT, 0);
      const int jt = int(m_cur->code.size()) - 1;
      emitInstr(AcOpcode::kPop);
      compileExpr(*e.right);
      const int endAddr = emitHere();
      m_cur->code[jt].a = endAddr;
    } else {
      compileExpr(*e.left);
      emitInstr(AcOpcode::kDup);
      emitInstr(AcOpcode::kJmpIfF, 0);
      const int jf = int(m_cur->code.size()) - 1;
      emitInstr(AcOpcode::kPop);
      compileExpr(*e.right);
      const int endAddr = emitHere();
      m_cur->code[jf].a = endAddr;
    }
    return;
  }
  compileExpr(*e.left);
  compileExpr(*e.right);
  switch (op) {
    case Expr::kAdd: emitInstr(AcOpcode::kAdd, 0, 0, 0, e.loc.line); break;
    case Expr::kSub: emitInstr(AcOpcode::kSub, 0, 0, 0, e.loc.line); break;
    case Expr::kMul: emitInstr(AcOpcode::kMul, 0, 0, 0, e.loc.line); break;
    case Expr::kDiv: emitInstr(AcOpcode::kDiv, 0, 0, 0, e.loc.line); break;
    case Expr::kMod: emitInstr(AcOpcode::kMod, 0, 0, 0, e.loc.line); break;
    case Expr::kEq: emitInstr(AcOpcode::kEq, 0, 0, 0, e.loc.line); break;
    case Expr::kNeq: emitInstr(AcOpcode::kNeq, 0, 0, 0, e.loc.line); break;
    case Expr::kLt: emitInstr(AcOpcode::kLt, 0, 0, 0, e.loc.line); break;
    case Expr::kGt: emitInstr(AcOpcode::kGt, 0, 0, 0, e.loc.line); break;
    case Expr::kLte: emitInstr(AcOpcode::kLte, 0, 0, 0, e.loc.line); break;
    case Expr::kGte: emitInstr(AcOpcode::kGte, 0, 0, 0, e.loc.line); break;
    default: break;
  }
}

void AcCompiler::compileMethodCall(const Expr &e) {
  const MethodCall &mc = e.methodCall;
  const bool chained = (mc.object != nullptr);
  const bool isSuper = (mc.objName == QString::fromLatin1(AcKeyword::kSuper));

  if (!chained && !isSuper && mc.objName == QStringLiteral("JSON")) {
    for (const auto &arg : mc.args) compileExpr(*arg);
    emitInstr(AcOpcode::kCallMethod, identOf(mc.methodName), int32_t(mc.args.size()),
         /*flags bit2 JSON*/ 4, e.loc.line);
    return;
  }
  if (chained) compileExpr(*mc.object);
  else if (isSuper) emitInstr(AcOpcode::kLoadThis);
  else emitInstr(AcOpcode::kLoadName, identOf(mc.objName));
  for (const auto &arg : mc.args) compileExpr(*arg);
  int32_t flags = 0;
  if (chained) flags |= 1;
  if (mc.isOptional) flags |= 2;
  if (isSuper) flags |= 8;
  emitInstr(AcOpcode::kCallMethod, identOf(mc.methodName), int32_t(mc.args.size()), flags, e.loc.line);
}

void AcCompiler::compileNew(const Expr &e) {
  for (const auto &arg : e.constructorArgs) compileExpr(*arg);
  emitInstr(AcOpcode::kNew, identOf(e.className), int32_t(e.constructorArgs.size()), 0, e.loc.line);
}

void AcCompiler::compileStaticAccess(const Expr &e) {
  for (const auto &arg : e.funcCall.args) compileExpr(*arg);
  emitInstr(AcOpcode::kCallStatic, identOf(e.className), identOf(e.prop),
       int32_t(e.funcCall.args.size()), e.loc.line);
}

void AcCompiler::compileIncDec(const Expr &e, double delta, bool post) {
  const Expr &lv = *e.operand;
  if (lv.kind == Expr::kIdent) {
    emitInstr(AcOpcode::kIncName, identOf(lv.ident), int32_t(delta), post ? 1 : 0, e.loc.line);
  } else if (lv.kind == Expr::kPropAccess) {
    if (lv.propObject) compileExpr(*lv.propObject);
    else if (!lv.ident.isEmpty()) emitInstr(AcOpcode::kLoadName, identOf(lv.ident));
    else emitInstr(AcOpcode::kLoadThis);
    emitInstr(AcOpcode::kIncProp, identOf(lv.prop), int32_t(delta), post ? 1 : 0, e.loc.line);
  } else if (lv.kind == Expr::kIndexAccess && lv.left->kind == Expr::kIdent) {
    emitInstr(AcOpcode::kLoadName, identOf(lv.left->ident));
    compileExpr(*lv.right);
    emitInstr(AcOpcode::kIncIndex, 0, int32_t(delta), post ? 1 : 0, e.loc.line);
  } else {
    emitInstr(AcOpcode::kNil);  // 不支持的目标静默（v1）
  }
}

void AcCompiler::compileAssignExpr(const Expr &e) {
  const Expr &lv = *e.left;
  compileExpr(*e.right);  // [val]
  // 赋值表达式结果 = 右值（留在栈顶供外层使用）：存储前复制一份
  emitInstr(AcOpcode::kDup);  // [val, val]
  if (lv.kind == Expr::kIdent) {
    emitInstr(AcOpcode::kStoreName, identOf(lv.ident));  // pop 副本 → [val]
  } else if (lv.kind == Expr::kPropAccess && !lv.propObject && !lv.ident.isEmpty()) {
    emitInstr(AcOpcode::kLoadName, identOf(lv.ident));  // [val, val, obj]
    emitInstr(AcOpcode::kSwap);                          // [val, obj, val]
    emitInstr(AcOpcode::kSetProp, identOf(lv.prop));     // → [val, obj']
    emitInstr(AcOpcode::kPop);                           // → [val]
  } else if (lv.kind == Expr::kIndexAccess && lv.left->kind == Expr::kIdent) {
    emitInstr(AcOpcode::kLoadName, identOf(lv.left->ident));  // [val, val, obj]
    compileExpr(*lv.right);                                  // [val, val, obj, idx]
    emitInstr(AcOpcode::kSwap3);                              // [val, idx, val, obj]
    emitInstr(AcOpcode::kSwap);                               // [val, obj, idx, val]
    emitInstr(AcOpcode::kSetIndex, 0, 0, 0, e.loc.line);      // → [val, obj']
    emitInstr(AcOpcode::kPop);                                // → [val]
  } else {
    emitInstr(AcOpcode::kPop);  // 不支持的目标：丢弃副本，保留右值
  }
}