/**
 * @file undeclared_ident_validator.cpp
 * @brief 未声明标识符检查器实现
 */

#include "undeclared_ident_validator.h"

#include "../ac_language.h"

void UndeclaredIdentValidator::validate(const Block &program, const QSet<QString> &declaredVars,
                                        QStringList &errors) {
  m_errors = &errors;
  m_scopeVars = declaredVars;
  m_ctx = ValidationContext();

  // 预扫描收集类定义和 new 赋值信息
  collectValidationInfo(program);

  // 遍历 AST 检查
  visitBlock(program);
}

void UndeclaredIdentValidator::collectValidationInfo(const Block &block) {
  for (const auto &stmt : block.stmts) {
    switch (stmt.kind) {
      case Block::Stmt::kClassDef:
        if (!stmt.classDef.name.isEmpty()) {
          QStringList methods;
          for (const auto &m : stmt.classDef.methods) methods.append(m.name);
          m_ctx.classMethods.insert(stmt.classDef.name, methods);
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
    m_errors->append(
        QStringLiteral("undefined variable '%1' at line %2").arg(msg, QString::number(line)));
  }
}

// ═════════════════════════════════════════════════════════════════════════════
// 语句 — 重写 AstVisitor
// ═════════════════════════════════════════════════════════════════════════════

void UndeclaredIdentValidator::visitAssignStmt(const AssignStmt &as) {
  // 先遍历值表达式
  AstVisitor::visitAssignStmt(as);

  // 记录 let x = new Car() → 变量→类名
  if (!as.name.isEmpty() && as.value.kind == Expr::kNewInstance && !as.value.className.isEmpty()) {
    m_ctx.varClass.insert(as.name, as.value.className);
  }
  // 变量声明（let x = ...）将 x 加入作用域
  m_scopeVars.insert(as.name);
}

void UndeclaredIdentValidator::visitForStmt(const ForStmt &fs) {
  visitExpr(fs.arrayExpr);
  QSet<QString> bodyScope = m_scopeVars;
  bodyScope.insert(fs.varName);
  // 在 body 子作用域中遍历
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
  for (const auto &prop : cd.properties) classScope.insert(prop.key);

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

void UndeclaredIdentValidator::visitReturnStmt(const Expr &retExpr) { visitExpr(retExpr); }

void UndeclaredIdentValidator::visitExprStmt(const Expr &expr) { visitExpr(expr); }

// ═════════════════════════════════════════════════════════════════════════════
// 表达式 — 重写 AstVisitor
// ═════════════════════════════════════════════════════════════════════════════

void UndeclaredIdentValidator::visitIdentExpr(const Expr &expr) {
  if (!m_scopeVars.contains(expr.ident)) {
    if (m_errors) {
      m_errors->append(QStringLiteral("undefined variable '%1' at line %2")
                           .arg(expr.ident, QString::number(expr.line)));
    }
  }
}

void UndeclaredIdentValidator::visitPropAccessExpr(const Expr &expr) {
  if (expr.ident != QString::fromLatin1(AcKeyword::kThis) && !m_scopeVars.contains(expr.ident)) {
    if (m_errors) {
      m_errors->append(QStringLiteral("undefined variable '%1' at line %2")
                           .arg(expr.ident, QString::number(expr.line)));
    }
  }
}

void UndeclaredIdentValidator::visitIndexAccessExpr(const Expr &expr) {
  if (expr.ident != QString::fromLatin1(AcKeyword::kThis) && !m_scopeVars.contains(expr.ident)) {
    if (m_errors) {
      m_errors->append(QStringLiteral("undefined variable '%1' at line %2")
                           .arg(expr.ident, QString::number(expr.line)));
    }
  }
}

void UndeclaredIdentValidator::visitFuncCallExpr(const Expr &expr) {
  AstVisitor::visitFuncCallExpr(expr);

  // 检查函数名是否已知（内置函数或用户自定义函数）
  if (!AcBuiltin::kAll.contains(expr.funcCall.name) && !m_scopeVars.contains(expr.funcCall.name)) {
    if (m_errors) {
      m_errors->append(QStringLiteral("unknown function '%1' at line %2")
                           .arg(expr.funcCall.name, QString::number(expr.line)));
    }
  }

  if (expr.funcCall.name == QString::fromLatin1(AcBuiltin::kReadFile) &&
      expr.funcCall.args.isEmpty()) {
    if (m_errors) {
      m_errors->append(
          QStringLiteral("readFile() requires a file path argument at line %1").arg(expr.line));
    }
  }
}

void UndeclaredIdentValidator::visitMethodCallExpr(const Expr &expr) {
  if (expr.methodCall.object) {
    // 链式访问：this.engine.start() → 访问对象表达式
    visitExpr(*expr.methodCall.object);
  } else {
    // 检查对象变量是否已声明
    if (expr.methodCall.objName != QString::fromLatin1(AcKeyword::kThis) &&
        !m_scopeVars.contains(expr.methodCall.objName)) {
      if (m_errors) {
        m_errors->append(QStringLiteral("undefined variable '%1' at line %2")
                             .arg(expr.methodCall.objName, QString::number(expr.line)));
      }
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
  for (const auto *arg : expr.methodCall.args) {
    if (arg) visitExpr(*arg);
  }
}

void UndeclaredIdentValidator::visitStaticAccessExpr(const Expr &expr) {
  AstVisitor::visitStaticAccessExpr(expr);
}

void UndeclaredIdentValidator::visitObjectExpr(const Expr &expr) {
  AstVisitor::visitObjectExpr(expr);
}

void UndeclaredIdentValidator::visitArrayExpr(const Expr &expr) {
  AstVisitor::visitArrayExpr(expr);
}

void UndeclaredIdentValidator::visitBinaryExpr(const Expr &expr) {
  AstVisitor::visitBinaryExpr(expr);
}