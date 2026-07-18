/**
 * @file ast_visitor.cpp
 * @brief AST 访问器基类 — 默认递归遍历实现
 */

#include "ast_visitor.h"

// ═════════════════════════════════════════════════════════════════════════════
// 核心遍历
// ═════════════════════════════════════════════════════════════════════════════

void AstVisitor::visitBlock(const Block &block) {
  for (const auto &stmt : block.stmts) visitStmt(stmt);
}

void AstVisitor::visitStmt(const Block::Stmt &stmt) {
  switch (stmt.kind) {
    case Block::Stmt::kCall:
      visitCallStmt(stmt.call);
      break;
    case Block::Stmt::kAssign:
      visitAssignStmt(stmt.assign);
      break;
    case Block::Stmt::kIndexAssign:
      visitIndexAssignStmt(stmt.indexAssign);
      break;
    case Block::Stmt::kPropAssign:
      visitExpr(stmt.propAssign.objectExpr);
      visitExpr(stmt.propAssign.value);
      break;
    case Block::Stmt::kFor:
      visitForStmt(stmt.forStmt);
      break;
    case Block::Stmt::kIf:
      visitIfStmt(stmt.ifStmt);
      break;
    case Block::Stmt::kClassDef:
      visitClassDef(stmt.classDef);
      break;
    case Block::Stmt::kInterfaceDef:
      visitInterfaceDef(stmt.interfaceDef);
      break;
    case Block::Stmt::kFuncDef:
      visitFuncDef(stmt.funcDef);
      break;
    case Block::Stmt::kReturn:
      visitReturnStmt(stmt.returnValue);
      break;
    case Block::Stmt::kExpr:
      visitExprStmt(stmt.exprStmt);
      break;
    case Block::Stmt::kImport:
      visitImportStmt(stmt.importStmt);
      break;
    case Block::Stmt::kWhile:
      visitWhileStmt(stmt.whileStmt);
      break;
    case Block::Stmt::kSwitch:
      visitSwitchStmt(stmt.switchStmt);
      break;
    case Block::Stmt::kBreak:
    case Block::Stmt::kContinue:
      break;
    case Block::Stmt::kUsing:
      visitUsingStmt(stmt.usingStmt);
      break;
  }
}

void AstVisitor::visitExpr(const Expr &expr) {
  switch (expr.kind) {
    case Expr::kIdent:
      visitIdentExpr(expr);
      break;
    case Expr::kString:
      visitStringExpr(expr);
      break;
    case Expr::kNumber:
      visitNumberExpr(expr);
      break;
    case Expr::kBool:
      visitBoolExpr(expr);
      break;
    case Expr::kThis:
      visitThisExpr(expr);
      break;
    case Expr::kPropAccess:
      visitPropAccessExpr(expr);
      break;
    case Expr::kIndexAccess:
      visitIndexAccessExpr(expr);
      break;
    case Expr::kFuncCall:
      visitFuncCallExpr(expr);
      break;
    case Expr::kMethodCall:
      visitMethodCallExpr(expr);
      break;
    case Expr::kStaticAccess:
      visitStaticAccessExpr(expr);
      break;
    case Expr::kNewInstance:
      visitNewInstanceExpr(expr);
      break;
    case Expr::kObject:
      visitObjectExpr(expr);
      break;
    case Expr::kArray:
      visitArrayExpr(expr);
      break;
    case Expr::kBinary:
      visitBinaryExpr(expr);
      break;
    case Expr::kUnary:
      visitUnaryExpr(expr);
      break;
    case Expr::kNull:
    case Expr::kUndefined:
      break;
    case Expr::kTernary:
      if (expr.left) visitExpr(*expr.left);
      if (expr.right) visitExpr(*expr.right);
      if (expr.operand) visitExpr(*expr.operand);
      break;
    case Expr::kFuncExpr:
      visitFuncExprExpr(expr);
      break;
  }
}

// ═════════════════════════════════════════════════════════════════════════════
// 语句 — 默认实现：递归遍历子节点
// ═════════════════════════════════════════════════════════════════════════════

void AstVisitor::visitCallStmt(const CallStmt &cs) {
  visitExpr(cs.className);
  visitExpr(cs.funcName);
  visitExpr(cs.args);
}

void AstVisitor::visitAssignStmt(const AssignStmt &as) { visitExpr(as.value); }

void AstVisitor::visitIndexAssignStmt(const IndexAssignStmt &ias) {
  visitExpr(ias.objectExpr);
  visitExpr(ias.indexExpr);
  visitExpr(ias.value);
}

void AstVisitor::visitForStmt(const ForStmt &fs) {
  visitExpr(fs.arrayExpr);
  visitBlock(fs.body);
}

void AstVisitor::visitIfStmt(const IfStmt &is) {
  visitExpr(is.condition);
  visitBlock(is.thenBlock);
  if (is.elseIfBranch)
    visitIfStmt(*is.elseIfBranch);
  else if (is.hasElse)
    visitBlock(is.elseBlock);
}

void AstVisitor::visitClassDef(const ClassDef &cd) {
  for (const auto &prop : cd.properties) {
    if (prop.value) visitExpr(*prop.value);
  }
  for (const auto &method : cd.methods) {
    visitBlock(method.body);
  }
}

void AstVisitor::visitInterfaceDef(const InterfaceDef &iface) { Q_UNUSED(iface); }

void AstVisitor::visitFuncDef(const MethodDef &md) { visitBlock(md.body); }

void AstVisitor::visitReturnStmt(const Expr &retExpr) { visitExpr(retExpr); }

void AstVisitor::visitExprStmt(const Expr &expr) { visitExpr(expr); }

void AstVisitor::visitImportStmt(const ImportStmt &imp) { Q_UNUSED(imp); }

void AstVisitor::visitWhileStmt(const WhileStmt &ws) {
  visitExpr(ws.condition);
  visitBlock(ws.body);
}

void AstVisitor::visitSwitchStmt(const SwitchStmt &ss) {
  visitExpr(ss.expr);
  for (const auto &sc : ss.cases) {
    if (!sc.isDefault) visitExpr(sc.value);
    visitBlock(sc.body);
  }
}

void AstVisitor::visitUsingStmt(const UsingStmt &us) {
  if (us.value) visitExpr(*us.value);
}

// ═════════════════════════════════════════════════════════════════════════════
// 表达式 — 默认实现：递归遍历子节点
// ═════════════════════════════════════════════════════════════════════════════

void AstVisitor::visitIdentExpr(const Expr &expr) { Q_UNUSED(expr); }
void AstVisitor::visitStringExpr(const Expr &expr) { Q_UNUSED(expr); }
void AstVisitor::visitNumberExpr(const Expr &expr) { Q_UNUSED(expr); }
void AstVisitor::visitBoolExpr(const Expr &expr) { Q_UNUSED(expr); }
void AstVisitor::visitThisExpr(const Expr &expr) { Q_UNUSED(expr); }

void AstVisitor::visitPropAccessExpr(const Expr &expr) {
  if (expr.propObject) visitExpr(*expr.propObject);
}

void AstVisitor::visitIndexAccessExpr(const Expr &expr) {
  if (expr.left) visitExpr(*expr.left);
  if (expr.right) visitExpr(*expr.right);
}

void AstVisitor::visitFuncCallExpr(const Expr &expr) {
  for (const auto &arg : expr.funcCall.args) {
    if (arg) visitExpr(*arg);
  }
}

void AstVisitor::visitMethodCallExpr(const Expr &expr) {
  for (const auto &arg : expr.methodCall.args) {
    if (arg) visitExpr(*arg);
  }
}

void AstVisitor::visitStaticAccessExpr(const Expr &expr) {
  for (const auto &arg : expr.funcCall.args) {
    if (arg) visitExpr(*arg);
  }
}

void AstVisitor::visitNewInstanceExpr(const Expr &expr) {
  for (const auto &arg : expr.constructorArgs) {
    if (arg) visitExpr(*arg);
  }
}

void AstVisitor::visitObjectExpr(const Expr &expr) {
  for (const auto &entry : expr.objEntries) {
    if (entry.value) visitExpr(*entry.value);
  }
}

void AstVisitor::visitArrayExpr(const Expr &expr) {
  for (const auto &item : expr.arrItems) {
    if (item) visitExpr(*item);
  }
}

void AstVisitor::visitBinaryExpr(const Expr &expr) {
  if (expr.left) visitExpr(*expr.left);
  if (expr.right) visitExpr(*expr.right);
}

void AstVisitor::visitUnaryExpr(const Expr &expr) {
  if (expr.operand) visitExpr(*expr.operand);
}

void AstVisitor::visitFuncExprExpr(const Expr &expr) { visitBlock(expr.funcExpr.body); }