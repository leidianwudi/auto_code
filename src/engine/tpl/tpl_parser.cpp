/**
 * @file tpl_parser.cpp
 * @brief 模板语法分析器实现
 *
 * 递归下降解析器：parseNodes() 解析一组连续的节点，遇到闭合标签时返回。
 */

#include "tpl_parser.h"

#include "../ac_language.h"

namespace TplParser {

/// @brief 应用空行控制规则
///
/// 规则：当块标签独占一行时，剔除该标签所在行的"缩进 + 标签 + 换行符"整体，
/// 让标签行从输出中完全消失，不影响其他行。
///
/// 具体规则（参考 Jinja2/Mustache 的 whitespace control）：
///   1. 如果块标签 Token 标记了 aloneOnLine=true：
///      - 从前一个 Text Token 末尾剔除"当前标签行的缩进"（保留 \n）
///        即剔除最后一个 \n 之后的所有空白字符
///      - 从后一个 Text Token 开头剔除"当前标签行的换行符"
///        即跳过开头的 \n（如果有）
///   2. 如果块标签 Token 标记了 aloneOnLine=false：
///      - 不做任何剔除，保留所有空白字符
///
/// 这样做的效果（核心思路：标签行的 \n 归标签自己所有，前一行保留自己的 \n）：
///   模板：
///     line1
///     ${if x}
///     line2
///     ${/if}
///     line3
///   输出（x=true）：line1\nline2\nline3
///   - line1 后的 \n 保留（line1 独占一行的换行符）
///   - ${if x} 行的 \n 剔除（标签行的换行符）
///   - line2 后的 \n 保留
///   - ${/if} 行的 \n 剔除
///   - line3 保留
static void applyWhitespaceControl(QList<TplLexer::Token> &tokens) {
  for (int i = 0; i < tokens.size(); ++i) {
    TplLexer::Token &tok = tokens[i];
    if (!tok.aloneOnLine) continue;
    if (tok.type == TplLexer::TokenType::Variable) continue;  // 变量不做处理

    // 从前一个 Text Token 末尾剔除"标签所在行的缩进"（\n 之后的空白字符）
    // 保留 \n（\n 属于前一行）
    if (i > 0 && tokens[i - 1].type == TplLexer::TokenType::Text) {
      QString &prev = tokens[i - 1].value;
      // 找到最后一个 \n，剔除其后的所有空白字符
      int lastNl = prev.lastIndexOf(QChar('\n'));
      if (lastNl != -1) {
        // 检查 lastNl 之后到末尾是否全是空白
        bool allBlank = true;
        for (int j = lastNl + 1; j < prev.length(); ++j) {
          if (prev[j] != QChar(' ') && prev[j] != QChar('\t')) {
            allBlank = false;
            break;
          }
        }
        if (allBlank) {
          // 只剔除 \n 之后的空白（缩进），保留 \n
          prev = prev.left(lastNl + 1);
        }
      } else {
        // 整个 prev 都是空白（开头就是缩进），全部剔除
        bool allBlank = true;
        for (int j = 0; j < prev.length(); ++j) {
          if (prev[j] != QChar(' ') && prev[j] != QChar('\t')) {
            allBlank = false;
            break;
          }
        }
        if (allBlank) prev.clear();
      }
    }

    // 从后一个 Text Token 开头剔除"标签行的换行符"（第一个 \n）
    // 保留 \n 之后的内容（下一行的内容）
    if (i + 1 < tokens.size() && tokens[i + 1].type == TplLexer::TokenType::Text) {
      QString &next = tokens[i + 1].value;
      // 跳过开头的空白字符直到第一个 \n（包含）
      int p = 0;
      while (p < next.length() && (next[p] == QChar(' ') || next[p] == QChar('\t'))) {
        ++p;
      }
      if (p < next.length() && next[p] == QChar('\n')) {
        ++p;  // 跳过 \n
        next = next.mid(p);
      }
    }
  }
}

/// @brief 判断 Token 是否为闭合标签
static bool isCloseToken(const TplLexer::Token &tok) {
  return tok.type == TplLexer::TokenType::EndIf || tok.type == TplLexer::TokenType::EndEach;
}

/// @brief 判断 Token 是否为分支切换标签（${else} 或 ${else if}）
static bool isBranchToken(const TplLexer::Token &tok) {
  return tok.type == TplLexer::TokenType::Else || tok.type == TplLexer::TokenType::ElseIf;
}

/// @brief 判断 Token 是否为块标签的结束（闭合标签或分支切换标签）
static bool isTerminator(const TplLexer::Token &tok) {
  return isCloseToken(tok) || isBranchToken(tok);
}

/// @brief 递归解析节点列表
///
/// 从 tokens[start] 开始解析，遇到 terminator（${else}/${else if}/${/if}/${/each}）时停止。
/// @param[out] endPos 返回停止时的位置（terminator Token 的索引）
/// @param[out] errorMsg 错误信息
static QList<QSharedPointer<TplAst::AstNode>> parseNodes(const QList<TplLexer::Token> &tokens,
                                                         int &pos, QString &errorMsg) {
  QList<QSharedPointer<TplAst::AstNode>> nodes;

  while (pos < tokens.size()) {
    const TplLexer::Token &tok = tokens[pos];

    // 遇到 terminator（${else}/${else if}/${/if}/${/each}）→ 停止，返回当前节点列表
    if (isTerminator(tok)) {
      return nodes;
    }

    switch (tok.type) {
      case TplLexer::TokenType::Text:
      case TplLexer::TokenType::Comment: {
        // 文本和注释都作为文本节点（注释的 value 在渲染时被忽略）
        // 但注释 Token 本身不产生输出，所以这里跳过
        if (tok.type == TplLexer::TokenType::Text) {
          nodes.append(QSharedPointer<TplAst::AstNode>(new TplAst::TextNode(tok.value)));
        }
        // 注释 Token 不产生任何节点
        ++pos;
        break;
      }

      case TplLexer::TokenType::Variable: {
        nodes.append(QSharedPointer<TplAst::AstNode>(new TplAst::VariableNode(tok.value)));
        ++pos;
        break;
      }

      case TplLexer::TokenType::IfOpen: {
        // 解析 ${if cond}...${else}...${/if} 块
        int ifPos = pos + 1;  // 跳过 ${if}
        TplAst::IfNode *ifNode = new TplAst::IfNode();
        QSharedPointer<TplAst::AstNode> ifNodePtr(ifNode);

        // 解析所有分支
        int branchPos = ifPos;
        bool firstBranch = true;
        while (true) {
          // 解析当前分支体
          int bodyEnd = branchPos;
          QList<QSharedPointer<TplAst::AstNode>> body = parseNodes(tokens, bodyEnd, errorMsg);
          if (!errorMsg.isEmpty()) return {};

          // 添加分支
          TplAst::IfNode::Branch branch;
          branch.body = body;
          if (firstBranch) {
            // 第一个分支：${if condition}
            // tok.value = "if condition"，提取 condition
            branch.condition = tok.value.mid(3).trimmed();  // 去掉 "if "
            branch.isElse = false;
            firstBranch = false;
          } else {
            // 后续分支：${else if condition} 或 ${else}
            const TplLexer::Token &branchTok = tokens[bodyEnd];
            if (branchTok.type == TplLexer::TokenType::ElseIf) {
              // ${else if condition}：提取 condition
              // branchTok.value = "else if condition"
              branch.condition = branchTok.value.mid(8).trimmed();  // 去掉 "else if "
              branch.isElse = false;
            } else {
              // ${else}：无条件分支
              branch.condition.clear();
              branch.isElse = true;
            }
          }
          ifNode->branches.append(branch);

          // 检查停止原因
          if (bodyEnd >= tokens.size()) {
            errorMsg = QStringLiteral("Unclosed ${if}");
            return {};
          }
          const TplLexer::Token &stopTok = tokens[bodyEnd];
          if (stopTok.type == TplLexer::TokenType::EndIf) {
            // ${/if} → 块结束
            pos = bodyEnd + 1;  // 跳过 ${/if}
            break;
          } else if (stopTok.type == TplLexer::TokenType::ElseIf) {
            // ${else if} → 继续解析下一个分支
            branchPos = bodyEnd + 1;  // 跳过 ${else if}
            continue;
          } else if (stopTok.type == TplLexer::TokenType::Else) {
            // ${else} → 继续解析最后一个分支
            branchPos = bodyEnd + 1;  // 跳过 ${else}
            continue;
          } else {
            // 其他情况（如 ${/each}）→ 嵌套错误
            errorMsg = QStringLiteral("Unexpected %1 at position %2 in ${if} block")
                           .arg(stopTok.value)
                           .arg(stopTok.pos);
            return {};
          }
        }

        nodes.append(ifNodePtr);
        break;
      }

      case TplLexer::TokenType::EachOpen: {
        // 解析 ${each item in items}...${/each} 块
        int eachPos = pos + 1;  // 跳过 ${each}
        TplAst::EachNode *eachNode = new TplAst::EachNode();
        QSharedPointer<TplAst::AstNode> eachNodePtr(eachNode);

        // 解析循环表达式
        // tok.value = "each item in items" 或 "each items"
        QString loopExpr = tok.value.mid(5).trimmed();  // 去掉 "each "
        int inPos = loopExpr.indexOf(QString::fromLatin1(AcTemplate::kEachIn));
        if (inPos != -1) {
          // 显式命名：each item in items
          eachNode->itemName = loopExpr.left(inPos).trimmed();
          eachNode->arrayName =
              loopExpr.mid(inPos + QString::fromLatin1(AcTemplate::kEachIn).length()).trimmed();
          eachNode->explicitNaming = true;
        } else {
          // 隐式命名：each items
          eachNode->itemName = loopExpr;
          eachNode->arrayName = loopExpr;
          eachNode->explicitNaming = false;
        }

        if (eachNode->itemName.isEmpty() || eachNode->arrayName.isEmpty()) {
          errorMsg = QStringLiteral("Invalid each syntax: %1").arg(tok.value);
          return {};
        }

        // 解析循环体
        int bodyEnd = eachPos;
        QList<QSharedPointer<TplAst::AstNode>> body = parseNodes(tokens, bodyEnd, errorMsg);
        if (!errorMsg.isEmpty()) return {};

        // 检查是否遇到 ${/each}
        if (bodyEnd >= tokens.size() || tokens[bodyEnd].type != TplLexer::TokenType::EndEach) {
          errorMsg = QStringLiteral("Unclosed ${each %1}").arg(loopExpr);
          return {};
        }
        eachNode->body = body;
        pos = bodyEnd + 1;  // 跳过 ${/each}
        nodes.append(eachNodePtr);
        break;
      }

      default:
        // 其他 Token（${else}/${else if}/${/if}/${/each}）不应该出现在这里
        // 理论上 isTerminator 已经处理，但保险起见报错
        errorMsg = QStringLiteral("Unexpected %1 at position %2").arg(tok.value).arg(tok.pos);
        return {};
    }
  }

  return nodes;
}

QList<QSharedPointer<TplAst::AstNode>> parse(const QList<TplLexer::Token> &tokens,
                                             QString &errorMsg) {
  errorMsg.clear();

  // 先应用空行控制规则（修改 Token 副本，不影响原始 Token）
  QList<TplLexer::Token> mutableTokens = tokens;
  applyWhitespaceControl(mutableTokens);

  // 递归下降解析
  int pos = 0;
  QList<QSharedPointer<TplAst::AstNode>> nodes = parseNodes(mutableTokens, pos, errorMsg);

  // 检查是否所有 Token 都被消费
  if (errorMsg.isEmpty() && pos < tokens.size()) {
    const TplLexer::Token &tok = tokens[pos];
    errorMsg = QStringLiteral("Unexpected %1 at position %2").arg(tok.value).arg(tok.pos);
    return {};
  }

  return nodes;
}

}  // namespace TplParser
