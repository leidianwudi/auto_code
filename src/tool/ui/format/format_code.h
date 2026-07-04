/**
 * @file format_code.h
 * @brief 代码自动排版格式化工具
 *
 * 支持 AC 脚本、TPL 模板、JSON 三种文件类型的自动缩进排版。
 * 缩进统一使用 2 个空格。
 */

#pragma once

#include <QChar>
#include <QString>

class FormatCode {
public:
  /// @brief 缩进大小常量（2 个空格）
  static constexpr int kIndentSize = 2;

  /// @brief 一对配对的字符
  struct CharPair {
    QChar open;  ///< 开字符, e.g. '{'
    QChar close; ///< 闭字符, e.g. '}'
  };

  /// @brief 所有支持的配对字符
  static const CharPair kPairs[];
  static constexpr int kPairCount = 4; // { }  [ ]  ( )  " "

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

public:
  // ── 字符配对 ──

  /// @brief 判断 ch 是否是某个配对的开字符（如 { [ ( "）
  static bool isOpenChar(QChar ch);

  /// @brief 判断 ch 是否是某个非自配对的闭字符（如 } ] ) ，排除 "）
  static bool isCloseChar(QChar ch);

  /// @brief 返回开字符对应的闭字符；若不是开字符返回 QChar()
  static QChar matchingCloseChar(QChar open);

  /// @brief 判断光标前是否应在键入 open 时自动补全闭字符
  /// @param open 用户键入的字符
  /// @param charBefore 光标前一个字符（用于 " 的特殊判断）
  static bool shouldAutoPair(QChar open, QChar charBefore);
};