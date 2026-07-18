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

  /// @brief 跳过块注释（/* 到 */）
  /// @param source 源码字符串
  /// @param pos 当前位置（会被更新到 */ 之后）
  /// @param line 当前行号（会被更新以反映注释中的换行）
  /// @param[out] error 错误信息（未闭合的块注释时设置）
  /// @return true 成功跳过，false 块注释未闭合
  static bool skipBlockComment(const QString &source, int &pos, int &line, QString &error);

  /// @brief 解析字符串字面量（从当前位置的 " 开始）
  static Token parseStringLiteral(const QString &source, int &pos, int line, QString &error);

  /// @brief 解析模板字符串字面量（从当前位置的 ` 开始）
  static Token parseTemplateStringLiteral(const QString &source, int &pos, int &line,
                                          QString &error);

  /// @brief 解析数字字面量（从当前位置的数字开始）
  static Token parseNumberLiteral(const QString &source, int &pos, int line);

  /// @brief 解析标识符或关键字（从当前位置的字母/下划线开始）
  static Token parseIdentifier(const QString &source, int &pos, int line);
};