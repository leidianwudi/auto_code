/**
 * @file undeclared_ident_validator.h
 * @brief 未声明标识符检查器 — 检查脚本中所有变量/函数是否已声明
 *
 * 从 AcInterpreter 中提取，与执行逻辑分离。
 * 基于 AstVisitor 实现，减少与 AcTypeChecker 的重复 AST 遍历代码。
 */

#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

#include "ast_visitor.h"

/// @brief 未声明标识符检查器 — 检查脚本中所有变量/函数是否已声明
class UndeclaredIdentValidator : public AstVisitor {
public:
  /// @brief 对 AST 进行未声明标识符检查
  /// @param program     AST 根节点
  /// @param declaredVars 已用 let 声明的变量名集合
  /// @param[out] errors 错误信息列表
  void validate(const Block &program, const QSet<QString> &declaredVars, QStringList &errors);

protected:
  // ── AstVisitor 重写 ──
  void visitAssignStmt(const AssignStmt &as) override;
  void visitForStmt(const ForStmt &fs) override;
  void visitClassDef(const ClassDef &cd) override;
  void visitInterfaceDef(const InterfaceDef &iface) override;
  void visitFuncDef(const MethodDef &md) override;
  void visitReturnStmt(const Expr &retExpr) override;
  void visitExprStmt(const Expr &expr) override;
  void visitImportStmt(const ImportStmt &imp) override;

  void visitIdentExpr(const Expr &expr) override;
  void visitPropAccessExpr(const Expr &expr) override;
  void visitIndexAccessExpr(const Expr &expr) override;
  void visitFuncCallExpr(const Expr &expr) override;
  void visitMethodCallExpr(const Expr &expr) override;
  void visitStaticAccessExpr(const Expr &expr) override;
  void visitObjectExpr(const Expr &expr) override;
  void visitArrayExpr(const Expr &expr) override;
  void visitBinaryExpr(const Expr &expr) override;

private:
  /// @brief 验证辅助数据结构：类方法名列表 + 变量→类名映射
  struct ValidationContext {
    QHash<QString, QStringList> classMethods;  ///< 类名 → 方法名列表
    QHash<QString, QString> varClass;          ///< 变量名 → 类名（let x = new Car()）
  };

  /// @brief 预扫描 AST，收集类定义和 new 赋值信息
  void collectValidationInfo(const Block &block);

  /// @brief 报告错误
  void reportError(const QString &msg, int line);

  // ── 状态 ──
  QStringList *m_errors = nullptr;
  QSet<QString> m_scopeVars;
  ValidationContext m_ctx;
};