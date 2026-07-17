/**
 * @file ac_parser.h
 * @brief 语法分析器 — 将 Token 序列解析为 AST
 */

#pragma once

#include "ac_lexer.h"
#include "ac_type.h"

/// @brief 语法分析器 — 将 Token 序列解析为 AST
class AcParser {
public:
  /// @brief 解析入口：将 tokens 解析为 AST（跳过可选的 main 关键字）
  /// @param tokens 词法分析结果
  /// @param[out] program 输出的 AST 根节点
  /// @param[out] error 错误信息
  /// @param[in,out] declaredVars 已声明的变量名集合（解析过程中会新增）
  bool parse(const QVector<Token> &tokens, Block &program, QString &error,
             QSet<QString> &declaredVars);

private:
  // ── token 操作 ──
  Token peek();
  Token peek(int offset);
  Token advance();
  bool match(TokenType t);
  bool expect(TokenType t, const QString &msg);
  bool isPropertyName(TokenType t) const;

  // ── 语法分析（递归下降） ──
  bool parseProgram(Block &block);
  bool parseBlock(Block &block);
  bool parseBlockOrStmt(Block &block);
  bool parseStmt(Block::Stmt &stmt);
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

  // ── 类型解析 ──
  /// @brief 解析类型注解（: TypeExpr），返回类型结构
  AcType parseType();
  /// @brief 将类型名称解析为 AcType（内建类型 / 自定义类名）
  AcType resolveTypeName(const QString &name);

  // ── 内部状态 ──
  int m_pos = 0;
  QVector<Token> m_tokens;
  QString *m_error = nullptr;
  QSet<QString> *m_declaredVars = nullptr;
};