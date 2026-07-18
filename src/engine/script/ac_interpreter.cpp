/**
 * @file ac_interpreter.cpp
 * @brief 解释执行器实现文件
 */

#include "ac_interpreter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>  // for fmod()

#include "../ac_language.h"
#include "../function/fun_builtin.h"
#include "../function/fun_mgr.h"
#include "../tpl/tpl_engine.h"
#include "ac_builtin_eval.h"
#include "ac_object_manager.h"

// ═════════════════════════════════════════════════════════════════════════════
//  表达式求值
// ═════════════════════════════════════════════════════════════════════════════

QJsonValue AcInterpreter::evalExpr(const Expr &expr) {
  switch (expr.kind) {
    case Expr::kString:
      return QJsonValue(expr.strVal);

    case Expr::kNumber:
      return QJsonValue(expr.numVal);

    case Expr::kThis:
      return QJsonValue(m_currentThis);

    case Expr::kIdent: {
      if (!containsVar(expr.ident)) {
        *m_error = QStringLiteral("undefined variable '%1' at line %2")
                       .arg(expr.ident, QString::number(expr.line));
        return QJsonValue();
      }
      return resolveVar(expr.ident);
    }

    case Expr::kPropAccess: {
      QJsonValue obj;
      if (expr.propObject) {
        obj = evalExpr(*expr.propObject);
      } else {
        // 先尝试作为普通变量查找
        obj = resolveVar(expr.ident);

        // 如果变量不存在，尝试查找枚举成员或类静态属性（EnumName.Member / ClassName.staticProp）
        if (obj.isNull() && expr.ident != QString::fromLatin1(AcKeyword::kThis) &&
            expr.ident != QString::fromLatin1(AcKeyword::kSuper)) {
          QString enumMemberName = QStringLiteral("%1.%2").arg(expr.ident, expr.prop);
          if (containsVar(enumMemberName)) {
            return resolveVar(enumMemberName);
          }
          if (m_classes.contains(expr.ident)) {
            const ClassDef &cd = m_classes[expr.ident];
            if (!m_staticInited.contains(expr.ident)) {
              initStaticVars(cd);
            }
            for (const auto &prop : cd.properties) {
              if (prop.isStatic && prop.key == expr.prop) {
                QJsonObject sv = m_staticVars.value(expr.ident);
                return sv.value(expr.prop);
              }
            }
          }
          *m_error = QStringLiteral("undefined variable '%1'").arg(expr.ident);
          return QJsonValue();
        }
      }
      if (expr.prop == QStringLiteral("length")) {
        if (obj.isString()) return QJsonValue(obj.toString().length());
        if (obj.isArray()) return QJsonValue(obj.toArray().size());
      }

      // 尝试访问对象属性
      if (obj.isObject()) return obj.toObject().value(expr.prop);

      // 尝试访问数组元素（数字属性名）
      if (obj.isArray()) {
        bool ok = false;
        int idx = expr.prop.toInt(&ok);
        if (ok) {
          QJsonArray arr = obj.toArray();
          if (idx >= 0 && idx < arr.size()) return arr[idx];
        }
      }

      // 如果都不是，尝试查找枚举成员（obj 可能是枚举名）
      if (obj.isNull() || obj.isUndefined()) {
        QString enumMemberName = QStringLiteral("%1.%2").arg(expr.ident, expr.prop);
        if (containsVar(enumMemberName)) {
          return resolveVar(enumMemberName);
        }
      }

      return QJsonValue();
    }

    case Expr::kIndexAccess: {
      QJsonValue obj = evalExpr(*expr.left);
      QJsonValue idxVal = evalExpr(*expr.right);
      if (obj.isObject()) {
        QString key;
        if (idxVal.isString()) {
          key = idxVal.toString();
        } else {
          key = idxVal.isDouble() ? QString::number(idxVal.toDouble()) : idxVal.toString();
        }
        return obj.toObject().value(key);
      }
      if (obj.isArray()) {
        int idx = idxVal.toInt();
        QJsonArray arr = obj.toArray();
        if (idx >= 0 && idx < arr.size()) return arr[idx];
        return QJsonValue();
      }
      if (obj.isString()) {
        int idx = idxVal.toInt();
        QString s = obj.toString();
        if (idx >= 0 && idx < s.length()) return QJsonValue(QString(s[idx]));
        return QJsonValue();
      }
      *m_error = QStringLiteral("cannot access index on value at line %1").arg(expr.line);
      return QJsonValue();
    }

    case Expr::kObject: {
      QJsonObject obj;
      for (const auto &e : expr.objEntries) {
        QJsonValue v = evalExpr(*e.value);
        // 对象属性持有实例引用时 retain
        retainIfInstance(v);
        obj[e.key] = v;
      }
      return obj;
    }

    case Expr::kArray: {
      QJsonArray arr;
      for (const auto &e : expr.arrItems) {
        QJsonValue item = evalExpr(*e);
        // 数组持有实例引用时 retain
        retainIfInstance(item);
        arr.append(item);
      }
      return arr;
    }

    case Expr::kFuncCall:
      return callBuiltin(expr.funcCall.name, expr.funcCall.args, expr.line);

    case Expr::kFuncExpr: {
      // 函数表达式：注册到 m_functions 并返回函数引用值
      QString funcName = QStringLiteral("__lambda_%1").arg(++m_funcExprCounter);
      m_functions[funcName] = expr.funcExpr;
      m_functions[funcName].name = funcName;
      QJsonObject funcRef;
      funcRef[QString::fromLatin1(AcRuntime::kClassKey)] = QStringLiteral("__func__");
      funcRef[QStringLiteral("name")] = funcName;
      return QJsonValue(funcRef);
    }

    case Expr::kMethodCall:
      return evalMethodCall(expr);

    case Expr::kNewInstance:
      return evalNewInstance(expr);

    case Expr::kStaticAccess: {
      // ClassName::prop  — 访问静态变量或调用无参静态方法
      // ClassName::method(args)  — 调用有参静态方法
      QString className = expr.className;
      if (!m_classes.contains(className)) {
        *m_error = QStringLiteral("undefined class '%1' at line %2")
                       .arg(className, QString::number(expr.line));
        return QJsonValue();
      }
      const ClassDef &cd = m_classes[className];

      // 初始化静态变量（如果尚未初始化）
      if (!m_staticInited.contains(className)) {
        initStaticVars(cd);
      }

      // 先尝试匹配静态变量
      for (const auto &prop : cd.properties) {
        if (prop.isStatic && prop.key == expr.prop) {
          QJsonObject sv = m_staticVars.value(className);
          return sv.value(expr.prop);
        }
      }

      // 再尝试匹配静态方法（构建参数数组）
      QJsonArray callArgs;
      for (const auto &arg : expr.funcCall.args) {
        callArgs.append(evalExpr(*arg));
        if (!m_error->isEmpty()) return QJsonValue();
      }
      for (const auto &md : cd.methods) {
        if (md.isStatic && md.name == expr.prop) {
          // 静态方法传入空 thisObj
          return execMethod(md, QJsonObject(), QJsonValue(callArgs));
        }
      }

      *m_error = QStringLiteral("class '%1' has no static member '%2' at line %3")
                     .arg(className, expr.prop, QString::number(expr.line));
      return QJsonValue();
    }

    case Expr::kBinary:
      return evalBinary(expr);
    case Expr::kUnary:
      return evalUnary(expr);

    case Expr::kPreInc: {
      QJsonValue val = evalExpr(*expr.operand);
      double newVal = val.toDouble() + 1;
      // 回写变量
      if (expr.operand->kind == Expr::kIdent) {
        setVar(expr.operand->ident, QJsonValue(newVal));
      }
      return QJsonValue(newVal);
    }
    case Expr::kPreDec: {
      QJsonValue val = evalExpr(*expr.operand);
      double newVal = val.toDouble() - 1;
      if (expr.operand->kind == Expr::kIdent) {
        setVar(expr.operand->ident, QJsonValue(newVal));
      }
      return QJsonValue(newVal);
    }
    case Expr::kPostInc: {
      QJsonValue val = evalExpr(*expr.operand);
      double newVal = val.toDouble() + 1;
      if (expr.operand->kind == Expr::kIdent) {
        setVar(expr.operand->ident, QJsonValue(newVal));
      }
      return val;  // 返回原值
    }
    case Expr::kPostDec: {
      QJsonValue val = evalExpr(*expr.operand);
      double newVal = val.toDouble() - 1;
      if (expr.operand->kind == Expr::kIdent) {
        setVar(expr.operand->ident, QJsonValue(newVal));
      }
      return val;  // 返回原值
    }

    case Expr::kNull:
      return QJsonValue();
    case Expr::kUndefined:
      return QJsonValue();

    case Expr::kTernary:
      if (isTruthy(evalExpr(*expr.left)))
        return evalExpr(*expr.right);
      else
        return evalExpr(*expr.operand);
  }

  return QJsonValue();
}

QJsonValue AcInterpreter::evalExprWithThis(const Expr &expr, const QJsonObject &thisObj) {
  QJsonObject oldThis = m_currentThis;
  m_currentThis = thisObj;
  QJsonValue result = evalExpr(expr);
  m_currentThis = oldThis;
  return result;
}

// ═════════════════════════════════════════════════════════════════════════════
//  二元运算
// ═════════════════════════════════════════════════════════════════════════════

QJsonValue AcInterpreter::evalBinary(const Expr &expr) {
  QJsonValue l = evalExpr(*expr.left);
  QJsonValue r = evalExpr(*expr.right);

  switch (expr.binOp) {
    case Expr::kAdd:
      if (l.isString() || r.isString()) {
        auto valToStr = [](const QJsonValue &v) -> QString {
          if (v.isString()) return v.toString();
          if (v.isBool()) return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
          if (v.isNull()) return QStringLiteral("null");
          return QString::number(v.toDouble());
        };
        return QJsonValue(valToStr(l) + valToStr(r));
      }
      return QJsonValue(l.toDouble() + r.toDouble());
    case Expr::kSub:
      return QJsonValue(l.toDouble() - r.toDouble());
    case Expr::kMul:
      return QJsonValue(l.toDouble() * r.toDouble());
    case Expr::kDiv:
      if (r.toDouble() == 0.0) {
        *m_error = QStringLiteral("division by zero");
        return QJsonValue();
      }
      return QJsonValue(l.toDouble() / r.toDouble());
    case Expr::kMod:
      if (r.toDouble() == 0.0) {
        *m_error = QStringLiteral("modulo by zero");
        return QJsonValue();
      }
      return QJsonValue(fmod(l.toDouble(), r.toDouble()));
    case Expr::kOr:
      return isTruthy(l) ? l : r;
    case Expr::kAnd:
      return isTruthy(l) ? r : l;
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
  return QJsonValue();
}

int AcInterpreter::compareValues(const QJsonValue &l, const QJsonValue &r) {
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
  if (l.isNull() || r.isNull()) {
    if (l.isNull() && r.isNull()) return 0;
    return l.isNull() ? -1 : 1;
  }
  // 类型不同时转为字符串比较
  QString ls =
      l.isString() ? l.toString() : (l.isDouble() ? QString::number(l.toDouble()) : l.toString());
  QString rs =
      r.isString() ? r.toString() : (r.isDouble() ? QString::number(r.toDouble()) : r.toString());
  return ls.compare(rs);
}

QString AcInterpreter::inferTypeName(const QJsonValue &val) {
  if (val.isBool()) return QStringLiteral("Boolean");
  if (val.isDouble()) return QStringLiteral("Number");
  if (val.isString()) return QStringLiteral("String");
  if (val.isArray()) return QStringLiteral("Array");
  if (val.isObject()) {
    if (val.toObject().contains(QStringLiteral("__class__"))) {
      return val.toObject().value(QStringLiteral("__class__")).toString();
    }
    return QStringLiteral("Object");
  }
  if (val.isNull()) return QStringLiteral("Null");
  return QStringLiteral("Any");
}

QJsonValue AcInterpreter::evalUnary(const Expr &expr) {
  QJsonValue val = evalExpr(*expr.operand);
  switch (expr.unaryOp) {
    case Expr::kNot:
      return QJsonValue(!isTruthy(val));
  }
  return QJsonValue();
}

// ═════════════════════════════════════════════════════════════════════════════
//  辅助方法
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
      // 同一个受管理实例赋值时跳过 release/retain，避免误析构
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
  // 新变量
  retainIfInstance(val);
  m_scopeStack.last()[name] = val;
}

void AcInterpreter::pushScope() {
  m_scopeStack.append(QHash<QString, QJsonValue>());
  m_usingStack.append(QVector<QString>());
}

void AcInterpreter::popScope() {
  if (m_scopeStack.isEmpty()) return;

  // 先调用 using 变量的 dispose() 方法
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

  // 先从栈中取出作用域（detach），避免 releaseDeep 触发 __destruct__ 时
  // pushScope/popScope 修改 m_scopeStack 导致引用失效
  auto scope = m_scopeStack.takeLast();
  for (auto it = scope.begin(); it != scope.end(); ++it) {
    releaseDeep(it.value());
  }
  // 标记-清扫：回收循环引用垃圾
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

// ── 引用计数辅助 ──

void AcInterpreter::retainIfInstance(const QJsonValue &val) {
  if (!AcObjectManager::isManagedInstance(val)) return;
  AcObjectManager::ins().retain(AcObjectManager::getObjId(val));
}

void AcInterpreter::releaseIfInstance(const QJsonValue &val) {
  if (!AcObjectManager::isManagedInstance(val)) return;
  AcObjectManager::ins().release(AcObjectManager::getObjId(val));
}

void AcInterpreter::releaseIfInstanceWithDestruct(const QJsonValue &val) {
  if (!AcObjectManager::isManagedInstance(val)) return;
  AcObjectManager::ins().release(AcObjectManager::getObjId(val));
  // 处理所有待析构的实例
  QVector<AcObjectManager::DestructInfo> pending = AcObjectManager::ins().takePendingDestructs();
  for (const auto &info : pending) {
    processDestructInfo(info);
  }
}

/// 执行单个实例的析构：释放属性引用 + 调用 __destruct__
void AcInterpreter::processDestructInfo(const AcObjectManager::DestructInfo &info) {
  // 先递归释放实例内部属性持有的引用（如 this.data = new Xxx()）
  QJsonObject obj = info.instance;
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    if (it.key() == QString::fromLatin1(AcRuntime::kClassKey) ||
        it.key() == QString::fromLatin1(AcRuntime::kObjId))
      continue;
    releaseDeep(it.value());
  }
  // 原生类：路由到 FunMgr 的 __destruct__
  if (FunMgr::ins().contains(info.className, QString::fromLatin1(AcRuntime::kDestructor))) {
    QJsonArray args;
    args.append(QJsonValue(info.instance));
    FunMgr::ins().call(info.className, QString::fromLatin1(AcRuntime::kDestructor), args);
  }
  // 用户自定义类：执行 __destruct__ AST
  else if (m_classes.contains(info.className) && !m_classes[info.className].isNative) {
    for (const auto &method : m_classes[info.className].methods) {
      if (method.name == QString::fromLatin1(AcRuntime::kDestructor)) {
        execMethod(method, info.instance, QJsonValue(QJsonArray()));
        break;
      }
    }
  }
}

void AcInterpreter::releaseDeep(const QJsonValue &val) {
  if (AcObjectManager::isManagedInstance(val)) {
    releaseIfInstanceWithDestruct(val);
    return;
  }
  // 递归释放数组中的实例
  if (val.isArray()) {
    for (const QJsonValue &item : val.toArray()) {
      releaseDeep(item);
    }
    return;
  }
  // 递归释放对象属性中的实例（跳过 __class__ 和 __objId__ 内部键）
  // ⚠️ 必须先把 toObject() 存到局部变量，再对局部变量取 begin()/end()，
  //    否则 toObject() 返回的临时对象在分号处销毁，迭代器变为悬垂指针。
  if (val.isObject()) {
    QJsonObject obj = val.toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      if (it.key() == QString::fromLatin1(AcRuntime::kClassKey) ||
          it.key() == QString::fromLatin1(AcRuntime::kObjId))
        continue;
      releaseDeep(it.value());
    }
  }
}

// ── 标记-清扫（处理循环引用） ──

void AcInterpreter::markFromValue(const QJsonValue &val) {
  auto &mgr = AcObjectManager::ins();

  if (AcObjectManager::isManagedInstance(val)) {
    QString objId = AcObjectManager::getObjId(val);
    // 已标记或不在管理器中（已被 release 回收），跳过
    if (mgr.isMarked(objId) || !mgr.contains(objId)) return;
    mgr.mark(objId);
    // 递归标记实例的所有属性
    QJsonObject obj = mgr.getObject(objId);
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      if (it.key() == QString::fromLatin1(AcRuntime::kClassKey) ||
          it.key() == QString::fromLatin1(AcRuntime::kObjId))
        continue;
      markFromValue(it.value());
    }
    return;
  }
  if (val.isArray()) {
    for (const QJsonValue &item : val.toArray()) {
      markFromValue(item);
    }
    return;
  }
  if (val.isObject()) {
    QJsonObject obj = val.toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      if (it.key() == QString::fromLatin1(AcRuntime::kClassKey) ||
          it.key() == QString::fromLatin1(AcRuntime::kObjId))
        continue;
      markFromValue(it.value());
    }
  }
}

void AcInterpreter::collectCycles() {
  auto &mgr = AcObjectManager::ins();
  if (mgr.objectCount() == 0) return;

  // 1. 清除旧标记
  mgr.clearMarks();

  // 2. 从所有存活作用域出发标记
  for (const auto &scope : m_scopeStack) {
    for (auto it = scope.begin(); it != scope.end(); ++it) {
      markFromValue(it.value());
    }
  }
  // 3. 从静态变量出发标记
  for (auto it = m_staticVars.begin(); it != m_staticVars.end(); ++it) {
    for (auto pit = it.value().begin(); pit != it.value().end(); ++pit) {
      markFromValue(pit.value());
    }
  }
  // 4. 从当前 this 出发标记
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

  // 5. 清扫未标记的实例（循环引用垃圾）
  QVector<AcObjectManager::DestructInfo> cycles = mgr.collectUnmarked();
  for (const auto &info : cycles) {
    processDestructInfo(info);
  }
}

QJsonValue AcInterpreter::callBuiltin(const QString &name,
                                      const std::vector<std::unique_ptr<Expr>> &args, int line) {
  // 统一求值所有参数
  QJsonArray arr;
  for (const auto &a : args) arr.append(evalExpr(*a));

  // call() 特殊处理：前两个参数是类名和函数名
  if (name == AcBuiltin::kCall) {
    if (arr.size() < 2) {
      *m_error = QStringLiteral("call() requires at least 2 arguments");
      return QJsonValue();
    }
    QString cls = arr[0].toString();
    QString func = arr[1].toString();
    QJsonArray callArgs;
    if (arr.size() >= 3) callArgs.append(arr[2]);
    QJsonValue r = FunMgr::ins().call(cls, func, callArgs);
    QString err = FunMgr::takeError();
    if (!err.isEmpty()) {
      *m_error = QStringLiteral("%1 at line %2").arg(err).arg(line);
      return QJsonValue();
    }
    return r;
  }

  // 其余内置函数：优先检查是否已注册，避免与 null 返回值混淆
  const QString builtinClass = QString::fromLatin1(AcRuntime::kBuiltinClass);
  if (FunMgr::ins().contains(builtinClass, name)) {
    // 注入当前行号，printLog/printError 输出时带上行号
    FunBuiltin::setCurrentLine(line);
    QJsonValue r = FunMgr::ins().call(builtinClass, name, arr);
    FunBuiltin::setCurrentLine(0);
    // 检查内置函数是否设置了错误（如文件不存在）
    QString err = FunMgr::takeError();
    if (!err.isEmpty()) {
      *m_error = QStringLiteral("%1 at line %2").arg(err).arg(line);
      return QJsonValue();
    }
    return r;
  }

  // 尝试查找用户自定义函数
  auto it = m_functions.find(name);
  if (it != m_functions.end()) return execUserFunction(*it, QJsonValue(arr));

  // 尝试从变量中查找函数引用值（lambda / 函数表达式）
  if (containsVar(name)) {
    QJsonValue varVal = resolveVar(name);
    if (varVal.isObject()) {
      QJsonObject obj = varVal.toObject();
      if (obj.value(QString::fromLatin1(AcRuntime::kClassKey)).toString() ==
          QStringLiteral("__func__")) {
        QString funcName = obj.value(QStringLiteral("name")).toString();
        auto fi = m_functions.find(funcName);
        if (fi != m_functions.end()) return execUserFunction(*fi, QJsonValue(arr));
      }
    }
  }

  *m_error = QStringLiteral("unknown function '%1' at line %2").arg(name).arg(line);
  return QJsonValue();
}

QJsonValue AcInterpreter::evalMethodCall(const Expr &expr) {
  QJsonValue objVal;
  bool isChained = (expr.methodCall.object != nullptr);
  bool isSuper = (expr.methodCall.objName == QString::fromLatin1(AcKeyword::kSuper));

  // JSON 内置对象拦截
  if (!isChained && !isSuper && expr.methodCall.objName == QStringLiteral("JSON")) {
    const QString &method = expr.methodCall.methodName;
    if (method == QStringLiteral("parse")) {
      if (expr.methodCall.args.empty()) {
        *m_error = QStringLiteral("JSON.parse() requires 1 argument at line %1").arg(expr.line);
        return QJsonValue();
      }
      QJsonValue argVal = evalExpr(*expr.methodCall.args[0]);
      if (!argVal.isString()) {
        *m_error =
            QStringLiteral("JSON.parse() argument must be a string at line %1").arg(expr.line);
        return QJsonValue();
      }
      QJsonParseError parseError;
      QJsonDocument doc = QJsonDocument::fromJson(argVal.toString().toUtf8(), &parseError);
      if (parseError.error != QJsonParseError::NoError) {
        *m_error = QStringLiteral("JSON.parse() error: %1 at line %2")
                       .arg(parseError.errorString())
                       .arg(expr.line);
        return QJsonValue();
      }
      if (doc.isObject()) return doc.object();
      if (doc.isArray()) return doc.array();
      return QJsonValue();
    }
    if (method == QStringLiteral("stringify")) {
      if (expr.methodCall.args.empty()) {
        *m_error = QStringLiteral("JSON.stringify() requires 1 argument at line %1").arg(expr.line);
        return QJsonValue();
      }
      QJsonValue argVal = evalExpr(*expr.methodCall.args[0]);
      QJsonDocument doc;
      if (argVal.isObject())
        doc = QJsonDocument(argVal.toObject());
      else if (argVal.isArray())
        doc = QJsonDocument(argVal.toArray());
      else
        return QJsonValue(argVal.toString());
      return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    }
    *m_error = QStringLiteral("JSON has no method '%1' at line %2").arg(method).arg(expr.line);
    return QJsonValue();
  }

  if (isChained) {
    objVal = evalExpr(*expr.methodCall.object);
    if (objVal.isNull()) {
      *m_error = QStringLiteral("method call on null value at line %1").arg(expr.line);
      return QJsonValue();
    }
  } else if (isSuper) {
    objVal = QJsonValue(m_currentThis);
  } else {
    objVal = resolveVar(expr.methodCall.objName);
    if (objVal.isNull() && expr.methodCall.objName != QString::fromLatin1(AcKeyword::kThis)) {
      if (m_classes.contains(expr.methodCall.objName)) {
        QJsonObject classObj;
        classObj[QString::fromLatin1(AcRuntime::kClassKey)] = expr.methodCall.objName;
        objVal = QJsonValue(classObj);
      } else {
        *m_error =
            QStringLiteral("undefined variable '%1' in method call").arg(expr.methodCall.objName);
        return QJsonValue();
      }
    }
  }

  // 字符串内建方法
  if (objVal.isString()) {
    return evalStringBuiltin(objVal.toString(), expr.methodCall.methodName, expr.methodCall.args,
                             expr.line);
  }

  // 数组内建方法
  if (objVal.isArray()) {
    QJsonValue modifiedArr;
    QJsonValue result = evalArrayBuiltin(objVal.toArray(), expr.methodCall.methodName,
                                         expr.methodCall.args, expr.line, modifiedArr);
    if (!modifiedArr.isNull()) {
      if (isChained) {
        // 链式属性访问：this.fields.append(...) → 回写到 this 的属性
        if (expr.methodCall.object && expr.methodCall.object->kind == Expr::kPropAccess &&
            !expr.methodCall.object->prop.isEmpty()) {
          const Expr &propObj = *expr.methodCall.object;
          if (propObj.ident == QString::fromLatin1(AcKeyword::kThis)) {
            m_currentThis[propObj.prop] = modifiedArr;
            m_modifiedThis[propObj.prop] = modifiedArr;
          } else if (!propObj.ident.isEmpty()) {
            // 链式调用修改普通变量属性：obj.prop.append(...)
            if (containsVar(propObj.ident)) {
              QJsonObject varObj = resolveVar(propObj.ident).toObject();
              varObj[propObj.prop] = modifiedArr;
              setVar(propObj.ident, QJsonValue(varObj));
            }
          }
        }
      } else if (expr.methodCall.objName == QString::fromLatin1(AcKeyword::kThis)) {
        m_currentThis = modifiedArr.toObject();
        m_modifiedThis = modifiedArr.toObject();
      } else {
        setVar(expr.methodCall.objName, modifiedArr);
      }
    }
    return result;
  }

  if (!objVal.isObject()) {
    QString name = isChained ? QStringLiteral("chain expression") : expr.methodCall.objName;
    *m_error = QStringLiteral("cannot call method on non-object '%1' (type=%2) at line %3")
                   .arg(name)
                   .arg(objVal.isDouble()   ? QStringLiteral("Number")
                        : objVal.isBool()   ? QStringLiteral("Bool")
                        : objVal.isNull()   ? QStringLiteral("Null")
                        : objVal.isArray()  ? QStringLiteral("Array")
                        : objVal.isString() ? QStringLiteral("String")
                                            : QStringLiteral("Unknown"))
                   .arg(expr.line);
    return QJsonValue();
  }

  QJsonObject obj = objVal.toObject();
  QString className = obj.value(QString::fromLatin1(AcRuntime::kClassKey)).toString();
  if (className.isEmpty() || !m_classes.contains(className)) {
    QString name = isChained ? QStringLiteral("chain expression") : expr.methodCall.objName;
    *m_error = QStringLiteral("object '%1' has no class information").arg(name);
    return QJsonValue();
  }

  const ClassDef &cd = m_classes[className];

  // 原生类：路由到 FunMgr（传入实例对象作为第一个参数）
  if (cd.isNative) {
    QJsonArray args;
    args.append(QJsonValue(obj));
    for (const auto &argExpr : expr.methodCall.args) args.append(evalExpr(*argExpr));
    QJsonValue r = FunMgr::ins().call(className, expr.methodCall.methodName, args);
    QString err = FunMgr::takeError();
    if (!err.isEmpty()) {
      *m_error = QStringLiteral("%1 at line %2").arg(err).arg(expr.line);
      return QJsonValue();
    }
    return r;
  }

  // super.method() → 从父类开始查找方法
  QString searchClassName = className;
  if (isSuper) {
    if (cd.baseClass.isEmpty()) {
      *m_error = QStringLiteral("cannot use 'super' in class without base class at line %1")
                     .arg(expr.line);
      return QJsonValue();
    }
    searchClassName = cd.baseClass;
  }

  // 在当前类（或父类）中查找方法
  const MethodDef *foundMethod = nullptr;
  if (!isSuper) {
    // 先在当前类查找
    for (const auto &method : cd.methods) {
      if (method.name == expr.methodCall.methodName) {
        foundMethod = &method;
        break;
      }
    }
    // 当前类未找到，递归父类查找
    if (!foundMethod) {
      foundMethod = findMethod(className, expr.methodCall.methodName);
    }
  } else {
    // super → 从父类开始查找
    foundMethod = findMethod(searchClassName, expr.methodCall.methodName);
  }

  if (foundMethod) {
    QJsonArray args;
    for (const auto &argExpr : expr.methodCall.args) args.append(evalExpr(*argExpr));
    QJsonObject savedModifiedThis = m_modifiedThis;
    QJsonValue result = execMethod(*foundMethod, obj, QJsonValue(args));
    if (!isChained && !isSuper &&
        expr.methodCall.objName != QString::fromLatin1(AcKeyword::kThis)) {
      if (containsVar(expr.methodCall.objName))
        setVar(expr.methodCall.objName, QJsonValue(m_modifiedThis));
    } else if (isChained && !expr.methodCall.objName.isEmpty() &&
               expr.methodCall.objName != QString::fromLatin1(AcKeyword::kThis)) {
      if (containsVar(expr.methodCall.objName)) {
        setVar(expr.methodCall.objName, QJsonValue(m_modifiedThis));
      }
    }
    // super() 构造函数调用需要保留 m_modifiedThis（父类构造函数修改了 this）
    if (!isSuper) {
      m_modifiedThis = savedModifiedThis;
    }
    return result;
  }

  *m_error = QStringLiteral("method '%1' not found in class '%2'")
                 .arg(expr.methodCall.methodName, className);
  return QJsonValue();
}

QJsonValue AcInterpreter::evalNewInstance(const Expr &expr) {
  if (!m_classes.contains(expr.className)) {
    *m_error = QStringLiteral("undefined class '%1'").arg(expr.className);
    return QJsonValue();
  }

  const ClassDef &cd = m_classes[expr.className];
  QJsonObject instance;

  // 原生类：调用 FunMgr 构造器
  if (cd.isNative) {
    QJsonArray ctorArgs;
    for (const auto &arg : expr.constructorArgs) ctorArgs.append(evalExpr(*arg));
    QJsonValue ctorResult =
        FunMgr::ins().call(expr.className, QString::fromLatin1(AcRuntime::kConstructor), ctorArgs);
    if (ctorResult.isObject()) instance = ctorResult.toObject();
    instance[QString::fromLatin1(AcRuntime::kClassKey)] = expr.className;
    instance = AcObjectManager::ins().registerInstance(instance, expr.className);
    return QJsonValue(instance);
  }

  // 继承：先初始化父类属性
  if (!cd.baseClass.isEmpty()) {
    instance = createBaseInstance(cd.baseClass);
  }

  // 初始化当前类属性（覆盖父类同名属性）
  for (const auto &prop : cd.properties) {
    if (prop.value) {
      QJsonValue v = evalExpr(*prop.value);
      retainIfInstance(v);
      instance[prop.key] = v;
    } else {
      instance[prop.key] = QJsonValue();
    }
  }

  // 设置 __class__ 为当前类名（放在属性初始化之后，确保不被 createBaseInstance 覆盖）
  instance[QString::fromLatin1(AcRuntime::kClassKey)] = expr.className;

  // 用户自定义类也注册到对象管理器
  instance = AcObjectManager::ins().registerInstance(instance, expr.className);

  // 调用 constructor 方法（如果存在）
  for (const auto &m : cd.methods) {
    if (m.name == QStringLiteral("constructor")) {
      QJsonArray ctorArgs;
      for (const auto &arg : expr.constructorArgs) ctorArgs.append(evalExpr(*arg));
      QJsonValue ctorResult = execMethod(m, instance, QJsonValue(ctorArgs));
      if (!m_error->isEmpty()) return QJsonValue();
      // 构造函数可能修改了 this 上的属性，更新 instance
      instance = m_modifiedThis;
      break;
    }
  }

  return QJsonValue(instance);
}

// ═════════════════════════════════════════════════════════════════════════════
//  类方法执行
// ═════════════════════════════════════════════════════════════════════════════

QJsonValue AcInterpreter::execMethod(const MethodDef &method, const QJsonObject &thisObj,
                                     const QJsonValue &callArgs) {
  QJsonArray argsArr = callArgs.toArray();
  QJsonObject oldThis = m_currentThis;

  // 保存调用方的 hasReturned/returnValue 状态，方法返回后恢复
  bool savedReturned = m_hasReturned;
  QJsonValue savedReturnValue = m_returnValue;
  m_hasReturned = false;
  m_returnValue = QJsonValue();

  m_currentThis = thisObj;
  m_modifiedThis = thisObj;

  pushScope();

  // 绑定参数：形参名 → 实参值
  for (int i = 0; i < method.params.size(); ++i) {
    setVar(method.params[i].name, i < argsArr.size() ? argsArr[i] : QJsonValue());
  }

  execBlock(method.body);

  popScope();

  QJsonValue result = m_hasReturned ? m_returnValue : QJsonValue();

  // 恢复调用方的 hasReturned/returnValue 状态
  m_hasReturned = savedReturned;
  m_returnValue = savedReturnValue;

  m_currentThis = oldThis;
  // m_modifiedThis 不恢复，由调用方 evalMethodCall 在写回变量后自行恢复
  return result;
}

void AcInterpreter::initStaticVars(const ClassDef &cd) {
  if (m_staticInited.contains(cd.name)) return;
  m_staticInited.insert(cd.name);

  // 继承：先初始化父类静态变量
  if (!cd.baseClass.isEmpty() && m_classes.contains(cd.baseClass)) {
    initStaticVars(m_classes[cd.baseClass]);
  }

  QJsonObject sv;
  // 继承父类静态变量
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

  // 保存调用方的 hasReturned/returnValue 状态，函数返回后恢复
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

  // 恢复调用方的 hasReturned/returnValue 状态，避免函数内的 return 影响上层
  m_hasReturned = savedReturned;
  m_returnValue = savedReturnValue;

  return result;
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
      if (!m_error->isEmpty()) return;
      QString clsName = cls.toString();
      QString funcName = func.toString();
      QJsonArray argsArr = args.toArray();

      // call() 语句：调用 C++ 注册函数
      FunMgr::ins().call(clsName, funcName, argsArr);
      QString err = FunMgr::takeError();
      if (!err.isEmpty()) {
        *m_error = QStringLiteral("%1 at line %2").arg(err).arg(stmt.call.className.line);
      }
      break;
    }

    case Block::Stmt::kAssign: {
      QJsonValue val = evalExpr(stmt.assign.value);
      if (!m_error->isEmpty()) return;

      // 处理复合赋值运算符：+= -= *= /=
      if (stmt.assign.compoundOp != 0) {
        QJsonValue currentVal;
        if (stmt.assign.isStatic) {
          currentVal = m_staticVars[stmt.assign.staticClassName].value(stmt.assign.name);
        } else if (!stmt.assign.thisProp.isEmpty()) {
          currentVal = m_currentThis.value(stmt.assign.thisProp);
        } else {
          currentVal = resolveVar(stmt.assign.name);
        }
        // 复合赋值 +=：字符串类型做拼接，数值类型做加法
        if (stmt.assign.compoundOp == 1) {
          if (currentVal.isString() || val.isString()) {
            QString ls = currentVal.isString() ? currentVal.toString()
                                               : QString::number(currentVal.toDouble());
            QString rs = val.isString() ? val.toString() : QString::number(val.toDouble());
            val = QJsonValue(ls + rs);
          } else {
            val = QJsonValue(currentVal.toDouble() + val.toDouble());
          }
        } else {
          double left = currentVal.toDouble();
          double right = val.toDouble();
          switch (stmt.assign.compoundOp) {
            case 2:
              val = QJsonValue(left - right);
              break;  // -=
            case 3:
              val = QJsonValue(left * right);
              break;  // *=
            case 4:
              if (right == 0) {
                *m_error =
                    QStringLiteral("division by zero at line %1").arg(stmt.assign.value.line);
                return;
              }
              val = QJsonValue(left / right);
              break;  // /=
            case 5:
              if (right == 0) {
                *m_error = QStringLiteral("modulo by zero at line %1").arg(stmt.assign.value.line);
                return;
              }
              val = QJsonValue(fmod(left, right));
              break;  // %=
          }
        }
      }

      // 静态属性赋值：ClassName::prop = value
      if (stmt.assign.isStatic) {
        if (m_staticVars.contains(stmt.assign.staticClassName)) {
          QJsonObject sv = m_staticVars[stmt.assign.staticClassName];
          // release 旧值
          if (sv.contains(stmt.assign.name)) {
            releaseIfInstanceWithDestruct(sv.value(stmt.assign.name));
          }
          retainIfInstance(val);
          sv[stmt.assign.name] = val;
          m_staticVars[stmt.assign.staticClassName] = sv;
        }
        break;
      }

      // this.prop = value
      if (!stmt.assign.thisProp.isEmpty()) {
        QJsonValue old = m_currentThis.value(stmt.assign.thisProp);
        releaseIfInstanceWithDestruct(old);
        retainIfInstance(val);
        m_currentThis[stmt.assign.thisProp] = val;
        m_modifiedThis[stmt.assign.thisProp] = val;
        break;
      }

      // name = value
      setVar(stmt.assign.name, val);
      // 记录推导类型（仅当未显式标注类型时）
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
        // 回写对象
        if (stmt.indexAssign.objectExpr.kind == Expr::kIdent) {
          setVar(stmt.indexAssign.objectExpr.ident, obj);
        } else if (stmt.indexAssign.objectExpr.kind == Expr::kThis) {
          m_currentThis = obj;
          m_modifiedThis = obj;
        } else if (stmt.indexAssign.objectExpr.kind == Expr::kPropAccess) {
          QJsonValue parentVal = resolveVar(stmt.indexAssign.objectExpr.ident);
          if (parentVal.isObject()) {
            QJsonObject parentObj = parentVal.toObject();
            parentObj[stmt.indexAssign.objectExpr.prop] = obj;
            setVar(stmt.indexAssign.objectExpr.ident, parentObj);
          }
        }
      } else if (objVal.isArray()) {
        QJsonArray arr = objVal.toArray();
        int idx = idxVal.toInt();
        if (idx >= 0 && idx < arr.size()) {
          releaseIfInstanceWithDestruct(arr[idx]);
          arr.replace(idx, newVal);
        } else if (idx == arr.size()) {
          arr.append(newVal);
        }
        // 回写数组
        if (stmt.indexAssign.objectExpr.kind == Expr::kIdent) {
          setVar(stmt.indexAssign.objectExpr.ident, QJsonValue(arr));
        } else if (stmt.indexAssign.objectExpr.kind == Expr::kThis) {
          m_currentThis = QJsonObject();
          m_modifiedThis = QJsonObject();
        } else if (stmt.indexAssign.objectExpr.kind == Expr::kPropAccess) {
          QJsonValue parentVal = resolveVar(stmt.indexAssign.objectExpr.ident);
          if (parentVal.isObject()) {
            QJsonObject parentObj = parentVal.toObject();
            parentObj[stmt.indexAssign.objectExpr.prop] = QJsonValue(arr);
            setVar(stmt.indexAssign.objectExpr.ident, parentObj);
          }
        }
      }
      break;
    }

    case Block::Stmt::kPropAssign: {
      // 检查是否为类名静态属性赋值：ClassName.prop = value
      if (stmt.propAssign.objectExpr.kind == Expr::kIdent &&
          m_classes.contains(stmt.propAssign.objectExpr.ident)) {
        QJsonValue newVal = evalExpr(stmt.propAssign.value);
        retainIfInstance(newVal);
        QString className = stmt.propAssign.objectExpr.ident;
        if (!m_staticInited.contains(className)) {
          initStaticVars(m_classes[className]);
        }
        if (m_staticVars.contains(className)) {
          QJsonObject sv = m_staticVars[className];
          if (sv.contains(stmt.propAssign.prop)) {
            releaseIfInstanceWithDestruct(sv.value(stmt.propAssign.prop));
          }
          sv[stmt.propAssign.prop] = newVal;
          m_staticVars[className] = sv;
        }
        break;
      }
      QJsonValue objVal = evalExpr(stmt.propAssign.objectExpr);
      QJsonValue newVal = evalExpr(stmt.propAssign.value);
      retainIfInstance(newVal);
      if (objVal.isObject()) {
        QJsonObject obj = objVal.toObject();
        if (stmt.propAssign.compoundOp != 0) {
          QJsonValue oldVal = obj.value(stmt.propAssign.prop);
          double oldNum = oldVal.isDouble() ? oldVal.toDouble() : 0.0;
          double newNum = newVal.isDouble() ? newVal.toDouble() : 0.0;
          switch (stmt.propAssign.compoundOp) {
            case 1:
              newVal = QJsonValue(oldNum + newNum);
              break;
            case 2:
              newVal = QJsonValue(oldNum - newNum);
              break;
            case 3:
              newVal = QJsonValue(oldNum * newNum);
              break;
            case 4:
              newVal = QJsonValue(newNum != 0 ? oldNum / newNum : 0.0);
              break;
            case 5:
              newVal = QJsonValue(newNum != 0 ? fmod(oldNum, newNum) : 0.0);
              break;
          }
        }
        if (obj.contains(stmt.propAssign.prop)) {
          releaseIfInstanceWithDestruct(obj.value(stmt.propAssign.prop));
        }
        obj[stmt.propAssign.prop] = newVal;
        // 回写对象
        if (stmt.propAssign.objectExpr.kind == Expr::kIdent) {
          setVar(stmt.propAssign.objectExpr.ident, obj);
        } else if (stmt.propAssign.objectExpr.kind == Expr::kThis) {
          m_currentThis = obj;
          m_modifiedThis = obj;
        }
      }
      break;
    }

    case Block::Stmt::kFor: {
      if (stmt.forStmt.isStandard) {
        // 标准 for 循环：for (init; condition; update) { body }
        pushScope();
        execBlock(stmt.forStmt.initBlock);
        if (!m_error->isEmpty()) {
          popScope();
          return;
        }
        while (isTruthy(evalExpr(stmt.forStmt.condition))) {
          execBlock(stmt.forStmt.body);
          if (!m_error->isEmpty()) {
            popScope();
            return;
          }
          if (m_hasBreak) {
            m_hasBreak = false;
            break;
          }
          m_hasContinue = false;
          evalExpr(stmt.forStmt.updateExpr);
          if (!m_error->isEmpty()) {
            popScope();
            return;
          }
        }
        popScope();
      } else {
        // for-in 遍历循环：for (item in array/string/object) { body }
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
          if (!m_error->isEmpty()) return;
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
        if (!m_error->isEmpty()) {
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
          if (!m_error->isEmpty()) return;
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
      // 将枚举成员作为变量注册到当前作用域
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
      if (!m_error->isEmpty()) return;
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
      // import 语句在模块链接阶段已处理，此处忽略
      break;
  }
}

void AcInterpreter::execBlock(const Block &block) {
  for (int i = 0; i < block.stmts.size(); ++i) {
    const auto &stmt = block.stmts[i];
    execStmt(stmt);
    if (!m_error->isEmpty()) return;
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

QJsonValue AcInterpreter::evalStringBuiltin(const QString &obj, const QString &method,
                                            const std::vector<std::unique_ptr<Expr>> &args,
                                            int line) {
  QString err;
  QJsonValue result = AcBuiltinEval::evalStringMethod(*this, obj, method, args, line, err);
  if (!err.isEmpty()) *m_error = err;
  return result;
}

QJsonValue AcInterpreter::evalArrayBuiltin(const QJsonArray &arr, const QString &method,
                                           const std::vector<std::unique_ptr<Expr>> &args, int line,
                                           QJsonValue &modifiedArr) {
  QString err;
  QJsonValue result =
      AcBuiltinEval::evalArrayMethod(*this, arr, method, args, line, modifiedArr, err);
  if (!err.isEmpty()) *m_error = err;
  return result;
}

// ═════════════════════════════════════════════════════════════════════════════
//  执行入口
// ═════════════════════════════════════════════════════════════════════════════

QJsonValue AcInterpreter::execute(const Block &program, QString &error) {
  m_error = &error;
  m_scopeStack.clear();
  m_usingStack.clear();
  m_classes.clear();
  m_functions.clear();
  m_currentThis = QJsonObject();
  m_hasReturned = false;
  m_returnValue = QJsonValue();
  m_generatedFiles.clear();
  m_funcExprCounter = 0;

  // 全局作用域
  pushScope();

  // 预先注册 C++ 原生类（供 new 语法使用）
  for (const auto &name : AcClass::kAll) {
    ClassDef nativeClass;
    nativeClass.name = name;
    nativeClass.isNative = true;
    m_classes[name] = nativeClass;
  }

  // 注入解释器上下文给 FunBuiltin 使用
  FunMgr::init();
  FunBuiltin::setContext({m_scriptDir, m_rootDir, m_logCallback, &m_generatedFiles, 0});

  // 预扫描：将所有顶层函数/类/枚举定义先注册（函数提升，允许先调用后定义）
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
  if (!m_error->isEmpty()) {
    // 出错时也要清理对象管理器，避免资源泄漏
    AcObjectManager::ins().cleanup();
    return QJsonValue();
  }

  // 执行完毕，释放全局作用域中所有受管理的实例
  while (!m_scopeStack.isEmpty()) {
    popScope();
  }

  return m_hasReturned ? m_returnValue : QJsonValue();
}