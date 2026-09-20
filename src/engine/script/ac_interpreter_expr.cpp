/**
 * @file ac_interpreter_expr.cpp
 * @brief 解释器表达式求值实现文件
 */

#include <QDir>
#include <QFile>
#include <QFileInfo>
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
//  表达式求值
// ═════════════════════════════════════════════════════════════════════════════

namespace {
/// 运行时错误消息常量（多处复用，统一文案与维护）
const QString kErrUndefinedVariable = QStringLiteral("undefined variable '%1'");
const QString kErrUndefinedClass = QStringLiteral("undefined class '%1'");
}  // namespace

accore::AcJsonValue AcInterpreter::resolveClassAccess(const QString &className,
                                                      const QString &propName) {
  if (!m_classes.contains(className)) return accore::AcJsonValue();
  const ClassDef &cd = m_classes[className];
  if (!m_staticInited.contains(className)) {
    initStaticVars(cd);
  }
  for (const auto &prop : cd.properties) {
    if (prop.isStatic && prop.key == propName) {
      accore::AcJsonValue sv = m_staticVars.value(className);
      return sv.value(propName);
    }
  }
  return accore::AcJsonValue();
}

accore::AcJsonValue AcInterpreter::resolveClassPropOrMethod(const QString &className,
                                                            const QString &propName) {
  accore::AcJsonValue staticVal = resolveClassAccess(className, propName);
  if (!staticVal.isNull()) return staticVal;
  if (!m_classes.contains(className)) return accore::AcJsonValue();
  const ClassDef &cd = m_classes[className];
  for (const auto &m : cd.methods) {
    if (m.isStatic && m.name == propName) return makeClassRef(className);
  }
  return accore::AcJsonValue();
}

void AcInterpreter::setError(const QString &msg, int line) {
  if (line > 0) {
    m_error = QStringLiteral("%1 at line %2").arg(msg, QString::number(line));
  } else {
    m_error = msg;
  }
}

accore::AcJsonValue AcInterpreter::makeClassRef(const QString &className) const {
  // 类引用：专用的运行时种类（类名存于值内，不再用 __class__ 魔法键编码）
  return accore::AcJsonValue::makeClassRef(className);
}

accore::AcJsonValue AcInterpreter::getPropertyValue(const accore::AcJsonValue &obj,
                                                    const QString &prop, const QString &ident) {
  if (prop == QStringLiteral("length")) {
    if (obj.isString()) return accore::AcJsonValue(int(obj.toString().length()));
    if (obj.isArray()) return accore::AcJsonValue(obj.size());
  }
  if (obj.isObject()) return obj.value(prop);
  if (obj.isArray()) {
    bool ok = false;
    int idx = prop.toInt(&ok);
    if (ok) {
      if (idx >= 0 && idx < obj.size()) return obj.at(idx);
    }
  }
  if (obj.isNull()) {
    QString enumMemberName = QStringLiteral("%1.%2").arg(ident, prop);
    if (containsVar(enumMemberName)) return resolveVar(enumMemberName);
  }
  return accore::AcJsonValue();
}

accore::AcJsonValue AcInterpreter::evalPropertyChain(const Expr &expr) {
  accore::AcJsonValue obj;
  if (expr.propObject) {
    // 类名限定访问（X.member）：变量优先于类名；无同名变量且是类名时按静态成员解析，
    // 避免「先报未定义变量再 m_error.clear() 恢复」的脆弱错误吞掉模式
    if (expr.propObject->kind == Expr::kIdent && !containsVar(expr.propObject->ident) &&
        m_classes.contains(expr.propObject->ident)) {
      const QString &clsName = expr.propObject->ident;
      accore::AcJsonValue result = resolveClassPropOrMethod(clsName, expr.prop);
      if (!result.isNull()) return result;
      // 未赋值/不存在的静态成员：在类引用上取缺失键返回 Null（`X.y == null` 成立）
      return getPropertyValue(makeClassRef(clsName), expr.prop, expr.ident);
    }
    obj = evalExpr(*expr.propObject);
    if (!m_error.isEmpty()) return accore::AcJsonValue();
    if (expr.isOptional && obj.isNull()) return accore::AcJsonValue();  // ?. 短路
  } else if (expr.ident == QString::fromLatin1(AcKeyword::kThis)) {
    obj = m_currentThis;
  } else if (expr.ident == QString::fromLatin1(AcKeyword::kSuper)) {
    obj = m_currentThis;
  } else {
    obj = resolveVar(expr.ident);
    if (obj.isNull()) {
      if (expr.isOptional) return accore::AcJsonValue();  // ?. 短路：变量为 null
      QString enumMemberName = QStringLiteral("%1.%2").arg(expr.ident, expr.prop);
      if (containsVar(enumMemberName)) return resolveVar(enumMemberName);
      if (m_classes.contains(expr.ident)) {
        accore::AcJsonValue result = resolveClassPropOrMethod(expr.ident, expr.prop);
        if (!result.isNull()) return result;
        // 静态成员未赋值或不存在：回退为类引用对象继续通用属性读取——
        // classRef 上缺失键取值为 Null，保证 `X.member == null` 在未赋值时成立
        // （不能直接返回 classRef，否则未赋值静态属性会被误判为非空）
        return getPropertyValue(makeClassRef(expr.ident), expr.prop, expr.ident);
      }
      setError(QStringLiteral("variable '%1' is null").arg(expr.ident), expr.loc.line);
      return accore::AcJsonValue();
    }
  }
  return getPropertyValue(obj, expr.prop, expr.ident);
}

accore::AcJsonValue AcInterpreter::applyCompoundOp(const accore::AcJsonValue &currentVal,
                                                   const accore::AcJsonValue &newVal, CompoundOp op,
                                                   int line) {
  if (op == CompoundOp::kAdd) {
    if (currentVal.isString() || newVal.isString()) {
      // AcValueStr 已有 accore 原生重载，免转换
      QString ls = currentVal.isString() ? currentVal.toString() : AcValueStr::toString(currentVal);
      QString rs = newVal.isString() ? newVal.toString() : AcValueStr::toString(newVal);
      return accore::AcJsonValue(ls + rs);
    }
    return accore::AcJsonValue(currentVal.toDouble() + newVal.toDouble());
  }
  double left = currentVal.toDouble();
  double right = newVal.toDouble();
  switch (op) {
    case CompoundOp::kSub:
      return accore::AcJsonValue(left - right);
    case CompoundOp::kMul:
      return accore::AcJsonValue(left * right);
    case CompoundOp::kDiv:
      if (right == 0) {
        setError(QStringLiteral("division by zero"), line);
        return accore::AcJsonValue();
      }
      return accore::AcJsonValue(left / right);
    case CompoundOp::kMod:
      if (right == 0) {
        setError(QStringLiteral("modulo by zero"), line);
        return accore::AcJsonValue();
      }
      return accore::AcJsonValue(fmod(left, right));
    default:
      return newVal;
  }
}

/// 表达式求值嵌套深度上限：evalBinary/evalUnary 等经 evalExpr 递归求值子表达式，
/// 深嵌套 AST（受解析器 64 层限制）之外再兜底一层，防止任何来源的深 AST 打爆栈
static constexpr int kMaxEvalDepth = 128;

accore::AcJsonValue AcInterpreter::evalExpr(const Expr &expr) {
  if (m_exprDepth >= kMaxEvalDepth) {
    setError(QStringLiteral("expression nesting too deep (limit %1)").arg(kMaxEvalDepth),
             expr.loc.line);
    return accore::AcJsonValue();
  }
  ++m_exprDepth;
  struct DepthPop {
    int &d;
    ~DepthPop() { --d; }
  } pop{m_exprDepth};
  switch (expr.kind) {
    case Expr::kNull:
      return accore::AcJsonValue();

    case Expr::kBool:
      return accore::AcJsonValue(expr.boolVal);

    case Expr::kNumber:
      return accore::AcJsonValue(expr.numVal);

    case Expr::kString:
      return accore::AcJsonValue(expr.strVal);

    case Expr::kThis:
      return m_currentThis;

    case Expr::kIdent: {
      if (!containsVar(expr.ident)) {
        // 裸类名：求值为类引用对象（JS 语义：类名即类对象），不报「未定义变量」错误
        if (m_classes.contains(expr.ident)) return makeClassRef(expr.ident);
        setError(kErrUndefinedVariable.arg(expr.ident), expr.loc.line);
        return accore::AcJsonValue();
      }
      return resolveVar(expr.ident);
    }

    case Expr::kPropAccess:
      return evalPropertyChain(expr);

    case Expr::kIndexAccess: {
      accore::AcJsonValue obj = evalExpr(*expr.left);
      accore::AcJsonValue idxVal = evalExpr(*expr.right);
      if (obj.isObject()) {
        QString key;
        if (idxVal.isString()) {
          key = idxVal.toString();
        } else {
          key = idxVal.isDouble() ? QString::number(idxVal.toDouble()) : idxVal.toString();
        }
        return obj.value(key);
      }
      if (obj.isArray()) {
        // safeJsonToInt 保持 QJsonValue 签名（ac_language.h），经 toQJsonValue 转换
        int idx = safeJsonToInt(idxVal.toQJsonValue());
        if (idx >= 0 && idx < obj.size()) return obj.at(idx);
        return accore::AcJsonValue();
      }
      if (obj.isString()) {
        int idx = safeJsonToInt(idxVal.toQJsonValue());
        QString s = obj.toString();
        if (idx >= 0 && idx < s.length()) return accore::AcJsonValue(QString(s[idx]));
        return accore::AcJsonValue();
      }
      setError(QStringLiteral("cannot access index on value"), expr.loc.line);
      return accore::AcJsonValue();
    }

    case Expr::kObject: {
      accore::AcJsonValue obj = accore::AcJsonValue::makeObject();
      for (const auto &e : expr.objEntries) {
        accore::AcJsonValue v = evalExpr(*e.value);
        if (!m_error.isEmpty()) return accore::AcJsonValue();
        retainIfInstance(v);
        obj.set(e.key, v);
      }
      return obj;
    }

    case Expr::kArray: {
      accore::AcJsonValue arr = accore::AcJsonValue::makeArray();
      for (const auto &e : expr.arrItems) {
        accore::AcJsonValue item = evalExpr(*e);
        if (!m_error.isEmpty()) return accore::AcJsonValue();
        retainIfInstance(item);
        arr.append(item);
      }
      return arr;
    }

    case Expr::kCoalesce: {
      // 空值合并：左侧非 null 直接返回（含 false/0 —— 与 || 的区别）
      accore::AcJsonValue l = evalExpr(*expr.left);
      if (!m_error.isEmpty()) return accore::AcJsonValue();
      if (!l.isNull()) return l;
      return evalExpr(*expr.right);
    }

    case Expr::kFuncCall:
      return callBuiltin(expr.funcCall.name, expr.funcCall.args, expr.loc.line);

    case Expr::kFuncExpr: {
      QString funcName = QStringLiteral("__lambda_%1").arg(++m_funcExprCounter);
      m_functions[funcName] = expr.funcExpr;
      m_functions[funcName].name = funcName;
      // 函数引用：专用运行时种类（函数名存于值内）
      return accore::AcJsonValue::makeFuncRef(funcName);
    }

    case Expr::kMethodCall:
      return evalMethodCall(expr);

    case Expr::kNewInstance:
      return evalNewInstance(expr);

    case Expr::kStaticAccess: {
      QString className = expr.className;
      if (!m_classes.contains(className)) {
        setError(kErrUndefinedClass.arg(className), expr.loc.line);
        return accore::AcJsonValue();
      }
      const ClassDef &cd = m_classes[className];

      if (!m_staticInited.contains(className)) {
        initStaticVars(cd);
      }

      for (const auto &prop : cd.properties) {
        if (prop.isStatic && prop.key == expr.prop) {
          accore::AcJsonValue sv = m_staticVars.value(className);
          return sv.value(expr.prop);
        }
      }

      accore::AcJsonValue callArgs = accore::AcJsonValue::makeArray();
      for (const auto &arg : expr.funcCall.args) {
        callArgs.append(evalExpr(*arg));
        if (!m_error.isEmpty()) return accore::AcJsonValue();
      }
      for (const auto &md : cd.methods) {
        if (md.isStatic && md.name == expr.prop) {
          return execMethod(md, accore::AcJsonValue::makeObject(), callArgs);
        }
      }

      setError(QStringLiteral("class '%1' has no static member '%2'").arg(className, expr.prop),
               expr.loc.line);
      return accore::AcJsonValue();
    }

    case Expr::kBinary:
      return evalBinary(expr);
    case Expr::kUnary:
      return evalUnary(expr);

    case Expr::kPreInc:
      return applyIncDec(expr, +1.0, false);
    case Expr::kPreDec:
      return applyIncDec(expr, -1.0, false);
    case Expr::kPostInc:
      return applyIncDec(expr, +1.0, true);
    case Expr::kPostDec:
      return applyIncDec(expr, -1.0, true);

    case Expr::kAssign: {
      accore::AcJsonValue value = evalExpr(*expr.right);
      if (!m_error.isEmpty()) return accore::AcJsonValue();
      // 左值为标识符：直接设置变量
      if (expr.left->kind == Expr::kIdent) {
        setVar(expr.left->ident, value);
      }
      // 左值为属性访问：设置对象属性（错误语义与语句级 assignToProperty 一致）
      else if (expr.left->kind == Expr::kPropAccess) {
        // 简单属性赋值：obj.prop = value
        if (expr.left->propObject && expr.left->propObject->kind == Expr::kIdent) {
          const QString &baseName = expr.left->propObject->ident;
          accore::AcJsonValue obj = resolveVar(baseName);
          if (!obj.isObject()) {
            setError(QStringLiteral("cannot set property '%1' on value").arg(expr.left->prop),
                     expr.loc.line);
            return accore::AcJsonValue();
          }
          obj.set(expr.left->prop, value);
          setVar(baseName, obj);
        } else {
          // 链式基（a.b.c = value）：语句级解析器走 kPropAssign/kIndexAssign，表达式级不支持
          setError(QStringLiteral("unsupported assignment target"), expr.loc.line);
        }
      }
      // 左值为索引访问：obj[key] = value（对象/数组语义与语句级 assignToIndex 一致）
      else if (expr.left->kind == Expr::kIndexAccess) {
        if (expr.left->left->kind == Expr::kIdent) {
          accore::AcJsonValue target = resolveVar(expr.left->left->ident);
          accore::AcJsonValue idxVal = evalExpr(*expr.left->right);
          if (!m_error.isEmpty()) return accore::AcJsonValue();
          assignToIndex(target, idxVal, value, *expr.left->left);
        }
      }
      return value;
    }

    case Expr::kTernary:
      if (isTruthy(evalExpr(*expr.left)))
        return evalExpr(*expr.right);
      else
        return evalExpr(*expr.operand);
  }

  return accore::AcJsonValue();
}

accore::AcJsonValue AcInterpreter::evalExprWithThis(const Expr &expr,
                                                    const accore::AcJsonValue &thisObj) {
  accore::AcJsonValue oldThis = m_currentThis;
  m_currentThis = thisObj;
  accore::AcJsonValue result = evalExpr(expr);
  m_currentThis = oldThis;
  return result;
}

// ═════════════════════════════════════════════════════════════════════════════
//  二元运算
// ═════════════════════════════════════════════════════════════════════════════

accore::AcJsonValue AcInterpreter::evalBinary(const Expr &expr) {
  // 逻辑与/或必须短路求值：左侧已能决定结果时不再求值右侧，
  // 否则 a != null && a.b 之类防御写法在 a 为空时右侧仍会触发求值错误
  if (expr.binOp == Expr::kOr || expr.binOp == Expr::kAnd) {
    accore::AcJsonValue l = evalExpr(*expr.left);
    if (expr.binOp == Expr::kOr) {
      if (isTruthy(l)) return l;
    } else {
      if (!isTruthy(l)) return l;
    }
    return evalExpr(*expr.right);
  }

  accore::AcJsonValue l = evalExpr(*expr.left);
  if (!m_error.isEmpty()) return accore::AcJsonValue();
  accore::AcJsonValue r = evalExpr(*expr.right);
  if (!m_error.isEmpty()) return accore::AcJsonValue();

  switch (expr.binOp) {
    case Expr::kAdd:
      if (l.isString() || r.isString()) {
        auto valToStr = [](const accore::AcJsonValue &v) -> QString {
          if (v.isString()) return v.toString();
          return AcValueStr::toString(v);
        };
        return accore::AcJsonValue(valToStr(l) + valToStr(r));
      }
      return accore::AcJsonValue(l.toDouble() + r.toDouble());
    case Expr::kSub:
      return accore::AcJsonValue(l.toDouble() - r.toDouble());
    case Expr::kMul:
      return accore::AcJsonValue(l.toDouble() * r.toDouble());
    case Expr::kDiv:
      if (r.toDouble() == 0.0) {
        setError(QStringLiteral("division by zero"), expr.loc.line);
        return accore::AcJsonValue();
      }
      return accore::AcJsonValue(l.toDouble() / r.toDouble());
    case Expr::kMod:
      if (r.toDouble() == 0.0) {
        setError(QStringLiteral("modulo by zero"), expr.loc.line);
        return accore::AcJsonValue();
      }
      return accore::AcJsonValue(fmod(l.toDouble(), r.toDouble()));
    case Expr::kEq:
      return compareValues(l, r) == 0;
    case Expr::kNeq:
      return compareValues(l, r) != 0;
    case Expr::kLt:
      return compareValues(l, r) < 0;
    case Expr::kGt:
      return compareValues(l, r) > 0;
    case Expr::kLte:
      return compareValues(l, r) <= 0;
    case Expr::kGte:
      return compareValues(l, r) >= 0;
  }
  return accore::AcJsonValue();
}

int AcInterpreter::compareValues(const accore::AcJsonValue &l, const accore::AcJsonValue &r) {
  if (l.isString() && r.isString()) {
    return l.toString().compare(r.toString());
  }
  if (l.isDouble() && r.isDouble()) {
    double diff = l.toDouble() - r.toDouble();
    if (diff < 0) return -1;
    if (diff > 0) return 1;
    return 0;
  }
  if (l.isBool() && r.isBool()) {
    if (l.toBool() == r.toBool()) return 0;
    return l.toBool() ? 1 : -1;
  }
  // JS 语义：null == undefined 为 true（宽松相等）
  // 迁移后运行时值无 Undefined 类型，缺失/未定义统一为 Null
  auto isNullOrUndef = [](const accore::AcJsonValue &v) { return v.isNull(); };
  if (isNullOrUndef(l) || isNullOrUndef(r)) {
    if (isNullOrUndef(l) && isNullOrUndef(r)) return 0;
    return isNullOrUndef(l) ? -1 : 1;
  }
  // 容器与运行时种类的相等语义（对齐 JS；修复此前回退字符串比较导致任意两对象恒为相等）：
  // - 实例：引用相等（objId）；函数引用：函数名相等；类引用：类名相等
  // - 数组/普通对象：结构化深比较（.ac 数据为值语义，快照一致即相等）
  // - 混合种类：按种类号全序（仅供 < > 排序使用，无语义承诺）
  auto containerKind = [](const accore::AcJsonValue &v) {
    if (v.isInstance()) return 1;
    if (v.isClassRef()) return 2;
    if (v.isFuncRef()) return 3;
    if (v.isArray()) return 4;
    if (v.isObject()) return 5;
    return 0;
  };
  const int lk = containerKind(l);
  const int rk = containerKind(r);
  if (lk != 0 || rk != 0) {
    if (lk != rk) return lk < rk ? -1 : 1;
    if (lk == 1) return l.instanceObjId().compare(r.instanceObjId());
    if (lk == 2) return l.instanceClass().compare(r.instanceClass());
    if (lk == 3) return l.funcRefName().compare(r.funcRefName());
    if (l.size() != r.size()) return l.size() < r.size() ? -1 : 1;
    if (lk == 4) {
      for (int i = 0; i < l.size(); ++i) {
        const int c = compareValues(l.at(i), r.at(i));
        if (c != 0) return c;
      }
      return 0;
    }
    const auto &lm = l.members();
    const auto &rm = r.members();
    for (int i = 0; i < lm.size(); ++i) {
      if (lm.at(i).key != rm.at(i).key) return lm.at(i).key < rm.at(i).key ? -1 : 1;
      const int c = compareValues(lm.at(i).value, rm.at(i).value);
      if (c != 0) return c;
    }
    return 0;
  }
  QString ls =
      l.isString() ? l.toString() : (l.isDouble() ? QString::number(l.toDouble()) : l.toString());
  QString rs =
      r.isString() ? r.toString() : (r.isDouble() ? QString::number(r.toDouble()) : r.toString());
  return ls.compare(rs);
}

QString AcInterpreter::inferTypeName(const accore::AcJsonValue &val) {
  if (val.isBool()) return QString::fromLatin1(AcTypeName::kBoolean);
  if (val.isDouble()) return QString::fromLatin1(AcTypeName::kNumber);
  if (val.isString()) return QString::fromLatin1(AcTypeName::kString);
  if (val.isArray()) return QString::fromLatin1(AcTypeName::kArray);
  if (val.isInstance() || val.isClassRef()) return val.instanceClass();
  if (val.isFuncRef()) return QStringLiteral("Function");
  if (val.isObject()) return QString::fromLatin1(AcTypeName::kObject);
  if (val.isNull()) return QString::fromLatin1(AcTypeName::kNull);
  return QString::fromLatin1(AcTypeName::kAny);
}

void AcInterpreter::recordInferredType(const QString &name, const accore::AcJsonValue &val) {
  m_inferredTypes[name] = inferTypeName(val);
}

accore::AcJsonValue AcInterpreter::callFunctionValue(const accore::AcJsonValue &funcRef,
                                                     const accore::AcJsonValue &callArgs) {
  if (!funcRef.isFuncRef()) {
    m_error = QStringLiteral("callback is not a function");
    return accore::AcJsonValue();
  }
  const auto it = m_functions.find(funcRef.funcRefName());
  if (it == m_functions.end()) {
    m_error = QStringLiteral("callback function '%1' not found").arg(funcRef.funcRefName());
    return accore::AcJsonValue();
  }
  return execUserFunction(*it, callArgs);
}

QString AcInterpreter::takeError() {
  QString e = m_error;
  m_error.clear();
  return e;
}

accore::AcJsonValue AcInterpreter::evalObjectBuiltin(const accore::AcJsonValue &obj,
                                                     const QString &method,
                                                     const std::vector<std::unique_ptr<Expr>> &args,
                                                     int line) {
  if (method == QStringLiteral("keys")) {
    accore::AcJsonValue out = accore::AcJsonValue::makeArray();
    for (const auto &m : obj.members()) out.append(accore::AcJsonValue(m.key));
    return out;
  }
  if (method == QStringLiteral("values")) {
    accore::AcJsonValue out = accore::AcJsonValue::makeArray();
    for (const auto &m : obj.members()) out.append(m.value);
    return out;
  }
  if (method == QStringLiteral("has")) {
    if (args.empty()) {
      setError(QStringLiteral("has() requires 1 argument"), line);
      return accore::AcJsonValue();
    }
    accore::AcJsonValue k = evalExpr(*args[0]);
    if (!m_error.isEmpty()) return accore::AcJsonValue();
    return accore::AcJsonValue(obj.has(k.toString()));
  }
  // size()
  return accore::AcJsonValue(obj.size());
}

accore::AcJsonValue AcInterpreter::evalUnary(const Expr &expr) {
  accore::AcJsonValue val = evalExpr(*expr.operand);
  if (!m_error.isEmpty()) return accore::AcJsonValue();
  switch (expr.unaryOp) {
    case Expr::kNot:
      return accore::AcJsonValue(!isTruthy(val));
  }
  return accore::AcJsonValue();
}

// applyIncDec — ++/-- 统一实现（前置/后置）
// 左值支持三种：标识符（setVar）、属性 obj.prop（回写宿主对象）、
// 索引 obj[key]（assignToIndex）。此前仅标识符回写，属性/索引自增会静默丢失。
accore::AcJsonValue AcInterpreter::applyIncDec(const Expr &expr, double delta, bool postReturnOld) {
  accore::AcJsonValue val = evalExpr(*expr.operand);
  if (!m_error.isEmpty()) return accore::AcJsonValue();

  const double oldVal = val.toDouble();
  const double newVal = oldVal + delta;
  const accore::AcJsonValue result = postReturnOld ? val : accore::AcJsonValue(newVal);
  const Expr &lv = *expr.operand;

  if (lv.kind == Expr::kIdent) {
    setVar(lv.ident, accore::AcJsonValue(newVal));
  } else if (lv.kind == Expr::kPropAccess) {
    // 属性自增回写。基名两种形态：
    //   简单访问 o.n++   —— parsePrimary 构造，基名在 ident，propObject 为空
    //   链式访问 a.b.n++ —— parsePostfix 构造，基表达式在 propObject（仅支持基为标识符）
    QString baseName;
    if (!lv.ident.isEmpty()) {
      baseName = lv.ident;
    } else if (lv.propObject && lv.propObject->kind == Expr::kIdent) {
      baseName = lv.propObject->ident;
    }
    if (baseName.isEmpty()) {
      setError(QStringLiteral("unsupported increment/decrement target"), expr.loc.line);
      return accore::AcJsonValue();
    }
    accore::AcJsonValue obj = resolveVar(baseName);
    if (!obj.isObject()) {
      setError(QStringLiteral("cannot set property '%1' on value").arg(lv.prop), expr.loc.line);
      return accore::AcJsonValue();
    }
    obj.set(lv.prop, accore::AcJsonValue(newVal));
    setVar(baseName, obj);
  } else if (lv.kind == Expr::kIndexAccess && lv.left->kind == Expr::kIdent) {
    // 索引自增：obj[key]++ / arr[i]++
    accore::AcJsonValue target = resolveVar(lv.left->ident);
    accore::AcJsonValue idxVal = evalExpr(*lv.right);
    if (!m_error.isEmpty()) return accore::AcJsonValue();
    assignToIndex(target, idxVal, accore::AcJsonValue(newVal), *lv.left);
  }
  return result;
}

// ═════════════════════════════════════════════════════════════════════════════
//  内置函数调用
// ═════════════════════════════════════════════════════════════════════════════

accore::AcJsonValue AcInterpreter::callBuiltin(const QString &name,
                                               const std::vector<std::unique_ptr<Expr>> &args,
                                               int line) {
  accore::AcJsonValue arr = accore::AcJsonValue::makeArray();
  for (const auto &a : args) arr.append(evalExpr(*a));
  if (!m_error.isEmpty()) return accore::AcJsonValue();

  if (name == AcBuiltin::kCall) {
    if (arr.size() < 2) {
      m_error = QStringLiteral("call() requires at least 2 arguments");
      return accore::AcJsonValue();
    }
    QString cls = arr.at(0).toString();
    QString func = arr.at(1).toString();
    // 收集第 3 个及以后的所有参数（call("类","方法", 参数...)）
    accore::AcJsonValue callArgs = accore::AcJsonValue::makeArray();
    for (int i = 2; i < arr.size(); ++i) callArgs.append(arr.at(i));
    accore::AcJsonValue r = FunMgr::ins().call(cls, func, callArgs);
    QString err = FunMgr::takeError();
    if (!err.isEmpty()) {
      setError(err, line);
      return accore::AcJsonValue();
    }
    return r;
  }

  const QString builtinClass = QString::fromLatin1(AcRuntime::kBuiltinClass);
  if (FunMgr::ins().contains(builtinClass, name)) {
    FunBuiltin::setCurrentLine(line);
    accore::AcJsonValue r = FunMgr::ins().call(builtinClass, name, arr);
    FunBuiltin::setCurrentLine(0);
    QString err = FunMgr::takeError();
    if (!err.isEmpty()) {
      setError(err, line);
      return accore::AcJsonValue();
    }
    return r;
  }

  auto it = m_functions.find(name);
  if (it != m_functions.end()) return execUserFunction(*it, arr);

  if (containsVar(name)) {
    accore::AcJsonValue varVal = resolveVar(name);
    if (varVal.isFuncRef()) {
      QString funcName = varVal.funcRefName();
      auto fi = m_functions.find(funcName);
      if (fi != m_functions.end()) return execUserFunction(*fi, arr);
    }
  }

  setError(QStringLiteral("unknown function '%1'").arg(name), line);
  return accore::AcJsonValue();
}

accore::AcJsonValue AcInterpreter::evalJSONBuiltin(const Expr &expr) {
  const QString &method = expr.methodCall.methodName;
  if (method == QStringLiteral("parse")) {
    if (expr.methodCall.args.empty()) {
      setError(QStringLiteral("JSON.parse() requires 1 argument"), expr.loc.line);
      return accore::AcJsonValue();
    }
    accore::AcJsonValue argVal = evalExpr(*expr.methodCall.args[0]);
    if (!argVal.isString()) {
      setError(QStringLiteral("JSON.parse() argument must be a string"), expr.loc.line);
      return accore::AcJsonValue();
    }
    // 改走自有解析器：对象键保序（迁移核心收益），并支持 JSON5 超集（注释/单引号/尾逗号）
    bool ok = false;
    QString err;
    accore::AcJsonValue parsed = accore::AcJsonValue::parse(argVal.toString(), &ok, &err);
    if (!ok) {
      setError(QStringLiteral("JSON.parse() error: %1").arg(err), expr.loc.line);
      return accore::AcJsonValue();
    }
    return parsed;
  }
  if (method == QStringLiteral("stringify")) {
    if (expr.methodCall.args.empty()) {
      setError(QStringLiteral("JSON.stringify() requires 1 argument"), expr.loc.line);
      return accore::AcJsonValue();
    }
    accore::AcJsonValue argVal = evalExpr(*expr.methodCall.args[0]);
    if (!argVal.isObject() && !argVal.isArray()) return accore::AcJsonValue(argVal.toString());
    return accore::AcJsonValue(argVal.serialize(false));
  }
  setError(QStringLiteral("JSON has no method '%1'").arg(method), expr.loc.line);
  return accore::AcJsonValue();
}

accore::AcJsonValue AcInterpreter::resolveMethodCallTarget(const Expr &expr) {
  bool isChained = (expr.methodCall.object != nullptr);
  bool isSuper = (expr.methodCall.objName == QString::fromLatin1(AcKeyword::kSuper));
  accore::AcJsonValue objVal;

  if (isChained) {
    objVal = evalExpr(*expr.methodCall.object);
    if (!m_error.isEmpty()) return accore::AcJsonValue();
    // 类名经 kIdent 求值直接得到类引用对象；此处仅拦截真正的空基调用
    if (objVal.isNull()) {
      if (expr.methodCall.isOptional) return accore::AcJsonValue();  // ?. 短路
      setError(QStringLiteral("method call on null value"), expr.loc.line);
      return accore::AcJsonValue();
    }
  } else if (isSuper) {
    objVal = m_currentThis;
  } else {
    objVal = resolveVar(expr.methodCall.objName);
    if (expr.methodCall.objName != QString::fromLatin1(AcKeyword::kThis)) {
      if (m_classes.contains(expr.methodCall.objName)) {
        objVal = makeClassRef(expr.methodCall.objName);
      } else if (!containsVar(expr.methodCall.objName)) {
        // 变量根本未定义（类似 TS 的 "Cannot find name 'x'"）
        setError(
            QStringLiteral("undefined variable '%1' in method call").arg(expr.methodCall.objName),
            expr.loc.line);
        return accore::AcJsonValue();
      } else if (objVal.isNull()) {
        // 变量已定义但值为 null（类似 TS 的 "Cannot read properties of null"）
        setError(QStringLiteral("cannot call method '%1' on null value of variable '%2'")
                     .arg(expr.methodCall.methodName, expr.methodCall.objName),
                 expr.loc.line);
        return accore::AcJsonValue();
      }
    }
  }
  return objVal;
}

accore::AcJsonValue AcInterpreter::evalMethodCall(const Expr &expr) {
  bool isChained = (expr.methodCall.object != nullptr);
  bool isSuper = (expr.methodCall.objName == QString::fromLatin1(AcKeyword::kSuper));

  if (!isChained && !isSuper && expr.methodCall.objName == QStringLiteral("JSON")) {
    return evalJSONBuiltin(expr);
  }

  accore::AcJsonValue objVal = resolveMethodCallTarget(expr);
  if (!m_error.isEmpty()) return accore::AcJsonValue();
  if (expr.methodCall.isOptional && objVal.isNull()) {
    return accore::AcJsonValue();  // ?. 短路：对象为 null 时整体返回 null
  }

  if (objVal.isString()) {
    return evalStringBuiltin(objVal.toString(), expr.methodCall.methodName, expr.methodCall.args,
                             expr.loc.line);
  }

  if (objVal.isArray()) {
    accore::AcJsonValue modifiedArr;
    accore::AcJsonValue result = evalArrayBuiltin(objVal, expr.methodCall.methodName,
                                                  expr.methodCall.args, expr.loc.line, modifiedArr);
    if (!modifiedArr.isNull()) {
      if (isChained) {
        if (expr.methodCall.object && expr.methodCall.object->kind == Expr::kPropAccess &&
            !expr.methodCall.object->prop.isEmpty()) {
          const Expr &propObj = *expr.methodCall.object;
          if (propObj.ident == QString::fromLatin1(AcKeyword::kThis)) {
            m_currentThis.set(propObj.prop, modifiedArr);
            m_modifiedThis.set(propObj.prop, modifiedArr);
          } else if (!propObj.ident.isEmpty()) {
            if (containsVar(propObj.ident)) {
              accore::AcJsonValue varObj = resolveVar(propObj.ident);
              varObj.set(propObj.prop, modifiedArr);
              setVar(propObj.ident, varObj);
            }
          }
        }
      } else if (expr.methodCall.objName == QString::fromLatin1(AcKeyword::kThis)) {
        m_currentThis = modifiedArr;
        m_modifiedThis = modifiedArr;
      } else {
        setVar(expr.methodCall.objName, modifiedArr);
      }
    }
    return result;
  }

  if (!objVal.isObject() && !objVal.isClassRef()) {
    QString name = isChained ? QStringLiteral("chain expression") : expr.methodCall.objName;
    QString type = objVal.isDouble()   ? QString::fromLatin1(AcTypeName::kNumber)
                   : objVal.isBool()   ? QString::fromLatin1(AcTypeName::kBool)
                   : objVal.isNull()   ? QString::fromLatin1(AcTypeName::kNull)
                   : objVal.isArray()  ? QString::fromLatin1(AcTypeName::kArray)
                   : objVal.isString() ? QString::fromLatin1(AcTypeName::kString)
                                       : QString::fromLatin1(AcTypeName::kUnknown);
    setError(QStringLiteral("cannot call method on non-object '%1' (type=%2)").arg(name, type),
             expr.loc.line);
    return accore::AcJsonValue();
  }

  // 普通对象的内置方法（keys/values/has/size）—— 类实例同样适用
  if (objVal.isObject() && (expr.methodCall.methodName == QStringLiteral("keys") ||
                            expr.methodCall.methodName == QStringLiteral("values") ||
                            expr.methodCall.methodName == QStringLiteral("has") ||
                            expr.methodCall.methodName == QStringLiteral("size"))) {
    return evalObjectBuiltin(objVal, expr.methodCall.methodName, expr.methodCall.args,
                             expr.loc.line);
  }

  // 类名来源：类引用或实例（运行时种类字段，不再读 __class__ 键）
  QString className = objVal.instanceClass();
  if (className.isEmpty() || !m_classes.contains(className)) {
    QString name = isChained ? QStringLiteral("chain expression") : expr.methodCall.objName;
    setError(QStringLiteral("object '%1' has no class information").arg(name), expr.loc.line);
    return accore::AcJsonValue();
  }

  return evalClassMethod(objVal, className, expr, isChained, isSuper);
}

accore::AcJsonValue AcInterpreter::evalClassMethod(const accore::AcJsonValue &obj,
                                                   const QString &className, const Expr &expr,
                                                   bool isChained, bool isSuper) {
  const ClassDef &cd = m_classes[className];

  if (cd.isNative) {
    // 原生类实例方法：通过 FunMgr 的显式 this 参数传递对象实例，
    // 实参为纯参数列表（不含对象自身）；接收实例的方法经 registerFuncsWithThis 注册
    accore::AcJsonValue args = accore::AcJsonValue::makeArray();
    for (const auto &argExpr : expr.methodCall.args) args.append(evalExpr(*argExpr));
    if (!m_error.isEmpty()) return accore::AcJsonValue();
    // FunMgr 已迁移到 accore 签名：this/实参直接传递，无边界转换
    accore::AcJsonValue r = FunMgr::ins().call(className, expr.methodCall.methodName, obj, args);
    QString err = FunMgr::takeError();
    if (!err.isEmpty()) {
      setError(err, expr.loc.line);
      return accore::AcJsonValue();
    }
    return r;
  }

  QString searchClassName = className;
  if (isSuper) {
    if (cd.baseClass.isEmpty()) {
      setError(QStringLiteral("cannot use 'super' in class without base class"), expr.loc.line);
      return accore::AcJsonValue();
    }
    searchClassName = cd.baseClass;
  }

  const MethodDef *foundMethod = nullptr;
  if (!isSuper) {
    for (const auto &method : cd.methods) {
      if (method.name == expr.methodCall.methodName) {
        foundMethod = &method;
        break;
      }
    }
    if (!foundMethod) {
      foundMethod = findMethod(className, expr.methodCall.methodName);
    }
  } else {
    foundMethod = findMethod(searchClassName, expr.methodCall.methodName);
  }

  if (foundMethod) {
    accore::AcJsonValue args = accore::AcJsonValue::makeArray();
    for (const auto &argExpr : expr.methodCall.args) args.append(evalExpr(*argExpr));
    if (!m_error.isEmpty()) return accore::AcJsonValue();
    accore::AcJsonValue savedModifiedThis = m_modifiedThis;
    accore::AcJsonValue result = execMethod(*foundMethod, obj, args);
    if (!isChained && !isSuper &&
        expr.methodCall.objName != QString::fromLatin1(AcKeyword::kThis)) {
      if (containsVar(expr.methodCall.objName)) setVar(expr.methodCall.objName, m_modifiedThis);
    } else if (isChained && !expr.methodCall.objName.isEmpty() &&
               expr.methodCall.objName != QString::fromLatin1(AcKeyword::kThis)) {
      if (containsVar(expr.methodCall.objName)) {
        setVar(expr.methodCall.objName, m_modifiedThis);
      }
    }
    if (!isSuper) {
      m_modifiedThis = savedModifiedThis;
    }
    return result;
  }

  setError(QStringLiteral("method '%1' not found in class '%2'")
               .arg(expr.methodCall.methodName, className),
           expr.loc.line);
  return accore::AcJsonValue();
}

accore::AcJsonValue AcInterpreter::evalNewInstance(const Expr &expr) {
  if (!m_classes.contains(expr.className)) {
    setError(QStringLiteral("undefined class '%1'").arg(expr.className), expr.loc.line);
    return accore::AcJsonValue();
  }

  const ClassDef &cd = m_classes[expr.className];
  accore::AcJsonValue instance = accore::AcJsonValue::makeInstance(expr.className);

  if (cd.isNative) {
    accore::AcJsonValue ctorArgs = accore::AcJsonValue::makeArray();
    for (const auto &arg : expr.constructorArgs) ctorArgs.append(evalExpr(*arg));
    if (!m_error.isEmpty()) return accore::AcJsonValue();
    accore::AcJsonValue ctorResult =
        FunMgr::ins().call(expr.className, QString::fromLatin1(AcRuntime::kConstructor), ctorArgs);
    // 构造器返回的属性集迁入实例（实例的类名/objId 由专用字段承载）
    if (ctorResult.isObject()) {
      instance = accore::AcJsonValue::instanceFrom(ctorResult, expr.className);
    }
    instance = m_objMgr.registerInstance(instance, expr.className);
    return instance;
  }

  if (!cd.baseClass.isEmpty()) {
    accore::AcJsonValue base = createBaseInstance(cd.baseClass);
    if (!m_error.isEmpty()) return accore::AcJsonValue();
    // 继承：基类属性复制到派生实例
    for (const auto &m : base.members()) instance.set(m.key, m.value);
  }

  for (const auto &prop : cd.properties) {
    if (prop.isStatic) continue;  // 静态属性不属于实例，不嵌入实例对象
    if (prop.value) {
      accore::AcJsonValue v = evalExpr(*prop.value);
      if (!m_error.isEmpty()) return accore::AcJsonValue();
      retainIfInstance(v);
      instance.set(prop.key, v);
    } else {
      instance.set(prop.key, accore::AcJsonValue());
    }
  }

  instance = m_objMgr.registerInstance(instance, expr.className);

  for (const auto &m : cd.methods) {
    if (m.name == QStringLiteral("constructor")) {
      accore::AcJsonValue ctorArgs = accore::AcJsonValue::makeArray();
      for (const auto &arg : expr.constructorArgs) ctorArgs.append(evalExpr(*arg));
      if (!m_error.isEmpty()) return accore::AcJsonValue();
      accore::AcJsonValue ctorResult = execMethod(m, instance, ctorArgs);
      if (!m_error.isEmpty()) return accore::AcJsonValue();
      instance = m_modifiedThis;
      break;
    }
  }

  return instance;
}

accore::AcJsonValue AcInterpreter::evalStringBuiltin(const QString &obj, const QString &method,
                                                     const std::vector<std::unique_ptr<Expr>> &args,
                                                     int line) {
  QString err;
  accore::AcJsonValue result = AcBuiltinEval::evalStringMethod(*this, obj, method, args, line, err);
  if (!err.isEmpty()) m_error = err;
  return result;
}

accore::AcJsonValue AcInterpreter::evalArrayBuiltin(const accore::AcJsonValue &arr,
                                                    const QString &method,
                                                    const std::vector<std::unique_ptr<Expr>> &args,
                                                    int line, accore::AcJsonValue &modifiedArr) {
  QString err;
  accore::AcJsonValue result =
      AcBuiltinEval::evalArrayMethod(*this, arr, method, args, line, modifiedArr, err);
  if (!err.isEmpty()) m_error = err;
  return result;
}
