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

#include "ac_object_manager.h"
#include "ac_type.h"

class TplEngine;
class AcBuiltinEval;

/// @brief 解释执行器 — 执行 AST 并返回结果
class AcInterpreter {
  friend class AcBuiltinEval;

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

  /// @brief 获取已解析的类定义集合（供类型检查器使用）
  const QHash<QString, ClassDef> &classes() const { return m_classes; }

  /// @brief 比较两个 JSON 值（用于 == 和数组 indexOf/includes）
  static int compareValues(const QJsonValue &l, const QJsonValue &r);

  /// @brief 判断值是否为真
  static bool isTruthy(const QJsonValue &cond);

  /// @brief 推导类型名称
  static QString inferTypeName(const QJsonValue &val);

private:
  // ── 执行 ──
  void execBlock(const Block &block);
  void execBlockWithThis(const Block &block, const QJsonObject &thisObj, QJsonValue &returnVal);
  void execStmt(const Block::Stmt &stmt);
  void execIfStmt(const IfStmt &ifStmt);
  QJsonValue evalExpr(const Expr &expr);
  QJsonValue evalExprWithThis(const Expr &expr, const QJsonObject &thisObj);
  /// @brief 解析类名.属性访问（静态属性 + 类对象回退），返回属性值；失败返回 Undefined
  QJsonValue resolveClassAccess(const QString &className, const QString &propName);
  /// @brief 解析类的静态属性或方法访问，返回属性值/类引用；失败返回 Undefined
  QJsonValue resolveClassPropOrMethod(const QString &className, const QString &propName);
  /// @brief 创建类引用对象 { __class__: className }
  QJsonValue makeClassRef(const QString &className) const;
  /// @brief 应用复合赋值运算符
  QJsonValue applyCompoundOp(const QJsonValue &currentVal, const QJsonValue &newVal, CompoundOp op,
                             int line);
  /// @brief 将修改后的值回写到变量/this/属性
  void writeBackVar(const Expr &objectExpr, const QJsonValue &val);
  /// @brief 执行静态属性赋值
  void execStaticAssign(const QString &className, const QString &propName, const QJsonValue &val);
  /// @brief 执行 this 属性赋值
  void execThisAssign(const QString &propName, const QJsonValue &val);
  QJsonValue evalBinary(const Expr &expr);
  QJsonValue evalUnary(const Expr &expr);
  QJsonValue evalMethodCall(const Expr &expr);
  QJsonValue evalNewInstance(const Expr &expr);
  QJsonValue callBuiltin(const QString &name, const std::vector<std::unique_ptr<Expr>> &args,
                         int line);
  QJsonValue evalStringBuiltin(const QString &obj, const QString &method,
                               const std::vector<std::unique_ptr<Expr>> &args, int line);
  QJsonValue evalArrayBuiltin(const QJsonArray &arr, const QString &method,
                              const std::vector<std::unique_ptr<Expr>> &args, int line,
                              QJsonValue &modifiedArr);
  /// @brief 执行类方法调用（native 类 + 用户类 + super）
  QJsonValue evalClassMethod(const QJsonObject &obj, const QString &className, const Expr &expr,
                             bool isChained, bool isSuper);

  // ── 变量操作 ──
  QJsonValue resolveVar(const QString &name) const;
  void setVar(const QString &name, const QJsonValue &val);
  bool containsVar(const QString &name) const;
  void pushScope();
  void popScope();

  // ── 引用计数辅助 ──
  /// 如果值是受管理的实例，retain
  void retainIfInstance(const QJsonValue &val);
  /// 如果值是受管理的实例，release（引用计数归零时触发析构）
  void releaseIfInstance(const QJsonValue &val);
  /// 如果值是受管理的实例，release（用户自定义类需执行 __destruct__ AST）
  void releaseIfInstanceWithDestruct(const QJsonValue &val);
  /// 递归释放值中的所有受管理实例（数组/对象内部的实例）
  void releaseDeep(const QJsonValue &val);

  // ── 标记-清扫（处理循环引用） ──
  /// 从根（存活作用域、静态变量）出发标记所有可达实例，回收未标记的循环引用垃圾
  void collectCycles();
  /// 递归标记 QJsonValue 中所有可达的受管理实例
  void markFromValue(const QJsonValue &val);
  /// 执行单个实例的析构（释放属性引用 + 调用 __destruct__）
  void processDestructInfo(const AcObjectManager::DestructInfo &info);

  // ── 类方法执行 ──
  QJsonValue execMethod(const MethodDef &method, const QJsonObject &thisObj,
                        const QJsonValue &callArgs);
  void initStaticVars(const ClassDef &cd);

  // ── 继承辅助 ──
  const MethodDef *findMethod(const QString &className, const QString &methodName) const;
  QJsonObject createBaseInstance(const QString &baseClassName);

  // ── 顶层函数执行 ──
  QJsonValue execUserFunction(const MethodDef &func, const QJsonValue &callArgs);

  // ── 运行状态 ──
  QString m_error;
  QString m_scriptDir;
  QString m_rootDir;
  AcObjectManager m_objMgr;
  QVector<QHash<QString, QJsonValue>> m_scopeStack;
  QVector<QVector<QString>> m_usingStack;
  QHash<QString, ClassDef> m_classes;
  QHash<QString, MethodDef> m_functions;
  int m_funcExprCounter = 0;  ///< 函数表达式计数器（生成唯一 lambda 名）
  QHash<QString, QJsonObject> m_staticVars;
  QSet<QString> m_staticInited;
  QJsonObject m_currentThis;
  QJsonObject m_modifiedThis;
  QHash<QString, QString> m_inferredTypes;  ///< 推导类型映射：变量名 → 类型名
  bool m_hasReturned = false;
  bool m_hasBreak = false;
  bool m_hasContinue = false;
  QJsonValue m_returnValue;
  QStringList m_generatedFiles;

  LogCallback m_logCallback;
};