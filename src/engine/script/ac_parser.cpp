/**
 * @file ac_parser.cpp
 * @brief 语法分析器辅助函数与解析入口
 */

#include "ac_parser.h"

#include <vector>

#include "../ac_language.h"

// ── token 操作 ──

Token AcParser::peek() {
  if (m_pos < m_tokens.size()) return m_tokens[m_pos];
  return {TOK_EOF, {}, 0};
}

Token AcParser::peek(int offset) {
  int pos = m_pos + offset;
  if (pos >= 0 && pos < m_tokens.size()) return m_tokens[pos];
  return {TOK_EOF, {}, 0};
}

Token AcParser::advance() {
  if (m_pos < m_tokens.size()) return m_tokens[m_pos++];
  return {TOK_EOF, {}, 0};
}

bool AcParser::match(TokenType t) {
  if (peek().type == t) {
    advance();
    return true;
  }
  return false;
}

bool AcParser::expect(TokenType t, const QString &msg) {
  if (peek().type == t) {
    advance();
    return true;
  }
  m_error = QStringLiteral("%1 at line %2").arg(msg).arg(peek().line);
  return false;
}

bool AcParser::isPropertyName(TokenType t) const {
  return t == TOK_IDENT || t == TOK_STRING || t == TOK_DEFAULT || t == TOK_CASE || t == TOK_NULL ||
         t == TOK_UNDEFINED || t == TOK_WHILE || t == TOK_BREAK || t == TOK_CONTINUE ||
         t == TOK_SWITCH || t == TOK_FOR || t == TOK_IF || t == TOK_ELSE || t == TOK_RETURN ||
         t == TOK_CLASS || t == TOK_FUNCTION || t == TOK_STATIC || t == TOK_PUBLIC ||
         t == TOK_PROTECTED || t == TOK_PRIVATE || t == TOK_EXTENDS || t == TOK_OVERRIDE ||
         t == TOK_INTERFACE || t == TOK_IMPLEMENTS || t == TOK_SUPER || t == TOK_EXPORT ||
         t == TOK_IMPORT || t == TOK_FROM || t == TOK_NEW || t == TOK_LET || t == TOK_IN ||
         t == TOK_TRUE || t == TOK_FALSE || t == TOK_THIS || t == TOK_ENUM || t == TOK_USING ||
         t == TOK_DISPOSE;
}

// ── 解析入口 ──

bool AcParser::parse(const QVector<Token> &tokens, Block &program, QSet<QString> &declaredVars) {
  m_tokens = tokens;
  m_pos = 0;
  m_error.clear();
  m_declaredVars = &declaredVars;
  program = Block();
  return parseProgram(program);
}

bool AcParser::parseProgram(Block &block) {
  while (peek().type != TOK_EOF) {
    Token t = peek();

    // import { A, B } from "file"
    if (t.type == TOK_IMPORT) {
      advance();
      Block::Stmt stmt;
      stmt.line = t.line;
      stmt.kind = Block::Stmt::kImport;
      if (!parseImportStmt(stmt.importStmt)) return false;
      block.stmts.append(stmt);
      if (!expect(TOK_SEMI, QStringLiteral("expected ';' after import statement"))) return false;
      continue;
    }

    // export let / export class / export function / export interface / export enum
    if (t.type == TOK_EXPORT) {
      Block::Stmt stmt;
      if (!parseStmt(stmt)) return false;
      block.stmts.append(stmt);
      if (stmt.kind != Block::Stmt::kClassDef && stmt.kind != Block::Stmt::kInterfaceDef &&
          stmt.kind != Block::Stmt::kEnumDef && stmt.kind != Block::Stmt::kFuncDef) {
        if (!expect(TOK_SEMI, QStringLiteral("expected ';' after statement"))) return false;
      }
      continue;
    }

    if (t.type == TOK_CLASS) {
      advance();
      Block::Stmt stmt;
      stmt.line = t.line;
      stmt.kind = Block::Stmt::kClassDef;
      if (!parseClassDef(stmt.classDef)) return false;
      block.stmts.append(stmt);
    } else if (t.type == TOK_INTERFACE) {
      advance();
      Block::Stmt stmt;
      stmt.line = t.line;
      stmt.kind = Block::Stmt::kInterfaceDef;
      if (!parseInterfaceDef(stmt.interfaceDef)) return false;
      block.stmts.append(stmt);
    } else if (t.type == TOK_ENUM) {
      advance();
      Block::Stmt stmt;
      stmt.line = t.line;
      stmt.kind = Block::Stmt::kEnumDef;
      if (!parseEnumDef(stmt.enumDef)) return false;
      block.stmts.append(stmt);
    } else if (t.type == TOK_FUNCTION) {
      advance();
      Block::Stmt stmt;
      stmt.line = t.line;
      stmt.kind = Block::Stmt::kFuncDef;
      if (!parseMethodDef(stmt.funcDef)) return false;
      block.stmts.append(stmt);
    } else if (t.type == TOK_LET) {
      advance();
      if (peek().type != TOK_IDENT) {
        m_error = QStringLiteral("expected variable name after 'let' at line %1").arg(peek().line);
        return false;
      }
      m_declaredVars->insert(peek().text);
      Block::Stmt stmt;
      stmt.line = t.line;
      stmt.kind = Block::Stmt::kAssign;
      if (!parseAssignStmt(stmt.assign)) return false;
      stmt.assign.line = t.line;
      block.stmts.append(stmt);
      if (!expect(TOK_SEMI, QStringLiteral("expected ';' after statement"))) return false;
    } else if (t.type == TOK_IDENT && t.text == QString::fromLatin1(AcKeyword::kMain)) {
      advance();
      if (!parseBlock(block)) return false;
    } else {
      // 顶层表达式语句（函数调用、赋值等）
      Block::Stmt stmt;
      if (!parseStmt(stmt)) return false;
      block.stmts.append(stmt);
      if (stmt.kind != Block::Stmt::kClassDef && stmt.kind != Block::Stmt::kInterfaceDef &&
          stmt.kind != Block::Stmt::kEnumDef && stmt.kind != Block::Stmt::kFuncDef) {
        if (!expect(TOK_SEMI, QStringLiteral("expected ';' after statement"))) return false;
      }
      continue;
    }
  }
  return true;
}

bool AcParser::parseBlock(Block &block) {
  if (!expect(TOK_LBRACE, QStringLiteral("expected '{'"))) return false;
  while (peek().type != TOK_RBRACE && peek().type != TOK_EOF) {
    Block::Stmt stmt;
    if (!parseStmt(stmt)) return false;
    block.stmts.append(stmt);
    // class/interface/enum/function 定义以 } 结尾，不需要分号
    if (stmt.kind != Block::Stmt::kClassDef && stmt.kind != Block::Stmt::kInterfaceDef &&
        stmt.kind != Block::Stmt::kEnumDef && stmt.kind != Block::Stmt::kFuncDef &&
        stmt.kind != Block::Stmt::kIf && stmt.kind != Block::Stmt::kFor &&
        stmt.kind != Block::Stmt::kWhile && stmt.kind != Block::Stmt::kSwitch) {
      if (!expect(TOK_SEMI, QStringLiteral("expected ';' after statement"))) return false;
    }
  }
  return expect(TOK_RBRACE, QStringLiteral("expected '}'"));
}

bool AcParser::parseBlockOrStmt(Block &block) {
  if (peek().type == TOK_LBRACE) {
    return parseBlock(block);
  }
  Block::Stmt stmt;
  if (!parseStmt(stmt)) return false;
  block.stmts.append(stmt);
  if (stmt.kind != Block::Stmt::kClassDef && stmt.kind != Block::Stmt::kInterfaceDef &&
      stmt.kind != Block::Stmt::kEnumDef && stmt.kind != Block::Stmt::kFuncDef &&
      stmt.kind != Block::Stmt::kIf && stmt.kind != Block::Stmt::kFor &&
      stmt.kind != Block::Stmt::kWhile && stmt.kind != Block::Stmt::kSwitch) {
    if (!expect(TOK_SEMI, QStringLiteral("expected ';' after statement"))) return false;
  }
  return true;
}

// ── 类型解析 ──

AcType AcParser::parseType() {
  // 类型必须以标识符开头（内建类型名或自定义类名）
  if (peek().type != TOK_IDENT) {
    m_error = QStringLiteral("expected type name at line %1").arg(peek().line);
    return AcType::any();
  }

  QString typeName = advance().text;

  // 泛型参数：List<T>
  if (peek().type == TOK_LT) {
    advance();
    auto elementType = std::make_shared<AcType>(parseType());
    auto type = AcType::arrayOf(*elementType);
    if (!expect(TOK_GT, QStringLiteral("expected '>' after type arguments"))) {
      return AcType::any();
    }
    return type;
  }

  // 简单类型名映射
  if (typeName == "Number" || typeName == "Int" || typeName == "Float" || typeName == "Double") {
    return AcType::number();
  } else if (typeName == "String") {
    return AcType::string();
  } else if (typeName == "Bool" || typeName == "Boolean") {
    return AcType::boolean();
  } else if (typeName == "Any") {
    return AcType::any();
  } else if (typeName == "Void") {
    return AcType::voidType();
  } else {
    // 自定义类类型
    return AcType::classType(typeName);
  }
}