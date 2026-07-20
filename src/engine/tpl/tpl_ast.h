/**
 * @file tpl_ast.h
 * @brief 模板 AST 节点定义
 *
 * 重构后的模板引擎采用三阶段处理：
 *   1. Lexer  (tpl_lexer)   — 把模板字符串切成 Token 流
 *   2. Parser (tpl_parser)  — 把 Token 流组装成 AST 树
 *   3. Renderer            — 遍历 AST 树，输出最终字符串
 *
 * AST 节点设计原则：
 *   - 所有节点继承自 AstNode，统一基类
 *   - 使用智能指针管理生命周期
 *   - 节点只保存结构信息，不保存渲染上下文
 *
 * 空行控制规则（明确且简单）：
 *   1. 块标签（if/each/else 等）独占一行时，Lexer 会把该行作为一个整体 Token
 *      整行（含缩进和换行符）从输出中剔除，不影响周围空行
 *   2. 块标签行内出现时（如 ${if x}A${/if}），保留所有空白字符
 *   3. 不做任何"智能空行压缩"，模板里几个 \n 就输出几个 \n
 */

#pragma once

#include <QList>
#include <QSharedPointer>
#include <QString>

namespace TplAst {

/// @brief AST 节点类型枚举
enum class NodeType {
  Text,       ///< 纯文本（原样输出）
  Variable,   ///< 变量引用 ${var} 或 ${obj.prop}
  If,         ///< 条件块 ${if cond}...${else}...${/if}
  Each,       ///< 循环块 ${each item in items}...${/each}
};

/// @brief AST 节点基类
struct AstNode {
  NodeType type;  ///< 节点类型

  explicit AstNode(NodeType t) : type(t) {}
  virtual ~AstNode() = default;
};

/// @brief 纯文本节点
///
/// 保存原始文本内容，渲染时原样输出（不做任何变量替换）。
struct TextNode : public AstNode {
  QString text;  ///< 文本内容

  explicit TextNode(const QString &t) : AstNode(NodeType::Text), text(t) {}
};

/// @brief 变量节点
///
/// ${expression} 形式的占位符，expression 可以是：
///   - 变量路径：user.name
///   - 算术表达式：a + b * c
///   - 函数调用：str.toLowerCase(x)
///   - 内置函数：printLog(text)、fileExists(path)
struct VariableNode : public AstNode {
  QString expr;  ///< 表达式内容（${...} 之间的部分，已 trim）

  explicit VariableNode(const QString &e) : AstNode(NodeType::Variable), expr(e) {}
};

/// @brief 条件分支节点
///
/// 对应模板：
///   ${if cond}
///     ...thenBranch...
///   ${else if cond2}
///     ...elseIfBranch...
///   ${else}
///     ...elseBranch...
///   ${/if}
///
/// 设计：把 else if 链展平为 branches 数组，每个分支有 condition 和 body。
/// 第一个分支的 condition 是 ${if} 的条件；
/// 后续分支的 condition 是 ${else if} 的条件；
/// 最后一个分支如果没有 condition，则表示 ${else} 分支。
struct IfNode : public AstNode {
  /// @brief 单个条件分支
  struct Branch {
    QString condition;                  ///< 条件表达式（空字符串表示 ${else} 无条件）
    bool isElse;                        ///< true 表示这是 ${else} 分支（无条件）
    QList<QSharedPointer<AstNode>> body;  ///< 分支体（AST 子节点列表）

    Branch() : isElse(false) {}
  };

  QList<Branch> branches;  ///< 所有分支（按模板顺序）

  IfNode() : AstNode(NodeType::If) {}
};

/// @brief 循环节点
///
/// 对应模板：
///   ${each item in items}
///     ...body...
///   ${/each}
///
/// 支持两种语法：
///   1. 显式命名：${each user in users} — body 中用 ${user.name} 引用
///   2. 隐式命名：${each users}        — body 中用 ${name} 或 ${.} 引用
struct EachNode : public AstNode {
  QString itemName;                       ///< 循环变量名（如 "user"）
  QString arrayName;                      ///< 数组变量名（如 "users"）
  bool explicitNaming;                    ///< true=显式命名(item in items)，false=隐式命名
  QList<QSharedPointer<AstNode>> body;    ///< 循环体（AST 子节点列表）

  EachNode() : AstNode(NodeType::Each), explicitNaming(false) {}
};

}  // namespace TplAst
