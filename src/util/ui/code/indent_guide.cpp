/**
 * @file indent_guide.cpp
 * @brief 缩进参考线模块实现（VS Code 逐行判定方案）
 */

#include "indent_guide.h"

// ──────────────────────────────────────────────────────────────
//  缩进层级计算
// ──────────────────────────────────────────────────────────────

int IndentGuide::lineIndentLevel(const QString &line, int tabWidth) {
  int indent = 0;
  for (int i = 0; i < line.size(); ++i) {
    QChar ch = line[i];
    if (ch == QLatin1Char(' ')) {
      ++indent;
    } else if (ch == QLatin1Char('\t')) {
      indent += tabWidth;
    } else {
      break;
    }
  }
  return indent;
}

// ──────────────────────────────────────────────────────────────
//  缩进粒度检测
// ──────────────────────────────────────────────────────────────

int IndentGuide::detectGranularity(const QTextDocument *doc, int tabWidth, int fallback) {
  int prev = -1;
  int best = -1;
  for (QTextBlock b = doc->firstBlock(); b.isValid(); b = b.next()) {
    const QString t = b.text();
    if (t.trimmed().isEmpty()) continue;  // 低频路径（防抖后调用），trimmed 拷贝可接受
    const int ind = lineIndentLevel(t, tabWidth);
    if (prev >= 0 && ind != prev) {
      const int d = qAbs(ind - prev);
      if (best < 0 || d < best) best = d;
      if (best == 1) break;  // 最小可能值，提前结束
    }
    prev = ind;
  }
  return best > 0 ? best : fallback;
}

// ──────────────────────────────────────────────────────────────
//  单行 guide 列集合
// ──────────────────────────────────────────────────────────────

QVector<int> IndentGuide::guideColumnsForLine(const QTextBlock &blk, int granularity,
                                              int tabWidth) {
  QVector<int> cols;
  if (granularity <= 0 || !blk.isValid()) return cols;

  const QString text = blk.text();
  // 空白行判定：所有字符均为空白（手写小循环避免 trimmed() 整行拷贝；
  // 不可用 lineIndentLevel >= length 判定——tab 折算宽大于字符数会误判）
  bool blank = true;
  for (int i = 0; i < text.size(); ++i) {
    const QChar ch = text[i];
    if (ch != QLatin1Char(' ') && ch != QLatin1Char('\t')) {
      blank = false;
      break;
    }
  }
  if (!blank) {
    // 非空行：{0, gran, 2×gran, ... < 行缩进}——guide 在列 C 显示 ⟺ 行缩进 > C
    // （VS Code 语义：最深一条对齐父级内容列，比本行内容浅一级）
    const int ind = lineIndentLevel(text, tabWidth);
    for (int c = 0; c < ind; c += granularity) {
      cols.append(c);
    }
    return cols;
  }

  // 空白行：继承前后最近非空行缩进的较大值（只要任一侧层级覆盖该列，线就穿过
  // 空行，保证块内空行处竖线连续；扫描设上限防退化）
  int before = -1;
  int after = -1;
  int walked = 0;
  for (QTextBlock b = blk.previous(); b.isValid() && walked < 500; b = b.previous(), ++walked) {
    if (!b.text().trimmed().isEmpty()) {
      before = lineIndentLevel(b.text(), tabWidth);
      break;
    }
  }
  walked = 0;
  for (QTextBlock b = blk.next(); b.isValid() && walked < 500; b = b.next(), ++walked) {
    if (!b.text().trimmed().isEmpty()) {
      after = lineIndentLevel(b.text(), tabWidth);
      break;
    }
  }
  int ind = qMax(before, after);
  if (ind < 0) return cols;
  for (int c = 0; c < ind; c += granularity) {
    cols.append(c);
  }
  return cols;
}