/**
 * @file ac_builtin_eval.h
 * @brief 内置类型方法求值 — 字符串方法和数组方法
 */

#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <QVector>

struct Expr;

class AcInterpreter;

/// @brief 内置类型方法求值辅助
class AcBuiltinEval {
public:
  /// @brief 求值字符串内置方法
  /// @param interpreter 解释器实例（用于 evalExpr 求值参数）
  /// @param obj 字符串对象
  /// @param method 方法名
  /// @param args 参数表达式列表
  /// @param line 源码行号
  /// @param error 错误输出
  /// @return 方法返回值
  static QJsonValue evalStringMethod(AcInterpreter &interpreter, const QString &obj,
                                     const QString &method, const QVector<Expr *> &args, int line,
                                     QString &error);

  /// @brief 求值数组内置方法
  /// @param interpreter 解释器实例
  /// @param arr 数组对象
  /// @param method 方法名
  /// @param args 参数表达式列表
  /// @param line 源码行号
  /// @param modifiedArr 修改后的数组（非 null 时需回写）
  /// @param error 错误输出
  /// @return 方法返回值
  static QJsonValue evalArrayMethod(AcInterpreter &interpreter, const QJsonArray &arr,
                                    const QString &method, const QVector<Expr *> &args, int line,
                                    QJsonValue &modifiedArr, QString &error);
};