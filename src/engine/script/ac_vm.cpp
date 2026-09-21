/**
 * @file ac_vm.cpp
 * @brief 字节码虚拟机实现 — 栈式执行 AcModule
 *
 * 语义镜像《ac_interpreter*.cpp》：
 * - 同一套作用域链哈希 + 引用计数 + 标记清扫（processDestructInfo/collectCycles）
 * - 同一套调用语义（参数默认值、this 绑定、递归深度上限 512）
 * - 同一套 try/catch 错误通道（m_error 在指令边界检测）
 */

#include "ac_vm.h"

#include <QDir>
#include <QFileInfo>
#include <cstdio>
#include <cmath>

#include "../ac_language.h"
#include "../ac_value_str.h"
#include "../function/fun_builtin.h"
#include "../function/fun_mgr.h"
#include "ac_builtin_loader.h"
#include "ac_interpreter.h"
#include "src/core/json/ac_json_value.h"

// 错误消息常量与解释器一致（ac_interpreter_expr.cpp 定义了同名单对象）
static const QString kVmErrUndefinedVariable = QStringLiteral("undefined variable '%1'");
static const QString kVmErrUndefinedClass = QStringLiteral("undefined class '%1'");

// ═════════════════════════════════════════════════════════════════════════════
//  入口
// ═════════════════════════════════════════════════════════════════════════════

accore::AcJsonValue AcVm::run(AcModule &module, QString &error) {
  m_module = &module;
  m_error.clear();
  m_vstack.clear();
  m_scopeStack.clear();
  m_usingStack.clear();
  m_staticVars.clear();
  m_staticInited.clear();
  m_classes.clear();
  m_frames.clear();
  m_iters.clear();
  m_tryStack.clear();
  m_currentThis = accore::AcJsonValue();
  m_modifiedThis = accore::AcJsonValue();
  m_generatedFiles.clear();
  m_callDepth = 0;
  m_objectsAtLastGc = 0;

  // 类表 + 顶层帧
  m_classes = module.classes;
  AcBuiltinLoader::registerNativeClasses(m_classes);
  FunMgr::init();
  FunBuiltin::setContext({m_scriptDir, m_rootDir, m_logCallback, &m_generatedFiles, 0});

  pushScope();  // 全局作用域（顶层语句深度 0 = 该层）

  // 顶层枚举/静态类初始化由指令执行；执行顶层单元
  callUnit(module.entry, 0, QStringLiteral("<main>"));
  if (m_callDepth > 0) {
    // 顶层单元以 kRet 终止（或隐式结束）：隐式结束时不弹帧 —— callUnit 已处理
  }

  // 收尾清理（与解释器 execute 相同：脚本执行完毕后弹空所有作用域）
  while (!m_scopeStack.isEmpty()) popScope();

  FunMgr::cleanup();
  if (!m_error.isEmpty()) {
    error = m_error;
    m_objMgr.cleanup();
    return accore::AcJsonValue();
  }
  error.clear();
  return m_retSlot;
}

// ═════════════════════════════════════════════════════════════════════════════
//  帧 / 函数调用
// ═════════════════════════════════════════════════════════════════════════════

/// 建立函数帧并执行单元，结果写入 m_retSlot
void AcVm::callUnit(int unitIdx, int argc, const QString &funcName) {
  constexpr int kMaxCallDepth = 512;
  if (m_callDepth >= kMaxCallDepth) {
    setError(QStringLiteral("调用栈溢出（递归过深，超过 %1 层）：函数 %2")
                 .arg(kMaxCallDepth)
                 .arg(funcName),
             0);
    return;
  }
  ++m_callDepth;

  AcFuncUnit &unit = m_module->funcs[unitIdx];

  // 声明参数（调用者已把实参压栈；栈深 ≥ argc）
  // 约定：调用方按自然顺序压栈（[a0, a1, ..., aN-1]，aN-1 在栈顶）；
  // takeLast 先弹栈顶 → 放参数末位，依次得到正序参数表
  QVector<accore::AcJsonValue> args;
  args.resize(argc);
  for (int i = 0; i < argc; ++i) args[argc - 1 - i] = m_vstack.takeLast();

  pushScope();
  // 槽位初始化（A2）：帧槽位数组按单元槽数分配，参数槽 = 实参/默认值/null
  Frame f;
  f.unitIdx = unitIdx;
  f.pc = 0;
  f.slotVals.resize(unit.numLocals > 0 ? unit.numLocals : 0);
  f.funcName = funcName;
  f.returned = false;
  for (int i = 0; i < unit.paramNames.size(); ++i) {
    accore::AcJsonValue v;
    if (i < args.size()) {
      v = args[i];
    } else if (i < unit.paramDefaults.size() && !unit.paramDefaults[i].isNull()) {
      v = unit.paramDefaults[i];
    } else {
      v = accore::AcJsonValue();
    }
    if (i < f.slotVals.size()) f.slotVals[i] = v;  // 参数字槽
    declareVar(unit.paramNames[i], v);       // 作用域声明（动态回退/闭包查找用）
  }

  // 帧作用域深度 = 参数声明 pushScope 之后的 scopeStack 大小：
  // 函数内 depth0 语句的绝对深度 = baseScope，kOpStmt 目标 = baseScope + depth
  f.baseScope = m_scopeStack.size();
  m_frames.append(f);

  executeUnit(unitIdx);

  // 收尾：弹到帧基线（函数作用域 + 帧内未平衡块）+ 结果
  Frame &finished = m_frames.last();
  m_retSlot = finished.retVal;
  while (m_scopeStack.size() > f.baseScope - 1) popScope();
  m_frames.pop_back();
  --m_callDepth;
}

// ═════════════════════════════════════════════════════════════════════════════
//  主循环
// ═════════════════════════════════════════════════════════════════════════════

void AcVm::executeUnit(int unitIdx) {
  AcFuncUnit &unit = m_module->funcs[unitIdx];
  const int n = int(unit.code.size());

  // 步数上限：防止跳转回填错误导致的死循环（对拍诊断用）
  int64_t steps = 0;
  constexpr int64_t kMaxSteps = 5000000;

  // 注意：不能在循环外持有 m_frames.last().pc 的引用——dispatch 内的嵌套
  // callUnit 会追加帧使 QVector 扩容，旧引用悬垂（写入失效内存 → pc 回环）。
  // 因此每次迭代都按 读 pc → 写回 ++ → dispatch（内部可覆写 pc） 重新同步。
  while (!m_frames.isEmpty() && m_frames.last().unitIdx == unitIdx) {
    const int pc = m_frames.last().pc;
    if (pc >= n) return;
    if (++steps > kMaxSteps) {
      setError(QStringLiteral("vm: runaway loop in unit '%1' (pc=%2/%3)")
                   .arg(unit.name)
                   .arg(pc)
                   .arg(n),
               unit.code[pc].line);
      return;
    }
    const AcInstr &ins = unit.code[pc];
    if (m_trace) {
      std::printf("[vm-trace] unit=%s pc=%d op=%d a=%d b=%d c=%d line=%d\n",
                  unit.name.toUtf8().constData(), pc, int(ins.op), ins.a, ins.b, ins.c, ins.line);
      std::fflush(stdout);
    }
    m_frames.last().pc = pc + 1;
    dispatch(ins, unitIdx);
    if (m_frames.isEmpty() || m_frames.last().unitIdx != unitIdx) return;  // 帧被弹出
    if (!m_error.isEmpty()) {
      handleError(unitIdx, m_frames.last().pc);
      if (!m_error.isEmpty()) return;  // 未捕获错误：终止当前单元
    }
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  指令分派
// ═════════════════════════════════════════════════════════════════════════════

void AcVm::dispatch(const AcInstr &ins, int unitIdx) {
  (void)unitIdx;
  AcFuncUnit &unit = m_module->funcs[unitIdx];
  int &pc = m_frames.last().pc;
  const auto ident = [&](int32_t id) -> const QString & { return m_module->idents[id]; };
  auto &st = m_vstack;

  switch (ins.op) {
    case AcOpcode::kNil: st.append(accore::AcJsonValue()); break;
    case AcOpcode::kTrue: st.append(accore::AcJsonValue(true)); break;
    case AcOpcode::kFalse: st.append(accore::AcJsonValue(false)); break;
    case AcOpcode::kConst: st.append(unit.constants[ins.a]); break;
    case AcOpcode::kLoadName: {
      const QString &name = ident(ins.a);
      if (name == QString::fromLatin1(AcKeyword::kThis) ||
          name == QString::fromLatin1(AcKeyword::kSuper)) {
        st.append(m_currentThis);
        break;
      }
      if (!containsVar(name)) {
        if (m_classes.contains(name)) {
          st.append(accore::AcJsonValue::makeClassRef(name));
          break;
        }
        setError(kVmErrUndefinedVariable.arg(name), ins.line);
        break;
      }
      st.append(resolveVar(name));
      break;
    }
    case AcOpcode::kStoreName: {
      accore::AcJsonValue v = st.takeLast();
      setVar(ident(ins.a), v);
      break;
    }
    case AcOpcode::kDeclareName: {
      accore::AcJsonValue v = st.takeLast();
      declareVar(ident(ins.a), v, ins.b != 0);
      break;
    }
    case AcOpcode::kCompoundName: {
      accore::AcJsonValue newV = st.takeLast();
      accore::AcJsonValue cur = resolveVar(ident(ins.a));
      accore::AcJsonValue r = applyCompoundOp(cur, newV, ins.b, ins.line);
      if (!m_error.isEmpty()) break;
      setVar(ident(ins.a), r);
      st.append(r);
      break;
    }
    // ── 槽位局部变量（A2）：O(1) 帧内访问；写穿作用域保持动态回退一致 ──
    case AcOpcode::kLoadSlot: {
      const auto &slotVals = m_frames.last().slotVals;
      if (ins.a >= 0 && ins.a < slotVals.size()) st.append(slotVals.at(ins.a));
      else st.append(accore::AcJsonValue());
      break;
    }
    case AcOpcode::kStoreSlot: {
      accore::AcJsonValue v = st.takeLast();
      auto &slotVals = m_frames.last().slotVals;
      if (ins.a >= 0 && ins.a < slotVals.size()) slotVals[ins.a] = v;
      setVar(ident(ins.b), v);  // 写穿作用域（动态/闭包回退可读到最新值）
      break;
    }
    case AcOpcode::kDeclareSlot: {
      accore::AcJsonValue v = st.takeLast();
      auto &slotVals = m_frames.last().slotVals;
      if (ins.a >= 0 && ins.a < slotVals.size()) slotVals[ins.a] = v;
      declareVar(ident(ins.b), v, ins.c != 0);
      break;
    }
    case AcOpcode::kCompoundSlot: {
      accore::AcJsonValue newV = st.takeLast();
      auto &slotVals = m_frames.last().slotVals;
      const accore::AcJsonValue cur =
          (ins.a >= 0 && ins.a < slotVals.size()) ? slotVals.at(ins.a) : accore::AcJsonValue();
      accore::AcJsonValue r = applyCompoundOp(cur, newV, ins.b, ins.line);
      if (!m_error.isEmpty()) break;
      if (ins.a >= 0 && ins.a < slotVals.size()) slotVals[ins.a] = r;
      setVar(ident(ins.c), r);
      st.append(r);
      break;
    }
    case AcOpcode::kLoadThis:
      st.append(m_currentThis);
      break;
    case AcOpcode::kStoreThisProp: {
      accore::AcJsonValue v = st.takeLast();
      accore::AcJsonValue old = m_currentThis.value(ident(ins.a));
      releaseIfInstanceWithDestruct(old);
      retainIfInstance(v);
      m_currentThis.set(ident(ins.a), v);
      m_modifiedThis.set(ident(ins.a), v);
      break;
    }
    case AcOpcode::kDeclareUsing: {
      accore::AcJsonValue v = st.takeLast();
      retainIfInstance(v);
      declareVar(ident(ins.a), v);
      if (!m_usingStack.isEmpty()) m_usingStack.last().append(ident(ins.a));
      break;
    }
    case AcOpcode::kLoadStatic: {
      initStatic(ident(ins.a));
      st.append(m_staticVars.value(ident(ins.a)).value(ident(ins.b)));
      break;
    }
    case AcOpcode::kStoreStatic: {
      accore::AcJsonValue v = st.takeLast();
      const QString &cls = ident(ins.a);
      const QString &prop = ident(ins.b);
      initStatic(cls);
      accore::AcJsonValue sv = m_staticVars[cls];
      if (sv.has(prop)) releaseIfInstanceWithDestruct(sv.value(prop));
      retainIfInstance(v);
      sv.set(prop, v);
      m_staticVars[cls] = sv;
      break;
    }
    case AcOpcode::kCompoundStatic: {
      accore::AcJsonValue newV = st.takeLast();
      const QString &cls = ident(ins.a);
      const QString &prop = ident(ins.b);
      initStatic(cls);
      accore::AcJsonValue cur = m_staticVars[cls].value(prop);
      accore::AcJsonValue r = applyCompoundOp(cur, newV, ins.c, ins.line);
      if (!m_error.isEmpty()) break;
      accore::AcJsonValue sv = m_staticVars[cls];
      if (sv.has(prop)) releaseIfInstanceWithDestruct(sv.value(prop));
      retainIfInstance(r);
      sv.set(prop, r);
      m_staticVars[cls] = sv;
      st.append(r);
      break;
    }
    case AcOpcode::kGetProp: {
      accore::AcJsonValue obj = st.takeLast();
      const QString &prop = ident(ins.a);
      if (obj.isArray() && prop == QStringLiteral("length")) {
        st.append(accore::AcJsonValue(double(obj.size())));
        break;
      }
      if (obj.isObject()) {
        st.append(obj.value(prop));
        break;
      }
      if (!obj.isNull() && ins.b != 0 && !obj.isObject()) {
        st.append(accore::AcJsonValue());  // ?. 对非对象短路
        break;
      }
      setError(QStringLiteral("cannot get property '%1' on value").arg(prop), ins.line);
      break;
    }
    case AcOpcode::kSetProp: {
      accore::AcJsonValue newV = st.takeLast();
      accore::AcJsonValue obj = st.takeLast();
      const QString &prop = ident(ins.a);
      if (!obj.isObject()) {
        setError(QStringLiteral("cannot set property '%1' on value").arg(prop), ins.line);
        break;
      }
      if (obj.has(prop)) releaseIfInstanceWithDestruct(obj.value(prop));
      obj.set(prop, newV);
      st.append(obj);  // 回写对象（v1：kSetProp 在语句级消费后由调用方 pop）
      break;
    }
    case AcOpcode::kCompoundProp: {
      accore::AcJsonValue newV = st.takeLast();
      accore::AcJsonValue obj = st.takeLast();
      const QString &prop = ident(ins.a);
      if (!obj.isObject()) {
        setError(QStringLiteral("cannot set property '%1' on value").arg(prop), ins.line);
        break;
      }
      accore::AcJsonValue cur = obj.value(prop);
      accore::AcJsonValue r = applyCompoundOp(cur, newV, ins.b, ins.line);
      if (!m_error.isEmpty()) break;
      if (obj.has(prop)) releaseIfInstanceWithDestruct(obj.value(prop));
      obj.set(prop, r);
      st.append(obj);
      st.append(r);
      break;
    }
    case AcOpcode::kIncProp: {
      accore::AcJsonValue obj = st.takeLast();
      const QString &prop = ident(ins.a);
      if (!obj.isObject()) {
        setError(QStringLiteral("cannot set property '%1' on value").arg(prop), ins.line);
        break;
      }
      const double oldV = obj.value(prop).toDouble();
      const double newV = oldV + ins.b;
      if (obj.has(prop)) releaseIfInstanceWithDestruct(obj.value(prop));
      obj.set(prop, accore::AcJsonValue(newV));
      if (ins.c & 2) {
        // 语句级写回（c bit1）：压更新后的对象，供调用方 kStoreName 写回变量
        st.append(obj);
      } else {
        st.append(ins.c & 1 ? accore::AcJsonValue(oldV) : accore::AcJsonValue(newV));
      }
      break;
    }
    case AcOpcode::kGetIndex: {
      accore::AcJsonValue idx = st.takeLast();
      accore::AcJsonValue obj = st.takeLast();
      if (obj.isObject()) {
        QString key = idx.isString() ? idx.toString()
                                     : (idx.isDouble() ? QString::number(idx.toDouble())
                                                       : idx.toString());
        st.append(obj.value(key));
        break;
      }
      if (obj.isArray()) {
        int i = safeJsonToInt(idx.toQJsonValue());
        if (i >= 0 && i < obj.size()) st.append(obj.at(i));
        else st.append(accore::AcJsonValue());
        break;
      }
      if (obj.isString()) {
        int i = safeJsonToInt(idx.toQJsonValue());
        QString s = obj.toString();
        if (i >= 0 && i < s.length()) st.append(accore::AcJsonValue(QString(s[i])));
        else st.append(accore::AcJsonValue());
        break;
      }
      setError(QStringLiteral("cannot access index on value"), ins.line);
      break;
    }
    case AcOpcode::kSetIndex: {
      accore::AcJsonValue newV = st.takeLast();
      accore::AcJsonValue idx = st.takeLast();
      accore::AcJsonValue obj = st.takeLast();
      QString key;
      if (obj.isObject()) {
        key = idx.isString() ? idx.toString() : QString::number(idx.toDouble());
        if (obj.has(key)) releaseIfInstanceWithDestruct(obj.value(key));
        obj.set(key, newV);
        st.append(obj);
        break;
      }
      if (obj.isArray()) {
        int i = safeJsonToInt(idx.toQJsonValue());
        if (i >= 0 && i < obj.size()) {
          releaseIfInstanceWithDestruct(obj.at(i));
          obj.replace(i, newV);
        } else if (i == obj.size()) {
          obj.append(newV);
        }
        st.append(obj);
        break;
      }
      setError(QStringLiteral("cannot index-assign on value"), ins.line);
      break;
    }
    case AcOpcode::kCompoundIndex: {
      accore::AcJsonValue newV = st.takeLast();
      accore::AcJsonValue idx = st.takeLast();
      accore::AcJsonValue obj = st.takeLast();
      QString key;
      if (obj.isObject()) {
        key = idx.isString() ? idx.toString() : QString::number(idx.toDouble());
        accore::AcJsonValue cur = obj.value(key);
        accore::AcJsonValue r = applyCompoundOp(cur, newV, ins.b, ins.line);
        if (!m_error.isEmpty()) break;
        if (obj.has(key)) releaseIfInstanceWithDestruct(obj.value(key));
        obj.set(key, r);
        st.append(obj);
        st.append(r);
        break;
      }
      if (obj.isArray()) {
        int i = safeJsonToInt(idx.toQJsonValue());
        if (i >= 0 && i < obj.size()) {
          accore::AcJsonValue cur = obj.at(i);
          accore::AcJsonValue r = applyCompoundOp(cur, newV, ins.b, ins.line);
          if (!m_error.isEmpty()) break;
          releaseIfInstanceWithDestruct(cur);
          obj.replace(i, r);
          st.append(obj);
          st.append(r);
          break;
        }
        setError(QStringLiteral("index out of range"), ins.line);
        break;
      }
      setError(QStringLiteral("cannot index-assign on value"), ins.line);
      break;
    }
    case AcOpcode::kIncIndex: {
      accore::AcJsonValue idx = st.takeLast();
      accore::AcJsonValue obj = st.takeLast();
      const double delta = ins.b;
      const bool post = (ins.c & 1) != 0;
      const bool store = (ins.c & 2) != 0;
      if (obj.isObject()) {
        const QString key = idx.isString() ? idx.toString() : QString::number(idx.toDouble());
        const double oldV = obj.value(key).toDouble();
        const double nv = oldV + delta;
        if (obj.has(key)) releaseIfInstanceWithDestruct(obj.value(key));
        obj.set(key, accore::AcJsonValue(nv));
        // 语句级写回（c bit1）：压更新后的对象供 kStoreName 写回变量
        if (store) st.append(obj);
        else st.append(post ? accore::AcJsonValue(oldV) : accore::AcJsonValue(nv));
        break;
      }
      if (obj.isArray()) {
        int i = safeJsonToInt(idx.toQJsonValue());
        if (i >= 0 && i < obj.size()) {
          const double oldV = obj.at(i).toDouble();
          const double nv = oldV + delta;
          releaseIfInstanceWithDestruct(obj.at(i));
          obj.replace(i, accore::AcJsonValue(nv));
          if (store) st.append(obj);
          else st.append(post ? accore::AcJsonValue(oldV) : accore::AcJsonValue(nv));
          break;
        }
      }
      setError(QStringLiteral("cannot index-assign on value"), ins.line);
      break;
    }
    case AcOpcode::kIncName: {
      const QString &name = ident(ins.a);
      const accore::AcJsonValue old = resolveVar(name);
      const double oldV = old.toDouble();
      const double nv = oldV + ins.b;
      setVar(name, accore::AcJsonValue(nv));
      st.append(ins.c ? accore::AcJsonValue(oldV) : accore::AcJsonValue(nv));
      break;
    }
    case AcOpcode::kCompoundThisProp: {
      accore::AcJsonValue newV = st.takeLast();
      const QString &prop = ident(ins.a);
      accore::AcJsonValue cur = m_currentThis.value(prop);
      accore::AcJsonValue r = applyCompoundOp(cur, newV, ins.b, ins.line);
      if (!m_error.isEmpty()) break;
      accore::AcJsonValue old = m_currentThis.value(prop);
      releaseIfInstanceWithDestruct(old);
      retainIfInstance(r);
      m_currentThis.set(prop, r);
      m_modifiedThis.set(prop, r);
      st.append(r);
      break;
    }
    // ── 运算 ──
    case AcOpcode::kAdd: case AcOpcode::kSub: case AcOpcode::kMul: case AcOpcode::kDiv:
    case AcOpcode::kMod: case AcOpcode::kEq: case AcOpcode::kNeq: case AcOpcode::kLt:
    case AcOpcode::kGt: case AcOpcode::kLte: case AcOpcode::kGte: {
      accore::AcJsonValue r = st.takeLast();
      accore::AcJsonValue l = st.takeLast();
      st.append(binOp(ins.op, l, r, ins.line));
      break;
    }
    case AcOpcode::kNot: {
      accore::AcJsonValue v = st.takeLast();
      st.append(accore::AcJsonValue(!AcInterpreter::isTruthy(v)));
      break;
    }
    case AcOpcode::kNeg: {
      accore::AcJsonValue v = st.takeLast();
      st.append(accore::AcJsonValue(-v.toDouble()));
      break;
    }
    // ── 跳转 ──
    case AcOpcode::kJmp: pc = ins.a; break;
    case AcOpcode::kJmpIfT: {
      accore::AcJsonValue v = st.takeLast();
      if (AcInterpreter::isTruthy(v)) pc = ins.a;
      break;
    }
    case AcOpcode::kJmpIfF: {
      accore::AcJsonValue v = st.takeLast();
      if (!AcInterpreter::isTruthy(v)) pc = ins.a;
      break;
    }
    case AcOpcode::kNullJmpT: {
      accore::AcJsonValue v = st.takeLast();
      if (!v.isNull()) pc = ins.a;
      else st.append(accore::AcJsonValue());  // 恢复栈（空值合并左值 null）
      break;
    }
    // ── 字面量 ──
    case AcOpcode::kNewArray: {
      const int n = ins.a;
      accore::AcJsonValue arr = accore::AcJsonValue::makeArray();
      QVector<accore::AcJsonValue> elems;
      for (int i = 0; i < n; ++i) elems.prepend(st.takeLast());
      for (auto &e : elems) {
        retainIfInstance(e);
        arr.append(e);
      }
      st.append(arr);
      break;
    }
    case AcOpcode::kNewObject: {
      const int n = ins.a;
      const int keyStart = ins.b;
      accore::AcJsonValue obj = accore::AcJsonValue::makeObject();
      QVector<accore::AcJsonValue> vals;
      for (int i = 0; i < n; ++i) vals.prepend(st.takeLast());
      for (int i = 0; i < n; ++i) {
        accore::AcJsonValue v = vals[i];
        retainIfInstance(v);
        obj.set(unit.constants[keyStart + i].toString(), v);
      }
      st.append(obj);
      break;
    }
    // ── 调用 ──
    case AcOpcode::kCallFunc:
      if (ins.c == 1) {
        // call("cls","fn",args) 形式：栈 [..., args数组]
        accore::AcJsonValue callArgs = st.takeLast();
        accore::AcJsonValue funcVal = st.takeLast();
        accore::AcJsonValue clsVal = st.takeLast();
        accore::AcJsonValue r =
            FunMgr::ins().call(clsVal.toString(), funcVal.toString(), callArgs);
        QString err = FunMgr::takeError();
        if (!err.isEmpty()) setError(err, ins.line);
        else st.append(r);
        break;
      }
      callFunc(ident(ins.a), ins.b, ins.line);
      break;
    case AcOpcode::kCallMethod:
      callMethod(ident(ins.a), ins.b, ins.c, ins.line);
      break;
    case AcOpcode::kCallStatic:
      callStaticMethod(ident(ins.a), ident(ins.b), ins.c, ins.line);
      break;
    case AcOpcode::kCallSuper: {
      // super.method(args)：栈上已有 [obj=this, args...]，直接复用 callMethod 的 super 路径
      callMethod(ident(ins.a), ins.b, /*flags=*/8, ins.line);
      break;
    }
    case AcOpcode::kCallValue: {
      accore::AcJsonValue funcRef = st.takeLast();
      callFuncRefValue(funcRef, ins.a, ins.line);
      break;
    }
    case AcOpcode::kNew: {
      const int argc = ins.b;
      QVector<accore::AcJsonValue> args;
      for (int i = 0; i < argc; ++i) args.prepend(st.takeLast());
      QVector<accore::AcJsonValue> ordered;
      for (int i = 0; i < argc; ++i) ordered.append(args[argc - 1 - i]);
      const QString &cls = ident(ins.a);
      if (!m_classes.contains(cls)) {
        setError(QStringLiteral("undefined class '%1'").arg(cls), ins.line);
        break;
      }
      const ClassDef &cd = m_classes[cls];
      if (cd.isNative) {
        accore::AcJsonValue ctorArgs = accore::AcJsonValue::makeArray();
        for (auto &a : ordered) ctorArgs.append(a);
        accore::AcJsonValue r =
            FunMgr::ins().call(cls, QString::fromLatin1(AcRuntime::kConstructor), ctorArgs);
        QString err = FunMgr::takeError();
        if (!err.isEmpty()) {
          setError(err, ins.line);
          break;
        }
        accore::AcJsonValue inst =
            r.isObject() ? accore::AcJsonValue::instanceFrom(r, cls) : accore::AcJsonValue::makeInstance(cls);
        st.append(m_objMgr.registerInstance(inst, cls));
        break;
      }
      // 用户类：实例初始化单元 → 组装 → 构造器
      accore::AcJsonValue inst = makeInstanceObject(cls);
      execConstructor(cls, inst, argc, ordered);
      if (!m_error.isEmpty()) break;
      st.append(inst);
      break;
    }
    case AcOpcode::kFuncExpr: {
      // 注册函数引用（名称已在编译期生成）
      const AcFuncUnit &fu = m_module->funcs[ins.a];
      MethodDef md;
      md.name = fu.name;
      for (int i = 0; i < fu.paramNames.size(); ++i) {
        ParamDef p;
        p.name = fu.paramNames[i];
        md.params.append(p);
      }
      m_module->funcUnits.insert(fu.name, ins.a);
      (void)md;
      st.append(accore::AcJsonValue::makeFuncRef(fu.name));
      break;
    }
    case AcOpcode::kRet: {
      // 保存帧返回值并终止当前函数执行
      Frame &rf = m_frames.last();
      rf.retVal = st.takeLast();
      rf.returned = true;
      rf.pc = int(unit.code.size());  // 结束循环
      break;
    }
    // ── try / catch ──
    case AcOpcode::kEnterTry: {
      ActiveTry t;
      const AcTryEntry &e = unit.tryTable[ins.a];
      t.tryIdx = ins.a;
      t.catchAddr = e.catchAddr;
      t.finallyAddr = e.finallyAddr;
      m_tryStack.append(t);
      break;
    }
    case AcOpcode::kLeaveTry: {
      if (!m_tryStack.isEmpty()) m_tryStack.pop_back();
      break;
    }
    case AcOpcode::kCatchBegin: {
      const AcTryEntry &e = unit.tryTable[ins.a];
      if (!e.catchVar.isEmpty()) {
        declareVar(e.catchVar, accore::AcJsonValue(m_pendingCatchErr));
      }
      break;
    }
    case AcOpcode::kCatchEnd:
      break;
    case AcOpcode::kFinallyBegin: {
      m_pendingPropagateErr = m_error;
      break;
    }
    case AcOpcode::kFinallyEnd: {
      // finally 执行完毕：若有待传播错误则恢复传播
      if (!m_pendingPropagateErr.isEmpty()) m_error = m_pendingPropagateErr;
      m_pendingPropagateErr.clear();
      break;
    }
    // ── for-in ──
    case AcOpcode::kForInInit: {
      accore::AcJsonValue iterVal = st.takeLast();
      IterFrame it;
      if (iterVal.isString()) {
        it.arr = accore::AcJsonValue::makeArray();
        QString s = iterVal.toString();
        for (int i = 0; i < s.length(); ++i) it.arr.append(accore::AcJsonValue(QString(s[i])));
      } else if (iterVal.isObject()) {
        it.arr = accore::AcJsonValue::makeArray();
        for (const auto &m : iterVal.members()) it.arr.append(accore::AcJsonValue(m.key));
      } else {
        it.arr = iterVal;
      }
      it.idx = 0;
      m_iters.append(it);
      break;
    }
    case AcOpcode::kForInNext: {
      if (m_iters.isEmpty()) {
        pc = ins.b;
        break;
      }
      IterFrame &it = m_iters.last();
      if (it.idx >= it.arr.size()) {
        m_iters.pop_back();
        pc = ins.b;
        break;
      }
      it.cur = it.arr.at(it.idx);
      ++it.idx;
      break;
    }
    case AcOpcode::kDeclareIterVar: {
      if (m_iters.isEmpty()) break;
      const QString &name = ident(ins.a);
      if (m_scopeStack.last().contains(name)) {
        releaseIfInstanceWithDestruct(m_scopeStack.last()[name]);
        m_scopeStack.last().remove(name);
      }
      declareVar(name, m_iters.last().cur);
      break;
    }
    case AcOpcode::kThrowValue: {
      accore::AcJsonValue v = st.takeLast();
      m_error = v.isString() ? v.toString() : AcValueStr::toString(v);
      break;
    }
    // ── 栈操作 ──
    case AcOpcode::kPop: st.pop_back(); break;
    case AcOpcode::kDup: st.append(st.last()); break;
    case AcOpcode::kSwap: {
      accore::AcJsonValue a = st.takeLast();
      accore::AcJsonValue b = st.takeLast();
      st.append(a);
      st.append(b);
      break;
    }
    case AcOpcode::kSwap3: {
      accore::AcJsonValue a = st.takeLast();
      accore::AcJsonValue b = st.takeLast();
      accore::AcJsonValue c = st.takeLast();
      st.append(a);
      st.append(c);
      st.append(b);
      break;
    }
    // ── 语句边界 / 调试 ──
    case AcOpcode::kOpStmt: {
      // 作用域深度同步：当前帧 base + 指令深度
      const Frame &f = m_frames.last();
      adjustScope(f.baseScope + ins.b);
      // 调试钩子（非调试会话零开销）
      if (m_debugger && ins.a > 0 && m_debugger->isDebugging()) {
        bool cont = m_debugger->onStatement(
            m_scriptFile, ins.a, m_callDepth,
            [this](QVector<AcDebugFrame> &stack, QList<AcDebugVar> &vars) {
              for (const auto &f : m_frames) {
                stack.append(AcDebugFrame{f.funcName, m_scriptFile, 0});
              }
              for (int i = m_scopeStack.size() - 1; i >= 0; --i) {
                const auto &scope = m_scopeStack[i];
                for (auto it = scope.cbegin(); it != scope.cend(); ++it) {
                  vars.append(AcDebugVar{QString(), it.key(), it.value().toQJsonValue(), QString(),
                                         0, i == 0 ? QString() : QString()});
                }
              }
            });
        if (!cont) {
          m_error = QStringLiteral("执行已取消");
          pc = -2;
        }
      }
      break;
    }
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  调用语义
// ═════════════════════════════════════════════════════════════════════════════

void AcVm::callFunc(const QString &name, int argc, int line) {
  // 内置一级函数 / FunMgr builtin / 用户函数 / 函数引用 依次尝试（与 callBuiltin 同序）
  accore::AcJsonValue arr = accore::AcJsonValue::makeArray();
  for (int i = 0; i < argc; ++i) {
    arr.append(m_vstack[m_vstack.size() - argc + i]);
  }
  for (int i = 0; i < argc; ++i) m_vstack.removeLast();

  const QString builtinClass = QString::fromLatin1(AcRuntime::kBuiltinClass);
  if (FunMgr::ins().contains(builtinClass, name)) {
    FunBuiltin::setCurrentLine(line);
    accore::AcJsonValue r = FunMgr::ins().call(builtinClass, name, arr);
    FunBuiltin::setCurrentLine(0);
    QString err = FunMgr::takeError();
    if (!err.isEmpty()) {
      setError(err, line);
      return;
    }
    m_vstack.append(r);
    return;
  }

  // 用户函数（字节码单元）：callUnit 需从栈取参，先把参数压回栈
  auto it = m_module->funcUnits.find(name);
  if (it != m_module->funcUnits.end()) {
    for (const auto &a : arr.items()) m_vstack.append(a);
    callUnit(it.value(), argc, name);
    m_vstack.append(m_retSlot);
    return;
  }
  // 变量中的函数引用
  if (containsVar(name)) {
    accore::AcJsonValue varVal = resolveVar(name);
    if (varVal.isFuncRef()) {
      const QString &fname = varVal.funcRefName();
      auto fi = m_module->funcUnits.find(fname);
      if (fi != m_module->funcUnits.end()) {
        for (const auto &a : arr.items()) m_vstack.append(a);
        callUnit(fi.value(), argc, fname);
        m_vstack.append(m_retSlot);
        return;
      }
    }
  }
  setError(QStringLiteral("unknown function '%1'").arg(name), line);
}

void AcVm::callFunMgrIfOwned(const QString &cls, const QString &func,
                             const accore::AcJsonValue &args, int line) {
  if (!FunMgr::ins().contains(cls, func)) return;
  accore::AcJsonValue r = FunMgr::ins().call(cls, func, args);
  QString err = FunMgr::takeError();
  if (!err.isEmpty()) {
    setError(err, line);
    return;
  }
  m_vstack.append(r);
}

void AcVm::callMethod(const QString &method, int argc, int32_t flags, int line) {
  const bool chained = (flags & 1) != 0;
  const bool optional = (flags & 2) != 0;
  const bool isJSON = (flags & 4) != 0;
  const bool isSuper = (flags & 8) != 0;

  // JSON.parse/stringify：编译未压对象，栈上只有参数
  if (isJSON) {
    QVector<accore::AcJsonValue> args;
    for (int i = m_vstack.size() - argc; i < m_vstack.size(); ++i) args.append(m_vstack[i]);
    m_vstack.resize(m_vstack.size() - argc);
    for (const auto &a : args) m_vstack.append(a);
    callJSONMethod(method, argc, line);
    return;
  }

  // 取对象与实参
  accore::AcJsonValue obj = m_vstack[m_vstack.size() - argc - 1];
  QVector<accore::AcJsonValue> args;
  for (int i = m_vstack.size() - argc; i < m_vstack.size(); ++i) args.append(m_vstack[i]);
  m_vstack.resize(m_vstack.size() - argc - 1);

  if (optional && obj.isNull()) {
    m_vstack.append(accore::AcJsonValue());  // ?. 短路
    return;
  }
  // 下列子调用器按自身约定从栈顶弹出实参：先把上面已取出的 args 压回栈
  if (obj.isString()) {
    for (const auto &a : args) m_vstack.append(a);
    callStringMethod(obj.toString(), method, argc, line);
    return;
  }
  if (obj.isArray()) {
    accore::AcJsonValue modified;
    for (const auto &a : args) m_vstack.append(a);
    callArrayMethod(obj, method, argc, line, &modified);
    // 语句级变异写回（flags bit4）：用变异后的数组替换标量结果，供 kStoreName 存储
    if ((flags & 16) != 0 && !modified.isNull()) {
      m_vstack.takeLast();  // 丢弃标量（如 push 的 size）
      m_vstack.append(modified);
    }
    return;
  }
  if (obj.isObject() && (method == QStringLiteral("keys") ||
                         method == QStringLiteral("values") ||
                         method == QStringLiteral("has") ||
                         method == QStringLiteral("size"))) {
    for (const auto &a : args) m_vstack.append(a);
    callObjectBuiltin(obj, method, argc, line);
    return;
  }
  // 类实例/类引用方法
  const QString &cls = obj.instanceClass();
  if (cls.isEmpty() || !m_classes.contains(cls)) {
    setError(QStringLiteral("object has no class information"), line);
    return;
  }
  const ClassDef &cd = m_classes[cls];
  if (cd.isNative) {
    accore::AcJsonValue a = accore::AcJsonValue::makeArray();
    for (auto &v : args) a.append(v);
    accore::AcJsonValue r = FunMgr::ins().call(cls, method, obj, a);
    QString err = FunMgr::takeError();
    if (!err.isEmpty()) {
      setError(err, line);
      return;
    }
    m_vstack.append(r);
    return;
  }
  // 用户类方法（super 从基类链查找）
  QString search = cls;
  if (isSuper) {
    search = cd.baseClass;
    if (search.isEmpty()) {
      setError(QStringLiteral("cannot use 'super' in class without base class"), line);
      return;
    }
  }
  QString defClass = search;
  const MethodDef *md = nullptr;
  QString c = search;
  while (!c.isEmpty() && m_classes.contains(c)) {
    for (const auto &m : m_classes[c].methods) {
      if (m.name == method) {
        md = &m;
        defClass = c;
        break;
      }
    }
    if (md) break;
    c = m_classes[c].baseClass;
  }
  if (!md) {
    setError(QStringLiteral("class '%1' has no method '%2'").arg(cls, method), line);
    return;
  }
  // 执行方法单元
  const QString fname = QStringLiteral("%1.%2").arg(defClass, method);
  auto it = m_module->funcUnits.find(fname);
  if (it == m_module->funcUnits.end()) {
    setError(QStringLiteral("method '%1' not compiled").arg(fname), line);
    return;
  }
  const int argc2 = argc;
  // 恢复 this 上下文 + 参数
  accore::AcJsonValue oldThis = m_currentThis;
  accore::AcJsonValue oldModified = m_modifiedThis;
  m_currentThis = obj;
  m_modifiedThis = obj;
  // 参数入栈（callUnit 会取走）
  for (auto &a : args) m_vstack.append(a);
  callUnit(it.value(), argc2, fname);
  m_currentThis = oldThis;
  m_modifiedThis = oldModified;
  m_vstack.append(m_retSlot);
}

void AcVm::callStaticMethod(const QString &cls, const QString &method, int argc, int line) {
  QVector<accore::AcJsonValue> args;
  for (int i = m_vstack.size() - argc; i < m_vstack.size(); ++i) args.append(m_vstack[i]);
  m_vstack.resize(m_vstack.size() - argc);
  if (!m_classes.contains(cls)) {
    setError(kVmErrUndefinedClass.arg(cls), line);
    return;
  }
  initStatic(cls);
  const ClassDef &cd = m_classes[cls];
  const MethodDef *md = nullptr;
  for (const auto &m : cd.methods) {
    if (m.isStatic && m.name == method) {
      md = &m;
      break;
    }
  }
  if (!md) {
    setError(QStringLiteral("class '%1' has no static member '%2'").arg(cls, method), line);
    return;
  }
  const QString fname = QStringLiteral("%1.%2").arg(cls, method);
  auto it = m_module->funcUnits.find(fname);
  if (it == m_module->funcUnits.end()) {
    setError(QStringLiteral("static method '%1' not compiled").arg(fname), line);
    return;
  }
  accore::AcJsonValue oldThis = m_currentThis;
  accore::AcJsonValue oldModified = m_modifiedThis;
  m_currentThis = accore::AcJsonValue::makeObject();
  m_modifiedThis = m_currentThis;
  for (auto &a : args) m_vstack.append(a);
  callUnit(it.value(), argc, fname);
  m_currentThis = oldThis;
  m_modifiedThis = oldModified;
  m_vstack.append(m_retSlot);
}

void AcVm::callFuncRefValue(const accore::AcJsonValue &funcRef, int argc, int line) {
  if (!funcRef.isFuncRef()) {
    setError(QStringLiteral("not a function reference"), line);
    return;
  }
  const QString &fname = funcRef.funcRefName();
  auto it = m_module->funcUnits.find(fname);
  if (it == m_module->funcUnits.end()) {
    setError(QStringLiteral("function '%1' not compiled").arg(fname), line);
    return;
  }
  callUnit(it.value(), argc, fname);
  m_vstack.append(m_retSlot);
}

// ═════════════════════════════════════════════════════════════════════════════
//  内置方法
// ═════════════════════════════════════════════════════════════════════════════

void AcVm::callJSONMethod(const QString &method, int argc, int line) {
  if (method == QStringLiteral("parse")) {
    if (argc < 1) {
      setError(QStringLiteral("JSON.parse() requires 1 argument"), line);
      return;
    }
    accore::AcJsonValue v = m_vstack.takeLast();
    if (!v.isString()) {
      setError(QStringLiteral("JSON.parse() argument must be a string"), line);
      return;
    }
    bool ok = false;
    QString err;
    accore::AcJsonValue parsed = accore::AcJsonValue::parse(v.toString(), &ok, &err);
    if (!ok) {
      setError(QStringLiteral("JSON.parse() error: %1").arg(err), line);
      return;
    }
    m_vstack.append(parsed);
    return;
  }
  if (method == QStringLiteral("stringify")) {
    if (argc < 1) {
      setError(QStringLiteral("JSON.stringify() requires 1 argument"), line);
      return;
    }
    accore::AcJsonValue v = m_vstack.takeLast();
    if (!v.isObject() && !v.isArray()) {
      m_vstack.append(accore::AcJsonValue(v.toString()));
      return;
    }
    m_vstack.append(accore::AcJsonValue(v.serialize(false)));
    return;
  }
  setError(QStringLiteral("JSON has no method '%1'").arg(method), line);
}

void AcVm::callStringMethod(const QString &str, const QString &method, int argc, int line) {
  QVector<accore::AcJsonValue> args;
  for (int i = 0; i < argc; ++i) args.prepend(m_vstack.takeLast());
  // 经 FunMgr "str" 域
  accore::AcJsonValue a = accore::AcJsonValue::makeArray();
  a.append(accore::AcJsonValue(str));
  for (auto it = args.rbegin(); it != args.rend(); ++it) a.append(*it);
  if (FunMgr::ins().contains(QStringLiteral("str"), method)) {
    accore::AcJsonValue r = FunMgr::ins().call(QStringLiteral("str"), method, a);
    QString err = FunMgr::takeError();
    if (!err.isEmpty()) setError(err, line);
    else m_vstack.append(r);
    return;
  }
  setError(QStringLiteral("string has no method '%1'").arg(method), line);
}

// ═════════════════════════════════════════════════════════════════════════════
//  数组 / 对象内置方法（v1 常用子集；回调经 callUnit 执行函数引用）
// ═════════════════════════════════════════════════════════════════════════════

void AcVm::callObjectBuiltin(accore::AcJsonValue &obj, const QString &method, int argc, int line) {
  QVector<accore::AcJsonValue> args;
  for (int i = 0; i < argc; ++i) args.prepend(m_vstack.takeLast());
  if (method == QStringLiteral("keys")) {
    accore::AcJsonValue arr = accore::AcJsonValue::makeArray();
    for (const auto &k : obj.keys()) arr.append(accore::AcJsonValue(k));
    m_vstack.append(arr);
    return;
  }
  if (method == QStringLiteral("values")) {
    accore::AcJsonValue arr = accore::AcJsonValue::makeArray();
    for (const auto &m : obj.members()) arr.append(m.value);
    m_vstack.append(arr);
    return;
  }
  if (method == QStringLiteral("has")) {
    if (args.isEmpty()) {
      setError(QStringLiteral("has() requires 1 argument"), line);
      return;
    }
    const QString &key = args.last().toString();
    m_vstack.append(accore::AcJsonValue(obj.has(key)));
    return;
  }
  if (method == QStringLiteral("size")) {
    m_vstack.append(accore::AcJsonValue(double(obj.size())));
    return;
  }
  setError(QStringLiteral("object has no method '%1'").arg(method), line);
}

void AcVm::callArrayMethod(accore::AcJsonValue &arr, const QString &method, int argc, int line,
                           accore::AcJsonValue *modifiedOut) {
  QVector<accore::AcJsonValue> args;
  for (int i = 0; i < argc; ++i) args.prepend(m_vstack.takeLast());
  std::reverse(args.begin(), args.end());

  if (method == QStringLiteral("push")) {
    for (auto &a : args) {
      retainIfInstance(a);
      arr.append(a);
    }
    *modifiedOut = arr;
    m_vstack.append(accore::AcJsonValue(double(arr.size())));
    return;
  }
  if (method == QStringLiteral("pop")) {
    if (arr.size() > 0) {
      accore::AcJsonValue v = arr.at(arr.size() - 1);
      arr.removeLast();
      *modifiedOut = arr;
      m_vstack.append(v);
    } else {
      m_vstack.append(accore::AcJsonValue());
    }
    return;
  }
  if (method == QStringLiteral("shift")) {
    if (arr.size() > 0) {
      accore::AcJsonValue v = arr.at(0);
      accore::AcJsonValue na = accore::AcJsonValue::makeArray();
      for (int i = 1; i < arr.size(); ++i) na.append(arr.at(i));
      *modifiedOut = na;
      m_vstack.append(v);
    } else {
      m_vstack.append(accore::AcJsonValue());
    }
    return;
  }
  if (method == QStringLiteral("unshift")) {
    accore::AcJsonValue na = accore::AcJsonValue::makeArray();
    for (auto it = args.rbegin(); it != args.rend(); ++it) {
      retainIfInstance(*it);
      na.append(*it);
    }
    for (int i = 0; i < arr.size(); ++i) na.append(arr.at(i));
    *modifiedOut = na;
    m_vstack.append(accore::AcJsonValue(double(na.size())));
    return;
  }
  if (method == QStringLiteral("join")) {
    const QString sep = args.isEmpty() ? QStringLiteral(",") : args[0].toString();
    // 字符串值直接 toString（AcValueStr::toString 会给字符串加引号，与解释器 valToStr 语义不符）
    const auto v2s = [](const accore::AcJsonValue &v) {
      return v.isString() ? v.toString() : AcValueStr::toString(v);
    };
    QStringList parts;
    for (int i = 0; i < arr.size(); ++i) parts.append(v2s(arr.at(i)));
    m_vstack.append(accore::AcJsonValue(parts.join(sep)));
    return;
  }
  if (method == QStringLiteral("indexOf")) {
    if (args.isEmpty()) {
      m_vstack.append(accore::AcJsonValue(-1.0));
      return;
    }
    for (int i = 0; i < arr.size(); ++i) {
      if (AcInterpreter::compareValues(arr.at(i), args[0]) == 0) {
        m_vstack.append(accore::AcJsonValue(double(i)));
        return;
      }
    }
    m_vstack.append(accore::AcJsonValue(-1.0));
    return;
  }
  if (method == QStringLiteral("includes") || method == QStringLiteral("contains")) {
    if (args.isEmpty()) {
      m_vstack.append(accore::AcJsonValue(false));
      return;
    }
    for (int i = 0; i < arr.size(); ++i) {
      if (AcInterpreter::compareValues(arr.at(i), args[0]) == 0) {
        m_vstack.append(accore::AcJsonValue(true));
        return;
      }
    }
    m_vstack.append(accore::AcJsonValue(false));
    return;
  }
  if (method == QStringLiteral("slice")) {
    const int start = args.size() > 0 ? safeJsonToInt(args[0].toQJsonValue()) : 0;
    const int end = args.size() > 1 ? safeJsonToInt(args[1].toQJsonValue()) : arr.size();
    accore::AcJsonValue na = accore::AcJsonValue::makeArray();
    int s = start < 0 ? arr.size() + start : start;
    int e = end < 0 ? arr.size() + end : end;
    s = qBound(0, s, arr.size());
    e = qBound(s, e, arr.size());
    for (int i = s; i < e; ++i) na.append(arr.at(i));
    m_vstack.append(na);
    return;
  }
  if (method == QStringLiteral("concat")) {
    accore::AcJsonValue na = accore::AcJsonValue::makeArray();
    for (int i = 0; i < arr.size(); ++i) na.append(arr.at(i));
    for (auto &a : args) {
      if (a.isArray()) {
        for (int i = 0; i < a.size(); ++i) na.append(a.at(i));
      } else {
        na.append(a);
      }
    }
    m_vstack.append(na);
    return;
  }
  // 高阶回调方法（回调 = 函数引用）
  if (method == QStringLiteral("map") || method == QStringLiteral("forEach") ||
      method == QStringLiteral("filter") || method == QStringLiteral("some") ||
      method == QStringLiteral("every") || method == QStringLiteral("find") ||
      method == QStringLiteral("findIndex") || method == QStringLiteral("reduce")) {
    const bool isReduce = (method == QStringLiteral("reduce"));
    if (args.isEmpty() || !args.first().isFuncRef()) {
      setError(QStringLiteral("'%1()' requires a function argument").arg(method), line);
      return;
    }
    const accore::AcJsonValue &fn = args.first();
    const QString &fnName = fn.funcRefName();
    auto fIt = m_module->funcUnits.constFind(fnName);
    if (fIt == m_module->funcUnits.constEnd()) {
      setError(QStringLiteral("callback function '%1' not compiled").arg(fnName), line);
      return;
    }
    const int uidx = fIt.value();

    auto invoke = [&](const accore::AcJsonValue &elem, const accore::AcJsonValue &acc,
                      int idx, bool withAcc) -> accore::AcJsonValue {
      // 参数协议与解释器一致（ac_builtin_eval 的 map/forEach 回调）：非 reduce → (elem, idx)；
      // reduce → (acc, elem, idx)。callUnit 按自然顺序取参：第一参数先压（栈底），最后参数后压（栈顶）。
      // 回调声明的参数更少时多余实参被 callUnit 丢弃。
      if (isReduce && withAcc) m_vstack.append(acc);
      m_vstack.append(elem);
      m_vstack.append(accore::AcJsonValue(double(idx)));
      const int nArgs = (isReduce && withAcc) ? 3 : 2;
      callUnit(uidx, nArgs, fnName);
      return m_retSlot;
    };

    if (isReduce) {
      int startIdx = 0;
      accore::AcJsonValue acc;
      bool hasAcc = false;
      if (args.size() > 1) {
        acc = args[1];
        hasAcc = true;
      } else if (arr.size() > 0) {
        acc = arr.at(0);
        startIdx = 1;
      }
      for (int i = startIdx; i < arr.size(); ++i) {
        acc = invoke(arr.at(i), acc, i, hasAcc || i > startIdx);
      }
      m_vstack.append(acc);
      return;
    }
    accore::AcJsonValue out = accore::AcJsonValue::makeArray();
    for (int i = 0; i < arr.size(); ++i) {
      accore::AcJsonValue r = invoke(arr.at(i), args.size() > 1 ? args[1] : accore::AcJsonValue(),
                                     i, false);
      if (method == QStringLiteral("map") || method == QStringLiteral("forEach")) {
        out.append(r);
      } else if (method == QStringLiteral("filter")) {
        if (AcInterpreter::isTruthy(r)) out.append(arr.at(i));
      } else if (method == QStringLiteral("some")) {
        if (AcInterpreter::isTruthy(r)) {
          m_vstack.append(accore::AcJsonValue(true));
          return;
        }
      } else if (method == QStringLiteral("every")) {
        if (!AcInterpreter::isTruthy(r)) {
          m_vstack.append(accore::AcJsonValue(false));
          return;
        }
      } else if (method == QStringLiteral("find")) {
        if (AcInterpreter::isTruthy(r)) {
          m_vstack.append(arr.at(i));
          return;
        }
      } else if (method == QStringLiteral("findIndex")) {
        if (AcInterpreter::isTruthy(r)) {
          m_vstack.append(accore::AcJsonValue(double(i)));
          return;
        }
      }
    }
    if (method == QStringLiteral("map") || method == QStringLiteral("forEach")) {
      *modifiedOut = out;
      m_vstack.append(out);
    } else if (method == QStringLiteral("filter")) {
      *modifiedOut = out;
      m_vstack.append(out);
    } else if (method == QStringLiteral("some") || method == QStringLiteral("every")) {
      m_vstack.append(accore::AcJsonValue(method == QStringLiteral("every") ? true : false));
    } else if (method == QStringLiteral("find")) {
      m_vstack.append(accore::AcJsonValue());
    } else if (method == QStringLiteral("findIndex")) {
      m_vstack.append(accore::AcJsonValue(-1.0));
    }
    return;
  }
  if (method == QStringLiteral("length") || method == QStringLiteral("size")) {
    m_vstack.append(accore::AcJsonValue(double(arr.size())));
    return;
  }
  if (method == QStringLiteral("sort")) {
    // 无参/比较回调排序（v1：默认按数值/字符串简单比较）
    accore::AcJsonValue na = accore::AcJsonValue::makeArray();
    QVector<accore::AcJsonValue> items;
    for (int i = 0; i < arr.size(); ++i) items.append(arr.at(i));
    std::stable_sort(items.begin(), items.end(),
                     [](const accore::AcJsonValue &a, const accore::AcJsonValue &b) {
                       return AcInterpreter::compareValues(a, b) < 0;
                     });
    for (auto &v : items) na.append(v);
    *modifiedOut = na;
    m_vstack.append(na);
    return;
  }
  setError(QStringLiteral("array has no method '%1'").arg(method), line);
}

// ═════════════════════════════════════════════════════════════════════════════
//  变量 / 作用域 / GC（复刻解释器，见 ac_interpreter.cpp 对应实现）
// ═════════════════════════════════════════════════════════════════════════════

accore::AcJsonValue AcVm::resolveVar(const QString &name) const {
  if (name == QString::fromLatin1(AcKeyword::kThis)) return m_currentThis;
  if (name == QString::fromLatin1(AcKeyword::kSuper)) return m_currentThis;
  for (int i = m_scopeStack.size() - 1; i >= 0; --i) {
    auto it = m_scopeStack[i].find(name);
    if (it != m_scopeStack[i].end()) return it.value();
  }
  return accore::AcJsonValue();
}

void AcVm::setVar(const QString &name, const accore::AcJsonValue &val) {
  for (int i = m_scopeStack.size() - 1; i >= 0; --i) {
    if (m_scopeStack[i].contains(name)) {
      if (i < m_constVars.size() && m_constVars[i].contains(name)) {
        setError(QStringLiteral("cannot assign to const '%1'").arg(name), 0);
        return;
      }
      const accore::AcJsonValue &old = m_scopeStack[i][name];
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

void AcVm::declareVar(const QString &name, const accore::AcJsonValue &val, bool isConst) {
  retainIfInstance(val);
  m_scopeStack.last()[name] = val;
  if (isConst) m_constVars.last().insert(name);
}

void AcVm::pushScope() {
  m_scopeStack.append(QHash<QString, accore::AcJsonValue>());
  m_constVars.append(QSet<QString>());
  m_usingStack.append(QVector<QString>());
}

void AcVm::popScope() {
  if (m_scopeStack.isEmpty()) return;
  if (!m_usingStack.isEmpty()) {
    auto usingVars = m_usingStack.takeLast();
    for (int i = usingVars.size() - 1; i >= 0; --i) {
      accore::AcJsonValue val = resolveVar(usingVars[i]);
      if (val.isInstance()) {
        const QString &cls = val.instanceClass();
        if (m_classes.contains(cls)) {
          const ClassDef &cd = m_classes[cls];
          if (cd.isNative) {
            FunMgr::ins().call(cls, QString::fromLatin1(AcKeyword::kDispose), val,
                               accore::AcJsonValue::makeArray());
            FunMgr::takeError();
          } else {
            const MethodDef *dm = findMethod(cls, QString::fromLatin1(AcKeyword::kDispose));
            if (dm) {
              const QString fname = QStringLiteral("%1.%2").arg(cls, dm->name);
              auto it = m_module->funcUnits.find(fname);
              if (it != m_module->funcUnits.end()) {
                accore::AcJsonValue oldThis = m_currentThis;
                m_currentThis = val;
                callUnit(it.value(), 0, fname);
                m_currentThis = oldThis;
              }
            }
          }
        }
      }
    }
  }
  auto scope = m_scopeStack.takeLast();
  m_constVars.removeLast();
  for (auto it = scope.begin(); it != scope.end(); ++it) releaseDeep(it.value());
  constexpr int kGcGrowthThreshold = 64;
  const int objCount = m_objMgr.objectCount();
  if (m_scopeStack.isEmpty() || objCount - m_objectsAtLastGc >= kGcGrowthThreshold) {
    collectCycles();
    m_objectsAtLastGc = m_objMgr.objectCount();
  }
}

bool AcVm::containsVar(const QString &name) const {
  for (int i = m_scopeStack.size() - 1; i >= 0; --i) {
    if (m_scopeStack[i].contains(name)) return true;
  }
  return false;
}

void AcVm::adjustScope(int absDepth) {
  while (m_scopeStack.size() < absDepth) pushScope();
  while (m_scopeStack.size() > absDepth) popScope();
}

// ═════════════════════════════════════════════════════════════════════════════
//  类支持
// ═════════════════════════════════════════════════════════════════════════════

const MethodDef *AcVm::findMethod(const QString &className, const QString &methodName) const {
  if (!m_classes.contains(className)) return nullptr;
  const ClassDef &cd = m_classes[className];
  for (const auto &m : cd.methods) {
    if (m.name == methodName) return &m;
  }
  if (!cd.baseClass.isEmpty()) return findMethod(cd.baseClass, methodName);
  return nullptr;
}

void AcVm::initStatic(const QString &className) {
  if (m_staticInited.contains(className)) return;
  if (!m_classes.contains(className)) return;
  const ClassDef &cd = m_classes[className];
  // 继承链先行（与解释器 initStaticVars 一致）
  if (!cd.baseClass.isEmpty()) initStatic(cd.baseClass);
  accore::AcJsonValue sv = accore::AcJsonValue::makeObject();
  if (!cd.baseClass.isEmpty() && m_staticVars.contains(cd.baseClass)) {
    sv = m_staticVars[cd.baseClass];  // COW 浅拷贝：继承基类静态成员
  }
  m_staticVars[className] = sv;
  m_staticInited.insert(className);
  // 执行静态初始化单元（若存在）：栈上交替 (key, value) 对
  const QString unitName = QStringLiteral("<static:%1>").arg(className);
  auto it = m_module->funcUnits.constFind(unitName);
  if (it == m_module->funcUnits.constEnd()) return;
  callUnit(it.value(), 0, unitName);
  if (!m_error.isEmpty()) return;
  int nPairs = 0;
  for (const auto &p : cd.properties) {
    if (p.isStatic) ++nPairs;
  }
  QVector<accore::AcJsonValue> vals;
  for (int i = 0; i < 2 * nPairs && !m_vstack.isEmpty(); ++i) vals.prepend(m_vstack.takeLast());
  accore::AcJsonValue obj = m_staticVars[className];
  for (int i = 0; i < nPairs && 2 * i + 1 < vals.size(); ++i) {
    const QString &key = vals[2 * i].toString();
    accore::AcJsonValue v = vals[2 * i + 1];
    retainIfInstance(v);
    obj.set(key, v);
  }
  m_staticVars[className] = obj;
}

accore::AcJsonValue AcVm::makeInstanceObject(const QString &className) {
  // v1：先建未注册实例，属性由 execConstructor 从初始化单元填充后注册
  return accore::AcJsonValue::makeInstance(className);
}

void AcVm::execConstructor(const QString &className, accore::AcJsonValue &instance, int argc,
                           const QVector<accore::AcJsonValue> &args) {
  // 1) 执行实例初始化单元：栈上按属性序压入初始值（base 在前）
  const QString initName = QStringLiteral("<init:%1>").arg(className);
  auto initIt = m_module->funcUnits.constFind(initName);
  if (initIt != m_module->funcUnits.constEnd()) {
    callUnit(initIt.value(), 0, initName);
    if (!m_error.isEmpty()) return;
  }
  // 2) 组装实例属性（按初始化单元压栈顺序）
  const ClassDef &cd = m_classes[className];
  int nProps = 0;
  if (!cd.baseClass.isEmpty()) {
    const auto bIt = m_classes.constFind(cd.baseClass);
    if (bIt != m_classes.constEnd()) {
      for (const auto &p : bIt.value().properties) {
        if (!p.isStatic) ++nProps;
      }
    }
  }
  for (const auto &p : cd.properties) {
    if (!p.isStatic) ++nProps;
  }
  QVector<accore::AcJsonValue> vals;
  for (int i = 0; i < nProps && !m_vstack.isEmpty(); ++i) vals.prepend(m_vstack.takeLast());
  accore::AcJsonValue inst = instance;
  int vi = 0;
  if (!cd.baseClass.isEmpty()) {
    const auto bIt = m_classes.constFind(cd.baseClass);
    if (bIt != m_classes.constEnd()) {
      for (const auto &p : bIt.value().properties) {
        if (p.isStatic) continue;
        if (vi < vals.size()) {
          retainIfInstance(vals[vi]);
          inst.set(p.key, vals[vi]);
        }
        ++vi;
      }
    }
  }
  for (const auto &p : cd.properties) {
    if (p.isStatic) continue;
    if (vi < vals.size()) {
      retainIfInstance(vals[vi]);
      inst.set(p.key, vals[vi]);
    }
    ++vi;
  }
  instance = m_objMgr.registerInstance(inst, className);
  // 3) 构造器
  const MethodDef *ctor = nullptr;
  for (const auto &m : cd.methods) {
    if (m.name == QStringLiteral("constructor")) {
      ctor = &m;
      break;
    }
  }
  if (ctor) {
    const QString fname = QStringLiteral("%1.%2").arg(className, ctor->name);
    auto it = m_module->funcUnits.find(fname);
    if (it != m_module->funcUnits.end()) {
      accore::AcJsonValue oldThis = m_currentThis;
      accore::AcJsonValue oldModified = m_modifiedThis;
      m_currentThis = instance;
      m_modifiedThis = instance;
      for (auto &a : args) m_vstack.append(a);
      callUnit(it.value(), int(args.size()), fname);
      if (!m_error.isEmpty()) return;
      instance = m_modifiedThis;
      m_currentThis = oldThis;
      m_modifiedThis = oldModified;
      return;
    }
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  运算 / 复合赋值（复刻解释器 evalBinary / applyCompoundOp）
// ═════════════════════════════════════════════════════════════════════════════

accore::AcJsonValue AcVm::binOp(AcOpcode op, const accore::AcJsonValue &l,
                                const accore::AcJsonValue &r, int line) {
  switch (op) {
    case AcOpcode::kAdd: {
      if (l.isString() || r.isString()) {
        // 与解释器一致：字符串拼接时字符串取原值，其余经 AcValueStr
        auto valToStr = [](const accore::AcJsonValue &v) -> QString {
          if (v.isString()) return v.toString();
          return AcValueStr::toString(v);
        };
        return accore::AcJsonValue(valToStr(l) + valToStr(r));
      }
      return accore::AcJsonValue(l.toDouble() + r.toDouble());
    }
    case AcOpcode::kSub: return accore::AcJsonValue(l.toDouble() - r.toDouble());
    case AcOpcode::kMul: return accore::AcJsonValue(l.toDouble() * r.toDouble());
    case AcOpcode::kDiv: {
      if (r.toDouble() == 0.0) {
        setError(QStringLiteral("division by zero"), line);
        return accore::AcJsonValue();
      }
      return accore::AcJsonValue(l.toDouble() / r.toDouble());
    }
    case AcOpcode::kMod: {
      if (r.toDouble() == 0.0) {
        setError(QStringLiteral("modulo by zero"), line);
        return accore::AcJsonValue();
      }
      return accore::AcJsonValue(std::fmod(l.toDouble(), r.toDouble()));
    }
    case AcOpcode::kEq: return accore::AcJsonValue(AcInterpreter::compareValues(l, r) == 0);
    case AcOpcode::kNeq: return accore::AcJsonValue(AcInterpreter::compareValues(l, r) != 0);
    case AcOpcode::kLt: return accore::AcJsonValue(l.toDouble() < r.toDouble());
    case AcOpcode::kGt: return accore::AcJsonValue(l.toDouble() > r.toDouble());
    case AcOpcode::kLte: return accore::AcJsonValue(l.toDouble() <= r.toDouble());
    case AcOpcode::kGte: return accore::AcJsonValue(l.toDouble() >= r.toDouble());
    default: return accore::AcJsonValue();
  }
}

accore::AcJsonValue AcVm::applyCompoundOp(const accore::AcJsonValue &cur,
                                          const accore::AcJsonValue &delta, int op, int line) {
  switch (CompoundOp(op)) {
    case CompoundOp::kAdd: {
      if (cur.isString() || delta.isString()) {
        auto valToStr = [](const accore::AcJsonValue &v) -> QString {
          if (v.isString()) return v.toString();
          return AcValueStr::toString(v);
        };
        return accore::AcJsonValue(valToStr(cur) + valToStr(delta));
      }
      return accore::AcJsonValue(cur.toDouble() + delta.toDouble());
    }
    case CompoundOp::kSub: return accore::AcJsonValue(cur.toDouble() - delta.toDouble());
    case CompoundOp::kMul: return accore::AcJsonValue(cur.toDouble() * delta.toDouble());
    case CompoundOp::kDiv: {
      if (delta.toDouble() == 0.0) {
        setError(QStringLiteral("division by zero"), line);
        return accore::AcJsonValue();
      }
      return accore::AcJsonValue(cur.toDouble() / delta.toDouble());
    }
    case CompoundOp::kMod: {
      if (delta.toDouble() == 0.0) {
        setError(QStringLiteral("modulo by zero"), line);
        return accore::AcJsonValue();
      }
      return accore::AcJsonValue(std::fmod(cur.toDouble(), delta.toDouble()));
    }
    default: return delta;
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  引用计数 / GC（复刻解释器）
// ═════════════════════════════════════════════════════════════════════════════

void AcVm::setError(const QString &msg, int line) {
  m_error = line > 0 ? QStringLiteral("%1 at line %2").arg(msg).arg(line) : msg;
}

void AcVm::retainIfInstance(const accore::AcJsonValue &val) {
  if (!AcObjectManager::isManagedInstance(val)) return;
  m_objMgr.retain(AcObjectManager::getObjId(val));
}

void AcVm::releaseIfInstance(const accore::AcJsonValue &val) {
  if (!AcObjectManager::isManagedInstance(val)) return;
  m_objMgr.release(AcObjectManager::getObjId(val));
}

void AcVm::releaseIfInstanceWithDestruct(const accore::AcJsonValue &val) {
  if (!AcObjectManager::isManagedInstance(val)) return;
  m_objMgr.release(AcObjectManager::getObjId(val));
  QVector<AcObjectManager::DestructInfo> pending = m_objMgr.takePendingDestructs();
  for (const auto &info : pending) processDestructInfo(info);
}

void AcVm::processDestructInfo(const AcObjectManager::DestructInfo &info) {
  const accore::AcJsonValue &obj = info.instance;
  for (const auto &m : obj.members()) releaseDeep(m.value);
  if (FunMgr::ins().contains(info.className, QString::fromLatin1(AcRuntime::kDestructor))) {
    FunMgr::ins().call(info.className, QString::fromLatin1(AcRuntime::kDestructor), obj,
                       accore::AcJsonValue::makeArray());
  } else if (m_classes.contains(info.className) && !m_classes[info.className].isNative) {
    const MethodDef *dtor = findMethod(info.className, QString::fromLatin1(AcRuntime::kDestructor));
    if (dtor) {
      const QString fname = QStringLiteral("%1.%2").arg(info.className, dtor->name);
      auto it = m_module->funcUnits.find(fname);
      if (it != m_module->funcUnits.end()) {
        accore::AcJsonValue oldThis = m_currentThis;
        m_currentThis = obj;
        callUnit(it.value(), 0, fname);
        m_currentThis = oldThis;
      }
    }
  }
}

void AcVm::traverseNested(const accore::AcJsonValue &val,
                          const std::function<void(const accore::AcJsonValue &)> &onChild) {
  if (val.isArray()) {
    for (const accore::AcJsonValue &item : val.items()) onChild(item);
  } else if (val.isObject()) {
    for (const auto &m : val.members()) onChild(m.value);
  }
}

void AcVm::releaseDeep(const accore::AcJsonValue &val) {
  if (AcObjectManager::isManagedInstance(val)) {
    releaseIfInstanceWithDestruct(val);
    return;
  }
  traverseNested(val, [this](const accore::AcJsonValue &child) { releaseDeep(child); });
}

void AcVm::markFromValue(const accore::AcJsonValue &val) {
  if (AcObjectManager::isManagedInstance(val)) {
    QString objId = AcObjectManager::getObjId(val);
    if (m_objMgr.isMarked(objId) || !m_objMgr.contains(objId)) return;
    m_objMgr.mark(objId);
    accore::AcJsonValue obj = m_objMgr.getAcObject(objId);
    traverseNested(obj, [this](const accore::AcJsonValue &child) { markFromValue(child); });
    return;
  }
  traverseNested(val, [this](const accore::AcJsonValue &child) { markFromValue(child); });
}

void AcVm::collectCycles() {
  auto &mgr = m_objMgr;
  if (mgr.objectCount() == 0) return;
  mgr.clearMarks();
  for (const auto &scope : m_scopeStack) {
    for (auto it = scope.begin(); it != scope.end(); ++it) markFromValue(it.value());
  }
  for (auto it = m_staticVars.begin(); it != m_staticVars.end(); ++it) {
    for (const auto &m : it.value().members()) markFromValue(m.value);
  }
  if (!m_currentThis.isEmpty()) {
    for (const auto &m : m_currentThis.members()) markFromValue(m.value);
  }
  if (!m_modifiedThis.isEmpty()) {
    for (const auto &m : m_modifiedThis.members()) markFromValue(m.value);
  }
  QVector<AcObjectManager::DestructInfo> cycles = mgr.collectUnmarked();
  for (const auto &info : cycles) processDestructInfo(info);
}

// ═════════════════════════════════════════════════════════════════════════════
//  错误处理（try/catch）
// ═════════════════════════════════════════════════════════════════════════════

void AcVm::handleError(int unitIdx, int pc) {
  // 从内到外找覆盖当前 pc 的活动 try
  for (int i = m_tryStack.size() - 1; i >= 0; --i) {
    const ActiveTry &t = m_tryStack[i];
    const AcTryEntry &e = m_module->funcs[unitIdx].tryTable[t.tryIdx];
    const QString errMsg = m_error;
    if (pc >= e.tryStart && pc <= e.tryEnd) {
      if (e.catchAddr >= 0) {
        m_pendingCatchErr = errMsg;
        m_pendingPropagateErr.clear();
        m_error.clear();
        // 弹出 try 栈到该层
        m_tryStack.resize(i);
        m_frames.last().pc = e.catchAddr;
        return;
      }
      if (e.finallyAddr >= 0) {
        m_pendingPropagateErr = errMsg;
        m_error.clear();
        m_tryStack.resize(i);
        m_frames.last().pc = e.finallyAddr;
        return;
      }
      // 无 catch 无 finally：继续向外传播
      m_tryStack.resize(i);
      return;
    }
  }
  // 无 try 覆盖：错误保持（向上传播，终止当前单元）
}
