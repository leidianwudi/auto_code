/**
 * @file tpl_block.cpp
 * @brief 模板块处理器 — 共享辅助函数实现
 */

#include "tpl_block.h"

#include "../../ac_language.h"

/**
 * @brief 查找匹配的 ${else} 或 ${else if ...}（支持嵌套，仅返回当前 if 层的 else）
 *
 * 从 startPos 开始扫描 block，正确计数嵌套的 ${if ...} / ${/if}，
 * 仅在当前 if 层级（depth == 1）时返回 ${else} 或 ${else if} 的位置。
 * 如果遇到当前 if 的匹配 ${/if}（depth 归零），返回 -1 表示没有 else。
 */
int TplBlock::findElsePos(const QString &block, int startPos, bool *isElseIf) {
  if (isElseIf) *isElseIf = false;
  int depth = 1;
  int pos = startPos;
  QString openTag =
      QString::fromLatin1(AcTemplate::kExprOpen) + QString::fromLatin1(AcTemplate::kIfPrefix);
  QString closeTag = QString::fromLatin1(AcTemplate::kIfClose);
  QString elseTag = QString::fromLatin1(AcTemplate::kElse);  // "${else}"
  // ${else if condition}：注意 else 和 if 之间有空格
  // 拼接为 "${" + "else" + " " + "if " = "${else if "
  QString elseIfTag = QString::fromLatin1(AcTemplate::kExprOpen) +
                      QString::fromLatin1(AcKeyword::kElse) + QLatin1Char(' ') +
                      QString::fromLatin1(AcTemplate::kIfPrefix);
  int openLen = openTag.length();
  int closeLen = closeTag.length();
  int elseLen = elseTag.length();
  int elseIfLen = elseIfTag.length();

  while (pos < block.length()) {
    // 查找四种标签中最近的一个
    int nextOpen = block.indexOf(openTag, pos);
    int nextClose = block.indexOf(closeTag, pos);
    int nextElseIf = block.indexOf(elseIfTag, pos);
    int nextElse = block.indexOf(elseTag, pos);

    // 排除 ${else if} 被 ${else} 误匹配的情况
    // 如果 nextElse 和 nextElseIf 相同位置，说明是 ${else if}，应走 elseIf 分支
    if (nextElse != -1 && nextElseIf != -1 && nextElse == nextElseIf) {
      nextElse = -1;  // 让 elseIf 优先处理
    }
    // 如果 nextElse 紧跟在 nextElseIf 后面（else 的位置恰好在 else if 的 else 部分），
    // 也排除掉，避免把 ${else if ...} 中的 ${else} 单独匹配
    if (nextElse != -1 && nextElseIf != -1 && nextElse >= nextElseIf &&
        nextElse < nextElseIf + elseIfLen) {
      nextElse = -1;
    }

    // 取最近的一个
    int nearest = -1;
    if (nextOpen != -1) nearest = nextOpen;
    if (nextClose != -1 && (nearest == -1 || nextClose < nearest)) nearest = nextClose;
    if (nextElseIf != -1 && (nearest == -1 || nextElseIf < nearest)) nearest = nextElseIf;
    if (nextElse != -1 && (nearest == -1 || nextElse < nearest)) nearest = nextElse;

    if (nearest == -1) return -1;

    if (nearest == nextOpen) {
      ++depth;
      pos = nearest + openLen;
    } else if (nearest == nextClose) {
      --depth;
      if (depth == 0) return -1;
      pos = nearest + closeLen;
    } else if (nearest == nextElseIf) {
      if (depth == 1) {
        if (isElseIf) *isElseIf = true;
        return nearest;
      }
      pos = nearest + elseIfLen;
    } else {
      // ${else}（不带 if）
      if (depth == 1) {
        if (isElseIf) *isElseIf = false;
        return nearest;
      }
      pos = nearest + elseLen;
    }
  }

  return -1;
}

/**
 * @brief 查找匹配的闭合标签（支持嵌套）
 *
 * 从 startPos 开始扫描 block，计数嵌套的 openTag 和 closeTag，
 * 返回深度归零时的位置（即匹配的闭合标签位置）。
 */
int TplBlock::findMatchingClose(const QString &block, int startPos, const QString &openPrefix,
                                const QString &closeTag) {
  int depth = 1;
  int pos = startPos;
  int openLen = QString::fromLatin1(AcTemplate::kExprOpen).length() + openPrefix.length();
  int closeLen = closeTag.length();

  while (pos < block.length()) {
    // 查找嵌套的开放标签
    int nextOpen = block.indexOf(QString::fromLatin1(AcTemplate::kExprOpen) + openPrefix, pos);
    // 查找闭合标签
    int nextClose = block.indexOf(closeTag, pos);

    if (nextClose == -1) return -1;

    if (nextOpen != -1 && nextOpen < nextClose) {
      // 遇到嵌套的开放标签，深度+1
      ++depth;
      pos = nextOpen + openLen;
    } else {
      // 遇到闭合标签
      --depth;
      if (depth == 0) return nextClose;
      pos = nextClose + closeLen;
    }
  }

  return -1;
}