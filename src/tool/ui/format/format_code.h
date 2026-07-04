/**
 * @file format_code.h
 * @brief 代码自动排版格式化工具
 *
 * 支持 AC 脚本、TPL 模板、JSON 三种文件类型的自动缩进排版。
 * 缩进统一使用 2 个空格。
 */

#pragma once

#include <QString>

class FormatCode {
public:
  /// @brief 缩进大小常量（2 个空格）
  static constexpr int kIndentSize = 2;

  /// @brief 格式化模式
  enum FormatMode {
    FormatJson, ///< JSON 文件格式化
    FormatAc,   ///< AC 脚本格式化
    FormatTpl   ///< TPL 模板格式化
  };

  /// @brief 格式化代码
  /// @param input  原始代码文本
  /// @param mode   格式化模式
  /// @return 格式化后的文本；如果无法格式化则返回原始文本
  static QString format(const QString &input, FormatMode mode);

private:
  // ── JSON 格式化 ──
  /// @brief JSON 格式化（2 空格缩进）
  static QString formatJson(const QString &input);

  // ── AC 脚本格式化 ──
  /// @brief AC 脚本格式化（基于 {} 缩进）
  static QString formatAc(const QString &input);

  // ── TPL 模板格式化 ──
  /// @brief TPL 模板格式化（基于 ${each}/${if} 缩进）
  static QString formatTpl(const QString &input);
};