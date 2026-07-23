/**
 * @file ac_type_checker.h
 * @brief 类型检查器 — 在解析阶段对 AST 进行静态类型检查
 *
 * 在代码执行前检查类型错误，确保：
 * - 参数类型与实际传入的实参类型匹配
 * - 返回类型与实际 return 表达式类型匹配
 * - 赋值类型兼容
 * - 二元运算操作数类型兼容
 * - 数组索引类型正确
 * - 类属性类型与赋值兼容
 */

#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

#include "ac_type.h"

/// @brief 类型检查器 — 编译期静态类型检查
class AcTypeChecker {
public:
  /// @brief 对 AST 进行类型检查
  /// @param program    解析后的 AST 根节点
  /// @param declaredVars 已声明的变量名集合
  /// @param classes    已定义的类集合（含原生类）
  /// @param functions  已定义的顶层函数集合
  /// @param[out] errors 类型错误列表
  void check(const Block &program, const QSet<QString> &declaredVars,
             const QHash<QString, ClassDef> &classes, const QHash<QString, MethodDef> &functions,
             QStringList &errors);

  /// @brief 设置当前检查的文件路径（用于错误消息）
  void setFilePath(const QString &path) { m_filePath = path; }

private:
  QString m_filePath;  ///< 当前检查的文件路径（用于错误消息）

  /// @brief 类型环境 — 变量名 → 类型映射
  struct TypeEnv {
    QHash<QString, AcType> varTypes;   ///< 变量名 → 类型
    QHash<QString, AcType> propTypes;  ///< this 的属性名 → 类型（当前类上下文）
    QString className;                 ///< 当前所在类名（空表示不在类中）
    AcType returnType;                 ///< 当前函数的声明返回类型（P3 返回值验证）
    QString funcName;                  ///< 当前函数名（P3 返回值验证）
  };

  // ── 内部状态 ──
  TypeEnv m_env;
  const QHash<QString, ClassDef> *m_classes = nullptr;
  const QHash<QString, MethodDef> *m_functions = nullptr;
  QStringList *m_errors = nullptr;
  QHash<QString, InterfaceDef> m_interfaces;  ///< 接口定义集合
  QSet<QString> m_declaredVars;               ///< 已声明的标识符（含跨文件导入的类名）

  // ── 核心检查 ──
  void checkBlock(const Block &block, TypeEnv &env);
  void checkStmt(const Block::Stmt &stmt, TypeEnv &env);
  AcType checkExpr(const Expr &expr, const TypeEnv &env);
  void checkAssign(const AssignStmt &as, const TypeEnv &env);
  void checkCallStmt(const CallStmt &cs, const TypeEnv &env);

  // ── 表达式分派检查（P5: 将 checkExpr 的 switch case 提取为独立方法）──
  AcType checkExprIdent(const Expr &expr, const TypeEnv &env);
  AcType checkExprPropAccess(const Expr &expr, const TypeEnv &env);
  AcType checkExprIndexAccess(const Expr &expr, const TypeEnv &env);
  AcType checkExprArray(const Expr &expr, const TypeEnv &env);
  AcType checkExprObject(const Expr &expr, const TypeEnv &env);
  AcType checkExprFuncExpr(const Expr &expr, const TypeEnv &env);
  AcType checkExprFuncCall(const Expr &expr, const TypeEnv &env);
  AcType checkExprMethodCall(const Expr &expr, const TypeEnv &env);
  AcType checkExprNewInstance(const Expr &expr, const TypeEnv &env);
  AcType checkExprStaticAccess(const Expr &expr, const TypeEnv &env);
  AcType checkExprBinary(const Expr &expr, const TypeEnv &env);

  // ── 继承与接口检查 ──
  void checkImplements(const ClassDef &cd);
  void checkOverride(const ClassDef &cd);
  void checkAccessInClass(const ClassDef &cd);
  bool isSignatureCompatible(const MethodDef &method, const InterfaceMethod &iface) const;
  bool isSubclassOf(const QString &sub, const QString &base) const;
  bool classImplementsInterface(const QString &className, const QString &ifaceName) const;

  // ── 类型兼容性 ──
  bool isCompatible(const AcType &from, const AcType &to) const;
  QString typeToString(const AcType &type) const;

  // ── 错误报告 ──
  void reportError(const QString &msg, int line);
};