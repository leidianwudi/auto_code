/**
 * @file ast_visitor.h
 * @brief AST 访问器基类 — 遍历 ac 脚本 AST 节点
 *
 * 提供默认的递归遍历实现，子类只需重写感兴趣的 visit 方法。
 * 消除多个验证器（类型检查器、未声明标识符检查器）中重复的 switch 遍历代码。
 */

#pragma once

#include "ac_type.h"

/// @brief AST 访问器基类 — 遍历 ac 脚本 AST 节点
///
/// 使用方式：
/// @code
/// class MyChecker : public AstVisitor {
///   void visitAssign(const AssignStmt &as) override {
///     // 每次遇到赋值语句时调用
///     // 子节点会自动递归遍历
///   }
/// };
/// MyChecker checker;
/// checker.visitBlock(program);  // 开始遍历
/// @endcode
class AstVisitor {
public:
  virtual ~AstVisitor() = default;

  /// @name 核心遍历入口
  /// @{
  virtual void visitBlock(const Block &block);
  virtual void visitStmt(const Block::Stmt &stmt);
  virtual void visitExpr(const Expr &expr);
  /// @}

  /// @name 语句类型（由 visitStmt 分发）
  /// @{
  virtual void visitCallStmt(const CallStmt &cs);
  virtual void visitAssignStmt(const AssignStmt &as);
  virtual void visitIndexAssignStmt(const IndexAssignStmt &ias);
  virtual void visitForStmt(const ForStmt &fs);
  virtual void visitIfStmt(const IfStmt &is);
  virtual void visitClassDef(const ClassDef &cd);
  virtual void visitInterfaceDef(const InterfaceDef &iface);
  virtual void visitFuncDef(const MethodDef &md);
  virtual void visitReturnStmt(const Expr &retExpr);
  virtual void visitExprStmt(const Expr &expr);
  virtual void visitImportStmt(const ImportStmt &imp);
  virtual void visitWhileStmt(const WhileStmt &ws);
  virtual void visitSwitchStmt(const SwitchStmt &ss);
  virtual void visitUsingStmt(const UsingStmt &us);
  /// @}

  /// @name 表达式类型（由 visitExpr 分发）
  /// @{
  virtual void visitIdentExpr(const Expr &expr);
  virtual void visitStringExpr(const Expr &expr);
  virtual void visitNumberExpr(const Expr &expr);
  virtual void visitBoolExpr(const Expr &expr);
  virtual void visitThisExpr(const Expr &expr);
  virtual void visitPropAccessExpr(const Expr &expr);
  virtual void visitIndexAccessExpr(const Expr &expr);
  virtual void visitFuncCallExpr(const Expr &expr);
  virtual void visitMethodCallExpr(const Expr &expr);
  virtual void visitStaticAccessExpr(const Expr &expr);
  virtual void visitNewInstanceExpr(const Expr &expr);
  virtual void visitObjectExpr(const Expr &expr);
  virtual void visitArrayExpr(const Expr &expr);
  virtual void visitBinaryExpr(const Expr &expr);
  virtual void visitUnaryExpr(const Expr &expr);
  virtual void visitFuncExprExpr(const Expr &expr);
  /// @}
};