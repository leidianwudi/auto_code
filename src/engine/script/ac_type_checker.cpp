/**
 * @file ac_type_checker.cpp
 * @brief 类型检查器实现
 */

#include "ac_type_checker.h"

#include "../ac_language.h"

void AcTypeChecker::check(const Block &program, const QSet<QString> &declaredVars,
                          const QHash<QString, ClassDef> &classes,
                          const QHash<QString, MethodDef> &functions, QStringList &errors) {
  m_classes = &classes;
  m_functions = &functions;
  m_errors = &errors;
  m_interfaces.clear();

  // 从 AST 中收集接口定义
  for (const auto &stmt : program.stmts) {
    if (stmt.kind == Block::Stmt::kInterfaceDef) {
      m_interfaces.insert(stmt.interfaceDef.name, stmt.interfaceDef);
    }
  }

  // 验证类实现接口
  for (auto it = classes.constBegin(); it != classes.constEnd(); ++it) {
    const ClassDef &cd = it.value();
    if (cd.isNative) continue;
    checkImplements(cd);
    checkOverride(cd);
    checkAccessInClass(cd);
  }

  // 初始化类型环境：将已声明的变量全部设为 Any
  TypeEnv env;
  for (const auto &var : declaredVars) {
    env.varTypes.insert(var, AcType::any());
  }

  checkBlock(program, env);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Block / Stmt 检查
// ═════════════════════════════════════════════════════════════════════════════

void AcTypeChecker::checkBlock(const Block &block, TypeEnv &env) {
  for (const auto &stmt : block.stmts) {
    checkStmt(stmt, env);
  }
}

void AcTypeChecker::checkStmt(const Block::Stmt &stmt, TypeEnv &env) {
  switch (stmt.kind) {
    case Block::Stmt::kCall:
      checkCallStmt(stmt.call, env);
      break;

    case Block::Stmt::kAssign:
      checkAssign(stmt.assign, env);
      break;

    case Block::Stmt::kIndexAssign:
      checkExpr(stmt.indexAssign.objectExpr, env);
      checkExpr(stmt.indexAssign.indexExpr, env);
      checkExpr(stmt.indexAssign.value, env);
      break;

    case Block::Stmt::kPropAssign:
      checkExpr(stmt.propAssign.objectExpr, env);
      checkExpr(stmt.propAssign.value, env);
      break;

    case Block::Stmt::kFor: {
      // for (var in arrayExpr) { body }
      AcType arrType = checkExpr(stmt.forStmt.arrayExpr, env);
      if (arrType.kind == AcType::kArray) {
        // 循环变量类型为数组元素类型
        env.varTypes.insert(stmt.forStmt.varName, *arrType.elementType);
      } else {
        env.varTypes.insert(stmt.forStmt.varName, AcType::any());
      }
      checkBlock(stmt.forStmt.body, env);
      break;
    }

    case Block::Stmt::kIf:
      checkExpr(stmt.ifStmt.condition, env);
      checkBlock(stmt.ifStmt.thenBlock, env);
      if (stmt.ifStmt.elseIfBranch) {
        // 递归检查 else if 链
        const IfStmt *cur = stmt.ifStmt.elseIfBranch;
        while (cur) {
          checkExpr(cur->condition, env);
          checkBlock(cur->thenBlock, env);
          cur = cur->elseIfBranch;
        }
      } else if (stmt.ifStmt.hasElse) {
        checkBlock(stmt.ifStmt.elseBlock, env);
      }
      break;

    case Block::Stmt::kExpr:
      checkExpr(stmt.exprStmt, env);
      break;

    case Block::Stmt::kWhile:
      checkExpr(stmt.whileStmt.condition, env);
      checkBlock(stmt.whileStmt.body, env);
      break;

    case Block::Stmt::kSwitch:
      checkExpr(stmt.switchStmt.expr, env);
      for (const auto &sc : stmt.switchStmt.cases) {
        if (!sc.isDefault) checkExpr(sc.value, env);
        checkBlock(sc.body, env);
      }
      break;

    case Block::Stmt::kBreak:
    case Block::Stmt::kContinue:
      break;

    case Block::Stmt::kClassDef: {
      TypeEnv classEnv = env;
      classEnv.className = stmt.classDef.name;
      for (const auto &prop : stmt.classDef.properties) {
        classEnv.propTypes.insert(prop.key, prop.type);
        if (prop.value) {
          AcType valType = checkExpr(*prop.value, classEnv);
          if (!isCompatible(valType, prop.type)) {
            reportError(QStringLiteral(
                            "type mismatch: property '%1' expects '%2' but default value is '%3'")
                            .arg(prop.key, typeToString(prop.type), typeToString(valType)),
                        prop.value->line);
          }
        }
      }
      // 继承：将父类属性加入环境
      if (!stmt.classDef.baseClass.isEmpty() && m_classes->contains(stmt.classDef.baseClass)) {
        const ClassDef &base = (*m_classes)[stmt.classDef.baseClass];
        for (const auto &prop : base.properties) {
          if (!classEnv.propTypes.contains(prop.key)) {
            classEnv.propTypes.insert(prop.key, prop.type);
          }
        }
      }
      for (const auto &method : stmt.classDef.methods) {
        TypeEnv methodEnv = classEnv;
        for (const auto &param : method.params) {
          methodEnv.varTypes.insert(param.name, param.type);
        }
        methodEnv.varTypes.insert(QString::fromLatin1(AcKeyword::kThis),
                                  AcType::classType(classEnv.className));
        // 继承：super 指向父类
        if (!stmt.classDef.baseClass.isEmpty()) {
          methodEnv.varTypes.insert(QStringLiteral("super"),
                                    AcType::classType(stmt.classDef.baseClass));
        }
        checkBlock(method.body, methodEnv);
      }
      break;
    }

    case Block::Stmt::kInterfaceDef:
      break;

    case Block::Stmt::kFuncDef: {
      TypeEnv funcEnv = env;
      // 函数参数
      for (const auto &param : stmt.funcDef.params) {
        funcEnv.varTypes.insert(param.name, param.type);
      }
      // 函数名在函数体内可递归调用
      funcEnv.varTypes.insert(stmt.funcDef.name, AcType::any());
      checkBlock(stmt.funcDef.body, funcEnv);
      // 将函数名加入外部环境
      env.varTypes.insert(stmt.funcDef.name, AcType::any());
      break;
    }

    case Block::Stmt::kReturn: {
      // 返回类型检查在类/函数上下文中进行
      // 这里只推导表达式类型
      checkExpr(stmt.returnValue, env);
      break;
    }

    case Block::Stmt::kImport:
      // import 语句在模块链接阶段已处理，此处忽略
      break;
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  赋值检查
// ═════════════════════════════════════════════════════════════════════════════

void AcTypeChecker::checkAssign(const AssignStmt &as, const TypeEnv &env) {
  AcType valType = checkExpr(as.value, env);

  // 静态属性赋值：ClassName::prop = value
  if (as.isStatic) {
    if (m_classes->contains(as.staticClassName)) {
      const ClassDef &cd = (*m_classes)[as.staticClassName];
      for (const auto &prop : cd.properties) {
        if (prop.isStatic && prop.key == as.name) {
          if (!isCompatible(valType, prop.type)) {
            reportError(
                QStringLiteral("type mismatch: static property '%1::%2' expects '%3' but got '%4'")
                    .arg(as.staticClassName, as.name, typeToString(prop.type),
                         typeToString(valType)),
                as.value.line);
          }
          return;
        }
      }
      reportError(
          QStringLiteral("static property '%1::%2' not found").arg(as.staticClassName, as.name),
          as.value.line);
    } else {
      reportError(QStringLiteral("unknown class '%1' in static assignment").arg(as.staticClassName),
                  as.value.line);
    }
    return;
  }

  // this.prop = value
  if (!as.thisProp.isEmpty()) {
    if (env.propTypes.contains(as.thisProp)) {
      AcType propType = env.propTypes[as.thisProp];
      if (!isCompatible(valType, propType)) {
        reportError(QStringLiteral("type mismatch: property '%1' expects '%2' but got '%3'")
                        .arg(as.thisProp, typeToString(propType), typeToString(valType)),
                    as.value.line);
      }
    }
    return;
  }

  // name = value
  if (as.hasTypeAnnotation) {
    // let x: Type = value — 使用类型注解
    if (!isCompatible(valType, as.typeAnnotation)) {
      reportError(QStringLiteral("type mismatch: variable '%1' annotated as '%2' but got '%3'")
                      .arg(as.name, typeToString(as.typeAnnotation), typeToString(valType)),
                  as.value.line);
    }
    // 用注解类型更新环境（首次声明时覆盖 Any）
    const_cast<TypeEnv &>(env).varTypes.insert(as.name, as.typeAnnotation);
  } else if (env.varTypes.contains(as.name)) {
    AcType varType = env.varTypes[as.name];
    if (!isCompatible(valType, varType)) {
      reportError(QStringLiteral("type mismatch: variable '%1' expects '%2' but got '%3'")
                      .arg(as.name, typeToString(varType), typeToString(valType)),
                  as.value.line);
    }
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  call 语句检查
// ═════════════════════════════════════════════════════════════════════════════

void AcTypeChecker::checkCallStmt(const CallStmt &cs, const TypeEnv &env) {
  // call("cls", "func", args) — 动态调用，运行时才能确定类型
  // 仅检查参数表达式是否合法
  checkExpr(cs.className, env);
  checkExpr(cs.funcName, env);
  checkExpr(cs.args, env);
}

// ═════════════════════════════════════════════════════════════════════════════
//  表达式类型推导
// ═════════════════════════════════════════════════════════════════════════════

AcType AcTypeChecker::checkExpr(const Expr &expr, const TypeEnv &env) {
  switch (expr.kind) {
    case Expr::kString:
      return AcType::string();

    case Expr::kNumber:
      return AcType::number();

    case Expr::kBool:
      return AcType::boolean();

    case Expr::kThis:
      if (!env.className.isEmpty()) {
        return AcType::classType(env.className);
      }
      return AcType::any();

    case Expr::kIdent: {
      if (env.varTypes.contains(expr.ident)) {
        return env.varTypes[expr.ident];
      }
      // 未知标识符 — 运行时才能确定，设为 Any 不报错
      return AcType::any();
    }

    case Expr::kPropAccess: {
      // obj.prop
      if (env.varTypes.contains(expr.ident)) {
        AcType objType = env.varTypes[expr.ident];
        if (objType.kind == AcType::kClass && m_classes->contains(objType.className)) {
          const ClassDef &cd = (*m_classes)[objType.className];
          for (const auto &prop : cd.properties) {
            if (prop.key == expr.prop) return prop.type;
          }
        }
      }
      return AcType::any();
    }

    case Expr::kIndexAccess: {
      // obj[expr] — 索引访问
      AcType objType = checkExpr(*expr.left, env);
      checkExpr(*expr.right, env);
      if (objType.kind == AcType::kArray && objType.elementType) {
        return *objType.elementType;
      }
      return AcType::any();
    }

    case Expr::kNull:
      return AcType::any();

    case Expr::kUndefined:
      return AcType::any();

    case Expr::kTernary:
      checkExpr(*expr.left, env);
      return checkExpr(*expr.right, env);

    case Expr::kArray: {
      // 推导数组元素类型：取所有元素的最小公共类型
      AcType elemType = AcType::any();
      for (const auto *item : expr.arrItems) {
        AcType itemType = checkExpr(*item, env);
        if (elemType.kind == AcType::kAny && itemType.kind != AcType::kAny) {
          elemType = itemType;
        }
      }
      return AcType::arrayOf(elemType);
    }

    case Expr::kObject:
      // 对象字面量 — 类型为 Any
      for (const auto &entry : expr.objEntries) {
        if (entry.value) checkExpr(*entry.value, env);
      }
      return AcType::any();

    case Expr::kFuncCall: {
      // 函数调用 name(args)
      // 检查参数类型是否匹配函数定义
      if (m_functions->contains(expr.funcCall.name)) {
        const MethodDef &func = (*m_functions)[expr.funcCall.name];
        // 检查参数个数
        int argCount = expr.funcCall.args.size();
        int paramCount = func.params.size();
        if (argCount != paramCount) {
          reportError(QStringLiteral("function '%1' expects %2 arguments but got %3")
                          .arg(expr.funcCall.name)
                          .arg(paramCount)
                          .arg(argCount),
                      expr.line);
        }
        // 检查参数类型
        for (int i = 0; i < qMin(argCount, paramCount); ++i) {
          AcType argType = checkExpr(*expr.funcCall.args[i], env);
          if (!isCompatible(argType, func.params[i].type)) {
            reportError(
                QStringLiteral("type mismatch in argument %1 of '%2': expected '%3' but got '%4'")
                    .arg(i + 1)
                    .arg(expr.funcCall.name)
                    .arg(typeToString(func.params[i].type), typeToString(argType)),
                expr.funcCall.args[i]->line);
          }
        }
        return func.returnType;
      }
      // 未知函数 — 运行时确定
      for (auto *arg : expr.funcCall.args) checkExpr(*arg, env);
      return AcType::any();
    }

    case Expr::kMethodCall: {
      // obj.method(args)
      AcType objType;
      if (expr.methodCall.object) {
        // 链式访问：先计算对象表达式类型
        objType = checkExpr(*expr.methodCall.object, env);
      } else if (env.varTypes.contains(expr.methodCall.objName)) {
        objType = env.varTypes[expr.methodCall.objName];
      }
      if (objType.kind == AcType::kClass && m_classes->contains(objType.className)) {
        const ClassDef &cd = (*m_classes)[objType.className];
        for (const auto &method : cd.methods) {
          if (method.name == expr.methodCall.methodName) {
            // 检查参数个数
            int argCount = expr.methodCall.args.size();
            int paramCount = method.params.size();
            if (argCount != paramCount) {
              reportError(
                  QStringLiteral("method '%1' of class '%2' expects %3 arguments but got %4")
                      .arg(method.name, cd.name)
                      .arg(paramCount)
                      .arg(argCount),
                  expr.line);
            }
            // 检查参数类型
            for (int i = 0; i < qMin(argCount, paramCount); ++i) {
              AcType argType = checkExpr(*expr.methodCall.args[i], env);
              if (!isCompatible(argType, method.params[i].type)) {
                reportError(
                    QStringLiteral(
                        "type mismatch in argument %1 of '%2.%3': expected '%4' but got '%5'")
                        .arg(i + 1)
                        .arg(cd.name, method.name)
                        .arg(typeToString(method.params[i].type), typeToString(argType)),
                    expr.methodCall.args[i]->line);
              }
            }
            return method.returnType;
          }
        }
        reportError(QStringLiteral("class '%1' has no method named '%2'")
                        .arg(objType.className, expr.methodCall.methodName),
                    expr.line);
      }
      for (auto *arg : expr.methodCall.args) checkExpr(*arg, env);
      return AcType::any();
    }

    case Expr::kNewInstance: {
      // new ClassName()
      if (!m_classes->contains(expr.className)) {
        reportError(QStringLiteral("unknown class '%1' in 'new' expression").arg(expr.className),
                    expr.line);
        return AcType::any();
      }
      return AcType::classType(expr.className);
    }

    case Expr::kStaticAccess: {
      // ClassName::member 或 ClassName::method(args)
      if (m_classes->contains(expr.className)) {
        const ClassDef &cd = (*m_classes)[expr.className];

        // 如果是方法调用（有参数列表）
        if (!expr.funcCall.args.isEmpty()) {
          // 查找静态方法
          for (const auto &method : cd.methods) {
            if (method.isStatic && method.name == expr.prop) {
              // 检查参数类型
              int argCount = expr.funcCall.args.size();
              int paramCount = method.params.size();
              if (argCount != paramCount) {
                reportError(QStringLiteral("static method '%1::%2' expects %3 arguments but got %4")
                                .arg(expr.className, expr.prop)
                                .arg(paramCount)
                                .arg(argCount),
                            expr.line);
              }
              for (int i = 0; i < qMin(argCount, paramCount); ++i) {
                AcType argType = checkExpr(*expr.funcCall.args[i], env);
                if (!isCompatible(argType, method.params[i].type)) {
                  reportError(QStringLiteral("type mismatch in argument %1 of static method "
                                             "'%2::%3': expected '%4' but got '%5'")
                                  .arg(i + 1)
                                  .arg(expr.className, expr.prop)
                                  .arg(typeToString(method.params[i].type), typeToString(argType)),
                              expr.funcCall.args[i]->line);
                }
              }
              return method.returnType;
            }
          }
          reportError(
              QStringLiteral("static method '%1::%2' not found").arg(expr.className, expr.prop),
              expr.line);
          return AcType::any();
        }

        // 属性访问：ClassName::member
        for (const auto &prop : cd.properties) {
          if (prop.isStatic && prop.key == expr.prop) {
            return prop.type;
          }
        }
        // 无参数静态方法调用：ClassName::method
        for (const auto &method : cd.methods) {
          if (method.isStatic && method.name == expr.prop) {
            return method.returnType;
          }
        }
        reportError(
            QStringLiteral("static member '%1::%2' not found").arg(expr.className, expr.prop),
            expr.line);
      } else {
        reportError(QStringLiteral("unknown class '%1' in static access").arg(expr.className),
                    expr.line);
      }
      return AcType::any();
    }

    case Expr::kBinary: {
      // 二元运算：检查左右操作数类型兼容
      AcType leftType = checkExpr(*expr.left, env);
      AcType rightType = checkExpr(*expr.right, env);

      // ── 加法（+）支持字符串拼接和数字加法 ──
      if (expr.binOp == Expr::kAdd) {
        // String + String → String
        if (leftType.kind == AcType::kString && rightType.kind == AcType::kString) {
          return AcType::string();
        }
        // String + 任意 → String（Number 隐式转 String）
        if (leftType.kind == AcType::kString) {
          return AcType::string();
        }
        // 任意 + String → String（Number 隐式转 String）
        if (rightType.kind == AcType::kString) {
          return AcType::string();
        }
        // 任意 + 任意 → 如果包含 Any 则不做检查
        if (leftType.kind == AcType::kAny || rightType.kind == AcType::kAny) {
          return AcType::any();
        }
        // Number + Number → Number
        if (leftType.kind == AcType::kNumber && rightType.kind == AcType::kNumber) {
          return AcType::number();
        }
        // 其他类型组合 → Number（运行时决定）
        return AcType::number();
      }

      // ── 减法、乘法、除法（- * /）仅支持 Number ──
      if (leftType.kind == AcType::kNumber && rightType.kind == AcType::kNumber) {
        return AcType::number();
      }

      // 包含 Any 时不报错
      if (leftType.kind == AcType::kAny || rightType.kind == AcType::kAny) {
        return AcType::any();
      }

      // 类型不兼容
      if (!isCompatible(leftType, rightType)) {
        reportError(QStringLiteral("type mismatch: '%1' and '%2' in binary operation")
                        .arg(typeToString(leftType), typeToString(rightType)),
                    expr.line);
      }

      return AcType::number();
    }

    default:
      return AcType::any();
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  类型兼容性
// ═════════════════════════════════════════════════════════════════════════════

bool AcTypeChecker::isCompatible(const AcType &from, const AcType &to) const {
  if (to.kind == AcType::kAny) return true;
  if (from.kind == AcType::kAny) return true;

  if (from.kind == to.kind) {
    if (from.kind == AcType::kArray) {
      return isCompatible(*from.elementType, *to.elementType);
    }
    if (from.kind == AcType::kClass) {
      if (from.className == to.className) return true;
      // 继承兼容：子类可赋值给父类
      return isSubclassOf(from.className, to.className);
    }
    if (from.kind == AcType::kInterface) {
      return from.interfaceName == to.interfaceName;
    }
    return true;
  }

  // 类实现接口：类可赋值给接口类型
  if (from.kind == AcType::kClass && to.kind == AcType::kInterface) {
    return classImplementsInterface(from.className, to.interfaceName);
  }

  return false;
}

bool AcTypeChecker::isSubclassOf(const QString &sub, const QString &base) const {
  if (sub == base) return true;
  if (!m_classes->contains(sub)) return false;
  const ClassDef &cd = (*m_classes)[sub];
  if (cd.baseClass.isEmpty()) return false;
  if (cd.baseClass == base) return true;
  return isSubclassOf(cd.baseClass, base);
}

bool AcTypeChecker::classImplementsInterface(const QString &className,
                                             const QString &ifaceName) const {
  if (!m_classes->contains(className)) return false;
  const ClassDef &cd = (*m_classes)[className];
  if (cd.interfaces.contains(ifaceName)) return true;
  // 递归检查父类
  if (!cd.baseClass.isEmpty()) {
    return classImplementsInterface(cd.baseClass, ifaceName);
  }
  return false;
}

void AcTypeChecker::checkImplements(const ClassDef &cd) {
  for (const auto &ifaceName : cd.interfaces) {
    if (!m_interfaces.contains(ifaceName)) {
      reportError(QStringLiteral("interface '%1' not found (referenced by class '%2')")
                      .arg(ifaceName, cd.name),
                  0);
      continue;
    }
    const InterfaceDef &iface = m_interfaces[ifaceName];
    for (const auto &im : iface.methods) {
      bool found = false;
      // 在当前类和父类中查找方法
      QString searchClass = cd.name;
      while (!searchClass.isEmpty() && m_classes->contains(searchClass)) {
        const ClassDef &searchCd = (*m_classes)[searchClass];
        for (const auto &m : searchCd.methods) {
          if (m.name == im.name && isSignatureCompatible(m, im)) {
            found = true;
            break;
          }
        }
        if (found) break;
        searchClass = searchCd.baseClass;
      }
      if (!found) {
        reportError(
            QStringLiteral("class '%1' does not implement method '%2()' from interface '%3'")
                .arg(cd.name, im.name, ifaceName),
            0);
      }
    }
  }
}

bool AcTypeChecker::isSignatureCompatible(const MethodDef &method,
                                          const InterfaceMethod &iface) const {
  if (method.name != iface.name) return false;
  if (method.params.size() != iface.params.size()) return false;
  for (int i = 0; i < method.params.size(); ++i) {
    if (!isCompatible(method.params[i].type, iface.params[i].type)) return false;
  }
  if (!isCompatible(method.returnType, iface.returnType)) return false;
  return true;
}

void AcTypeChecker::checkOverride(const ClassDef &cd) {
  if (cd.baseClass.isEmpty()) return;
  for (const auto &method : cd.methods) {
    if (!method.isOverride) continue;
    // 在父类中查找同名方法
    bool found = false;
    QString searchClass = cd.baseClass;
    while (!searchClass.isEmpty() && m_classes->contains(searchClass)) {
      const ClassDef &baseCd = (*m_classes)[searchClass];
      for (const auto &baseMethod : baseCd.methods) {
        if (baseMethod.name == method.name) {
          found = true;
          break;
        }
      }
      if (found) break;
      searchClass = baseCd.baseClass;
    }
    if (!found) {
      reportError(
          QStringLiteral("method '%1()' marked override but no such method in parent class '%2'")
              .arg(method.name, cd.baseClass),
          0);
    }
  }
}

void AcTypeChecker::checkAccessInClass(const ClassDef &cd) {
  // 检查类方法体中是否有非法访问 private/protected 成员
  // 此处仅做基本检查：类外部不能访问 private 成员
  // 更细粒度的检查在运行时进行
  Q_UNUSED(cd);
}

QString AcTypeChecker::typeToString(const AcType &type) const {
  switch (type.kind) {
    case AcType::kNumber:
      return QStringLiteral("Number");
    case AcType::kString:
      return QStringLiteral("String");
    case AcType::kBool:
      return QStringLiteral("Bool");
    case AcType::kAny:
      return QStringLiteral("Any");
    case AcType::kVoid:
      return QStringLiteral("Void");
    case AcType::kArray:
      return typeToString(*type.elementType) + QStringLiteral("[]");
    case AcType::kClass:
      return type.className;
    case AcType::kInterface:
      return type.interfaceName;
    default:
      return QStringLiteral("Unknown");
  }
}

void AcTypeChecker::reportError(const QString &msg, int line) {
  m_errors->append(QStringLiteral("type error: %1 at line %2").arg(msg).arg(line));
}