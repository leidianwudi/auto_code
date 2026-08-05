/**
 * @file undeclared_ident_validator.cpp
 * @brief 未声明标识符检查器实现
 */

#include "undeclared_ident_validator.h"

#include <QFileInfo>

#include "../ac_language.h"

void UndeclaredIdentValidator::validate(const Block &program, const QSet<QString> &declaredVars,
                                        QStringList &errors) {
  m_errors = &errors;
  m_scopeVars = declaredVars;
  m_ctx = ValidationContext();

  // 预扫描收集类定义和 new 赋值信息
  collectValidationInfo(program);

  // 遍历 AST 检查（利用基类 AstVisitor 虚分派）
  visitBlock(program);
}

/// 每个语句在递归前记录其所属文件，供错误消息定位到真实来源
/// （import 导入的模块语句携带各自的 filePath，与入口脚本文件不同）
void UndeclaredIdentValidator::visitStmt(const Block::Stmt &stmt) {
  if (!stmt.filePath.isEmpty()) m_currentFile = stmt.filePath;
  AstVisitor::visitStmt(stmt);
}

void UndeclaredIdentValidator::collectValidationInfo(const Block &block) {
  for (const auto &stmt : block.stmts) {
    switch (stmt.kind) {
      case Block::Stmt::kClassDef:
        if (!stmt.classDef.name.isEmpty()) {
          QStringList methods;
          for (const auto &m : stmt.classDef.methods) methods.append(m.name);
          m_ctx.classMethods.insert(stmt.classDef.name, methods);
          m_scopeVars.insert(stmt.classDef.name);
        }
        break;
      case Block::Stmt::kEnumDef:
        if (!stmt.enumDef.name.isEmpty()) {
          m_scopeVars.insert(stmt.enumDef.name);
        }
        break;
      case Block::Stmt::kFuncDef:
        if (!stmt.funcDef.name.isEmpty()) {
          m_scopeVars.insert(stmt.funcDef.name);
        }
        break;
      case Block::Stmt::kAssign:
        if (!stmt.assign.name.isEmpty() && stmt.assign.value.kind == Expr::kNewInstance &&
            !stmt.assign.value.className.isEmpty()) {
          m_ctx.varClass.insert(stmt.assign.name, stmt.assign.value.className);
        }
        break;
      default:
        break;
    }
  }
}

void UndeclaredIdentValidator::reportError(const QString &msg, int line) {
  if (m_errors) {
    // 优先使用当前语句所属文件（import 导入的真实文件），否则回退到入口文件
    const QString &file = m_currentFile.isEmpty() ? m_filePath : m_currentFile;
    QString fileTag = file.isEmpty() ? QString() : QFileInfo(file).fileName();
    QString location =
        line > 0 ? QStringLiteral("at line %1").arg(line) : QStringLiteral("(line unknown)");
    if (!fileTag.isEmpty()) {
      m_errors->append(
          QStringLiteral("undefined variable '%1' %2 in %3").arg(msg, location, fileTag));
    } else {
      m_errors->append(QStringLiteral("undefined variable '%1' %2").arg(msg, location));
    }
  }
}

// ═════════════════════════════════════════════════════════════════════════════
// 语句 — 重写 AstVisitor
// ═════════════════════════════════════════════════════════════════════════════

void UndeclaredIdentValidator::visitAssignStmt(const AssignStmt &as) {
  AstVisitor::visitAssignStmt(as);

  // 记录 let x = new Car() → 变量→类名
  if (!as.name.isEmpty() && as.value.kind == Expr::kNewInstance && !as.value.className.isEmpty()) {
    m_ctx.varClass.insert(as.name, as.value.className);
  }
  // 变量声明（let x = ...）将 x 加入作用域
  m_scopeVars.insert(as.name);
}

void UndeclaredIdentValidator::visitUsingStmt(const UsingStmt &us) {
  AstVisitor::visitUsingStmt(us);
  if (!us.varName.isEmpty() && us.value && us.value->kind == Expr::kNewInstance &&
      !us.value->className.isEmpty()) {
    m_ctx.varClass.insert(us.varName, us.value->className);
  }
  m_scopeVars.insert(us.varName);
}

void UndeclaredIdentValidator::visitForStmt(const ForStmt &fs) {
  // 先访问数组表达式（当前作用域）
  visitExpr(fs.arrayExpr);
  // for 循环变量在 body 中可见（不调基类 AstVisitor::visitForStmt，避免 body 被遍历两次）
  QSet<QString> bodyScope = m_scopeVars;
  bodyScope.insert(fs.varName);
  QSet<QString> savedScope = m_scopeVars;
  m_scopeVars = bodyScope;
  visitBlock(fs.body);
  m_scopeVars = savedScope;
}

void UndeclaredIdentValidator::visitClassDef(const ClassDef &cd) {
  QSet<QString> classScope = m_scopeVars;
  for (const auto &prop : cd.properties) {
    if (prop.value) visitExpr(*prop.value);
  }
  classScope.insert(QString::fromLatin1(AcKeyword::kThis));
  // 注意：类属性必须通过 this.xxx 访问，不加入裸作用域。
  // 这样误写属性名（如把 this.tableName 写成 tableName）会被正确标记为未定义变量。

  // 继承：子类可访问父类属性
  if (!cd.baseClass.isEmpty()) {
    classScope.insert(QStringLiteral("super"));
  }

  for (const auto &method : cd.methods) {
    QSet<QString> methodScope = classScope;
    for (const auto &param : method.params) methodScope.insert(param.name);
    QSet<QString> savedScope = m_scopeVars;
    m_scopeVars = methodScope;
    visitBlock(method.body);
    m_scopeVars = savedScope;
  }
}

void UndeclaredIdentValidator::visitFuncDef(const MethodDef &md) {
  QSet<QString> funcScope = m_scopeVars;
  funcScope.insert(md.name);  // 函数体内允许递归调用
  for (const auto &param : md.params) funcScope.insert(param.name);
  QSet<QString> savedScope = m_scopeVars;
  m_scopeVars = funcScope;
  visitBlock(md.body);
  m_scopeVars = savedScope;
  m_scopeVars.insert(md.name);  // 定义后后续语句可调用
}

void UndeclaredIdentValidator::visitImportStmt(const ImportStmt &imp) {
  // import 语句中的符号名由模块链接器注册，此处使用别名或原始名
  for (const auto &name : imp.names) {
    QString localName = imp.aliases.contains(name) ? imp.aliases[name] : name;
    m_scopeVars.insert(localName);
  }
}

// ═════════════════════════════════════════════════════════════════════════════
// 表达式 — 重写 AstVisitor
// ═════════════════════════════════════════════════════════════════════════════

void UndeclaredIdentValidator::visitIdentExpr(const Expr &expr) {
  if (expr.ident.isEmpty()) return;
  if (expr.ident == QStringLiteral("JSON")) return;
  if (!m_scopeVars.contains(expr.ident)) {
    reportError(expr.ident, expr.line);
  }
}

void UndeclaredIdentValidator::visitPropAccessExpr(const Expr &expr) {
  if (expr.propObject) {
    visitExpr(*expr.propObject);
  } else if (!expr.ident.isEmpty() && expr.ident != QString::fromLatin1(AcKeyword::kThis) &&
             expr.ident != QStringLiteral("JSON") && !m_scopeVars.contains(expr.ident)) {
    reportError(expr.ident, expr.line);
  }
}

void UndeclaredIdentValidator::visitFuncCallExpr(const Expr &expr) {
  AstVisitor::visitFuncCallExpr(expr);

  // 检查函数名是否已知（内置函数或用户自定义函数）
  if (!AcBuiltin::kAll.contains(expr.funcCall.name) && !m_scopeVars.contains(expr.funcCall.name)) {
    if (m_errors) {
      // 优先使用当前语句所属文件（import 导入的真实文件），否则回退到入口文件
      const QString &file = m_currentFile.isEmpty() ? m_filePath : m_currentFile;
      QString fileTag = file.isEmpty() ? QString() : QFileInfo(file).fileName();
      QString loc = QStringLiteral("at line %1").arg(expr.line);
      if (!fileTag.isEmpty()) {
        m_errors->append(
            QStringLiteral("unknown function '%1' %2 in %3").arg(expr.funcCall.name, loc, fileTag));
      } else {
        m_errors->append(QStringLiteral("unknown function '%1' %2").arg(expr.funcCall.name, loc));
      }
    }
  }

  if (expr.funcCall.name == QString::fromLatin1(AcBuiltin::kReadFile) &&
      expr.funcCall.args.empty()) {
    if (m_errors) {
      m_errors->append(
          QStringLiteral("readFile() requires a file path argument at line %1").arg(expr.line));
    }
  }
}

void UndeclaredIdentValidator::visitMethodCallExpr(const Expr &expr) {
  if (expr.methodCall.object) {
    visitExpr(*expr.methodCall.object);
  } else {
    // 检查对象变量是否已声明（跳过空 objName，链式方法调用时 objName 可能为空）
    if (expr.methodCall.objName != QString::fromLatin1(AcKeyword::kThis) &&
        expr.methodCall.objName != QString::fromLatin1(AcKeyword::kSuper) &&
        expr.methodCall.objName != QStringLiteral("JSON") &&
        !m_scopeVars.contains(expr.methodCall.objName)) {
      reportError(expr.methodCall.objName, expr.line);
    }

    // 检查方法名是否在对应的类中定义
    if (m_ctx.varClass.contains(expr.methodCall.objName)) {
      QString clsName = m_ctx.varClass.value(expr.methodCall.objName);
      if (m_ctx.classMethods.contains(clsName) &&
          !m_ctx.classMethods[clsName].contains(expr.methodCall.methodName)) {
        if (m_errors) {
          m_errors->append(
              QStringLiteral("class '%1' has no method '%2' at line %3")
                  .arg(clsName, expr.methodCall.methodName, QString::number(expr.line)));
        }
      }
    }
  }

  // 检查参数表达式
  for (const auto &arg : expr.methodCall.args) {
    if (arg) visitExpr(*arg);
  }
}

void UndeclaredIdentValidator::visitFuncExprExpr(const Expr &expr) {
  // 函数表达式：将参数加入作用域，遍历函数体
  QSet<QString> savedScopeVars = m_scopeVars;
  for (const auto &param : expr.funcExpr.params) {
    m_scopeVars.insert(param.name);
  }
  visitBlock(expr.funcExpr.body);
  m_scopeVars = savedScopeVars;
}

// ═════════════════════════════════════════════════════════════════════════════
// 以下委托方法仅用于保持 vtable 布局与旧编译单元一致，避免链接错误
// ═════════════════════════════════════════════════════════════════════════════

void UndeclaredIdentValidator::visitInterfaceDef(const InterfaceDef &iface) {
  AstVisitor::visitInterfaceDef(iface);
}

void UndeclaredIdentValidator::visitReturnStmt(const Expr &retExpr) {
  AstVisitor::visitReturnStmt(retExpr);
}

void UndeclaredIdentValidator::visitExprStmt(const Expr &expr) { AstVisitor::visitExprStmt(expr); }

void UndeclaredIdentValidator::visitIndexAccessExpr(const Expr &expr) {
  AstVisitor::visitIndexAccessExpr(expr);
}

void UndeclaredIdentValidator::visitStaticAccessExpr(const Expr &expr) {
  AstVisitor::visitStaticAccessExpr(expr);
}

void UndeclaredIdentValidator::visitObjectExpr(const Expr &expr) {
  AstVisitor::visitObjectExpr(expr);
}