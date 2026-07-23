/**
 * @file ac_symbol_table.cpp
 * @brief AC 脚本符号表实现
 */

#include "ac_symbol_table.h"

#include <QDebug>

// ──────────────────────────────────────────────────────────────
//  buildFromAst / buildFromBlock / collectStmt
// ──────────────────────────────────────────────────────────────

void AcSymbolTable::buildFromAst(const QString &filePath, const Block::Stmt &stmt) {
  m_filePath = filePath;
  collectFromStmt(stmt);
}

void AcSymbolTable::buildFromBlock(const QString &filePath, const Block &block) {
  m_filePath = filePath;
  for (const auto &stmt : block.stmts) {
    collectFromStmt(stmt);
  }
}

void AcSymbolTable::collectStmt(const Block::Stmt &stmt) { collectFromStmt(stmt); }

// ──────────────────────────────────────────────────────────────
//  collectFromStmt — 递归遍历 AST 收集符号
// ──────────────────────────────────────────────────────────────

void AcSymbolTable::collectFromStmt(const Block::Stmt &stmt) {
  switch (stmt.kind) {
    case Block::Stmt::kFuncDef: {
      const auto &fd = stmt.funcDef;
      AcSymbolEntry entry;
      entry.name = fd.name;
      entry.kind = AcSymbolKind::kFunction;
      entry.filePath = m_filePath;
      entry.line = stmt.line;
      entry.returnType = acTypeToString(fd.returnType);
      entry.params = fd.params;
      entry.signature = makeFunctionSignature(fd.name, fd.params, fd.returnType);
      entry.isExported = fd.isExported;
      m_symbols.insert(fd.name, entry);

      // 收集函数参数到符号表（支持参数跳转和悬停）
      for (const auto &param : fd.params) {
        if (!param.name.isEmpty()) {
          AcSymbolEntry paramEntry;
          paramEntry.name = param.name;
          paramEntry.kind = AcSymbolKind::kParameter;
          paramEntry.filePath = m_filePath;
          paramEntry.line = stmt.line;
          paramEntry.parentClass = fd.name;
          paramEntry.returnType = acTypeToString(param.type);
          paramEntry.signature = param.name + QStringLiteral(": ") + acTypeToString(param.type);
          m_symbols.insert(param.name, paramEntry);
        }
      }

      // 递归收集函数体内的符号
      buildFromBlock(m_filePath, fd.body);
      break;
    }

    case Block::Stmt::kInterfaceDef: {
      const auto &iface = stmt.interfaceDef;
      AcSymbolEntry entry;
      entry.name = iface.name;
      entry.kind = AcSymbolKind::kClass;
      entry.filePath = m_filePath;
      entry.line = stmt.line;
      entry.signature = QStringLiteral("interface ") + iface.name;
      if (!iface.baseInterfaces.isEmpty()) {
        entry.signature +=
            QStringLiteral(" extends ") + iface.baseInterfaces.join(QStringLiteral(", "));
      }
      m_symbols.insert(iface.name, entry);

      // 收集接口方法
      for (const auto &method : iface.methods) {
        AcSymbolEntry methodEntry;
        methodEntry.name = method.name;
        methodEntry.kind = AcSymbolKind::kMethod;
        methodEntry.filePath = m_filePath;
        methodEntry.line = stmt.line;
        methodEntry.parentClass = iface.name;
        methodEntry.returnType = acTypeToString(method.returnType);
        methodEntry.params = method.params;
        methodEntry.signature =
            makeMethodSignature(iface.name, method.name, method.params, method.returnType, false);
        m_symbols.insert(iface.name + QStringLiteral(".") + method.name, methodEntry);
      }
      break;
    }

    case Block::Stmt::kClassDef: {
      const auto &cd = stmt.classDef;
      // 设置当前类名上下文（用于 this 关键字类型解析）
      QString savedClassName = m_currentClassName;
      m_currentClassName = cd.name;

      AcSymbolEntry entry;
      entry.name = cd.name;
      entry.kind = AcSymbolKind::kClass;
      entry.filePath = m_filePath;
      entry.line = stmt.line;
      entry.comment =
          cd.baseClass.isEmpty() ? QString() : QStringLiteral("extends %1").arg(cd.baseClass);
      entry.signature =
          cd.name +
          (cd.baseClass.isEmpty() ? QString() : QStringLiteral(" extends %1").arg(cd.baseClass));
      m_symbols.insert(cd.name, entry);

      // 收集类属性
      for (const auto &prop : cd.properties) {
        AcSymbolEntry propEntry;
        propEntry.name = prop.key;
        propEntry.kind = AcSymbolKind::kProperty;
        propEntry.filePath = m_filePath;
        propEntry.line = prop.line > 0 ? prop.line : stmt.line;
        propEntry.parentClass = cd.name;
        propEntry.returnType = acTypeToString(prop.type);
        propEntry.signature = prop.key + QStringLiteral(": ") + acTypeToString(prop.type);
        m_symbols.insert(cd.name + QStringLiteral(".") + prop.key, propEntry);
      }

      // 收集类方法
      for (const auto &method : cd.methods) {
        AcSymbolEntry methodEntry;
        methodEntry.name = method.name;
        methodEntry.kind = AcSymbolKind::kMethod;
        methodEntry.filePath = m_filePath;
        methodEntry.line = method.line > 0 ? method.line : stmt.line;
        methodEntry.parentClass = cd.name;
        methodEntry.returnType = acTypeToString(method.returnType);
        methodEntry.params = method.params;
        methodEntry.isStatic = method.isStatic;
        methodEntry.access = method.access;
        methodEntry.signature = makeMethodSignature(cd.name, method.name, method.params,
                                                    method.returnType, method.isStatic);
        m_symbols.insert(cd.name + QStringLiteral(".") + method.name, methodEntry);

        // 收集方法参数
        for (const auto &param : method.params) {
          if (!param.name.isEmpty()) {
            AcSymbolEntry paramEntry;
            paramEntry.name = param.name;
            paramEntry.kind = AcSymbolKind::kParameter;
            paramEntry.filePath = m_filePath;
            paramEntry.line = stmt.line;
            paramEntry.parentClass = cd.name + QStringLiteral(".") + method.name;
            paramEntry.returnType = acTypeToString(param.type);
            paramEntry.signature = param.name + QStringLiteral(": ") + acTypeToString(param.type);
            m_symbols.insert(param.name, paramEntry);
          }
        }

        // 递归收集方法体内的符号
        buildFromBlock(m_filePath, method.body);
      }
      // 恢复类名上下文
      m_currentClassName = savedClassName;
      break;
    }

    case Block::Stmt::kAssign: {
      const auto &as = stmt.assign;
      if (!as.name.isEmpty()) {
        AcSymbolEntry entry;
        entry.name = as.name;
        entry.kind = AcSymbolKind::kVariable;
        entry.filePath = m_filePath;
        entry.line = as.line > 0 ? as.line : stmt.line;
        if (as.hasTypeAnnotation) {
          // 显式类型注解：let x: Type = ...
          entry.returnType = acTypeToString(as.typeAnnotation);
          entry.signature = as.name + QStringLiteral(": ") + acTypeToString(as.typeAnnotation);
        } else {
          // 类型推断：从初始化表达式推断变量类型
          QString inferredType = inferTypeFromExpr(as.value);
          if (!inferredType.isEmpty()) {
            entry.returnType = inferredType;
            entry.signature = as.name + QStringLiteral(": ") + inferredType;
          } else {
            entry.signature = as.name;
          }
        }
        m_symbols.insert(as.name, entry);

        // 对象字面量：收集属性到符号表（如 let x = {a: 1} → x.a: Number）
        if (as.value.kind == Expr::kObject) {
          collectObjectProperties(as.name, as.value);
        }
        // 数组字面量：如果首元素是对象，收集属性（如 let arr = [{a: 1}] → arr.a: Number）
        if (as.value.kind == Expr::kArray && !as.value.arrItems.empty() && as.value.arrItems[0] &&
            as.value.arrItems[0]->kind == Expr::kObject) {
          collectObjectProperties(as.name, *as.value.arrItems[0]);
        }
      }
      break;
    }

    case Block::Stmt::kFor: {
      // for (var in arrayExpr) { body }
      const auto &fs = stmt.forStmt;
      if (!fs.varName.isEmpty()) {
        AcSymbolEntry entry;
        entry.name = fs.varName;
        entry.kind = AcSymbolKind::kVariable;
        entry.filePath = m_filePath;
        entry.line = fs.line > 0 ? fs.line : stmt.line;

        // 类型推断：优先使用显式注解，其次从可迭代表达式推断元素类型
        QString elemType;
        if (!fs.varType.isEmpty()) {
          // 显式注解：for (let item: Type in arr)
          elemType = fs.varType;
        } else {
          // 从 arrayExpr 推断：如果类型是 ElementType[]，提取 ElementType
          QString arrType = inferTypeFromExpr(fs.arrayExpr);
          if (arrType.endsWith(QStringLiteral("[]"))) {
            elemType = arrType.left(arrType.length() - 2);  // 去掉 "[]"
          }
        }

        if (!elemType.isEmpty()) {
          entry.returnType = elemType;
          entry.signature = fs.varName + QStringLiteral(": ") + elemType;
        } else {
          entry.signature = fs.varName + QStringLiteral(": iterator");
        }
        m_symbols.insert(fs.varName, entry);
      }
      buildFromBlock(m_filePath, fs.body);
      break;
    }

    case Block::Stmt::kIf: {
      buildFromBlock(m_filePath, stmt.ifStmt.thenBlock);
      if (stmt.ifStmt.hasElse && stmt.ifStmt.elseBlock.stmts.size() > 0) {
        buildFromBlock(m_filePath, stmt.ifStmt.elseBlock);
      }
      // else if 链
      if (stmt.ifStmt.elseIfBranch) {
        collectFromIfBranch(stmt.ifStmt.elseIfBranch.get());
      }
      break;
    }

    case Block::Stmt::kWhile: {
      buildFromBlock(m_filePath, stmt.whileStmt.body);
      break;
    }

    case Block::Stmt::kUsing: {
      const auto &us = stmt.usingStmt;
      if (!us.varName.isEmpty()) {
        AcSymbolEntry entry;
        entry.name = us.varName;
        entry.kind = AcSymbolKind::kVariable;
        entry.filePath = m_filePath;
        entry.line = stmt.line;
        entry.signature = us.varName + QStringLiteral(": using");
        m_symbols.insert(us.varName, entry);
      }
      break;
    }

    default:
      break;
  }
}

void AcSymbolTable::collectFromIfBranch(const IfStmt *ifStmt) {
  if (!ifStmt) return;
  buildFromBlock(m_filePath, ifStmt->thenBlock);
  if (ifStmt->hasElse && ifStmt->elseBlock.stmts.size() > 0) {
    buildFromBlock(m_filePath, ifStmt->elseBlock);
  }
  if (ifStmt->elseIfBranch) {
    collectFromIfBranch(ifStmt->elseIfBranch.get());
  }
}

// ──────────────────────────────────────────────────────────────
//  AcType 转字符串
// ──────────────────────────────────────────────────────────────

QString AcSymbolTable::acTypeToString(const AcType &type) const {
  switch (type.kind) {
    case AcType::kNumber:
      return QStringLiteral("Number");
    case AcType::kString:
      return QStringLiteral("String");
    case AcType::kBool:
      return QStringLiteral("Boolean");
    case AcType::kAny:
      return QStringLiteral("Any");
    case AcType::kVoid:
      return QStringLiteral("void");
    case AcType::kNull:
      return QStringLiteral("null");
    case AcType::kArray:
      if (type.elementType) return acTypeToString(*type.elementType) + QStringLiteral("[]");
      return QStringLiteral("Array");
    case AcType::kClass:
      return type.className;
    case AcType::kInterface:
      return type.interfaceName;
    default:
      return QStringLiteral("Any");
  }
}

// ──────────────────────────────────────────────────────────────
//  查找符号
// ──────────────────────────────────────────────────────────────

AcSymbolEntry *AcSymbolTable::findSymbol(const QString &name) {
  auto it = m_symbols.find(name);
  if (it != m_symbols.end()) return &it.value();
  return nullptr;
}

const AcSymbolEntry *AcSymbolTable::findSymbol(const QString &name) const {
  auto it = m_symbols.find(name);
  if (it != m_symbols.end()) return &it.value();
  return nullptr;
}

QVector<AcSymbolEntry> AcSymbolTable::findSymbolsByPrefix(const QString &prefix) const {
  QVector<AcSymbolEntry> result;
  for (auto it = m_symbols.begin(); it != m_symbols.end(); ++it) {
    if (it.key().startsWith(prefix)) {
      result.append(it.value());
    }
  }
  return result;
}

void AcSymbolTable::clear() {
  m_symbols.clear();
  m_filePath.clear();
}

void AcSymbolTable::mergeFrom(const AcSymbolTable &other, const QStringList &names) {
  const auto &otherSymbols = other.allSymbols();
  if (names.isEmpty()) {
    // 空列表：合并全部符号（用于 builtin.d.ac）
    for (auto it = otherSymbols.begin(); it != otherSymbols.end(); ++it) {
      m_symbols.insert(it.key(), it.value());
    }
    return;
  }
  for (const auto &name : names) {
    // 精确匹配（类名、函数名、变量名）
    auto it = otherSymbols.find(name);
    if (it != otherSymbols.end()) {
      m_symbols.insert(name, it.value());
    }
    // 前缀匹配：ClassName.method / ClassName.prop
    QString prefix = name + QStringLiteral(".");
    for (auto sit = otherSymbols.begin(); sit != otherSymbols.end(); ++sit) {
      if (sit.key().startsWith(prefix)) {
        m_symbols.insert(sit.key(), sit.value());
      }
    }
  }
}

// ──────────────────────────────────────────────────────────────
//  签名生成
// ──────────────────────────────────────────────────────────────

QString AcSymbolTable::makeFunctionSignature(const QString &name, const QVector<ParamDef> &params,
                                             const AcType &returnType) const {
  QString sig = QStringLiteral("function ");
  sig += name;
  sig += QLatin1Char('(');
  for (int i = 0; i < params.size(); ++i) {
    if (i > 0) sig += QStringLiteral(", ");
    sig += params[i].name;
    if (params[i].type.kind != AcType::kAny) {
      sig += QStringLiteral(": ") + acTypeToString(params[i].type);
    }
  }
  sig += QLatin1Char(')');
  if (returnType.kind != AcType::kAny && returnType.kind != AcType::kVoid) {
    sig += QStringLiteral(": ") + acTypeToString(returnType);
  }
  return sig;
}

QString AcSymbolTable::makeMethodSignature(const QString &className, const QString &name,
                                           const QVector<ParamDef> &params,
                                           const AcType &returnType, bool isStatic) const {
  QString sig;
  if (isStatic) sig += QStringLiteral("static ");
  sig += name;
  sig += QLatin1Char('(');
  for (int i = 0; i < params.size(); ++i) {
    if (i > 0) sig += QStringLiteral(", ");
    sig += params[i].name;
    if (params[i].type.kind != AcType::kAny) {
      sig += QStringLiteral(": ") + acTypeToString(params[i].type);
    }
  }
  sig += QLatin1Char(')');
  if (returnType.kind != AcType::kAny && returnType.kind != AcType::kVoid) {
    sig += QStringLiteral(": ") + acTypeToString(returnType);
  }
  return sig;
}

// ──────────────────────────────────────────────────────────────
//  类型推断引擎 — 从表达式 AST 推断变量类型
// ──────────────────────────────────────────────────────────────

QString AcSymbolTable::inferTypeFromExpr(const Expr &expr) const {
  switch (expr.kind) {
    // 字面量类型
    case Expr::kString:
      return QStringLiteral("String");
    case Expr::kNumber:
      return QStringLiteral("Number");
    case Expr::kBool:
      return QStringLiteral("Boolean");
    case Expr::kArray:
      return QStringLiteral("Array");
    case Expr::kObject:
      return QStringLiteral("Object");
    case Expr::kNull:
    case Expr::kUndefined:
      return QString();

    // 变量引用：查找符号表中的 returnType
    case Expr::kIdent: {
      const AcSymbolEntry *entry = findSymbol(expr.ident);
      if (entry && !entry->returnType.isEmpty() && entry->returnType != QStringLiteral("Any")) {
        return entry->returnType;
      }
      return QString();
    }

    // this 关键字 → 当前类名
    case Expr::kThis:
      return m_currentClassName;

    // new ClassName() → 类型为 ClassName
    case Expr::kNewInstance:
      return expr.className;

    // 函数调用：查找函数的返回类型
    case Expr::kFuncCall: {
      const AcSymbolEntry *entry = findSymbol(expr.funcCall.name);
      if (entry && !entry->returnType.isEmpty() && entry->returnType != QStringLiteral("Any") &&
          entry->returnType != QStringLiteral("void")) {
        return entry->returnType;
      }
      return QString();
    }

    // 属性访问 obj.prop → 查找对象类型，再查找 ClassName.prop 的 returnType
    case Expr::kPropAccess: {
      QString objType;
      // 链式访问：propObject 优先
      if (expr.propObject) {
        objType = inferTypeFromExpr(*expr.propObject);
      } else if (!expr.ident.isEmpty()) {
        const AcSymbolEntry *objEntry = findSymbol(expr.ident);
        if (objEntry) objType = objEntry->returnType;
      }
      if (objType.isEmpty()) return QString();

      // 查找 ClassName.prop
      QString qualifiedKey = objType + QStringLiteral(".") + expr.prop;
      const AcSymbolEntry *propEntry = findSymbol(qualifiedKey);
      if (propEntry && !propEntry->returnType.isEmpty() &&
          propEntry->returnType != QStringLiteral("Any")) {
        return propEntry->returnType;
      }
      return QString();
    }

    // 方法调用 obj.method() → 查找对象类型，再查找 ClassName.method 的 returnType
    case Expr::kMethodCall: {
      QString objType;
      // 链式访问：object 表达式优先
      if (expr.methodCall.object) {
        objType = inferTypeFromExpr(*expr.methodCall.object);
      } else if (!expr.methodCall.objName.isEmpty()) {
        const AcSymbolEntry *objEntry = findSymbol(expr.methodCall.objName);
        if (objEntry) objType = objEntry->returnType;
      }
      if (objType.isEmpty()) return QString();

      // 查找 ClassName.method
      QString qualifiedKey = objType + QStringLiteral(".") + expr.methodCall.methodName;
      const AcSymbolEntry *methodEntry = findSymbol(qualifiedKey);
      if (methodEntry && !methodEntry->returnType.isEmpty() &&
          methodEntry->returnType != QStringLiteral("Any") &&
          methodEntry->returnType != QStringLiteral("void")) {
        return methodEntry->returnType;
      }
      return QString();
    }

    // 二元运算：字符串拼接 → String，否则 Number
    case Expr::kBinary: {
      if (expr.binOp == Expr::kAdd) {
        QString lType = expr.left ? inferTypeFromExpr(*expr.left) : QString();
        QString rType = expr.right ? inferTypeFromExpr(*expr.right) : QString();
        if (lType == QStringLiteral("String") || rType == QStringLiteral("String")) {
          return QStringLiteral("String");
        }
        if (lType == QStringLiteral("Number") && rType == QStringLiteral("Number")) {
          return QStringLiteral("Number");
        }
      }
      // 比较运算 → Boolean
      if (expr.binOp >= Expr::kEq && expr.binOp <= Expr::kGte) {
        return QStringLiteral("Boolean");
      }
      // 算术运算 → Number
      if (expr.binOp >= Expr::kAdd && expr.binOp <= Expr::kMod) {
        return QStringLiteral("Number");
      }
      // 逻辑运算 → Boolean
      if (expr.binOp == Expr::kOr || expr.binOp == Expr::kAnd) {
        return QStringLiteral("Boolean");
      }
      return QString();
    }

    // 三元运算：取 trueExpr 的类型
    case Expr::kTernary: {
      if (expr.right) return inferTypeFromExpr(*expr.right);
      return QString();
    }

    // 索引访问 obj[index] → 提取元素类型
    case Expr::kIndexAccess: {
      QString objType = expr.left ? inferTypeFromExpr(*expr.left) : QString();
      if (objType.isEmpty()) return QString();
      // ElementType[] → ElementType
      if (objType.endsWith(QStringLiteral("[]"))) {
        return objType.left(objType.length() - 2);
      }
      // String[i] → String（单字符仍是 String）
      if (objType == QStringLiteral("String")) {
        return QStringLiteral("String");
      }
      return QString();
    }

    // 一元运算 !expr → Boolean
    case Expr::kUnary:
      return QStringLiteral("Boolean");

    // 静态访问 ClassName::member
    case Expr::kStaticAccess: {
      // ident 是类名，prop 是成员名
      QString qualifiedKey = expr.ident + QStringLiteral(".") + expr.prop;
      const AcSymbolEntry *entry = findSymbol(qualifiedKey);
      if (entry && !entry->returnType.isEmpty() && entry->returnType != QStringLiteral("Any")) {
        return entry->returnType;
      }
      return QString();
    }

    default:
      return QString();
  }
}

// ──────────────────────────────────────────────────────────────
//  对象字面量属性收集 — 为 {a: 1, b: "s"} 创建 varName.a / varName.b 符号
// ──────────────────────────────────────────────────────────────

void AcSymbolTable::collectObjectProperties(const QString &varName, const Expr &expr) {
  if (expr.kind != Expr::kObject) return;

  for (const auto &entry : expr.objEntries) {
    if (entry.key.isEmpty()) continue;

    // 推断属性值类型
    QString propType;
    if (entry.value) {
      propType = inferTypeFromExpr(*entry.value);
    }

    AcSymbolEntry propEntry;
    propEntry.name = entry.key;
    propEntry.kind = AcSymbolKind::kProperty;
    propEntry.filePath = m_filePath;
    propEntry.line = entry.line;
    propEntry.returnType = propType;
    if (!propType.isEmpty()) {
      propEntry.signature = entry.key + QStringLiteral(": ") + propType;
    } else {
      propEntry.signature = entry.key;
    }
    m_symbols.insert(varName + QStringLiteral(".") + entry.key, propEntry);
  }
}