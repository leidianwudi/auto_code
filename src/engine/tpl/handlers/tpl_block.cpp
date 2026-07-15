/**
 * @file tpl_block.cpp
 * @brief 模板块处理器 — 共享辅助函数实现
 */

#include "tpl_block.h"

#include "../../ac_language.h"

/**
 * @brief 查找匹配的 ${else}（支持嵌套，仅返回当前 if 层的 else）
 *
 * 从 startPos 开始扫描 block，正确计数嵌套的 ${if ...} / ${/if}，
 * 仅在当前 if 层级（depth == 1）时返回 ${else} 的位置。
 * 如果遇到当前 if 的匹配 ${/if}（depth 归零），返回 -1 表示没有 else。
 */
int TplBlock::findElsePos(const QString &block, int startPos) {
  int depth = 1;
  int pos = startPos;
  QString openTag =
      QString::fromLatin1(AcTemplate::kExprOpen) + QString::fromLatin1(AcTemplate::kIfPrefix);
  QString closeTag = QString::fromLatin1(AcTemplate::kIfClose);
  QString elseTag = QString::fromLatin1(AcTemplate::kElse);
  int openLen = openTag.length();
  int closeLen = closeTag.length();
  int elseLen = elseTag.length();

  while (pos < block.length()) {
    // 查找三种标签中最近的一个
    int nextOpen = block.indexOf(openTag, pos);
    int nextClose = block.indexOf(closeTag, pos);
    int nextElse = block.indexOf(elseTag, pos);

    // 取最近的一个
    int nearest = -1;
    if (nextOpen != -1) nearest = nextOpen;
    if (nextClose != -1 && (nearest == -1 || nextClose < nearest)) nearest = nextClose;
    if (nextElse != -1 && (nearest == -1 || nextElse < nearest)) nearest = nextElse;

    if (nearest == -1) return -1;  // 什么都没找到

    if (nearest == nextOpen) {
      // 嵌套的 ${if ...}，深度+1
      ++depth;
      pos = nearest + openLen;
    } else if (nearest == nextClose) {
      // ${/if}
      --depth;
      if (depth == 0) return -1;  // 当前 if 层闭合，没有 else
      pos = nearest + closeLen;
    } else {
      // ${else}
      if (depth == 1) return nearest;  // 当前 if 层的 else
      // 嵌套 if 内部的 else，忽略
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