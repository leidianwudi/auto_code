/**
 * @file bracket_matcher.cpp
 * @brief 括号匹配算法实现
 */

#include "bracket_matcher.h"

#include <QTextBlock>
#include <QTextDocument>

// ──────────────────────────────────────────────────────────────
//  静态常量定义
// ──────────────────────────────────────────────────────────────

const QHash<QChar, QChar> kBracketPairs = {
    {QLatin1Char('('), QLatin1Char(')')}, {QLatin1Char(')'), QLatin1Char('(')},
    {QLatin1Char('['), QLatin1Char(']')}, {QLatin1Char(']'), QLatin1Char('[')},
    {QLatin1Char('{'), QLatin1Char('}')}, {QLatin1Char('}'), QLatin1Char('{')}};

const QString kBrackets = QStringLiteral("()[]{}");

// ──────────────────────────────────────────────────────────────
//  字符串/注释检测（带缓存优化）
// ──────────────────────────────────────────────────────────────

bool BracketMatcher::isInStringOrComment(int pos, const QString &text, QHash<int, bool> &cache) {
  if (pos < 0 || pos > text.size()) return false;

  auto it = cache.find(pos);
  if (it != cache.end()) return it.value();

  // 注意：这里简化了实现，实际应该传入 QTextDocument 来获取 block 信息
  // 为了解耦，我们使用简化的行级检测
  int lineStart = text.lastIndexOf(QLatin1Char('\n'), pos - 1) + 1;
  int lineEnd = text.indexOf(QLatin1Char('\n'), pos);
  if (lineEnd == -1) lineEnd = text.size();

  QString line = text.mid(lineStart, lineEnd - lineStart);
  int localPos = pos - lineStart;

  // 单行注释检测
  int commentPos = line.indexOf(QLatin1String("//"));
  if (commentPos != -1 && localPos >= commentPos) {
    for (int i = localPos; i < line.size(); ++i) {
      cache[lineStart + i] = true;
    }
    return true;
  }

  // 字符串检测（简化版）
  bool inSingleQuote = false;
  bool inDoubleQuote = false;
  bool inTemplateLiteral = false;

  for (int i = 0; i <= localPos && i < line.size(); ++i) {
    QChar ch = line[i];
    QChar prevCh = (i > 0) ? line[i - 1] : QChar();

    if (ch == QLatin1Char('\'') && !inDoubleQuote && !inTemplateLiteral &&
        prevCh != QLatin1Char('\\')) {
      inSingleQuote = !inSingleQuote;
    } else if (ch == QLatin1Char('"') && !inSingleQuote && !inTemplateLiteral &&
               prevCh != QLatin1Char('\\')) {
      inDoubleQuote = !inDoubleQuote;
    } else if (ch == QLatin1Char('`') && !inSingleQuote && !inDoubleQuote &&
               prevCh != QLatin1Char('\\')) {
      inTemplateLiteral = !inTemplateLiteral;
    }

    cache[lineStart + i] = (inSingleQuote || inDoubleQuote || inTemplateLiteral);
  }

  return (inSingleQuote || inDoubleQuote || inTemplateLiteral);
}

// ──────────────────────────────────────────────────────────────
//  核心匹配算法
// ──────────────────────────────────────────────────────────────

QChar BracketMatcher::getMatchingBracket(QChar bracket) {
  auto it = kBracketPairs.find(bracket);
  return (it != kBracketPairs.end()) ? it.value() : QChar::Null;
}

int BracketMatcher::findMatchingBracket(int pos, QChar bracket, QChar &matchBracket,
                                        const QString &text, QHash<int, bool> *cache) {
  matchBracket = getMatchingBracket(bracket);
  if (matchBracket.isNull()) return -1;

  // 创建临时缓存（如果未提供）
  QHash<int, bool> tempCache;
  QHash<int, bool> &cacheRef = cache ? *cache : tempCache;

  if (isInStringOrComment(pos - 1, text, cacheRef)) return -1;

  bool forward =
      (bracket == QLatin1Char('(') || bracket == QLatin1Char('[') || bracket == QLatin1Char('{'));

  int depth = 1;
  int searchStart = forward ? pos : pos - 2;
  int searchEnd = forward ? text.size() : -1;
  int step = forward ? 1 : -1;

  for (int i = searchStart; i != searchEnd; i += step) {
    if (isInStringOrComment(i, text, cacheRef)) continue;

    QChar currentChar = text[i];
    if (currentChar == bracket) {
      ++depth;
    } else if (currentChar == matchBracket) {
      --depth;
      if (depth == 0) return i;
    }
  }

  return -1;
}

// ──────────────────────────────────────────────────────────────
//  光标位置检测
// ──────────────────────────────────────────────────────────────

BracketMatcher::MatchResult BracketMatcher::findMatchAtCursor(int pos, const QString &text) {
  MatchResult result;

  // 检查光标前后2个位置是否有括号
  for (int offset = 0; offset <= 1; ++offset) {
    int checkPos = pos - 1 + offset;
    if (checkPos >= 0 && checkPos < text.size()) {
      QChar ch = text[checkPos];
      if (kBrackets.contains(ch)) {
        result.openChar = ch;
        QChar matchCh;
        int matchPos = findMatchingBracket(checkPos + 1, ch, matchCh, text);

        if (matchPos >= 0) {
          result.openPos = checkPos;
          result.closePos = matchPos;
        }
        break;
      }
    }
  }

  return result;
}

// ──────────────────────────────────────────────────────────────
//  智能代码块检测
// ──────────────────────────────────────────────────────────────

BracketMatcher::MatchResult BracketMatcher::findEnclosingBrackets(int pos, const QString &text,
                                                                  QHash<int, bool> *cache) {
  struct CandidatePair {
    int openPos;
    int closePos;
    QChar openChar;
    int distance;
  };

  QVector<CandidatePair> candidates;

  static const QPair<QChar, QChar> kBracketTypes[] = {{QLatin1Char('('), QLatin1Char(')')},
                                                      {QLatin1Char('['), QLatin1Char(']')},
                                                      {QLatin1Char('{'), QLatin1Char('}')}};

  // 创建缓存（如果未提供）
  QHash<int, bool> tempCache;
  QHash<int, bool> &cacheRef = cache ? *cache : tempCache;

  for (const auto &pair : kBracketTypes) {
    QChar openCh = pair.first;
    QChar closeCh = pair.second;

    QVector<int> openStack;
    int foundOpen = -1;
    int foundClose = -1;

    for (int i = 0; i < text.size(); ++i) {
      if (isInStringOrComment(i, text, cacheRef)) continue;

      QChar ch = text[i];
      if (ch == openCh) {
        openStack.append(i);
      } else if (ch == closeCh) {
        if (!openStack.isEmpty()) {
          int openPos = openStack.takeLast();
          int closePos = i;

          // 只记录包含光标的括号对
          if (openPos < pos && closePos >= pos) {
            foundOpen = openPos;
            foundClose = closePos;
            break;
          }
        }
      }
    }

    if (foundOpen >= 0 && foundClose >= 0) {
      CandidatePair candidate;
      candidate.openPos = foundOpen;
      candidate.closePos = foundClose;
      candidate.openChar = openCh;
      candidate.distance = (pos - foundOpen) + (foundClose - pos);
      candidates.append(candidate);
    }
  }

  // 选择距离最近的括号对
  MatchResult bestResult;
  if (!candidates.isEmpty()) {
    bestResult.openPos = candidates[0].openPos;
    bestResult.closePos = candidates[0].closePos;
    bestResult.openChar = candidates[0].openChar;
    for (int i = 1; i < candidates.size(); ++i) {
      if (candidates[i].distance < (bestResult.closePos - bestResult.openPos)) {
        bestResult.openPos = candidates[i].openPos;
        bestResult.closePos = candidates[i].closePos;
        bestResult.openChar = candidates[i].openChar;
      }
    }
  }

  return bestResult;
}