/**
 * @file ac_validator.h
 * @brief AC 脚本验证器 — 实现 IValidator 接口
 *
 * 整合词法分析、语法分析、未声明标识符检查和类型检查，
 * 对外提供统一的 validate() 接口。
 */

#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

#include "../validation_result.h"
#include "ac_lexer.h"
#include "ac_parser.h"
#include "ac_type.h"
#include "ac_type_checker.h"
#include "undeclared_ident_validator.h"

/// @brief AC 脚本验证器 — 实现 IValidator 接口
///
/// 验证流程：
/// 1. 词法分析（AcLexer）
/// 2. 语法分析（AcParser）
/// 3. 未声明标识符检查（UndeclaredIdentValidator）
/// 4. 静态类型检查（AcTypeChecker）
class AcValidator : public IValidator {
public:
  AcValidator() = default;

  QString name() const override { return QStringLiteral("AcValidator"); }

  /// @brief 对 AC 脚本源码进行完整验证
  /// @param source .ac 脚本源码字符串
  /// @return 验证结果列表
  QVector<ValidationResult> validate(const QString &source) override;

private:
  /// @brief 从 AST 中提取类定义和函数定义
  void collectClassesAndFunctions(const Block &program);

  /// @brief 将错误信息字符串转为 ValidationResult
  /// @param msg 错误信息，格式如 "undefined variable 'x' at line 5"
  /// @return 解析后的 ValidationResult
  ValidationResult parseError(const QString &msg) const;

  /// @brief 从错误信息中提取行号
  /// @param msg 错误信息字符串
  /// @return 行号（1-based），未找到返回 0
  int extractLine(const QString &msg) const;

  // ── 子模块 ──
  AcLexer m_lexer;
  AcParser m_parser;
  UndeclaredIdentValidator m_undeclaredValidator;
  AcTypeChecker m_typeChecker;

  // ── AST 缓存 ──
  Block m_program;
  QSet<QString> m_declaredVars;
  QHash<QString, ClassDef> m_classes;
  QHash<QString, MethodDef> m_functions;
};