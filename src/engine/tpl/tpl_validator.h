/**
 * @file tpl_validator.h
 * @brief 模板语法验证器 — 实现 IValidator 接口
 *
 * 将原来散落在 code_editor.cpp 中的模板验证逻辑（括号匹配、标签配对、
 * 方法检查等）封装到独立的验证器中，实现统一的验证接口。
 */

#pragma once

#include <QRegularExpression>
#include <QString>
#include <QVector>

#include "../validation_result.h"

/// @brief 模板语法验证器 — 实现 IValidator 接口
///
/// 执行 4 步验证：
/// 1. 普通括号 () [] {} 匹配
/// 2. ${...} 闭合检查
/// 3. ${each}/${if} 标签配对
/// 4. 表达式方法调用检查
class TplValidator : public IValidator {
public:
  QString name() const override { return QStringLiteral("TplValidator"); }

  /// @brief 对模板源码进行语法验证
  /// @param source 模板源码字符串
  /// @return 验证结果列表（按行号排序）
  QVector<ValidationResult> validate(const QString &source) override;

private:
  /// @brief 第 1 步：检查普通括号 () [] {} 匹配
  void checkBrackets(const QString &text, QVector<ValidationResult> &results);

  /// @brief 第 2 步：检查 ${...} 是否闭合
  void checkDollarBraces(const QString &text, QVector<ValidationResult> &results);

  /// @brief 第 3 步：检查 ${each}/${if} 标签配对
  void checkTags(const QString &text, QVector<ValidationResult> &results);

  /// @brief 第 4 步：检查 ${...} 表达式中不支持的方法调用
  void checkMethods(const QString &text, QVector<ValidationResult> &results);

  /// @brief 辅助：将文档偏移量转换为行号（1-based）
  int offsetToLine(const QString &text, int offset) const;
};