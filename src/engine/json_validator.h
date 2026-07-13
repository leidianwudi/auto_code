/**
 * @file json_validator.h
 * @brief JSON 语法验证器 — 实现 IValidator 接口
 *
 * 使用 Qt 内置的 QJsonDocument 解析器进行 JSON 语法验证，
 * 将错误偏移量转换为行列号，输出统一的 ValidationResult。
 */

#pragma once

#include <QString>
#include <QVector>

#include "validation_result.h"

/// @brief JSON 语法验证器 — 实现 IValidator 接口
///
/// 使用 QJsonDocument::fromJson 解析文本，
/// 将解析错误（偏移量）转换为行列号。
class JsonValidator : public IValidator {
public:
  QString name() const override { return QStringLiteral("JsonValidator"); }

  /// @brief 对 JSON 源码进行语法验证
  /// @param source JSON 源码字符串
  /// @return 验证结果列表
  QVector<ValidationResult> validate(const QString &source) override;
};