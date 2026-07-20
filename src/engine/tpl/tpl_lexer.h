/**
 * @file tpl_lexer.h
 * @brief 模板词法分析器
 *
 * 把模板字符串切成 Token 流，供 Parser 构造 AST。
 *
 * Token 类型：
 *   - Text     : 纯文本（${...} 之外的所有字符）
 *   - Variable : ${expression} — 变量引用/算术表达式/函数调用
 *   - IfOpen   : ${if condition}
 *   - EachOpen : ${each item in items}
 *   - ElseIf   : ${else if condition}
 *   - Else     : ${else}
 *   - EndIf    : ${/if}
 *   - EndEach  : ${/each}
 *   - Comment  : ${# 注释内容} — 渲染时整段跳过
 *
 * 空行控制核心规则：
 *   当块标签（IfOpen/EachOpen/Else/ElseIf/EndIf/EndEach/Comment）独占一行时，
 *   Lexer 会标记该 Token 为 "aloneOnLine"，并把它所在行的缩进和换行符
 *   一起从前后 Text Token 中剔除。这样：
 *     - 块标签行的换行符不会出现在输出中
 *     - 块标签前后的空行原样保留（模板里几个空行就几个空行）
 *
 *   块标签行内出现时（如 "prefix ${if x} suffix"），不做任何剔除。
 */

#pragma once

#include <QString>
#include <QList>

namespace TplLexer {

/// @brief Token 类型枚举
enum class TokenType {
  Text,       ///< 纯文本
  Variable,   ///< ${expression}
  IfOpen,     ///< ${if condition}
  EachOpen,   ///< ${each item in items}
  ElseIf,     ///< ${else if condition}
  Else,       ///< ${else}
  EndIf,      ///< ${/if}
  EndEach,    ///< ${/each}
  Comment,    ///< ${# 注释内容}
};

/// @brief 词法 Token
struct Token {
  TokenType type;       ///< Token 类型
  QString value;        ///< Token 内容
                             ///< - Text: 原始文本
                             ///< - Variable: 表达式（已 trim）
                             ///< - IfOpen/EachOpen/ElseIf: 条件/循环表达式（已 trim）
                             ///< - Comment: 注释内容
  int pos;                   ///< 在原始模板中的起始位置（用于错误报告）
  bool aloneOnLine;          ///< true=该标签独占一行（行内只有空白和该标签）

  Token(TokenType t, QString v, int p, bool alone = false)
      : type(t), value(std::move(v)), pos(p), aloneOnLine(alone) {}
};

/// @brief 词法分析：把模板字符串切成 Token 流
///
/// @param tmpl 模板字符串（已去除 \r）
/// @param[out] errorMsg 错误信息（无错误时为空）
/// @return Token 列表，失败时返回空列表并设置 errorMsg
QList<Token> tokenize(const QString &tmpl, QString &errorMsg);

}  // namespace TplLexer
