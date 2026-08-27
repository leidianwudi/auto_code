/**
 * @file comment_scan.h
 * @brief 注释区域扫描工具（查找引用 / 符号高亮共用）
 *
 * 提供 collectCommentRanges() 与 posInComments()：
 * - 收集文本中所有注释区域 [起始偏移, 长度)，含 // 单行注释与块注释
 * - 会跳过字符串（"..." 与模板字符串 `...`）内的内容，避免把字符串里的 // 误判为注释
 * - VSCode 规则：被注释的标识符不算引用
 */

#pragma once

#include <QPair>
#include <QString>
#include <QVector>

/// 收集文本中所有注释区域 [起始偏移, 长度)
inline QVector<QPair<int, int>> collectCommentRanges(const QString &text) {
  QVector<QPair<int, int>> ranges;
  const int n = text.size();
  int i = 0;
  bool inBlock = false;
  int blockStart = 0;
  while (i < n) {
    if (!inBlock) {
      const QChar c = text.at(i);
      if (c == QLatin1Char('"')) {
        int j = i + 1;
        while (j < n) {
          if (text.at(j) == QLatin1Char('\\')) {
            j += 2;
            continue;
          }
          if (text.at(j) == QLatin1Char('"')) {
            ++j;
            break;
          }
          ++j;
        }
        i = j;
      } else if (c == QLatin1Char('`')) {
        int j = i + 1;
        while (j < n) {
          if (text.at(j) == QLatin1Char('\\')) {
            j += 2;
            continue;
          }
          if (text.at(j) == QLatin1Char('`')) {
            ++j;
            break;
          }
          ++j;
        }
        i = j;
      } else if (text.mid(i, 2) == QStringLiteral("/*")) {
        inBlock = true;
        blockStart = i;
        i += 2;
      } else if (text.mid(i, 2) == QStringLiteral("//")) {
        int end = text.indexOf(QLatin1Char('\n'), i);
        if (end < 0) end = n;
        ranges.append(qMakePair(i, end - i));
        i = end;
      } else {
        ++i;
      }
    } else {
      const int close = text.indexOf(QStringLiteral("*/"), i);
      if (close < 0) {
        ranges.append(qMakePair(blockStart, n - blockStart));
        break;
      }
      ranges.append(qMakePair(blockStart, close + 2 - blockStart));
      i = close + 2;
      inBlock = false;
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
