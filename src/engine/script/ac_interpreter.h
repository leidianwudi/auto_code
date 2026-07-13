/**
 * @file ac_interpreter.h
 * @brief 解释执行器 — 执行 AST 并返回结果
 */

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QString>
#include <QStringList>
#include <functional>

#include "ac_type.h"

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

  /// @brief 设置日志回调
  void setLogCallback(const LogCallback &cb) { m_logCallback = cb; }

  /// @brief 设置脚本目录
  void setScriptDir(const QString &dir) { m_scriptDir = dir; }

  /// @brief 设置项目根目录
  void setRootDir(const QString &dir) { m_rootDir = dir; }

  /// @brief 获取本次执行生成的文件列表
  /// @return 文件路径列表
  QStringList generatedFiles() const { return m_generatedFiles; }

  // ── 辅助 ──
  /// @brief 校验表达式中是否有未声明的标识符
  QStringList validateUndeclaredIdents(const Block &program,
                                       const QSet<QString> &declaredVars) const;

private:
  // ── 执行 ──
  void execBlock(const Block &block);
  void execBlockWithThis(const Block &block, const QJsonObject &thisObj, QJsonValue &returnVal);
  void execStmt(const Block::Stmt &stmt);
  QJsonValue evalExpr(const Expr &expr);
  QJsonValue evalExprWithThis(const Expr &expr, const QJsonObject &thisObj);
  QJsonValue evalBinary(const Expr &expr);
  QJsonValue evalMethodCall(const Expr &expr);
  QJsonValue evalNewInstance(const Expr &expr);
  QJsonValue callBuiltin(const QString &name, const QVector<Expr *> &args, int line);

  // ── 变量操作 ──
  QJsonValue resolveVar(const QString &name) const;
  void setVar(const QString &name, const QJsonValue &val);
  bool containsVar(const QString &name) const;
  void pushScope();
  void popScope();
  static bool isTruthy(const QJsonValue &cond);

  // ── 引用计数辅助 ──
  /// 如果值是受管理的实例，retain
  void retainIfInstance(const QJsonValue &val);
  /// 如果值是受管理的实例，release（引用计数归零时触发析构）
  void releaseIfInstance(const QJsonValue &val);
  /// 如果值是受管理的实例，release（用户自定义类需执行 __destruct__ AST）
  void releaseIfInstanceWithDestruct(const QJsonValue &val);
  /// 递归释放值中的所有受管理实例（数组/对象内部的实例）
  void releaseDeep(const QJsonValue &val);

  // ── 类方法执行 ──
  QJsonValue execMethod(const MethodDef &method, const QJsonObject &thisObj,
                        const QJsonValue &callArgs);
  void initStaticVars(const ClassDef &cd);

  // ── 顶层函数执行 ──
  QJsonValue execUserFunction(const MethodDef &func, const QJsonValue &callArgs);

  // ── 验证 ──
  /// @brief 验证辅助数据结构：类方法名列表 + 变量→类名映射
  struct ValidationContext {
    QHash<QString, QStringList> classMethods;  ///< 类名 → 方法名列表
    QHash<QString, QString> varClass;          ///< 变量名 → 类名（let x = new Car()）
  };

  /// @brief 预扫描 AST，收集类定义和 new 赋值信息
  void collectValidationInfo(const Block &block, ValidationContext &ctx) const;

  void validateExprIdents(const Expr &expr, QStringList &errors, QSet<QString> &scopeVars,
                          const ValidationContext &ctx) const;
  void validateStmtIdents(const Block::Stmt &stmt, QStringList &errors, QSet<QString> &scopeVars,
                          ValidationContext &ctx) const;
  void validateBlockIdents(const Block &block, QStringList &errors, QSet<QString> &scopeVars,
                           ValidationContext &ctx) const;

  // ── 运行状态 ──
  QString *m_error = nullptr;
  QString m_scriptDir;
  QString m_rootDir;
  QVector<QHash<QString, QJsonValue>> m_scopeStack;
  QHash<QString, ClassDef> m_classes;
  QHash<QString, MethodDef> m_functions;
  QHash<QString, QJsonObject> m_staticVars;
  QSet<QString> m_staticInited;
  QJsonObject m_currentThis;
  QJsonObject m_modifiedThis;
  bool m_hasReturned = false;
  QJsonValue m_returnValue;
  QStringList m_generatedFiles;

  LogCallback m_logCallback;
};