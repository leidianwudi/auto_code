/**
 * @file validation_result.h
 * @brief 统一验证结果类型 — 供所有引擎校验器共用
 *
 * 所有验证器（ac 脚本、模板、JSON）统一返回此类型，
 * 确保 UI 层可以用同一套代码处理错误标记和高亮。
 */

#pragma once

#include <QString>
#include <QVector>

/// @brief 验证结果 — 单条错误/警告信息
struct ValidationResult {
  /// @brief 严重级别
  enum Severity { kError, kWarning };

  int line = 0;                ///< 行号（1-based）
  int column = 0;              ///< 列号
  int length = 0;              ///< 错误区间长度
  QString message;             ///< 错误描述
  Severity severity = kError;  ///< 严重级别

  ValidationResult() = default;

  ValidationResult(int line, int column, int length, const QString &message,
                   Severity severity = kError)
      : line(line), column(column), length(length), message(message), severity(severity) {}

  /// @brief 仅消息+行号的快捷构造（用于列号/长度不重要时）
  static ValidationResult atLine(int line, const QString &message, Severity sev = kError) {
    return ValidationResult(line, 0, 0, message, sev);
  }

  /// @brief 转换为旧格式字符串（兼容代码）
  QString toLegacyString() const {
    if (line > 0) return QStringLiteral("%1 at line %2").arg(message).arg(line);
    return message;
  }
};

/// @brief 验证器接口 — 所有引擎校验器实现此接口
class IValidator {
public:
  virtual ~IValidator() = default;

  /// @brief 验证器名称标识
  virtual QString name() const = 0;

  /// @brief 对源码进行验证，返回验证结果列表
  virtual QVector<ValidationResult> validate(const QString &source) = 0;
};