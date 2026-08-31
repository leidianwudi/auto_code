/**
 * @file comment_scan.h
 * @brief 代码区域扫描工具（查找引用 / 符号高亮共用）
 *
 * 提供 collectNonCodeRanges() 与 posInComments()：
 * - collectNonCodeRanges：收集所有"非代码"区域（字符串字面量 + 注释）
 * - posInComments：判断某偏移是否落在任一区域内
 * - VSCode 规则：注释/字符串里的标识符不算真实引用，查找引用时应跳过
 */

#pragma once

#include <QPair>
#include <QRegularExpression>
#include <QString>
#include <QVector>

/// 收集文本中所有"非代码"区域 [起始偏移, 长度)（字符串字面量 + 注释）。
/// 查找引用时应跳过这些区域——VSCode 规则：注释/字符串里的标识符不算真实引用。
/// 支持：字符串 "..."、'...'、`...`（含转义）；行注释 //；块注释 /* */；HTML 注释 <!-- -->。
inline QVector<QPair<int, int>> collectNonCodeRanges(const QString &text) {
  QVector<QPair<int, int>> ranges;
  const int n = text.size();
  int i = 0;
  bool inBlock = false;  // 是否处于块注释 /* */ 中
  bool inHtml = false;   // 是否处于 HTML 注释 <!-- --> 中
  int regionStart = 0;
  while (i < n) {
    if (inBlock) {
      const int close = text.indexOf(QStringLiteral("*/"), i);
      if (close < 0) {
        ranges.append(qMakePair(regionStart, n - regionStart));
        break;
      }
      ranges.append(qMakePair(regionStart, close + 2 - regionStart));
      i = close + 2;
      inBlock = false;
      continue;
    }
    if (inHtml) {
      const int close = text.indexOf(QStringLiteral("-->"), i);
      if (close < 0) {
        ranges.append(qMakePair(regionStart, n - regionStart));
        break;
      }
      ranges.append(qMakePair(regionStart, close + 3 - regionStart));
      i = close + 3;
      inHtml = false;
      continue;
    }
    const QChar c = text.at(i);
    if (c == QLatin1Char('"') || c == QLatin1Char('\'') || c == QLatin1Char('`')) {
      const QChar quote = c;
      int j = i + 1;
      if (quote == QLatin1Char('`')) {
        // 模板字符串（反引号）：${...} 插值是代码而非字符串，其中的标识符算真实引用，
        // 只把「插值之外」的字面量文本记为非代码区域。对照 VSCode，反引号内的标识符
        // 若不在 ${} 里仍是字符串内容、应跳过；在 ${} 插值里则是表达式、应计数。
        int segStart = i;  // 当前字符串字面量段起点
        while (j < n) {
          const QChar ch = text.at(j);
          if (ch == QLatin1Char('\\')) {
            j += 2;
            continue;
          }
          if (ch == QLatin1Char('`')) {
            ++j;
            break;
          }
          if (ch == QLatin1Char('$') && j + 1 < n && text.at(j + 1) == QLatin1Char('{')) {
            // ${ 之前累积的纯字面量段记为非代码
            if (segStart < j) ranges.append(qMakePair(segStart, j - segStart));
            // 整体跳过 ${ ... } 插值（按代码处理）：处理嵌套 {} 与其内部字符串/转义
            j += 2;
            int depth = 1;
            while (j < n && depth > 0) {
              const QChar ic = text.at(j);
              if (ic == QLatin1Char('\\')) {
                j += 2;
                continue;
              }
              if (ic == QLatin1Char('{')) {
                ++depth;
              } else if (ic == QLatin1Char('}')) {
                --depth;
              } else if (ic == QLatin1Char('"') || ic == QLatin1Char('\'') ||
                         ic == QLatin1Char('`')) {
                // 跳过插值内的字符串字面量
                const QChar iq = ic;
                ++j;
                while (j < n) {
                  if (text.at(j) == QLatin1Char('\\')) {
                    j += 2;
                    continue;
                  }
                  if (text.at(j) == iq) {
                    ++j;
                    break;
                  }
                  ++j;
                }
                continue;
              }
              ++j;
            }
            segStart = j;  // 插值结束位置继续累积字面量
            continue;
          }
          ++j;
        }
        if (segStart < j) ranges.append(qMakePair(segStart, j - segStart));
        i = j;
      } else {
        // 普通字符串 "..." / '...'：跳过引号内内容（含转义），整体记为一个非代码区域
        while (j < n) {
          if (text.at(j) == QLatin1Char('\\')) {
            j += 2;
            continue;
          }
          if (text.at(j) == quote) {
            ++j;
            break;
          }
          ++j;
        }
        ranges.append(qMakePair(i, j - i));
        i = j;
      }
    } else if (text.mid(i, 2) == QStringLiteral("/*")) {
      inBlock = true;
      regionStart = i;
      i += 2;
    } else if (text.mid(i, 2) == QStringLiteral("//")) {
      const int end = text.indexOf(QLatin1Char('\n'), i);
      ranges.append(qMakePair(i, (end < 0 ? n : end) - i));
      i = (end < 0 ? n : end);
    } else if (text.mid(i, 4) == QStringLiteral("<!--")) {
      inHtml = true;
      regionStart = i;
      i += 4;
    } else {
      ++i;
    }
  }
  return ranges;
}

/// 判断字符偏移位置是否位于任一注释区域内
inline bool posInComments(const QVector<QPair<int, int>> &ranges, int pos) {
  for (const auto &r : ranges) {
    if (pos >= r.first && pos < r.first + r.second) return true;
  }
  return false;
}

/// 查找文本中标识符 name 的所有出现位置（跳过注释/字符串中的出现，VSCode 规则）。
/// 返回 (起始偏移, 长度) 列表；name 为空或无命中时返回空列表。
/// 供引用扫描（findSymbolReferences / buildReferenceHighlights）与引用面板共用，
/// 消除三处重复的「\bword\b 正则 + collectNonCodeRanges + posInComments」扫描逻辑。
inline QVector<QPair<int, int>> findIdentifierRanges(const QString &text, const QString &name) {
  QVector<QPair<int, int>> hits;
  if (name.isEmpty()) return hits;

  const QVector<QPair<int, int>> nonCode = collectNonCodeRanges(text);
  QRegularExpression re(QStringLiteral("\\b") + QRegularExpression::escape(name) +
                        QStringLiteral("\\b"));
  int offset = 0;
  while (offset < text.size()) {
    auto match = re.match(text, offset);
    if (!match.hasMatch()) break;
    const int start = match.capturedStart();
    const int length = match.capturedLength();
    offset = start + length;
    if (posInComments(nonCode, start)) continue;
    hits.append(qMakePair(start, length));
  }
  return hits;
}

/// 将文本内的字符偏移换算为行号（1-based）与整行文本（不含换行）。
/// 供引用扫描把 findIdentifierRanges 的偏移结果转为"行号 + 行内容"。
inline QPair<int, QString> rangeToLineInfo(const QString &text, int pos) {
  const int line = text.left(pos).count(QLatin1Char('\n')) + 1;
  const int lineStart = text.lastIndexOf(QLatin1Char('\n'), pos) + 1;
  int lineEnd = text.indexOf(QLatin1Char('\n'), pos);
  if (lineEnd < 0) lineEnd = text.size();
  return qMakePair(line, text.mid(lineStart, lineEnd - lineStart).trimmed());
}
