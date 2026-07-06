/**
 * @file tpl_block.cpp
 * @brief 模板块处理器 — 共享辅助函数实现
 */

#include "tpl_block.h"

#include "../../ac_language.h"

/**
 * @brief 查找匹配的闭合标签（支持嵌套）
 *
 * 从 startPos 开始扫描 block，计数嵌套的 openTag 和 closeTag，
 * 返回深度归零时的位置（即匹配的闭合标签位置）。
 */
int TplBlock::findMatchingClose(const QString &block, int startPos,
                                const QString &openPrefix,
                                const QString &closeTag) {
  int depth = 1;
  int pos = startPos;
  int openLen = 2 + openPrefix.length(); // "${" + prefix
  int closeLen = closeTag.length();

  while (pos < block.length()) {
    // 查找嵌套的开放标签
    int nextOpen = block.indexOf(QString::fromLatin1(AcTpl::kExprOpen) + openPrefix, pos);
    // 查找闭合标签
    int nextClose = block.indexOf(closeTag, pos);

    if (nextClose == -1)
      return -1;

    if (nextOpen != -1 && nextOpen < nextClose) {
      // 遇到嵌套的开放标签，深度+1
      ++depth;
      pos = nextOpen + openLen;
    } else {
      // 遇到闭合标签
      --depth;
      if (depth == 0)
        return nextClose;
      pos = nextClose + closeLen;
    }
  }

  return -1;
}