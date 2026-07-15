/**
 * @file ac_lexer.h
 * @brief 词法分析器 — 将 .ac 源码拆分为 Token 序列
 */

#pragma once

#include "ac_type.h"

/// @brief 词法分析器 — 将 .ac 源码拆分为 Token 序列
class AcLexer {
public:
  /// @brief 将源码字符串拆分为 token 序列
  /// @param source .ac 源码
  /// @param[out] error 错误信息（解析失败时设置）
  /// @return token 序列，失败时返回空数组
  static QVector<Token> tokenize(const QString &source, QString &error);

private:
  /// @brief 跳过行注释（// 到行尾）
  static void skipLineComment(const QString &source, int &pos);
};