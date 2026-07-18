/**
 * @file ac_interpreter_expr.cpp
 * @brief 解释器表达式求值实现文件
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
//  表达式求值
// ═════════════════════════════════════════════════════════════════════════════

QJsonValue AcInterpreter::resolveClassAccess(const QString &className, const QString &propName) {
  if (!m_classes.contains(className)) return QJsonValue(QJsonValue::Undefined);
  const ClassDef &cd = m_classes[className];
  if (!m_staticInited.contains(className)) {
    initStaticVars(cd);
  }
  for (const auto &prop : cd.properties) {
    if (prop.isStatic && prop.key == propName) {
      QJsonObject sv = m_staticVars.value(className);
      return sv.value(propName);
    }
  }
  return QJsonValue(QJsonValue::Undefined);
}

QJsonValue AcInterpreter::evalExpr(const Expr &expr) {
  switch (expr.kind) {
    case Expr::kNull:
      return QJsonValue();

    case Expr::kBool:
      return QJsonValue(expr.boolVal);

    case Expr::kNumber:
      return QJsonValue(expr.numVal);

    case Expr::kString:
      return QJsonValue(expr.strVal);

    case Expr::kThis:
      return QJsonValue(m_currentThis);

    case Expr::kIdent: {
      if (!containsVar(expr.ident)) {
        m_error = QStringLiteral("undefined variable '%1' at line %2")
                      .arg(expr.ident, QString::number(expr.line));
        return QJsonValue();
      }
      return resolveVar(expr.ident);
    }

    case Expr::kPropAccess: {
      QJsonValue obj;
      if (expr.propObject) {
        obj = evalExpr(*expr.propObject);
        if (obj.isNull() && expr.propObject->kind == Expr::kIdent &&
            m_classes.contains(expr.propObject->ident)) {
          const QString &clsName = expr.propObject->ident;
          QJsonValue staticVal = resolveClassAccess(clsName, expr.prop);
          if (!staticVal.isUndefined()) {
            m_error.clear();
            return staticVal;
          }
          const ClassDef &cd = m_classes[clsName];
          for (const auto &p : cd.properties) {
            if (p.isStatic && p.key == expr.prop) {
              m_error.clear();
              return staticVal;
            }
          }
          for (const auto &m : cd.methods) {
            if (m.isStatic && m.name == expr.prop) {
              QJsonObject classObj;
              classObj[QString::fromLatin1(AcRuntime::kClassKey)] = clsName;
              m_error.clear();
              return QJsonValue(classObj);
            }
          }
          QJsonObject classObj;
          classObj[QString::fromLatin1(AcRuntime::kClassKey)] = clsName;
          obj = QJsonValue(classObj);
          m_error.clear();
        }
      } else if (expr.ident == QString::fromLatin1(AcKeyword::kThis)) {
        obj = QJsonValue(m_currentThis);
      } else if (expr.ident == QString::fromLatin1(AcKeyword::kSuper)) {
        obj = QJsonValue(m_currentThis);
      } else {
        obj = resolveVar(expr.ident);

        if (obj.isNull() && expr.ident != QString::fromLatin1(AcKeyword::kThis) &&
            expr.ident != QString::fromLatin1(AcKeyword::kSuper)) {
          QString enumMemberName = QStringLiteral("%1.%2").arg(expr.ident, expr.prop);
          if (containsVar(enumMemberName)) {
            return resolveVar(enumMemberName);
          }
          if (m_classes.contains(expr.ident)) {
            QJsonValue staticVal = resolveClassAccess(expr.ident, expr.prop);
            if (!staticVal.isUndefined()) {
              return staticVal;
            }
            const ClassDef &cd = m_classes[expr.ident];
            for (const auto &p : cd.properties) {
              if (p.isStatic && p.key == expr.prop) {
                return staticVal;
              }
            }
            for (const auto &m : cd.methods) {
              if (m.isStatic && m.name == expr.prop) {
                QJsonObject classObj;
                classObj[QString::fromLatin1(AcRuntime::kClassKey)] = expr.ident;
                return QJsonValue(classObj);
              }
            }
          }
          m_error = QStringLiteral("undefined variable '%1'").arg(expr.ident);
          return QJsonValue();
        }
      }
      if (expr.prop == QStringLiteral("length")) {
        if (obj.isString()) return QJsonValue(obj.toString().length());
        if (obj.isArray()) return QJsonValue(obj.toArray().size());
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
      m_error = QStringLiteral("cannot access index on value at line %1").arg(expr.line);
      return QJsonValue();
    }

    case Expr::kObject: {
      QJsonObject obj;
      for (const auto &e : expr.objEntries) {
        QJsonValue v = evalExpr(*e.value);
        retainIfInstance(v);
        obj[e.key] = v;
      }
      return obj;
    }

    case Expr::kArray: {
      QJsonArray arr;
      for (const auto &e : expr.arrItems) {
        QJsonValue item = evalExpr(*e);
        retainIfInstance(item);
        arr.append(item);
      }
      return arr;
    }

    case Expr::kFuncCall:
      return callBuiltin(expr.funcCall.name, expr.funcCall.args, expr.line);

    case Expr::kFuncExpr: {
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
      QString className = expr.className;
      if (!m_classes.contains(className)) {
        m_error = QStringLiteral("undefined class '%1' at line %2")
                      .arg(className, QString::number(expr.line));
        return QJsonValue();
      }
      const ClassDef &cd = m_classes[className];

      if (!m_staticInited.contains(className)) {
        initStaticVars(cd);
      }

      for (const auto &prop : cd.properties) {
        if (prop.isStatic && prop.key == expr.prop) {
          QJsonObject sv = m_staticVars.value(className);
          return sv.value(expr.prop);
        }
      }

      QJsonArray callArgs;
      for (const auto &arg : expr.funcCall.args) {
        callArgs.append(evalExpr(*arg));
        if (!m_error.isEmpty()) return QJsonValue();
      }
      for (const auto &md : cd.methods) {
        if (md.isStatic && md.name == expr.prop) {
          return execMethod(md, QJsonObject(), QJsonValue(callArgs));
        }
      }

      m_error = QStringLiteral("class '%1' has no static member '%2' at line %3")
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
      return val;
    }
    case Expr::kPostDec: {
      QJsonValue val = evalExpr(*expr.operand);
      double newVal = val.toDouble() - 1;
      if (expr.operand->kind == Expr::kIdent) {
        setVar(expr.operand->ident, QJsonValue(newVal));
      }
      return val;
    }

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
        m_error = QStringLiteral("division by zero");
        return QJsonValue();
      }
      return QJsonValue(l.toDouble() / r.toDouble());
    case Expr::kMod:
      if (r.toDouble() == 0.0) {
        m_error = QStringLiteral("modulo by zero");
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
//  内置函数调用
// ═════════════════════════════════════════════════════════════════════════════

QJsonValue AcInterpreter::callBuiltin(const QString &name,
                                      const std::vector<std::unique_ptr<Expr>> &args, int line) {
  QJsonArray arr;
  for (const auto &a : args) arr.append(evalExpr(*a));

  if (name == AcBuiltin::kCall) {
    if (arr.size() < 2) {
      m_error = QStringLiteral("call() requires at least 2 arguments");
      return QJsonValue();
    }
    QString cls = arr[0].toString();
    QString func = arr[1].toString();
    QJsonArray callArgs;
    if (arr.size() >= 3) callArgs.append(arr[2]);
    QJsonValue r = FunMgr::ins().call(cls, func, callArgs);
    QString err = FunMgr::takeError();
    if (!err.isEmpty()) {
      m_error = QStringLiteral("%1 at line %2").arg(err).arg(line);
      return QJsonValue();
    }
    return r;
  }

  const QString builtinClass = QString::fromLatin1(AcRuntime::kBuiltinClass);
  if (FunMgr::ins().contains(builtinClass, name)) {
    FunBuiltin::setCurrentLine(line);
    QJsonValue r = FunMgr::ins().call(builtinClass, name, arr);
    FunBuiltin::setCurrentLine(0);
    QString err = FunMgr::takeError();
    if (!err.isEmpty()) {
      m_error = QStringLiteral("%1 at line %2").arg(err).arg(line);
      return QJsonValue();
    }
    return r;
  }

  auto it = m_functions.find(name);
  if (it != m_functions.end()) return execUserFunction(*it, QJsonValue(arr));

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

  m_error = QStringLiteral("unknown function '%1' at line %2").arg(name).arg(line);
  return QJsonValue();
}

QJsonValue AcInterpreter::evalMethodCall(const Expr &expr) {
  QJsonValue objVal;
  bool isChained = (expr.methodCall.object != nullptr);
  bool isSuper = (expr.methodCall.objName == QString::fromLatin1(AcKeyword::kSuper));

  if (!isChained && !isSuper && expr.methodCall.objName == QStringLiteral("JSON")) {
    const QString &method = expr.methodCall.methodName;
    if (method == QStringLiteral("parse")) {
      if (expr.methodCall.args.empty()) {
        m_error = QStringLiteral("JSON.parse() requires 1 argument at line %1").arg(expr.line);
        return QJsonValue();
      }
      QJsonValue argVal = evalExpr(*expr.methodCall.args[0]);
      if (!argVal.isString()) {
        m_error =
            QStringLiteral("JSON.parse() argument must be a string at line %1").arg(expr.line);
        return QJsonValue();
      }
      QJsonParseError parseError;
      QJsonDocument doc = QJsonDocument::fromJson(argVal.toString().toUtf8(), &parseError);
      if (parseError.error != QJsonParseError::NoError) {
        m_error = QStringLiteral("JSON.parse() error: %1 at line %2")
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
        m_error = QStringLiteral("JSON.stringify() requires 1 argument at line %1").arg(expr.line);
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
    m_error = QStringLiteral("JSON has no method '%1' at line %2").arg(method).arg(expr.line);
    return QJsonValue();
  }

  if (isChained) {
    objVal = evalExpr(*expr.methodCall.object);
    if (objVal.isNull()) {
      if (expr.methodCall.object->kind == Expr::kIdent &&
          m_classes.contains(expr.methodCall.object->ident)) {
        QJsonObject classObj;
        classObj[QString::fromLatin1(AcRuntime::kClassKey)] = expr.methodCall.object->ident;
        objVal = QJsonValue(classObj);
        m_error.clear();
      } else {
        m_error = QStringLiteral("method call on null value at line %1").arg(expr.line);
        return QJsonValue();
      }
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
        m_error =
            QStringLiteral("undefined variable '%1' in method call").arg(expr.methodCall.objName);
        return QJsonValue();
      }
    }
  }

  if (objVal.isString()) {
    return evalStringBuiltin(objVal.toString(), expr.methodCall.methodName, expr.methodCall.args,
                             expr.line);
  }

  if (objVal.isArray()) {
    QJsonValue modifiedArr;
    QJsonValue result = evalArrayBuiltin(objVal.toArray(), expr.methodCall.methodName,
                                         expr.methodCall.args, expr.line, modifiedArr);
    if (!modifiedArr.isNull()) {
      if (isChained) {
        if (expr.methodCall.object && expr.methodCall.object->kind == Expr::kPropAccess &&
            !expr.methodCall.object->prop.isEmpty()) {
          const Expr &propObj = *expr.methodCall.object;
          if (propObj.ident == QString::fromLatin1(AcKeyword::kThis)) {
            m_currentThis[propObj.prop] = modifiedArr;
            m_modifiedThis[propObj.prop] = modifiedArr;
          } else if (!propObj.ident.isEmpty()) {
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
    m_error = QStringLiteral("cannot call method on non-object '%1' (type=%2) at line %3")
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
    m_error = QStringLiteral("object '%1' has no class information").arg(name);
    return QJsonValue();
  }

  return evalClassMethod(obj, className, expr, isChained, isSuper);
}

QJsonValue AcInterpreter::evalClassMethod(const QJsonObject &obj, const QString &className,
                                          const Expr &expr, bool isChained, bool isSuper) {
  const ClassDef &cd = m_classes[className];

  if (cd.isNative) {
    QJsonArray args;
    args.append(QJsonValue(obj));
    for (const auto &argExpr : expr.methodCall.args) args.append(evalExpr(*argExpr));
    QJsonValue r = FunMgr::ins().call(className, expr.methodCall.methodName, args);
    QString err = FunMgr::takeError();
    if (!err.isEmpty()) {
      m_error = QStringLiteral("%1 at line %2").arg(err).arg(expr.line);
      return QJsonValue();
    }
    return r;
  }

  QString searchClassName = className;
  if (isSuper) {
    if (cd.baseClass.isEmpty()) {
      m_error = QStringLiteral("cannot use 'super' in class without base class at line %1")
                    .arg(expr.line);
      return QJsonValue();
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
    if (!isSuper) {
      m_modifiedThis = savedModifiedThis;
    }
    return result;
  }

  m_error = QStringLiteral("method '%1' not found in class '%2'")
                .arg(expr.methodCall.methodName, className);
  return QJsonValue();
}

QJsonValue AcInterpreter::evalNewInstance(const Expr &expr) {
  if (!m_classes.contains(expr.className)) {
    m_error = QStringLiteral("undefined class '%1'").arg(expr.className);
    return QJsonValue();
  }

  const ClassDef &cd = m_classes[expr.className];
  QJsonObject instance;

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

  instance[QString::fromLatin1(AcRuntime::kClassKey)] = expr.className;

  instance = AcObjectManager::ins().registerInstance(instance, expr.className);

  for (const auto &m : cd.methods) {
    if (m.name == QStringLiteral("constructor")) {
      QJsonArray ctorArgs;
      for (const auto &arg : expr.constructorArgs) ctorArgs.append(evalExpr(*arg));
      QJsonValue ctorResult = execMethod(m, instance, QJsonValue(ctorArgs));
      if (!m_error.isEmpty()) return QJsonValue();
      instance = m_modifiedThis;
      break;
    }
  }

  return QJsonValue(instance);
}

QJsonValue AcInterpreter::evalStringBuiltin(const QString &obj, const QString &method,
                                            const std::vector<std::unique_ptr<Expr>> &args,
                                            int line) {
  QString err;
  QJsonValue result = AcBuiltinEval::evalStringMethod(*this, obj, method, args, line, err);
  if (!err.isEmpty()) m_error = err;
  return result;
}

QJsonValue AcInterpreter::evalArrayBuiltin(const QJsonArray &arr, const QString &method,
                                           const std::vector<std::unique_ptr<Expr>> &args, int line,
                                           QJsonValue &modifiedArr) {
  QString err;
  QJsonValue result =
      AcBuiltinEval::evalArrayMethod(*this, arr, method, args, line, modifiedArr, err);
  if (!err.isEmpty()) m_error = err;
  return result;
}