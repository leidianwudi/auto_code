/**
 * @file ac_parser.h
 * @brief 语法分析器 — 将 Token 序列解析为 AST
 */

#pragma once

#include <utility>
#include <vector>

#include "ac_lexer.h"
#include "ac_type.h"

/// @brief 语法分析器 — 将 Token 序列解析为 AST
class AcParser {
public:
  /// @brief 解析入口：将 tokens 解析为 AST（跳过可选的 main 关键字）
  /// @param tokens 词法分析结果
  /// @param[out] program 输出的 AST 根节点
  /// @param[in,out] declaredVars 已声明的变量名集合（解析过程中会新增）
  /// @return true 解析成功，false 解析失败（调用 error() 获取错误信息）
  bool parse(const QVector<Token> &tokens, Block &program, QSet<QString> &declaredVars);

  /// @brief 获取错误信息
  QString error() const { return m_error; }

private:
  // ── token 操作 ──
  Token peek();
  Token peek(int offset);
  Token advance();
  bool match(TokenType t);
  bool expect(TokenType t, const QString &msg);
  bool isPropertyName(TokenType t) const;

  // ── 二元运算解析辅助（模板化，消除重复代码） ──
  using ParseNextFn = bool (AcParser::*)(Expr &);
  template <ParseNextFn parseNext>
  bool parseBinary(Expr &expr, const std::vector<std::pair<TokenType, Expr::BinaryOp>> &ops);

  // ── 语法分析（递归下降） ──
  bool parseProgram(Block &block);
  bool parseBlock(Block &block);
  bool parseBlockOrStmt(Block &block);
  bool parseStmt(Block::Stmt &stmt);
  /// @brief 解析以标识符开头的语句（赋值/属性赋值/方法调用/索引赋值/表达式）
  bool parseIdentStmt(Block::Stmt &stmt, const Token &t);
  /// @brief 解析静态属性赋值 (ClassName::member = value)，成功时返回 true
  bool tryParseStaticAssign(Block::Stmt &stmt);
  /// @brief 解析以 this 开头的语句（属性赋值/表达式）
  bool parseThisStmt(Block::Stmt &stmt);
  bool parseCallStmt(CallStmt &cs);
  bool parseAssignStmt(AssignStmt &as);
  bool parseIndexAssignStmt(IndexAssignStmt &ias);
  bool parseForStmt(ForStmt &fs);
  bool parseIfStmt(IfStmt &is);
  bool parseWhileStmt(WhileStmt &ws);
  bool parseSwitchStmt(SwitchStmt &ss);
  bool parseImportStmt(ImportStmt &imp);
  bool parseClassDef(ClassDef &cd);
  bool parseInterfaceDef(InterfaceDef &iface);
  bool parseEnumDef(EnumDef &ed);
  bool parseReturnStmt(Expr &retVal);
  bool parseExpr(Expr &expr);
  bool parseTernary(Expr &expr);
  bool parseLogicalOr(Expr &expr);
  bool parseLogicalAnd(Expr &expr);
  bool parseComparison(Expr &expr);
  bool parseAddSub(Expr &expr);
  bool parseMulDiv(Expr &expr);
  bool parseUnary(Expr &expr);
  bool parsePostfix(Expr &expr);
  bool parsePrimary(Expr &expr);
  bool parseObject(Expr &expr);
  bool parseArray(Expr &expr);
  bool parseFuncCall(const QString &name, Expr &expr);
  bool parseTemplateString(Expr &expr);
  bool parseMethodDef(MethodDef &md);
  /// @brief 解析类属性（static let / let / ident = value），返回是否成功
  bool parseClassProperty(ClassDef &cd, AccessLevel access, bool isStatic);

  // ── 复合赋值运算符解析 ──
  /// @brief 检查当前 token 是否为复合赋值运算符，如果是则消耗并返回对应枚举值
  CompoundOp parseCompoundOp();

  // ── 类型解析 ──
  /// @brief 解析类型注解（: TypeExpr），返回类型结构
  AcType parseType();
  /// @brief 将类型名称解析为 AcType（内建类型 / 自定义类名）
  AcType resolveTypeName(const QString &name);
  /// @brief 解析可选的 : Type 类型注解，返回是否解析成功
  /// @param outType 输出解析到的类型（如果有注解）
  /// @return true 表示有类型注解并已解析，false 表示无注解
  bool parseTypeAnnotation(AcType &outType);

  // ── 内部状态 ──
  int m_pos = 0;
  QVector<Token> m_tokens;
  QString m_error;
  QSet<QString> *m_declaredVars = nullptr;
};