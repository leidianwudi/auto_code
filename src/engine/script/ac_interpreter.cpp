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

#include "../ac_language.h"
#include "../function/fun_builtin.h"
#include "../function/fun_mgr.h"
#include "../tpl/tpl_engine.h"
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
        *m_error = QStringLiteral("undefined variable '%1'").arg(expr.ident);
        return QJsonValue();
      }
      return resolveVar(expr.ident);
    }

    case Expr::kPropAccess: {
      QJsonValue obj = resolveVar(expr.ident);
      if (obj.isNull() && expr.ident != QString::fromLatin1(AcKeyword::kThis)) {
        *m_error = QStringLiteral("undefined variable '%1'").arg(expr.ident);
        return QJsonValue();
      }
      if (obj.isObject()) return obj.toObject().value(expr.prop);
      if (obj.isArray()) {
        bool ok = false;
        int idx = expr.prop.toInt(&ok);
        if (ok) {
          QJsonArray arr = obj.toArray();
          if (idx >= 0 && idx < arr.size()) return arr[idx];
        }
      }
      return QJsonValue();
    }

    case Expr::kIndexAccess: {
      QJsonValue obj = resolveVar(expr.ident);
      if (obj.isNull() && expr.ident != QString::fromLatin1(AcKeyword::kThis)) {
        *m_error = QStringLiteral("undefined variable '%1'").arg(expr.ident);
        return QJsonValue();
      }
      if (obj.isObject()) return obj.toObject().value(expr.indexKey);
      if (obj.isArray()) {
        bool ok = false;
        int idx = expr.indexKey.toInt(&ok);
        if (ok) {
          QJsonArray arr = obj.toArray();
          if (idx >= 0 && idx < arr.size()) return arr[idx];
        }
      }
      *m_error = QStringLiteral("cannot access index '%1' on '%2'").arg(expr.indexKey, expr.ident);
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
      for (auto *e : expr.arrItems) {
        QJsonValue item = evalExpr(*e);
        // 数组持有实例引用时 retain
        retainIfInstance(item);
        arr.append(item);
      }
      return arr;
    }

    case Expr::kFuncCall:
      return callBuiltin(expr.funcCall.name, expr.funcCall.args, expr.line);

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
      for (auto *arg : expr.funcCall.args) {
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
        QString ls = l.isString() ? l.toString() : QString::number(l.toDouble());
        QString rs = r.isString() ? r.toString() : QString::number(r.toDouble());
        return QJsonValue(ls + rs);
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
  }
  return QJsonValue();
}

// ═════════════════════════════════════════════════════════════════════════════
//  辅助方法
// ═════════════════════════════════════════════════════════════════════════════

QJsonValue AcInterpreter::resolveVar(const QString &name) const {
  if (name == QString::fromLatin1(AcKeyword::kThis)) return QJsonValue(m_currentThis);
  for (int i = m_scopeStack.size() - 1; i >= 0; --i) {
    auto it = m_scopeStack[i].find(name);
    if (it != m_scopeStack[i].end()) return it.value();
  }
  // 在方法内部，未在作用域中找到的变量回退到 this 实例的属性
  if (!m_currentThis.isEmpty() && m_currentThis.contains(name)) return m_currentThis[name];
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
  // 在方法内部，未在作用域中找到的变量回退到 this 实例的属性
  if (!m_currentThis.isEmpty() && m_currentThis.contains(name)) {
    m_currentThis[name] = val;
    m_modifiedThis = m_currentThis;
    return;
  }
  // 新变量
  retainIfInstance(val);
  m_scopeStack.last()[name] = val;
}

void AcInterpreter::pushScope() { m_scopeStack.append(QHash<QString, QJsonValue>()); }

void AcInterpreter::popScope() {
  if (m_scopeStack.isEmpty()) return;
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
  // 在方法内部，未在作用域中找到的变量回退到 this 实例的属性
  if (!m_currentThis.isEmpty() && m_currentThis.contains(name)) return true;
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

QJsonValue AcInterpreter::callBuiltin(const QString &name, const QVector<Expr *> &args, int line) {
  // 统一求值所有参数
  QJsonArray arr;
  for (auto *a : args) arr.append(evalExpr(*a));

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
  qDebug() << "[callBuiltin] name:" << name
           << "contains:" << FunMgr::ins().contains(builtinClass, name) << "args:" << arr
           << "line:" << line;
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
    qDebug() << "[callBuiltin] call result:" << r;
    return r;
  }

  // 尝试查找用户自定义函数
  auto it = m_functions.find(name);
  if (it != m_functions.end()) return execUserFunction(*it, QJsonValue(arr));

  *m_error = QStringLiteral("unknown function '%1' at line %2").arg(name).arg(line);
  return QJsonValue();
}

QJsonValue AcInterpreter::evalMethodCall(const Expr &expr) {
  QJsonValue objVal = resolveVar(expr.methodCall.objName);
  if (objVal.isNull() && expr.methodCall.objName != QString::fromLatin1(AcKeyword::kThis)) {
    *m_error =
        QStringLiteral("undefined variable '%1' in method call").arg(expr.methodCall.objName);
    return QJsonValue();
  }

  if (!objVal.isObject()) {
    *m_error = QStringLiteral("cannot call method on non-object '%1'").arg(expr.methodCall.objName);
    return QJsonValue();
  }

  QJsonObject obj = objVal.toObject();
  QString className = obj.value(QString::fromLatin1(AcRuntime::kClassKey)).toString();
  if (className.isEmpty() || !m_classes.contains(className)) {
    *m_error = QStringLiteral("object '%1' has no class information").arg(expr.methodCall.objName);
    return QJsonValue();
  }

  const ClassDef &cd = m_classes[className];

  // 原生类：路由到 FunMgr（传入实例对象作为第一个参数）
  if (cd.isNative) {
    QJsonArray args;
    args.append(QJsonValue(obj));
    for (auto *argExpr : expr.methodCall.args) args.append(evalExpr(*argExpr));
    QJsonValue r = FunMgr::ins().call(className, expr.methodCall.methodName, args);
    QString err = FunMgr::takeError();
    if (!err.isEmpty()) {
      *m_error = QStringLiteral("%1 at line %2").arg(err).arg(expr.line);
      return QJsonValue();
    }
    return r;
  }

  for (const auto &method : cd.methods) {
    if (method.name == expr.methodCall.methodName) {
      QJsonArray args;
      for (auto *argExpr : expr.methodCall.args) args.append(evalExpr(*argExpr));
      // 保存当前 m_modifiedThis，execMethod 内部会修改它，返回后写回变量再恢复
      QJsonObject savedModifiedThis = m_modifiedThis;
      QJsonValue result = execMethod(method, obj, QJsonValue(args));
      if (expr.methodCall.objName != QString::fromLatin1(AcKeyword::kThis)) {
        if (containsVar(expr.methodCall.objName))
          setVar(expr.methodCall.objName, QJsonValue(m_modifiedThis));
      }
      m_modifiedThis = savedModifiedThis;
      return result;
    }
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
  instance[QString::fromLatin1(AcRuntime::kClassKey)] = expr.className;

  // 原生类：调用 FunMgr 构造器
  if (cd.isNative) {
    QJsonArray ctorArgs;
    for (auto *arg : expr.constructorArgs) ctorArgs.append(evalExpr(*arg));
    QJsonValue ctorResult =
        FunMgr::ins().call(expr.className, QString::fromLatin1(AcRuntime::kConstructor), ctorArgs);
    if (ctorResult.isObject()) instance = ctorResult.toObject();
    instance[QString::fromLatin1(AcRuntime::kClassKey)] = expr.className;
    // 注册到对象管理器，分配唯一ID并设置初始引用计数
    instance = AcObjectManager::ins().registerInstance(instance, expr.className);
    return QJsonValue(instance);
  }

  for (const auto &prop : cd.properties) {
    if (prop.value) {
      QJsonValue v = evalExpr(*prop.value);
      // 属性值如果是受管理实例，retain 使其引用计数 +1
      retainIfInstance(v);
      instance[prop.key] = v;
    } else {
      instance[prop.key] = QJsonValue();
    }
  }

  // 用户自定义类也注册到对象管理器
  instance = AcObjectManager::ins().registerInstance(instance, expr.className);
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

  QJsonObject sv;
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
        break;
      }

      // name = value
      setVar(stmt.assign.name, val);
      break;
    }

    case Block::Stmt::kIndexAssign: {
      QJsonValue objVal = resolveVar(stmt.indexAssign.objName);
      QJsonObject obj = objVal.toObject();
      // release 旧值（如果属性已存在且是受管理实例）
      if (obj.contains(stmt.indexAssign.key)) {
        releaseIfInstanceWithDestruct(obj.value(stmt.indexAssign.key));
      }
      QJsonValue newVal = evalExpr(stmt.indexAssign.value);
      // retain 新值
      retainIfInstance(newVal);
      obj[stmt.indexAssign.key] = newVal;
      if (stmt.indexAssign.objName == QString::fromLatin1(AcKeyword::kThis))
        m_currentThis = obj;
      else
        setVar(stmt.indexAssign.objName, obj);
      break;
    }

    case Block::Stmt::kFor: {
      QJsonValue arrVal = evalExpr(stmt.forStmt.arrayExpr);
      QJsonArray arr = arrVal.toArray();
      if (arr.isEmpty() && arrVal.isArray()) break;
      for (const QJsonValue &v : arr) {
        pushScope();
        setVar(stmt.forStmt.varName, v);
        execBlock(stmt.forStmt.body);
        popScope();
        if (!m_error->isEmpty()) return;
      }
      break;
    }

    case Block::Stmt::kIf:
      if (isTruthy(evalExpr(stmt.ifStmt.condition))) {
        pushScope();
        execBlock(stmt.ifStmt.thenBlock);
        popScope();
      } else if (stmt.ifStmt.hasElse) {
        pushScope();
        execBlock(stmt.ifStmt.elseBlock);
        popScope();
      }
      break;

    case Block::Stmt::kExpr:
      evalExpr(stmt.exprStmt);
      break;

    case Block::Stmt::kClassDef:
      m_classes[stmt.classDef.name] = stmt.classDef;
      initStaticVars(stmt.classDef);  // 初始化静态变量
      break;

    case Block::Stmt::kFuncDef:
      m_functions[stmt.funcDef.name] = stmt.funcDef;
      break;

    case Block::Stmt::kReturn:
      m_hasReturned = true;
      m_returnValue = evalExpr(stmt.returnValue);
      break;
  }
}

void AcInterpreter::execBlock(const Block &block) {
  qDebug() << "[execBlock] stmts count:" << block.stmts.size();
  for (int i = 0; i < block.stmts.size(); ++i) {
    const auto &stmt = block.stmts[i];
    qDebug() << "[execBlock] stmt" << i << "kind:" << (int)stmt.kind;
    execStmt(stmt);
    if (!m_error->isEmpty()) {
      qDebug() << "[execBlock] error after stmt" << i << ":" << *m_error;
      return;
    }
    if (m_hasReturned) return;
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

// ═════════════════════════════════════════════════════════════════════════════
//  执行入口
// ═════════════════════════════════════════════════════════════════════════════

QJsonValue AcInterpreter::execute(const Block &program, QString &error) {
  m_error = &error;
  m_scopeStack.clear();
  m_classes.clear();
  m_functions.clear();
  m_currentThis = QJsonObject();
  m_hasReturned = false;
  m_returnValue = QJsonValue();
  m_generatedFiles.clear();

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
  qDebug() << "[AcInterpreter::execute] m_logCallback is null:" << !m_logCallback
           << "m_scriptDir:" << m_scriptDir << "m_rootDir:" << m_rootDir;
  FunBuiltin::setContext({m_scriptDir, m_rootDir, m_logCallback, &m_generatedFiles, 0});

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