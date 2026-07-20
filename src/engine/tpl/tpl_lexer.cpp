/**
 * @file tpl_lexer.cpp
 * @brief 模板词法分析器实现
 */

#include "tpl_lexer.h"

#include "../ac_language.h"

namespace TplLexer {

/// @brief 判断表达式内容是否为块标签（非变量）
static bool isBlockTagExpr(const QString &expr, TokenType &outType) {
  // 注释 ${# ...}
  if (expr.startsWith(QChar('#'))) {
    outType = TokenType::Comment;
    return true;
  }
  // ${if condition}
  if (expr.startsWith(QString::fromLatin1(AcTemplate::kIfPrefix))) {
    outType = TokenType::IfOpen;
    return true;
  }
  // ${each item in items}
  if (expr.startsWith(QString::fromLatin1(AcTemplate::kEachPrefix))) {
    outType = TokenType::EachOpen;
    return true;
  }
  // ${else if condition}
  if (expr.startsWith(QString::fromLatin1(AcKeyword::kElse) + QLatin1Char(' ') +
                      QString::fromLatin1(AcTemplate::kIfPrefix))) {
    outType = TokenType::ElseIf;
    return true;
  }
  // ${else}
  if (expr == QString::fromLatin1(AcKeyword::kElse)) {
    outType = TokenType::Else;
    return true;
  }
  // ${/if}
  if (expr == QStringLiteral("/if")) {
    outType = TokenType::EndIf;
    return true;
  }
  // ${/each}
  if (expr == QStringLiteral("/each")) {
    outType = TokenType::EndEach;
    return true;
  }
  return false;
}

/// @brief 判断从 lineStart 到 tagStart 之间是否全是空白
static bool isLineLeadingWhitespace(const QString &s, int lineStart, int tagStart) {
  for (int i = lineStart; i < tagStart; ++i) {
    if (!s[i].isSpace()) return false;
  }
  return true;
}

/// @brief 判断从 tagEnd 到行尾（或字符串尾）之间是否只有空白（无其他标签或文本）
static bool isLineTrailingWhitespace(const QString &s, int tagEnd, int end) {
  for (int i = tagEnd; i < end; ++i) {
    if (s[i] == QChar('\n')) return true;  // 遇到行尾
    if (!s[i].isSpace()) return false;
  }
  return true;  // 字符串结束
}

/// @brief 判断标签是否独占一行
///
/// 规则：标签所在行从行首到行尾（或下一个 \n）之间只有空白字符和该标签本身。
/// 注意：如果一行有多个 ${...}（即使都是块标签），都不算"独占一行"。
static bool isTagAloneOnLine(const QString &tmpl, int tagStart, int tagEnd) {
  // 找标签所在行的行首
  int lineStart = tagStart;
  while (lineStart > 0 && tmpl[lineStart - 1] != QChar('\n')) {
    --lineStart;
  }
  // 找标签所在行的行尾（不包含 \n）
  int lineEnd = tagEnd;
  while (lineEnd < tmpl.length() && tmpl[lineEnd] != QChar('\n')) {
    ++lineEnd;
  }
  // 检查标签前是否只有空白
  if (!isLineLeadingWhitespace(tmpl, lineStart, tagStart)) return false;
  // 检查标签后是否只有空白
  if (!isLineTrailingWhitespace(tmpl, tagEnd, lineEnd)) return false;
  return true;
}

/// @brief 找注释标签 ${# ...} 的结束位置 }
///
/// 注释使用贪婪匹配：找到行内最后一个 } 作为注释结束符。
/// 这样注释内容中的 }（如代码示例 import { A } from 'x'）不会被误判为注释结束。
static int findCommentEnd(const QString &tmpl, int startPos, int exprStart) {
  int lineEnd = tmpl.indexOf(QChar('\n'), exprStart);
  if (lineEnd == -1) lineEnd = tmpl.length();
  int lastClose = tmpl.lastIndexOf(QChar('}'), lineEnd - 1);
  if (lastClose > startPos) return lastClose + 1;  // 返回 } 的下一个位置
  return -1;
}

QList<Token> tokenize(const QString &tmpl, QString &errorMsg) {
  QList<Token> tokens;
  errorMsg.clear();

  int pos = 0;
  while (pos < tmpl.length()) {
    // 找下一个 ${
    int tagStart = tmpl.indexOf(QString::fromLatin1(AcTemplate::kExprOpen), pos);
    if (tagStart == -1) {
      // 没有更多标签，剩余都是文本
      if (pos < tmpl.length()) {
        tokens.append(Token(TokenType::Text, tmpl.mid(pos), pos));
      }
      break;
    }

    // 标签前的文本
    if (tagStart > pos) {
      tokens.append(Token(TokenType::Text, tmpl.mid(pos, tagStart - pos), pos));
    }

    // 判断是否为注释 ${# ...}
    int exprStart = tagStart + 2;  // 跳过 ${
    int tagEnd;
    QString expr;

    if (exprStart < tmpl.length() && tmpl[exprStart] == QChar('#')) {
      // 注释：贪婪匹配到行内最后一个 }
      tagEnd = findCommentEnd(tmpl, tagStart, exprStart);
      if (tagEnd == -1) {
        errorMsg = QStringLiteral("Unclosed comment at position %1").arg(tagStart);
        return {};
      }
      // 注释内容（去掉 # 前缀）
      QString comment = tmpl.mid(exprStart + 1, tagEnd - exprStart - 2).trimmed();
      bool alone = isTagAloneOnLine(tmpl, tagStart, tagEnd);
      tokens.append(Token(TokenType::Comment, comment, tagStart, alone));
      pos = tagEnd;
      continue;
    }

    // 普通标签：找第一个 } 作为结束
    int closePos = tmpl.indexOf(QChar('}'), exprStart);
    if (closePos == -1) {
      errorMsg = QStringLiteral("Unclosed ${ at position %1").arg(tagStart);
      return {};
    }
    tagEnd = closePos + 1;
    expr = tmpl.mid(exprStart, closePos - exprStart).trimmed();

    // 判断标签类型
    TokenType tokenType;
    if (isBlockTagExpr(expr, tokenType)) {
      bool alone = isTagAloneOnLine(tmpl, tagStart, tagEnd);
      tokens.append(Token(tokenType, expr, tagStart, alone));
    } else {
      // 普通变量 ${expression}
      tokens.append(Token(TokenType::Variable, expr, tagStart, false));
    }
    pos = tagEnd;
  }

  return tokens;
}

}  // namespace TplLexer
