/**
 * @file ac_interpreter.h
 * @brief 解释执行器 — 执行 AST 并返回结果
 */

#pragma once

#include "ac_type.h"

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QString>
#include <QStringList>
#include <functional>

class TplEngine;

/// @brief 解释执行器 — 执行 AST 并返回结果
class AcInterpreter {
public:
  /// 日志回调：print() 的输出通过此回调通知 UI
  using LogCallback = std::function<void(const QString &text, bool isError)>;

  /// @brief 执行 AST
  /// @param program AST 根节点
  /// @param error 错误信息输出
  /// @return 执行结果
  QJsonValue execute(const Block &program, QString &error);

  void setScriptDir(const QString &dir) { m_scriptDir = dir; }
  void setRootDir(const QString &dir) { m_rootDir = dir; }
  void setLogCallback(LogCallback cb) { m_logCallback = std::move(cb); }
  QStringList generatedFiles() const { return m_generatedFiles; }

  // ── 验证 ──
  QStringList validateUndeclaredIdents(const Block &program,
                                       const QSet<QString> &declaredVars) const;

private:
  // ── 表达式求值 ──
  QJsonValue evalExpr(const Expr &expr);
  QJsonValue evalExprWithThis(const Expr &expr, const QJsonObject &thisObj);
  QJsonValue evalBinary(const Expr &expr);
  QJsonValue evalMethodCall(const Expr &expr);
  QJsonValue evalNewInstance(const Expr &expr);
  QJsonValue callBuiltin(const QString &name, const QVector<Expr *> &args);

  // ── 辅助方法 ──
  QJsonValue resolveVar(const QString &name) const;
  static bool isTruthy(const QJsonValue &cond);

  // ── 语句执行 ──
  void execStmt(const Block::Stmt &stmt);
  void execBlock(const Block &block);
  void execBlockWithThis(const Block &block, const QJsonObject &thisObj,
                         QJsonValue &returnVal);

  // ── 类方法执行 ──
  QJsonValue execMethod(const MethodDef &method, const QJsonObject &thisObj,
                        const QJsonValue &callArgs);

  // ── 验证 ──
  void validateExprIdents(const Expr &expr, QStringList &errors,
                          const QSet<QString> &scopeVars) const;
  void validateStmtIdents(const Block::Stmt &stmt, QStringList &errors,
                          const QSet<QString> &scopeVars) const;
  void validateBlockIdents(const Block &block, QStringList &errors,
                           const QSet<QString> &scopeVars) const;

  // ── 运行状态 ──
  QString *m_error = nullptr;
  QString m_scriptDir;
  QString m_rootDir;
  QHash<QString, QJsonValue> m_vars;
  QHash<QString, QJsonValue> m_globals;
  QHash<QString, ClassDef> m_classes;
  QHash<QString, QJsonObject> m_instances;
  QJsonObject m_currentThis;
  QJsonObject m_modifiedThis;
  bool m_hasReturned = false;
  QJsonValue m_returnValue;
  QStringList m_generatedFiles;
  LogCallback m_logCallback;
};