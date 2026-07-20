/**
 * @file tpl_parser.h
 * @brief 模板语法分析器
 *
 * 把 Lexer 产生的 Token 流组装成 AST 树。
 *
 * Parser 的核心职责：
 *   1. 处理块标签的嵌套关系（${if}...${else}...${/if}，${each}...${/each}）
 *   2. 应用"独占一行"的空行控制规则
 *   3. 把 Token 转换为 AST 节点（TextNode/VariableNode/IfNode/EachNode）
 *
 * 空行控制规则（在 Parser 中应用，而非 Lexer）：
 *   - 当块标签独占一行时，从相邻 Text Token 中剔除该标签行的缩进和换行符
 *   - 具体规则见 applyWhitespaceControl() 函数注释
 */

#pragma once

#include <QList>
#include <QSharedPointer>

#include "tpl_ast.h"
#include "tpl_lexer.h"

namespace TplParser {

/// @brief 解析 Token 流，构造 AST 树
///
/// @param tokens Lexer 产生的 Token 列表
/// @param[out] errorMsg 错误信息（无错误时为空）
/// @return AST 根节点列表，失败时返回空列表并设置 errorMsg
QList<QSharedPointer<TplAst::AstNode>> parse(const QList<TplLexer::Token> &tokens,
                                              QString &errorMsg);

}  // namespace TplParser
