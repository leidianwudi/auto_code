/**
 * @file bracket_matcher.h
 * @brief 括号匹配算法（从 CodeEditor 中拆分）
 *
 * 负责括号的匹配、高亮和导航功能，
 * 支持 () [] {} 三种括号类型，
 * 提供智能代码块检测和性能优化。
 */

#pragma once

#include <QChar>
#include <QHash>
#include <QString>
#include <QVector>

class QTextDocument;

/**
 * @class BracketMatcher
 * @brief 括号匹配器
 *
 * 独立的括号匹配算法实现，支持：
 * - 成对括号查找与高亮
 * - 智能代码块检测（光标在代码中间时自动找到包含它的最近括号对）
 * - 字符串/注释过滤（避免误匹配）
 * - 性能优化的缓存机制
 */
class BracketMatcher {
public:
  /// 括号类型枚举
  enum class BracketType {
    Paren,   // ()
    Square,  // []
    Brace    // {}
  };

  /// 匹配结果结构体
  struct MatchResult {
    int openPos = -1;   ///< 左括号位置（0-based）
    int closePos = -1;  ///< 右括号位置（0-based）
    QChar openChar;     ///< 左括号字符
    bool isValid() const { return openPos >= 0 && closePos >= 0; }
  };

  /**
   * @brief 查找光标位置的括号匹配
   * @param pos 光标位置（1-based，用于 findMatchingBracket 兼容）
   * @param text 文本内容
   * @return 匹配结果
   */
  static MatchResult findMatchAtCursor(int pos, const QString &text);

  /**
   * @brief 查找包含光标位置的最近括号对（智能检测）
   * @param pos 光标位置（0-based）
   * @param text 文本内容
   * @param cache 字符串/注释缓存（可选，用于性能优化）
   * @return 距离最近的括号对
   */
  static MatchResult findEnclosingBrackets(int pos, const QString &text,
                                           QHash<int, bool> *cache = nullptr);

  /**
   * @brief 查找指定位置的匹配括号
   * @param pos 括号位置（1-based）
   * @param bracket 括号字符
   * @param matchBracket 输出：匹配的括号字符
   * @param text 文本内容
   * @param cache 字符串/注释缓存（可选）
   * @return 匹配位置（-1 表示未找到或无匹配）
   */
  static int findMatchingBracket(int pos, QChar bracket, QChar &matchBracket, const QString &text,
                                 QHash<int, bool> *cache = nullptr);

  /**
   * @brief 判断位置是否在字符串或注释中
   * @param pos 位置（0-based）
   * @param document 文档对象
   * @param text 文本内容
   * @param cache 结果缓存（避免重复计算）
   * @return true 如果在字符串或注释中
   */
  static bool isInStringOrComment(int pos, const QString &text, QHash<int, bool> &cache);

private:
  /// 根据括号字符获取对应的闭合括号
  static QChar getMatchingBracket(QChar bracket);
};