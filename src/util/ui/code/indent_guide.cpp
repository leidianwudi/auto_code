/**
 * @file indent_guide.cpp
 * @brief 缩进参考线模块实现（VS Code 风格）
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
//  computeIndentRanges 栈扫描算法
// ──────────────────────────────────────────────────────────────

void IndentGuide::compute(const QString &text, int revision, int tabWidth) {
  if (revision == m_cacheRevision) return;

  m_ranges.clear();

  QStringList lines = text.split(QLatin1Char('\n'));
  int lineCount = lines.size();

  struct StackEntry {
    int indent;
    int startLine;
  };
  QVector<StackEntry> stack;
  stack.reserve(32);

  for (int i = 0; i < lineCount; ++i) {
    const QString &line = lines[i];
    int indent = lineIndentLevel(line, tabWidth);

    // 空行或仅含空白行的缩进视为与上下文相同，跳过入栈/出栈
    if (line.trimmed().isEmpty()) continue;

    // 弹出栈中缩进 >= 当前的项，形成 IndentRange
    while (!stack.isEmpty() && stack.last().indent >= indent) {
      StackEntry top = stack.takeLast();
      if (top.startLine < i) {
        m_ranges.append({top.startLine + 1, i, top.indent});
      }
    }

    // 当前行缩进增加，入栈
    if (indent > 0 && (stack.isEmpty() || stack.last().indent < indent)) {
      stack.append({indent, i});
    }
  }

  // 处理栈中剩余项（到文件末尾）
  while (!stack.isEmpty()) {
    StackEntry top = stack.takeLast();
    if (top.startLine < lineCount - 1) {
      m_ranges.append({top.startLine + 1, lineCount, top.indent});
    }
  }

  m_cacheRevision = revision;
}