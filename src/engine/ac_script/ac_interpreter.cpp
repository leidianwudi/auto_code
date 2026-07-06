#include "ac_interpreter.h"

#include "../engine_tpl.h"
#include "../function/fun_mgr.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

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

  case Expr::kIdent:
    if (m_vars.contains(expr.ident))
      return m_vars[expr.ident];
    if (m_globals.contains(expr.ident))
      return m_globals[expr.ident];
    *m_error = QStringLiteral("undefined variable '%1'").arg(expr.ident);
    return QJsonValue();

  case Expr::kPropAccess: {
    QJsonValue obj;
    if (expr.ident == QStringLiteral("this"))
      obj = QJsonValue(m_currentThis);
    else if (m_vars.contains(expr.ident))
      obj = m_vars[expr.ident];
    else if (m_globals.contains(expr.ident))
      obj = m_globals[expr.ident];
    else {
      *m_error = QStringLiteral("undefined variable '%1'").arg(expr.ident);
      return QJsonValue();
    }
    if (obj.isObject())
      return obj.toObject().value(expr.prop);
    if (obj.isArray()) {
      bool ok = false;
      int idx = expr.prop.toInt(&ok);
      if (ok) {
        QJsonArray arr = obj.toArray();
        if (idx >= 0 && idx < arr.size())
          return arr[idx];
      }
    }
    return QJsonValue();
  }

  case Expr::kIndexAccess: {
    QJsonValue obj;
    if (expr.ident == QStringLiteral("this"))
      obj = QJsonValue(m_currentThis);
    else if (m_vars.contains(expr.ident))
      obj = m_vars[expr.ident];
    else if (m_globals.contains(expr.ident))
      obj = m_globals[expr.ident];
    if (obj.isObject())
      return obj.toObject().value(expr.indexKey);
    if (obj.isArray()) {
      bool ok = false;
      int idx = expr.indexKey.toInt(&ok);
      if (ok) {
        QJsonArray arr = obj.toArray();
        if (idx >= 0 && idx < arr.size())
          return arr[idx];
      }
    }
    *m_error = QStringLiteral("cannot access index '%1' on '%2'")
                   .arg(expr.indexKey, expr.ident);
    return QJsonValue();
  }

  case Expr::kObject: {
    QJsonObject obj;
    for (const auto &e : expr.objEntries)
      obj[e.key] = evalExpr(*e.value);
    return obj;
  }

  case Expr::kArray: {
    QJsonArray arr;
    for (auto *e : expr.arrItems)
      arr.append(evalExpr(*e));
    return arr;
  }

  case Expr::kFuncCall:
    return callBuiltin(expr.funcCall.name, expr.funcCall.args);

  case Expr::kMethodCall: {
    QJsonValue objVal;
    if (expr.methodCall.objName == QStringLiteral("this"))
      objVal = QJsonValue(m_currentThis);
    else if (m_vars.contains(expr.methodCall.objName))
      objVal = m_vars[expr.methodCall.objName];
    else if (m_globals.contains(expr.methodCall.objName))
      objVal = m_globals[expr.methodCall.objName];
    else {
      *m_error = QStringLiteral("undefined variable '%1' in method call")
                     .arg(expr.methodCall.objName);
      return QJsonValue();
    }

    if (!objVal.isObject()) {
      *m_error = QStringLiteral("cannot call method on non-object '%1'")
                     .arg(expr.methodCall.objName);
      return QJsonValue();
    }

    QJsonObject obj = objVal.toObject();
    QString className = obj.value(QStringLiteral("__class__")).toString();
    if (className.isEmpty() || !m_classes.contains(className)) {
      *m_error = QStringLiteral("object '%1' has no class information")
                     .arg(expr.methodCall.objName);
      return QJsonValue();
    }

    const ClassDef &cd = m_classes[className];
    for (const auto &method : cd.methods) {
      if (method.name == expr.methodCall.methodName) {
        QJsonArray args;
        for (auto *argExpr : expr.methodCall.args)
          args.append(evalExpr(*argExpr));
        QJsonValue result = execMethod(method, obj, QJsonValue(args));
        if (expr.methodCall.objName != QStringLiteral("this")) {
          if (m_vars.contains(expr.methodCall.objName))
            m_vars[expr.methodCall.objName] = QJsonValue(m_modifiedThis);
          else if (m_globals.contains(expr.methodCall.objName))
            m_globals[expr.methodCall.objName] = QJsonValue(m_modifiedThis);
        }
        return result;
      }
    }

    *m_error = QStringLiteral("method '%1' not found in class '%2'")
                   .arg(expr.methodCall.methodName, className);
    return QJsonValue();
  }

  case Expr::kNewInstance: {
    if (!m_classes.contains(expr.className)) {
      *m_error = QStringLiteral("undefined class '%1'").arg(expr.className);
      return QJsonValue();
    }

    const ClassDef &cd = m_classes[expr.className];
    QJsonObject instance;
    instance[QStringLiteral("__class__")] = expr.className;

    for (const auto &prop : cd.properties) {
      if (prop.value)
        instance[prop.key] = evalExpr(*prop.value);
      else
        instance[prop.key] = QJsonValue();
    }

    return QJsonValue(instance);
  }

  case Expr::kBinary:
    return evalBinary(expr);
  }

  return QJsonValue();
}

QJsonValue AcInterpreter::evalExprWithThis(const Expr &expr,
                                           const QJsonObject &thisObj) {
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
//  内置函数 & FunMgr
// ═════════════════════════════════════════════════════════════════════════════

QJsonValue AcInterpreter::callFunMgr(const QString &cls, const QString &func,
                                     const QJsonValue &args) {
  QJsonArray arr;
  if (args.isArray())
    arr = args.toArray();
  else if (!args.isNull() && !args.isUndefined())
    arr.append(args);
  return FunMgr::ins().call(cls, func, arr);
}

QJsonValue AcInterpreter::callBuiltin(const QString &name,
                                      const QVector<Expr *> &args) {
  if (name == QStringLiteral("call")) {
    if (args.size() < 2) {
      *m_error = QStringLiteral("call() requires at least 2 arguments");
      return QJsonValue();
    }
    QString cls = evalExpr(*args[0]).toString();
    QString func = evalExpr(*args[1]).toString();
    QJsonValue a = args.size() >= 3 ? evalExpr(*args[2]) : QJsonValue();
    return callFunMgr(cls, func, a);
  }

  if (name == QStringLiteral("readJson")) {
    if (args.isEmpty()) {
      *m_error = QStringLiteral("readJson() requires a path argument");
      return QJsonValue();
    }
    QString path = evalExpr(*args[0]).toString();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
      return QJsonObject();
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    return doc.isObject() ? QJsonValue(doc.object()) : QJsonValue();
  }

  if (name == QStringLiteral("render")) {
    if (args.size() < 2) {
      *m_error =
          QStringLiteral("render() requires template and data arguments");
      return QJsonValue();
    }
    QString tplPath = evalExpr(*args[0]).toString();
    QFileInfo tplInfo(tplPath);
    if (tplInfo.isRelative())
      tplPath = m_scriptDir + QStringLiteral("/") + tplPath;

    QFile f(tplPath);
    if (!f.open(QIODevice::ReadOnly))
      return QJsonValue();
    QString tpl = QString::fromUtf8(f.readAll());

    TemplateEngine engine;
    if (m_logCallback)
      engine.setLogCallback(m_logCallback);
    QJsonValue data = evalExpr(*args[1]);
    if (!data.isObject()) {
      *m_error = QStringLiteral("render() data must be a JSON object");
      return QJsonValue();
    }
    return QJsonValue(engine.render(tpl, data.toObject()));
  }

  if (name == QStringLiteral("print")) {
    if (args.isEmpty()) {
      *m_error = QStringLiteral("print() requires at least one argument");
      return QJsonValue();
    }
    QString text = evalExpr(*args[0]).toString();
    if (m_logCallback)
      m_logCallback(text, false);
    return QJsonValue(true);
  }

  if (name == QStringLiteral("write")) {
    if (args.size() < 2) {
      *m_error = QStringLiteral("write() requires path and content arguments");
      return QJsonValue(false);
    }
    QString path = evalExpr(*args[0]).toString();
    QString content = evalExpr(*args[1]).toString();
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
      return QJsonValue(false);
    f.write(content.toUtf8());
    m_generatedFiles.append(QDir::toNativeSeparators(path));
    return QJsonValue(true);
  }

  if (name == QStringLiteral("getCheckedFiles")) {
    QString treePath = m_rootDir.isEmpty()
                           ? m_scriptDir + QStringLiteral("/tree.config")
                           : m_rootDir + QStringLiteral("/tree.config");
    QFile f(treePath);
    QJsonArray result;
    if (f.open(QIODevice::ReadOnly)) {
      QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
      QJsonArray checked =
          doc.object().value(QStringLiteral("checked")).toArray();
      for (const QJsonValue &v : checked) {
        if (v.isString())
          result.append(QDir::cleanPath(m_rootDir.isEmpty()
                                            ? m_scriptDir
                                            : m_rootDir + QStringLiteral("/") +
                                                  v.toString()));
      }
    }
    return result;
  }

  if (name == QStringLiteral("merge")) {
    if (args.size() < 2) {
      *m_error = QStringLiteral("merge() requires two arguments");
      return QJsonValue();
    }
    QJsonValue a = evalExpr(*args[0]);
    QJsonValue b = evalExpr(*args[1]);
    QJsonObject result = a.toObject();
    QJsonObject ob = b.toObject();
    for (auto it = ob.begin(); it != ob.end(); ++it)
      result[it.key()] = it.value();
    return result;
  }

  if (name == QStringLiteral("basename")) {
    if (args.isEmpty())
      return QJsonValue();
    QString path = evalExpr(*args[0]).toString();
    return QJsonValue(QFileInfo(path).completeBaseName());
  }

  *m_error = QStringLiteral("unknown function '%1'").arg(name);
  return QJsonValue();
}

// ═════════════════════════════════════════════════════════════════════════════
//  类方法执行
// ═════════════════════════════════════════════════════════════════════════════

QJsonValue AcInterpreter::execMethod(const MethodDef &method,
                                     const QJsonObject &thisObj,
                                     const QJsonValue &callArgs) {
  QJsonObject oldThis = m_currentThis;
  bool oldHasReturned = m_hasReturned;
  QJsonValue oldReturnValue = m_returnValue;

  m_currentThis = thisObj;
  m_hasReturned = false;
  m_returnValue = QJsonValue();

  QHash<QString, QJsonValue> savedVars = m_vars;
  m_vars.clear();

  for (auto it = thisObj.begin(); it != thisObj.end(); ++it)
    m_vars[it.key()] = it.value();

  QJsonArray argsArr = callArgs.toArray();
  for (int i = 0; i < method.params.size(); ++i) {
    if (i < argsArr.size())
      m_vars[method.params[i]] = argsArr[i];
    else
      m_vars[method.params[i]] = QJsonValue();
  }

  execBlock(method.body);

  QJsonValue result = m_hasReturned ? m_returnValue : QJsonValue();
  m_modifiedThis = m_currentThis;

  m_vars = savedVars;
  m_currentThis = oldThis;
  m_hasReturned = oldHasReturned;
  m_returnValue = oldReturnValue;

  return result;
}

// ═════════════════════════════════════════════════════════════════════════════
//  语句执行
// ═════════════════════════════════════════════════════════════════════════════

void AcInterpreter::execStmt(const Block::Stmt &stmt) {
  switch (stmt.kind) {
  case Block::Stmt::kCall: {
    QString cls = evalExpr(stmt.call.className).toString();
    QString func = evalExpr(stmt.call.funcName).toString();
    QJsonValue a =
        stmt.call.args.kind != Expr::kString || !stmt.call.args.strVal.isEmpty()
            ? evalExpr(stmt.call.args)
            : QJsonValue();
    callFunMgr(cls, func, a);
    break;
  }

  case Block::Stmt::kAssign: {
    QJsonValue val = evalExpr(stmt.assign.value);
    if (!stmt.assign.thisProp.isEmpty()) {
      if (!m_currentThis.isEmpty()) {
        QJsonObject updated = m_currentThis;
        updated[stmt.assign.thisProp] = val;
        m_currentThis = updated;
      }
    } else {
      m_vars[stmt.assign.name] = val;
      if (!m_currentThis.isEmpty() &&
          m_currentThis.contains(stmt.assign.name)) {
        QJsonObject updated = m_currentThis;
        updated[stmt.assign.name] = val;
        m_currentThis = updated;
      }
    }
    break;
  }

  case Block::Stmt::kIndexAssign: {
    QJsonValue objVal;
    if (stmt.indexAssign.objName == QStringLiteral("this"))
      objVal = QJsonValue(m_currentThis);
    else
      objVal = m_vars.value(stmt.indexAssign.objName);
    QJsonObject obj = objVal.toObject();
    obj[stmt.indexAssign.key] = evalExpr(stmt.indexAssign.value);
    if (stmt.indexAssign.objName == QStringLiteral("this"))
      m_currentThis = obj;
    else
      m_vars[stmt.indexAssign.objName] = obj;
    break;
  }

  case Block::Stmt::kFor: {
    QJsonValue arrVal = evalExpr(stmt.forStmt.arrayExpr);
    QJsonArray arr = arrVal.toArray();
    if (arr.isEmpty() && arrVal.isArray())
      break;
    for (const QJsonValue &v : arr) {
      m_vars[stmt.forStmt.varName] = v;
      execBlock(stmt.forStmt.body);
      if (!m_error->isEmpty())
        return;
    }
    break;
  }

  case Block::Stmt::kIf: {
    QJsonValue cond = evalExpr(stmt.ifStmt.condition);
    bool truthy = false;
    if (cond.isBool())
      truthy = cond.toBool();
    else if (cond.isString())
      truthy = !cond.toString().isEmpty();
    else if (cond.isDouble())
      truthy = cond.toDouble() != 0.0;
    else if (cond.isNull() || cond.isUndefined())
      truthy = false;
    else
      truthy = true;

    if (truthy)
      execBlock(stmt.ifStmt.thenBlock);
    else if (stmt.ifStmt.hasElse)
      execBlock(stmt.ifStmt.elseBlock);
    break;
  }

  case Block::Stmt::kExpr:
    evalExpr(stmt.exprStmt);
    break;

  case Block::Stmt::kClassDef:
    m_classes[stmt.classDef.name] = stmt.classDef;
    break;

  case Block::Stmt::kReturn:
    m_hasReturned = true;
    m_returnValue = evalExpr(stmt.returnValue);
    break;
  }
}

void AcInterpreter::execBlock(const Block &block) {
  for (const auto &stmt : block.stmts) {
    execStmt(stmt);
    if (!m_error->isEmpty())
      return;
    if (m_hasReturned)
      return;
  }
}

void AcInterpreter::execBlockWithThis(const Block &block,
                                      const QJsonObject &thisObj,
                                      QJsonValue &returnVal) {
  QJsonObject oldThis = m_currentThis;
  m_currentThis = thisObj;
  execBlock(block);
  if (m_hasReturned)
    returnVal = m_returnValue;
  m_currentThis = oldThis;
}

// ═════════════════════════════════════════════════════════════════════════════
//  执行入口
// ═════════════════════════════════════════════════════════════════════════════

QJsonValue AcInterpreter::execute(const Block &program, QString &error) {
  m_error = &error;
  m_vars.clear();
  m_globals.clear();
  m_classes.clear();
  m_instances.clear();
  m_currentThis = QJsonObject();
  m_hasReturned = false;
  m_returnValue = QJsonValue();
  m_generatedFiles.clear();

  execBlock(program);
  if (!m_error->isEmpty())
    return QJsonValue();

  return m_hasReturned ? m_returnValue : QJsonValue();
}

// ═════════════════════════════════════════════════════════════════════════════
//  未声明标识符验证
// ═════════════════════════════════════════════════════════════════════════════

QStringList AcInterpreter::validateUndeclaredIdents(
    const Block &program, const QSet<QString> &declaredVars) const {
  QStringList errors;
  QSet<QString> scopeVars = declaredVars;
  validateBlockIdents(program, errors, scopeVars);
  return errors;
}

void AcInterpreter::validateBlockIdents(const Block &block, QStringList &errors,
                                        const QSet<QString> &scopeVars) const {
  for (const auto &stmt : block.stmts)
    validateStmtIdents(stmt, errors, scopeVars);
}

void AcInterpreter::validateStmtIdents(const Block::Stmt &stmt,
                                       QStringList &errors,
                                       const QSet<QString> &scopeVars) const {
  switch (stmt.kind) {
  case Block::Stmt::kCall:
    validateExprIdents(stmt.call.className, errors, scopeVars);
    validateExprIdents(stmt.call.funcName, errors, scopeVars);
    validateExprIdents(stmt.call.args, errors, scopeVars);
    break;
  case Block::Stmt::kAssign:
    validateExprIdents(stmt.assign.value, errors, scopeVars);
    break;
  case Block::Stmt::kIndexAssign:
    validateExprIdents(stmt.indexAssign.value, errors, scopeVars);
    break;
  case Block::Stmt::kFor: {
    validateExprIdents(stmt.forStmt.arrayExpr, errors, scopeVars);
    QSet<QString> bodyScope = scopeVars;
    bodyScope.insert(stmt.forStmt.varName);
    validateBlockIdents(stmt.forStmt.body, errors, bodyScope);
    break;
  }
  case Block::Stmt::kIf:
    validateExprIdents(stmt.ifStmt.condition, errors, scopeVars);
    validateBlockIdents(stmt.ifStmt.thenBlock, errors, scopeVars);
    if (stmt.ifStmt.hasElse)
      validateBlockIdents(stmt.ifStmt.elseBlock, errors, scopeVars);
    break;
  case Block::Stmt::kExpr:
    validateExprIdents(stmt.exprStmt, errors, scopeVars);
    break;
  case Block::Stmt::kClassDef: {
    QSet<QString> classScope = scopeVars;
    for (const auto &prop : stmt.classDef.properties) {
      if (prop.value)
        validateExprIdents(*prop.value, errors, classScope);
    }
    classScope.insert(QStringLiteral("this"));
    for (const auto &prop : stmt.classDef.properties)
      classScope.insert(prop.key);
    for (const auto &method : stmt.classDef.methods) {
      QSet<QString> methodScope = classScope;
      for (const auto &param : method.params)
        methodScope.insert(param);
      validateBlockIdents(method.body, errors, methodScope);
    }
    break;
  }
  case Block::Stmt::kReturn:
    validateExprIdents(stmt.returnValue, errors, scopeVars);
    break;
  }
}

void AcInterpreter::validateExprIdents(const Expr &expr, QStringList &errors,
                                       const QSet<QString> &scopeVars) const {
  switch (expr.kind) {
  case Expr::kIdent:
    if (!scopeVars.contains(expr.ident)) {
      errors << QStringLiteral("undefined variable '%1' at line %2")
                    .arg(expr.ident, QString::number(expr.line));
    }
    break;
  case Expr::kPropAccess:
    if (expr.ident != QStringLiteral("this") &&
        !scopeVars.contains(expr.ident)) {
      errors << QStringLiteral("undefined variable '%1' at line %2")
                    .arg(expr.ident, QString::number(expr.line));
    }
    break;
  case Expr::kIndexAccess:
    if (expr.ident != QStringLiteral("this") &&
        !scopeVars.contains(expr.ident)) {
      errors << QStringLiteral("undefined variable '%1' at line %2")
                    .arg(expr.ident, QString::number(expr.line));
    }
    break;
  case Expr::kFuncCall:
    for (const auto *arg : expr.funcCall.args)
      validateExprIdents(*arg, errors, scopeVars);
    break;
  case Expr::kMethodCall:
    if (expr.methodCall.objName != QStringLiteral("this") &&
        !scopeVars.contains(expr.methodCall.objName)) {
      errors << QStringLiteral("undefined variable '%1' at line %2")
                    .arg(expr.methodCall.objName, QString::number(expr.line));
    }
    for (const auto *arg : expr.methodCall.args)
      validateExprIdents(*arg, errors, scopeVars);
    break;
  case Expr::kNewInstance:
    break;
  case Expr::kThis:
    break;
  case Expr::kObject:
    for (const auto &entry : expr.objEntries) {
      if (entry.value)
        validateExprIdents(*entry.value, errors, scopeVars);
    }
    break;
  case Expr::kArray:
    for (const auto *item : expr.arrItems)
      validateExprIdents(*item, errors, scopeVars);
    break;
  case Expr::kBinary:
    if (expr.left)
      validateExprIdents(*expr.left, errors, scopeVars);
    if (expr.right)
      validateExprIdents(*expr.right, errors, scopeVars);
    break;
  default:
    break;
  }
}