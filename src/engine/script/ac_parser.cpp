/**
 * @file ac_parser.cpp
 * @brief 语法分析器实现文件
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
  *m_error = QStringLiteral("%1 at line %2").arg(msg).arg(peek().line);
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

bool AcParser::parse(const QVector<Token> &tokens, Block &program, QString &error,
                     QSet<QString> &declaredVars) {
  m_tokens = tokens;
  m_pos = 0;
  m_error = &error;
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
      stmt.kind = Block::Stmt::kClassDef;
      if (!parseClassDef(stmt.classDef)) return false;
      block.stmts.append(stmt);
    } else if (t.type == TOK_INTERFACE) {
      advance();
      Block::Stmt stmt;
      stmt.kind = Block::Stmt::kInterfaceDef;
      if (!parseInterfaceDef(stmt.interfaceDef)) return false;
      block.stmts.append(stmt);
    } else if (t.type == TOK_ENUM) {
      advance();
      Block::Stmt stmt;
      stmt.kind = Block::Stmt::kEnumDef;
      if (!parseEnumDef(stmt.enumDef)) return false;
      block.stmts.append(stmt);
    } else if (t.type == TOK_FUNCTION) {
      advance();
      Block::Stmt stmt;
      stmt.kind = Block::Stmt::kFuncDef;
      if (!parseMethodDef(stmt.funcDef)) return false;
      block.stmts.append(stmt);
    } else if (t.type == TOK_LET) {
      advance();
      if (peek().type != TOK_IDENT) {
        *m_error = QStringLiteral("expected variable name after 'let' at line %1").arg(peek().line);
        return false;
      }
      m_declaredVars->insert(peek().text);
      Block::Stmt stmt;
      stmt.kind = Block::Stmt::kAssign;
      if (!parseAssignStmt(stmt.assign)) return false;
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

// ── 语句解析 ──

bool AcParser::parseStmt(Block::Stmt &stmt) {
  Token t = peek();

  // ── import { A, B } from "file" ──
  if (t.type == TOK_IMPORT) {
    advance();
    stmt.kind = Block::Stmt::kImport;
    return parseImportStmt(stmt.importStmt);
  }

  // ── export let / export class / export function / export interface ──
  if (t.type == TOK_EXPORT) {
    advance();
    Token next = peek();

    if (next.type == TOK_LET) {
      advance();
      if (peek().type != TOK_IDENT) {
        *m_error =
            QStringLiteral("expected variable name after 'export let' at line %1").arg(peek().line);
        return false;
      }
      m_declaredVars->insert(peek().text);
      stmt.kind = Block::Stmt::kAssign;
      if (!parseAssignStmt(stmt.assign)) return false;
      stmt.assign.isExported = true;
      return true;
    }

    if (next.type == TOK_CLASS) {
      advance();
      stmt.kind = Block::Stmt::kClassDef;
      if (!parseClassDef(stmt.classDef)) return false;
      stmt.classDef.isExported = true;
      return true;
    }

    if (next.type == TOK_FUNCTION) {
      advance();
      stmt.kind = Block::Stmt::kFuncDef;
      if (!parseMethodDef(stmt.funcDef)) return false;
      stmt.funcDef.isExported = true;
      return true;
    }

    if (next.type == TOK_INTERFACE) {
      advance();
      stmt.kind = Block::Stmt::kInterfaceDef;
      if (!parseInterfaceDef(stmt.interfaceDef)) return false;
      stmt.interfaceDef.isExported = true;
      return true;
    }

    if (next.type == TOK_ENUM) {
      advance();
      stmt.kind = Block::Stmt::kEnumDef;
      if (!parseEnumDef(stmt.enumDef)) return false;
      stmt.enumDef.isExported = true;
      return true;
    }

    *m_error =
        QStringLiteral(
            "expected 'let', 'class', 'function', 'interface', or 'enum' after 'export' at line %1")
            .arg(next.line);
    return false;
  }

  if (t.type == TOK_CLASS) {
    advance();
    stmt.kind = Block::Stmt::kClassDef;
    return parseClassDef(stmt.classDef);
  }

  if (t.type == TOK_INTERFACE) {
    advance();
    stmt.kind = Block::Stmt::kInterfaceDef;
    return parseInterfaceDef(stmt.interfaceDef);
  }

  if (t.type == TOK_ENUM) {
    advance();
    stmt.kind = Block::Stmt::kEnumDef;
    return parseEnumDef(stmt.enumDef);
  }

  if (t.type == TOK_FUNCTION) {
    advance();
    stmt.kind = Block::Stmt::kFuncDef;
    return parseMethodDef(stmt.funcDef);
  }

  if (t.type == TOK_RETURN) {
    advance();
    stmt.kind = Block::Stmt::kReturn;
    return parseReturnStmt(stmt.returnValue);
  }

  if (t.type == TOK_IDENT && t.text == QString::fromLatin1(AcKeyword::kCall)) {
    advance();
    stmt.kind = Block::Stmt::kCall;
    return parseCallStmt(stmt.call);
  }

  if (t.type == TOK_FOR) {
    stmt.kind = Block::Stmt::kFor;
    return parseForStmt(stmt.forStmt);
  }

  if (t.type == TOK_IF) {
    stmt.kind = Block::Stmt::kIf;
    return parseIfStmt(stmt.ifStmt);
  }

  if (t.type == TOK_WHILE) {
    stmt.kind = Block::Stmt::kWhile;
    return parseWhileStmt(stmt.whileStmt);
  }

  if (t.type == TOK_SWITCH) {
    stmt.kind = Block::Stmt::kSwitch;
    return parseSwitchStmt(stmt.switchStmt);
  }

  if (t.type == TOK_BREAK) {
    advance();
    stmt.kind = Block::Stmt::kBreak;
    return true;
  }

  if (t.type == TOK_CONTINUE) {
    advance();
    stmt.kind = Block::Stmt::kContinue;
    return true;
  }

  if (t.type == TOK_USING) {
    advance();
    if (peek().type != TOK_IDENT) {
      *m_error = QStringLiteral("expected variable name after 'using' at line %1").arg(peek().line);
      return false;
    }
    stmt.usingStmt.varName = advance().text;
    m_declaredVars->insert(stmt.usingStmt.varName);
    if (!expect(TOK_EQUALS, QStringLiteral("expected '=' after 'using varName'"))) return false;
    stmt.kind = Block::Stmt::kUsing;
    stmt.usingStmt.value = std::make_unique<Expr>();
    return parseExpr(*stmt.usingStmt.value);
  }

  if (t.type == TOK_LET) {
    advance();
    if (peek().type != TOK_IDENT) {
      *m_error = QStringLiteral("expected variable name after 'let' at line %1").arg(peek().line);
      return false;
    }
    if (!peek().text.isEmpty() && peek().text[0].isDigit()) {
      *m_error =
          QStringLiteral("variable name cannot start with a digit at line %1").arg(peek().line);
      return false;
    }
    m_declaredVars->insert(peek().text);
    stmt.kind = Block::Stmt::kAssign;
    return parseAssignStmt(stmt.assign);
  }

  if (t.type == TOK_IDENT) {
    // ClassName::prop = value  — 静态属性赋值
    if (m_pos + 2 < m_tokens.size() && m_tokens[m_pos + 1].type == TOK_SCOPE &&
        m_tokens[m_pos + 2].type == TOK_IDENT && m_pos + 3 < m_tokens.size() &&
        m_tokens[m_pos + 3].type == TOK_EQUALS) {
      stmt.kind = Block::Stmt::kAssign;
      stmt.assign.isStatic = true;
      stmt.assign.staticClassName = advance().text;
      advance();                          // skip ::
      stmt.assign.name = advance().text;  // member name
      if (!expect(TOK_EQUALS, QStringLiteral("expected '='"))) return false;
      return parseExpr(stmt.assign.value);
    }
    // ClassName::member() 或 ClassName::member  — 静态访问表达式语句
    if (m_pos + 1 < m_tokens.size() && m_tokens[m_pos + 1].type == TOK_SCOPE) {
      stmt.kind = Block::Stmt::kExpr;
      return parseExpr(stmt.exprStmt);
    }
    if (m_pos + 1 < m_tokens.size() &&
        (m_tokens[m_pos + 1].type == TOK_EQUALS || m_tokens[m_pos + 1].type == TOK_PLUSEQ ||
         m_tokens[m_pos + 1].type == TOK_MINUSEQ || m_tokens[m_pos + 1].type == TOK_MULEQ ||
         m_tokens[m_pos + 1].type == TOK_DIVEQ || m_tokens[m_pos + 1].type == TOK_MODEQ)) {
      if (!m_declaredVars->contains(t.text)) {
        *m_error = QStringLiteral(
                       "variable '%1' must be declared with 'let' "
                       "before assignment at line %2")
                       .arg(t.text, QString::number(t.line));
        return false;
      }
      stmt.kind = Block::Stmt::kAssign;
      return parseAssignStmt(stmt.assign);
    }
    if (m_pos + 1 < m_tokens.size() && m_tokens[m_pos + 1].type == TOK_LBRACKET) {
      stmt.kind = Block::Stmt::kIndexAssign;
      return parseIndexAssignStmt(stmt.indexAssign);
    }
    if (m_pos + 1 < m_tokens.size() && m_tokens[m_pos + 1].type == TOK_LPAREN) {
      stmt.kind = Block::Stmt::kExpr;
      QString name = advance().text;
      return parseFuncCall(name, stmt.exprStmt);
    }
    if (m_pos + 1 < m_tokens.size() && m_tokens[m_pos + 1].type == TOK_DOT) {
      // 检查是否为 ident.prop[key] = value（链式索引赋值）
      if (m_pos + 2 < m_tokens.size() && m_tokens[m_pos + 2].type == TOK_IDENT &&
          m_pos + 3 < m_tokens.size() && m_tokens[m_pos + 3].type == TOK_LBRACKET) {
        stmt.kind = Block::Stmt::kIndexAssign;
        stmt.indexAssign.objectExpr.kind = Expr::kPropAccess;
        stmt.indexAssign.objectExpr.ident = advance().text;
        stmt.indexAssign.objectExpr.line = t.line;
        advance();  // skip .
        stmt.indexAssign.objectExpr.prop = advance().text;
        advance();  // skip [
        if (!parseExpr(stmt.indexAssign.indexExpr)) return false;
        if (!expect(TOK_RBRACKET, QStringLiteral("expected ']'"))) return false;
        if (!expect(TOK_EQUALS, QStringLiteral("expected '='"))) return false;
        return parseExpr(stmt.indexAssign.value);
      }
      // 检查是否为 ident.prop = value（属性赋值）
      if (m_pos + 2 < m_tokens.size() && m_tokens[m_pos + 2].type == TOK_IDENT &&
          m_pos + 3 < m_tokens.size() &&
          (m_tokens[m_pos + 3].type == TOK_EQUALS || m_tokens[m_pos + 3].type == TOK_PLUSEQ ||
           m_tokens[m_pos + 3].type == TOK_MINUSEQ || m_tokens[m_pos + 3].type == TOK_MULEQ ||
           m_tokens[m_pos + 3].type == TOK_DIVEQ || m_tokens[m_pos + 3].type == TOK_MODEQ)) {
        stmt.kind = Block::Stmt::kPropAssign;
        stmt.propAssign.objectExpr.kind = Expr::kIdent;
        stmt.propAssign.objectExpr.ident = advance().text;
        stmt.propAssign.objectExpr.line = t.line;
        advance();  // skip .
        stmt.propAssign.prop = advance().text;
        Token opToken = peek();
        if (opToken.type == TOK_PLUSEQ) {
          stmt.propAssign.compoundOp = 1;
          advance();
        } else if (opToken.type == TOK_MINUSEQ) {
          stmt.propAssign.compoundOp = 2;
          advance();
        } else if (opToken.type == TOK_MULEQ) {
          stmt.propAssign.compoundOp = 3;
          advance();
        } else if (opToken.type == TOK_DIVEQ) {
          stmt.propAssign.compoundOp = 4;
          advance();
        } else if (opToken.type == TOK_MODEQ) {
          stmt.propAssign.compoundOp = 5;
          advance();
        } else {
          if (!expect(TOK_EQUALS, QStringLiteral("expected '='"))) return false;
        }
        return parseExpr(stmt.propAssign.value);
      }
      // 检查是否为 ident.prop[expr] = value（链式索引赋值，走表达式）
      // 或者 ident.prop.method()（方法调用，走表达式）
      stmt.kind = Block::Stmt::kExpr;
      return parseExpr(stmt.exprStmt);
    }
  }

  if (t.type == TOK_THIS) {
    if (m_pos + 1 < m_tokens.size() && m_tokens[m_pos + 1].type == TOK_DOT) {
      advance();  // skip this
      advance();  // skip .
      if (!isPropertyName(peek().type)) {
        *m_error =
            QStringLiteral("expected property name after 'this.' at line %1").arg(peek().line);
        return false;
      }
      QString prop = peek().text;
      TokenType assignOp = peek(1).type;
      bool isCompoundAssign =
          (assignOp == TOK_PLUSEQ || assignOp == TOK_MINUSEQ || assignOp == TOK_MULEQ ||
           assignOp == TOK_DIVEQ || assignOp == TOK_MODEQ);
      // 检查 this.prop 后面是否跟着 '=' 或复合赋值运算符，如果是则走赋值，否则走表达式语句
      if (m_pos + 2 < m_tokens.size() && m_tokens[m_pos + 1].type == TOK_DOT &&
          m_tokens[m_pos + 2].type == TOK_IDENT && m_pos + 3 < m_tokens.size() &&
          (m_tokens[m_pos + 3].type == TOK_EQUALS || isCompoundAssign)) {
        advance();  // skip property name
        stmt.assign.thisProp = prop;
        Token opToken = peek();
        if (opToken.type == TOK_PLUSEQ) {
          stmt.assign.compoundOp = 1;
          advance();
        } else if (opToken.type == TOK_MINUSEQ) {
          stmt.assign.compoundOp = 2;
          advance();
        } else if (opToken.type == TOK_MULEQ) {
          stmt.assign.compoundOp = 3;
          advance();
        } else if (opToken.type == TOK_DIVEQ) {
          stmt.assign.compoundOp = 4;
          advance();
        } else if (opToken.type == TOK_MODEQ) {
          stmt.assign.compoundOp = 5;
          advance();
        } else {
          if (!expect(TOK_EQUALS,
                      QStringLiteral("expected '=' after 'this.%1'").arg(stmt.assign.thisProp)))
            return false;
        }
        stmt.kind = Block::Stmt::kAssign;
        return parseExpr(stmt.assign.value);
      }
      // this.prop [=|+=|-=|*=/=] value 的简单检测
      if (m_pos + 1 < m_tokens.size() &&
          (m_tokens[m_pos + 1].type == TOK_EQUALS || isCompoundAssign)) {
        advance();  // skip property name
        stmt.assign.thisProp = prop;
        Token opToken = peek();
        if (opToken.type == TOK_PLUSEQ) {
          stmt.assign.compoundOp = 1;
          advance();
        } else if (opToken.type == TOK_MINUSEQ) {
          stmt.assign.compoundOp = 2;
          advance();
        } else if (opToken.type == TOK_MULEQ) {
          stmt.assign.compoundOp = 3;
          advance();
        } else if (opToken.type == TOK_DIVEQ) {
          stmt.assign.compoundOp = 4;
          advance();
        } else if (opToken.type == TOK_MODEQ) {
          stmt.assign.compoundOp = 5;
          advance();
        } else {
          advance();  // skip =
        }
        stmt.kind = Block::Stmt::kAssign;
        return parseExpr(stmt.assign.value);
      }
      // 不是赋值，回退走表达式语句
      m_pos -= 2;  // 回退到 this 位置
      stmt.kind = Block::Stmt::kExpr;
      return parseExpr(stmt.exprStmt);
    }
    stmt.kind = Block::Stmt::kExpr;
    return parseExpr(stmt.exprStmt);
  }

  if (t.type == TOK_SUPER) {
    stmt.kind = Block::Stmt::kExpr;
    return parseExpr(stmt.exprStmt);
  }

  *m_error =
      QStringLiteral("unexpected token '%1' at line %2").arg(t.text, QString::number(t.line));
  return false;
}

bool AcParser::parseCallStmt(CallStmt &cs) {
  if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after call"))) return false;
  if (!parseExpr(cs.className)) return false;
  if (!expect(TOK_COMMA, QStringLiteral("expected ',' after class name"))) return false;
  if (!parseExpr(cs.funcName)) return false;
  if (match(TOK_COMMA)) {
    if (!parseExpr(cs.args)) return false;
  }
  return expect(TOK_RPAREN, QStringLiteral("expected ')'"));
}

bool AcParser::parseAssignStmt(AssignStmt &as) {
  as.name = advance().text;

  // 类型注解：let name: Type = value（可选，省略时由右侧表达式推导）
  if (peek().type == TOK_COLON) {
    advance();
    as.typeAnnotation = parseType();
    as.hasTypeAnnotation = true;
  } else {
    as.typeAnnotation = AcType::any();
    as.hasTypeAnnotation = false;
  }

  // 检查复合赋值运算符
  Token opToken = peek();
  if (opToken.type == TOK_PLUSEQ) {
    as.compoundOp = 1;  // +=
    advance();
  } else if (opToken.type == TOK_MINUSEQ) {
    as.compoundOp = 2;  // -=
    advance();
  } else if (opToken.type == TOK_MULEQ) {
    as.compoundOp = 3;  // *=
    advance();
  } else if (opToken.type == TOK_DIVEQ) {
    as.compoundOp = 4;  // /=
    advance();
  } else if (opToken.type == TOK_MODEQ) {
    as.compoundOp = 5;  // %=
    advance();
  } else {
    if (!expect(TOK_EQUALS, QStringLiteral("expected '='"))) return false;
  }
  return parseExpr(as.value);
}

bool AcParser::parseIndexAssignStmt(IndexAssignStmt &ias) {
  // 构建对象表达式
  ias.objectExpr.kind = Expr::kIdent;
  ias.objectExpr.ident = advance().text;
  if (!expect(TOK_LBRACKET, QStringLiteral("expected '['"))) return false;
  if (!parseExpr(ias.indexExpr)) return false;
  if (!expect(TOK_RBRACKET, QStringLiteral("expected ']'"))) return false;
  if (!expect(TOK_EQUALS, QStringLiteral("expected '='"))) return false;
  return parseExpr(ias.value);
}

bool AcParser::parseForStmt(ForStmt &fs) {
  advance();
  if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after for"))) return false;

  // 判断是标准 for 还是 for-in
  // 标准 for：for (let i = 0; ...) 或 for (let i: Number = 0; ...)
  // for-in：for (item in array) / for (let item in array) / for (let item: Type in array)
  //
  // 判断逻辑（peek 偏移从当前位置算起）：
  //   let ident = ...    → peek(2)==TOK_EQUALS  → 标准 for
  //   let ident in ...   → peek(2)==TOK_IN      → for-in
  //   let ident : Type = ... → peek(2)==TOK_COLON, peek(4)==TOK_EQUALS → 标准 for
  //   let ident : Type in ...→ peek(2)==TOK_COLON, peek(4)==TOK_IN     → for-in
  if (peek().type == TOK_LET) {
    bool isStandardFor = false;
    if (peek(2).type == TOK_EQUALS) {
      isStandardFor = true;
    } else if (peek(2).type == TOK_COLON && peek(4).type == TOK_EQUALS) {
      isStandardFor = true;
    }

    if (isStandardFor) {
      // 标准 for 循环：for (let i = 0; i < n; i++) 或 for (let i: Number = 0; ...)
      fs.isStandard = true;
      advance();  // 消耗 let
      if (peek().type != TOK_IDENT) {
        *m_error = QStringLiteral("expected variable name after 'let' at line %1").arg(peek().line);
        return false;
      }
      m_declaredVars->insert(peek().text);
      Block::Stmt s;
      s.kind = Block::Stmt::kAssign;
      if (!parseAssignStmt(s.assign)) return false;
      fs.initBlock.stmts.append(s);

      if (!expect(TOK_SEMI, QStringLiteral("expected ';' after for init"))) return false;
      if (!parseExpr(fs.condition)) return false;
      if (!expect(TOK_SEMI, QStringLiteral("expected ';' after for condition"))) return false;
      if (!parseExpr(fs.updateExpr)) return false;
      if (!expect(TOK_RPAREN, QStringLiteral("expected ')'"))) return false;
      return parseBlockOrStmt(fs.body);
    }
    // 否则走 for-in 分支
  }

  // for-in 循环：for (item in array) 或 for (let item[: Type] in array)
  if (peek().type == TOK_LET) {
    advance();  // 消耗 let
  }
  if (peek().type != TOK_IDENT) {
    *m_error = QStringLiteral("expected variable name or 'let' in for at line %1").arg(peek().line);
    return false;
  }
  fs.varName = advance().text;
  m_declaredVars->insert(fs.varName);
  // 可选类型注解：for (let item: Type in array)
  if (peek().type == TOK_COLON) {
    advance();  // 消耗 :
    if (peek().type != TOK_IDENT) {
      *m_error =
          QStringLiteral("expected type name after ':' in for-in at line %1").arg(peek().line);
      return false;
    }
    fs.varType = advance().text;
  }
  if (!expect(TOK_IN, QStringLiteral("expected 'in'"))) return false;
  if (!parseExpr(fs.arrayExpr)) return false;
  if (!expect(TOK_RPAREN, QStringLiteral("expected ')'"))) return false;
  return parseBlockOrStmt(fs.body);
}

bool AcParser::parseIfStmt(IfStmt &is) {
  advance();
  if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after if"))) return false;
  if (!parseExpr(is.condition)) return false;
  if (!expect(TOK_RPAREN, QStringLiteral("expected ')'"))) return false;
  if (!parseBlockOrStmt(is.thenBlock)) return false;

  // 检查 else 或 else if
  if (peek().type == TOK_ELSE) {
    advance();  // skip else
    is.hasElse = true;

    // else if 链式写法
    if (peek().type == TOK_IF) {
      is.elseIfBranch = std::make_unique<IfStmt>();
      is.elseIfBranch->isElseIf = true;
      return parseIfStmt(*is.elseIfBranch);
    }

    // 普通 else（块或单语句）
    return parseBlockOrStmt(is.elseBlock);
  }
  return true;
}

bool AcParser::parseImportStmt(ImportStmt &imp) {
  // import { A, B as C } from "file"
  if (!expect(TOK_LBRACE, QStringLiteral("expected '{' after 'import'"))) return false;

  do {
    if (peek().type != TOK_IDENT) {
      *m_error = QStringLiteral("expected identifier in import list at line %1").arg(peek().line);
      return false;
    }
    QString name = advance().text;
    imp.names.append(name);

    // 检查 as 别名
    if (peek().type == TOK_AS) {
      advance();
      if (peek().type != TOK_IDENT) {
        *m_error = QStringLiteral("expected alias name after 'as' at line %1").arg(peek().line);
        return false;
      }
      QString alias = advance().text;
      imp.aliases[name] = alias;
    }

    if (peek().type != TOK_COMMA) break;
    advance();
  } while (true);

  if (!expect(TOK_RBRACE, QStringLiteral("expected '}' in import statement"))) return false;

  // from 关键字
  if (peek().type != TOK_FROM) {
    *m_error = QStringLiteral("expected 'from' after import list at line %1").arg(peek().line);
    return false;
  }
  advance();

  // 文件路径（字符串）
  if (peek().type != TOK_STRING) {
    *m_error = QStringLiteral("expected file path string after 'from' at line %1").arg(peek().line);
    return false;
  }
  imp.filePath = advance().text;

  return true;
}

bool AcParser::parseWhileStmt(WhileStmt &ws) {
  advance();
  if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after while"))) return false;
  if (!parseExpr(ws.condition)) return false;
  if (!expect(TOK_RPAREN, QStringLiteral("expected ')'"))) return false;
  return parseBlockOrStmt(ws.body);
}

bool AcParser::parseSwitchStmt(SwitchStmt &ss) {
  advance();
  if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after switch"))) return false;
  if (!parseExpr(ss.expr)) return false;
  if (!expect(TOK_RPAREN, QStringLiteral("expected ')'"))) return false;
  if (!expect(TOK_LBRACE, QStringLiteral("expected '{' after switch"))) return false;

  while (peek().type != TOK_RBRACE && peek().type != TOK_EOF) {
    SwitchCase sc;
    if (peek().type == TOK_DEFAULT) {
      advance();
      sc.isDefault = true;
    } else if (peek().type == TOK_CASE) {
      advance();
      if (!parseExpr(sc.value)) return false;
    } else {
      *m_error =
          QStringLiteral("expected 'case' or 'default' in switch at line %1").arg(peek().line);
      return false;
    }
    if (!expect(TOK_COLON, QStringLiteral("expected ':' after case/default"))) return false;
    while (peek().type != TOK_CASE && peek().type != TOK_DEFAULT && peek().type != TOK_RBRACE &&
           peek().type != TOK_EOF) {
      Block::Stmt stmt;
      if (!parseStmt(stmt)) return false;
      sc.body.stmts.append(stmt);
      if (stmt.kind != Block::Stmt::kClassDef && stmt.kind != Block::Stmt::kInterfaceDef &&
          stmt.kind != Block::Stmt::kFuncDef && stmt.kind != Block::Stmt::kIf &&
          stmt.kind != Block::Stmt::kFor && stmt.kind != Block::Stmt::kWhile &&
          stmt.kind != Block::Stmt::kSwitch) {
        if (!expect(TOK_SEMI, QStringLiteral("expected ';' after statement"))) return false;
      }
    }
    ss.cases.append(sc);
  }
  return expect(TOK_RBRACE, QStringLiteral("expected '}' after switch body"));
}

// ── 类定义解析 ──

bool AcParser::parseClassDef(ClassDef &cd) {
  if (peek().type != TOK_IDENT) {
    *m_error = QStringLiteral("expected class name at line %1").arg(peek().line);
    return false;
  }
  cd.name = advance().text;

  // extends BaseClass
  if (peek().type == TOK_EXTENDS) {
    advance();
    if (peek().type != TOK_IDENT) {
      *m_error =
          QStringLiteral("expected base class name after 'extends' at line %1").arg(peek().line);
      return false;
    }
    cd.baseClass = advance().text;
  }

  // implements I1, I2, ...
  if (peek().type == TOK_IMPLEMENTS) {
    advance();
    do {
      if (peek().type != TOK_IDENT) {
        *m_error = QStringLiteral("expected interface name after 'implements' at line %1")
                       .arg(peek().line);
        return false;
      }
      cd.interfaces.append(advance().text);
      if (peek().type == TOK_COMMA)
        advance();
      else
        break;
    } while (true);
  }

  if (!expect(TOK_LBRACE, QStringLiteral("expected '{' after class declaration"))) return false;

  while (peek().type != TOK_RBRACE && peek().type != TOK_EOF) {
    // 收集修饰符前缀：public/private/protected/static/override
    AccessLevel access = AccessLevel::kPublic;
    bool isStatic = false;
    bool isOverride = false;

    while (true) {
      if (peek().type == TOK_PUBLIC) {
        access = AccessLevel::kPublic;
        advance();
        continue;
      }
      if (peek().type == TOK_PRIVATE) {
        access = AccessLevel::kPrivate;
        advance();
        continue;
      }
      if (peek().type == TOK_PROTECTED) {
        access = AccessLevel::kProtected;
        advance();
        continue;
      }
      if (peek().type == TOK_STATIC) {
        isStatic = true;
        advance();
        continue;
      }
      if (peek().type == TOK_OVERRIDE) {
        isOverride = true;
        advance();
        continue;
      }
      break;
    }

    if (peek().type == TOK_LET) {
      // let 声明（兼容旧语法）
      advance();
      if (peek().type != TOK_IDENT) {
        *m_error = QStringLiteral("expected property name in class at line %1").arg(peek().line);
        return false;
      }
      ObjectEntry prop;
      prop.key = advance().text;
      prop.isStatic = isStatic;
      prop.access = access;
      if (peek().type == TOK_COLON) {
        advance();
        prop.type = parseType();
      } else {
        prop.type = AcType::any();
      }
      prop.value = nullptr;
      if (peek().type == TOK_EQUALS) {
        advance();
        prop.value = std::make_unique<Expr>();
        if (!parseExpr(*prop.value)) {
          prop.value = nullptr;
          return false;
        }
      }
      cd.properties.append(prop);
      if (!expect(TOK_SEMI, QStringLiteral("expected ';' after property declaration")))
        return false;
      continue;
    }

    if (peek().type == TOK_FUNCTION) {
      advance();
      MethodDef md;
      md.isStatic = isStatic;
      md.access = access;
      md.isOverride = isOverride;
      if (!parseMethodDef(md)) return false;
      cd.methods.append(md);
      continue;
    }

    // constructor 关键字：构造函数（不需要返回类型注解）
    if (peek().type == TOK_CONSTRUCTOR) {
      advance();
      MethodDef md;
      md.name = QStringLiteral("constructor");
      md.isStatic = false;
      md.access = access;
      md.isOverride = false;
      if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after constructor"))) return false;

      while (peek().type == TOK_IDENT) {
        ParamDef pd;
        pd.name = advance().text;
        if (peek().type == TOK_COLON) {
          advance();
          pd.type = parseType();
        } else {
          pd.type = AcType::any();
        }
        md.params.append(pd);
        m_declaredVars->insert(pd.name);
        if (peek().type == TOK_COMMA) advance();
      }

      if (!expect(TOK_RPAREN, QStringLiteral("expected ')' after constructor parameters")))
        return false;

      md.returnType = AcType::any();
      if (!parseBlock(md.body)) return false;
      cd.methods.append(md);
      continue;
    }

    // TS 风格属性：public brand: String = "Tesla"（无 let 关键字，类型注解可选）
    if (peek().type == TOK_IDENT) {
      ObjectEntry prop;
      prop.key = advance().text;
      prop.isStatic = isStatic;
      prop.access = access;
      if (peek().type == TOK_COLON) {
        advance();
        prop.type = parseType();
      } else {
        prop.type = AcType::any();
      }
      prop.value = nullptr;
      if (peek().type == TOK_EQUALS) {
        advance();
        prop.value = std::make_unique<Expr>();
        if (!parseExpr(*prop.value)) {
          prop.value = nullptr;
          return false;
        }
      }
      cd.properties.append(prop);
      if (!expect(TOK_SEMI, QStringLiteral("expected ';' after property declaration")))
        return false;
      continue;
    }

    *m_error = QStringLiteral("unexpected token '%1' in class body at line %2")
                   .arg(peek().text, QString::number(peek().line));
    return false;
  }

  return expect(TOK_RBRACE, QStringLiteral("expected '}' after class body"));
}

// ── 接口定义解析 ──

bool AcParser::parseInterfaceDef(InterfaceDef &iface) {
  if (peek().type != TOK_IDENT) {
    *m_error = QStringLiteral("expected interface name at line %1").arg(peek().line);
    return false;
  }
  iface.name = advance().text;

  // 接口继承：interface A extends B, C
  if (peek().type == TOK_EXTENDS) {
    advance();
    do {
      if (peek().type != TOK_IDENT) {
        *m_error =
            QStringLiteral("expected interface name after 'extends' at line %1").arg(peek().line);
        return false;
      }
      iface.baseInterfaces.append(advance().text);
      if (peek().type == TOK_COMMA)
        advance();
      else
        break;
    } while (true);
  }

  if (!expect(TOK_LBRACE, QStringLiteral("expected '{' after interface name"))) return false;

  while (peek().type != TOK_RBRACE && peek().type != TOK_EOF) {
    if (peek().type == TOK_FUNCTION) {
      advance();
      InterfaceMethod im;
      if (peek().type != TOK_IDENT) {
        *m_error = QStringLiteral("expected method name in interface at line %1").arg(peek().line);
        return false;
      }
      im.name = advance().text;

      if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after method name"))) return false;

      while (peek().type == TOK_IDENT) {
        ParamDef pd;
        pd.name = advance().text;
        if (peek().type != TOK_COLON) {
          *m_error =
              QStringLiteral("parameter '%1' requires a type annotation (e.g. %1: Type) at line %2")
                  .arg(pd.name)
                  .arg(peek().line);
          return false;
        }
        advance();
        pd.type = parseType();
        im.params.append(pd);
        if (peek().type == TOK_COMMA) advance();
      }

      if (!expect(TOK_RPAREN, QStringLiteral("expected ')' after parameters"))) return false;

      if (peek().type != TOK_COLON) {
        *m_error =
            QStringLiteral("method '%1' requires a return type annotation (e.g. : Type) at line %2")
                .arg(im.name)
                .arg(peek().line);
        return false;
      }
      advance();
      im.returnType = parseType();

      if (!expect(TOK_SEMI, QStringLiteral("expected ';' after interface method signature")))
        return false;
      iface.methods.append(im);
      continue;
    }

    *m_error = QStringLiteral("expected 'function' in interface body at line %1").arg(peek().line);
    return false;
  }

  return expect(TOK_RBRACE, QStringLiteral("expected '}' after interface body"));
}

bool AcParser::parseEnumDef(EnumDef &ed) {
  if (peek().type != TOK_IDENT) {
    *m_error = QStringLiteral("expected enum name at line %1").arg(peek().line);
    return false;
  }
  ed.name = advance().text;

  if (!expect(TOK_LBRACE, QStringLiteral("expected '{' after enum name"))) return false;

  int nextValue = 0;  // 自动递增计数器

  while (peek().type != TOK_RBRACE && peek().type != TOK_EOF) {
    EnumMember member;

    // 解析成员名
    if (peek().type != TOK_IDENT) {
      *m_error = QStringLiteral("expected enum member name at line %1").arg(peek().line);
      return false;
    }
    member.name = advance().text;

    // 检查是否有显式值赋值（= value）
    if (peek().type == TOK_EQUALS) {
      advance();  // 跳过 =

      // 解析值（支持数字或字符串）
      Token valToken = peek();
      if (valToken.type == TOK_NUMBER) {
        member.value = QJsonValue(valToken.text.toDouble());
        nextValue = (int)valToken.text.toDouble() + 1;
        member.hasValue = true;
        advance();
      } else if (valToken.type == TOK_STRING) {
        member.value = QJsonValue(valToken.text);
        member.hasValue = true;
        advance();
      } else {
        *m_error = QStringLiteral("expected number or string value for enum member '%1' at line %2")
                       .arg(member.name)
                       .arg(valToken.line);
        return false;
      }
    } else {
      // 自动分配值
      member.value = QJsonValue(nextValue++);
      member.hasValue = false;
    }

    ed.members.append(member);

    // 检查是否有逗号分隔
    if (peek().type == TOK_COMMA) {
      advance();
    }
  }

  return expect(TOK_RBRACE, QStringLiteral("expected '}' after enum body"));
}

bool AcParser::parseMethodDef(MethodDef &md) {
  if (peek().type != TOK_IDENT) {
    *m_error = QStringLiteral("expected method name at line %1").arg(peek().line);
    return false;
  }
  md.name = advance().text;

  if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after method name"))) return false;

  while (peek().type == TOK_IDENT) {
    ParamDef pd;
    pd.name = advance().text;
    // 强制类型注解：name : Type（必须写类型）
    if (peek().type != TOK_COLON) {
      *m_error =
          QStringLiteral("parameter '%1' requires a type annotation (e.g. %1: Type) at line %2")
              .arg(pd.name)
              .arg(peek().line);
      return false;
    }
    advance();  // 跳过 ':'
    pd.type = parseType();
    md.params.append(pd);
    m_declaredVars->insert(pd.name);  // 将参数注册为已声明变量，允许在函数体内重新赋值
    if (peek().type == TOK_COMMA) advance();
  }

  if (!expect(TOK_RPAREN, QStringLiteral("expected ')' after parameters"))) return false;

  // 强制返回类型注解
  if (peek().type != TOK_COLON) {
    *m_error =
        QStringLiteral("function '%1' requires a return type annotation (e.g. : Type) at line %2")
            .arg(md.name)
            .arg(peek().line);
    return false;
  }
  advance();  // 跳过 ':'
  md.returnType = parseType();

  return parseBlock(md.body);
}

bool AcParser::parseReturnStmt(Expr &retVal) {
  if (peek().type == TOK_SEMI || peek().type == TOK_RBRACE) {
    retVal.kind = Expr::kNumber;
    retVal.numVal = 0;
    return true;
  }
  return parseExpr(retVal);
}

// ── 表达式解析 ──

bool AcParser::parseExpr(Expr &expr) {
  if (!parseTernary(expr)) return false;
  // 后置自增 expr++
  if (peek().type == TOK_PLUSPLUS) {
    advance();
    auto operand = std::make_unique<Expr>(std::move(expr));
    expr.kind = Expr::kPostInc;
    expr.operand = std::move(operand);
    return true;
  }
  // 后置自减 expr--
  if (peek().type == TOK_MINUSMINUS) {
    advance();
    auto operand = std::make_unique<Expr>(std::move(expr));
    expr.kind = Expr::kPostDec;
    expr.operand = std::move(operand);
    return true;
  }
  return true;
}

bool AcParser::parseTernary(Expr &expr) {
  if (!parseLogicalOr(expr)) return false;
  if (peek().type == TOK_QUESTION) {
    int ternaryLine = peek().line;
    advance();
    auto cond = std::make_unique<Expr>(std::move(expr));
    auto trueExpr = std::make_unique<Expr>();
    if (!parseLogicalOr(*trueExpr)) {
      return false;
    }
    if (!expect(TOK_COLON, QStringLiteral("expected ':' in ternary expression"))) {
      return false;
    }
    auto falseExpr = std::make_unique<Expr>();
    if (!parseTernary(*falseExpr)) {
      return false;
    }
    expr.kind = Expr::kTernary;
    expr.line = ternaryLine;
    expr.left = std::move(cond);
    expr.right = std::move(trueExpr);
    expr.operand = std::move(falseExpr);
    return true;
  }
  return true;
}

// ── 二元运算解析（模板化，消除重复代码） ──

using ParseNextFn = bool (AcParser::*)(Expr &);

template <ParseNextFn parseNext>
bool AcParser::parseBinary(Expr &expr,
                           const std::vector<std::pair<TokenType, Expr::BinaryOp>> &ops) {
  if (!(this->*parseNext)(expr)) return false;
  while (true) {
    Expr::BinaryOp matchedOp = Expr::kAdd;
    bool found = false;
    for (const auto &[tokType, binOp] : ops) {
      if (peek().type == tokType) {
        matchedOp = binOp;
        found = true;
        break;
      }
    }
    if (!found) break;
    Token opToken = peek();
    advance();
    auto left = std::make_unique<Expr>(std::move(expr));
    auto right = std::make_unique<Expr>();
    if (!(this->*parseNext)(*right)) return false;
    Expr binary;
    binary.kind = Expr::kBinary;
    binary.line = opToken.line;
    binary.binOp = matchedOp;
    binary.left = std::move(left);
    binary.right = std::move(right);
    expr = std::move(binary);
  }
  return true;
}

bool AcParser::parseLogicalOr(Expr &expr) {
  return parseBinary<&AcParser::parseLogicalAnd>(expr, {{TOK_OR, Expr::kOr}});
}

bool AcParser::parseLogicalAnd(Expr &expr) {
  return parseBinary<&AcParser::parseComparison>(expr, {{TOK_AND, Expr::kAnd}});
}

bool AcParser::parseComparison(Expr &expr) {
  return parseBinary<&AcParser::parseAddSub>(expr, {{TOK_EQ, Expr::kEq},
                                                    {TOK_NEQ, Expr::kNeq},
                                                    {TOK_LT, Expr::kLt},
                                                    {TOK_GT, Expr::kGt},
                                                    {TOK_LTE, Expr::kLte},
                                                    {TOK_GTE, Expr::kGte}});
}

bool AcParser::parseAddSub(Expr &expr) {
  return parseBinary<&AcParser::parseMulDiv>(expr,
                                             {{TOK_PLUS, Expr::kAdd}, {TOK_MINUS, Expr::kSub}});
}

bool AcParser::parseMulDiv(Expr &expr) {
  return parseBinary<&AcParser::parseUnary>(
      expr, {{TOK_MUL, Expr::kMul}, {TOK_DIV, Expr::kDiv}, {TOK_MOD, Expr::kMod}});
}

bool AcParser::parseUnary(Expr &expr) {
  Token t = peek();

  if (t.type == TOK_PLUSPLUS) {
    advance();
    auto operand = std::make_unique<Expr>();
    if (!parseUnary(*operand)) {
      return false;
    }
    expr.kind = Expr::kPreInc;
    expr.line = t.line;
    expr.operand = std::move(operand);
    return true;
  }

  if (t.type == TOK_MINUSMINUS) {
    advance();
    auto operand = std::make_unique<Expr>();
    if (!parseUnary(*operand)) {
      return false;
    }
    expr.kind = Expr::kPreDec;
    expr.line = t.line;
    expr.operand = std::move(operand);
    return true;
  }

  if (t.type == TOK_NOT) {
    advance();
    auto operand = std::make_unique<Expr>();
    if (!parseUnary(*operand)) {
      return false;
    }
    expr.kind = Expr::kUnary;
    expr.line = t.line;
    expr.unaryOp = Expr::kNot;
    expr.operand = std::move(operand);
    return true;
  }

  if (t.type == TOK_MINUS) {
    advance();
    if (peek().type == TOK_NUMBER) {
      expr.kind = Expr::kNumber;
      expr.numVal = -advance().text.toDouble();
      return true;
    }
    auto right = std::make_unique<Expr>();
    if (!parseUnary(*right)) {
      return false;
    }
    Expr binary;
    binary.kind = Expr::kBinary;
    binary.binOp = Expr::kSub;
    binary.left = std::make_unique<Expr>();
    binary.left->kind = Expr::kNumber;
    binary.left->numVal = 0;
    binary.right = std::move(right);
    expr = std::move(binary);
    return true;
  }

  return parsePostfix(expr);
}

bool AcParser::parsePostfix(Expr &expr) {
  if (!parsePrimary(expr)) return false;
  while (true) {
    if (peek().type == TOK_DOT) {
      advance();
      if (!isPropertyName(peek().type)) {
        *m_error = QStringLiteral("expected property name after '.' at line %1").arg(peek().line);
        return false;
      }
      QString memberName = advance().text;
      if (peek().type == TOK_LPAREN) {
        advance();
        Expr chained;
        chained.kind = Expr::kMethodCall;
        chained.line = peek().line;
        chained.methodCall.methodName = memberName;
        if (expr.kind == Expr::kIdent) {
          chained.methodCall.objName = expr.ident;
        }
        chained.methodCall.object = std::make_unique<Expr>(std::move(expr));
        while (peek().type != TOK_RPAREN && peek().type != TOK_EOF) {
          auto arg = std::make_unique<Expr>();
          if (!parseLogicalOr(*arg)) {
            return false;
          }
          chained.methodCall.args.push_back(std::move(arg));
          if (peek().type == TOK_COMMA) advance();
        }
        if (!expect(TOK_RPAREN,
                    QStringLiteral("expected ')' after method call at line %1").arg(peek().line)))
          return false;
        expr = std::move(chained);
      } else {
        Expr propAccess;
        propAccess.kind = Expr::kPropAccess;
        propAccess.line = peek().line;
        propAccess.prop = memberName;
        propAccess.propObject = std::make_unique<Expr>(std::move(expr));
        expr = std::move(propAccess);
      }
    } else if (peek().type == TOK_LBRACKET) {
      advance();
      auto idxExpr = std::make_unique<Expr>();
      if (!parseLogicalOr(*idxExpr)) {
        return false;
      }
      if (!expect(TOK_RBRACKET, QStringLiteral("expected ']' after index expression"))) {
        return false;
      }
      Expr idxAccess;
      idxAccess.kind = Expr::kIndexAccess;
      idxAccess.line = peek().line;
      idxAccess.left = std::make_unique<Expr>(std::move(expr));
      idxAccess.right = std::move(idxExpr);
      expr = std::move(idxAccess);
    } else {
      break;
    }
  }
  return true;
}

bool AcParser::parsePrimary(Expr &expr) {
  Token t = peek();

  if (t.type == TOK_THIS) {
    advance();
    if (peek().type == TOK_DOT) {
      advance();
      if (!isPropertyName(peek().type)) {
        *m_error =
            QStringLiteral("expected property name after 'this.' at line %1").arg(peek().line);
        return false;
      }
      QString propName = advance().text;
      if (peek().type == TOK_LPAREN) {
        expr.kind = Expr::kMethodCall;
        expr.line = peek().line;
        expr.methodCall.objName = QString::fromLatin1(AcKeyword::kThis);
        expr.methodCall.methodName = propName;
        advance();
        while (peek().type != TOK_RPAREN && peek().type != TOK_EOF) {
          auto arg = std::make_unique<Expr>();
          if (!parseLogicalOr(*arg)) {
            return false;
          }
          expr.methodCall.args.push_back(std::move(arg));
          if (peek().type == TOK_COMMA) advance();
        }
        return expect(TOK_RPAREN, QStringLiteral("expected ')'"));
      }
      expr.kind = Expr::kPropAccess;
      expr.ident = QString::fromLatin1(AcKeyword::kThis);
      expr.prop = propName;
      return true;
    }
    if (peek().type == TOK_LBRACKET) {
      advance();
      expr.kind = Expr::kIndexAccess;
      expr.left = std::make_unique<Expr>();
      expr.left->kind = Expr::kThis;
      expr.right = std::make_unique<Expr>();
      if (!parseExpr(*expr.right)) return false;
      return expect(TOK_RBRACKET, QStringLiteral("expected ']'"));
    }
    expr.kind = Expr::kThis;
    return true;
  }

  if (t.type == TOK_SUPER) {
    advance();
    // super(args) — 父类构造函数调用
    if (peek().type == TOK_LPAREN) {
      expr.kind = Expr::kMethodCall;
      expr.line = peek().line;
      expr.methodCall.objName = QString::fromLatin1(AcKeyword::kSuper);
      expr.methodCall.methodName = QStringLiteral("constructor");
      advance();
      while (peek().type != TOK_RPAREN && peek().type != TOK_EOF) {
        auto arg = std::make_unique<Expr>();
        if (!parseLogicalOr(*arg)) {
          return false;
        }
        expr.methodCall.args.push_back(std::move(arg));
        if (peek().type == TOK_COMMA) advance();
      }
      return expect(TOK_RPAREN, QStringLiteral("expected ')'"));
    }
    // super.method(args) 或 super.method
    if (peek().type != TOK_DOT) {
      *m_error = QStringLiteral("expected '.' or '(' after 'super' at line %1").arg(peek().line);
      return false;
    }
    advance();
    if (peek().type != TOK_IDENT) {
      *m_error = QStringLiteral("expected method name after 'super.' at line %1").arg(peek().line);
      return false;
    }
    QString methodName = advance().text;
    if (peek().type == TOK_LPAREN) {
      expr.kind = Expr::kMethodCall;
      expr.line = peek().line;
      expr.methodCall.objName = QString::fromLatin1(AcKeyword::kSuper);
      expr.methodCall.methodName = methodName;
      advance();
      while (peek().type != TOK_RPAREN && peek().type != TOK_EOF) {
        auto arg = std::make_unique<Expr>();
        if (!parseLogicalOr(*arg)) {
          return false;
        }
        expr.methodCall.args.push_back(std::move(arg));
        if (peek().type == TOK_COMMA) advance();
      }
      return expect(TOK_RPAREN, QStringLiteral("expected ')'"));
    }
    expr.kind = Expr::kPropAccess;
    expr.ident = QString::fromLatin1(AcKeyword::kSuper);
    expr.prop = methodName;
    return true;
  }

  if (t.type == TOK_NEW) {
    int newLine = t.line;
    advance();
    if (peek().type != TOK_IDENT) {
      *m_error = QStringLiteral("expected class name after 'new' at line %1").arg(peek().line);
      return false;
    }
    expr.kind = Expr::kNewInstance;
    expr.line = newLine;
    expr.className = advance().text;
    if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after class name"))) return false;
    // 解析构造参数列表（支持任意多个参数）
    if (peek().type != TOK_RPAREN) {
      do {
        auto arg = std::make_unique<Expr>();
        if (!parseLogicalOr(*arg)) {
          return false;
        }
        expr.constructorArgs.push_back(std::move(arg));
        if (peek().type != TOK_COMMA) break;
        advance();
      } while (true);
    }
    return expect(TOK_RPAREN, QStringLiteral("expected ')'"));
  }

  switch (t.type) {
    case TOK_STRING:
      expr.kind = Expr::kString;
      expr.strVal = advance().text;
      return true;

    case TOK_NUMBER:
      expr.kind = Expr::kNumber;
      expr.numVal = advance().text.toDouble();
      return true;

    case TOK_TRUE:
      expr.kind = Expr::kBool;
      expr.boolVal = true;
      advance();
      return true;

    case TOK_FALSE:
      expr.kind = Expr::kBool;
      expr.boolVal = false;
      advance();
      return true;

    case TOK_NULL:
      expr.kind = Expr::kNull;
      advance();
      return true;

    case TOK_UNDEFINED:
      expr.kind = Expr::kUndefined;
      advance();
      return true;

    case TOK_TEMPLATE_STRING:
      return parseTemplateString(expr);

    case TOK_LPAREN: {
      advance();
      if (!parseExpr(expr)) return false;
      return expect(TOK_RPAREN, QStringLiteral("expected ')'"));
    }

    case TOK_IDENT: {
      QString name = advance().text;
      if (peek().type == TOK_SCOPE) {
        int scopeLine = peek().line;
        advance();
        if (peek().type != TOK_IDENT) {
          *m_error = QStringLiteral("expected member name after '::' at line %1").arg(peek().line);
          return false;
        }
        QString member = advance().text;
        expr.kind = Expr::kStaticAccess;
        expr.line = scopeLine;
        expr.className = name;
        expr.prop = member;
        if (peek().type == TOK_LPAREN) {
          advance();
          while (peek().type != TOK_RPAREN && peek().type != TOK_EOF) {
            auto arg = std::make_unique<Expr>();
            if (!parseLogicalOr(*arg)) {
              return false;
            }
            expr.funcCall.args.push_back(std::move(arg));
            if (peek().type == TOK_COMMA) advance();
          }
          return expect(TOK_RPAREN, QStringLiteral("expected ')' after static method call"));
        }
        return true;
      }
      if (peek().type == TOK_DOT) {
        advance();
        if (!isPropertyName(peek().type)) {
          *m_error = QStringLiteral("expected property name after '.' at line %1").arg(peek().line);
          return false;
        }
        QString propName = advance().text;
        if (peek().type == TOK_LPAREN) {
          expr.kind = Expr::kMethodCall;
          expr.line = peek().line;
          expr.methodCall.objName = name;
          expr.methodCall.methodName = propName;
          advance();
          while (peek().type != TOK_RPAREN && peek().type != TOK_EOF) {
            auto arg = std::make_unique<Expr>();
            if (!parseLogicalOr(*arg)) {
              return false;
            }
            expr.methodCall.args.push_back(std::move(arg));
            if (peek().type == TOK_COMMA) advance();
          }
          return expect(TOK_RPAREN, QStringLiteral("expected ')'"));
        }
        expr.kind = Expr::kPropAccess;
        expr.ident = name;
        expr.prop = propName;
        return true;
      }
      if (peek().type == TOK_LPAREN) {
        return parseFuncCall(name, expr);
      }
      if (peek().type == TOK_LBRACKET) {
        advance();
        expr.kind = Expr::kIndexAccess;
        expr.left = std::make_unique<Expr>();
        expr.left->kind = Expr::kIdent;
        expr.left->ident = name;
        expr.left->line = t.line;
        expr.right = std::make_unique<Expr>();
        if (!parseExpr(*expr.right)) return false;
        return expect(TOK_RBRACKET, QStringLiteral("expected ']'"));
      }
      expr.kind = Expr::kIdent;
      expr.ident = name;
      expr.line = t.line;
      return true;
    }

    case TOK_LBRACE:
      return parseObject(expr);

    case TOK_LBRACKET:
      return parseArray(expr);

    case TOK_FUNCTION: {
      advance();
      expr.kind = Expr::kFuncExpr;
      // 匿名函数表达式：function(params): Type { body }
      // 可选函数名
      if (peek().type == TOK_IDENT) {
        expr.funcExpr.name = advance().text;
      } else {
        expr.funcExpr.name = QStringLiteral("__anonymous__");
      }
      if (!expect(TOK_LPAREN, QStringLiteral("expected '(' in function expression"))) return false;
      while (peek().type == TOK_IDENT) {
        ParamDef pd;
        pd.name = advance().text;
        if (peek().type != TOK_COLON) {
          *m_error =
              QStringLiteral("parameter '%1' requires a type annotation (e.g. %1: Type) at line %2")
                  .arg(pd.name)
                  .arg(peek().line);
          return false;
        }
        advance();
        pd.type = parseType();
        expr.funcExpr.params.append(pd);
        m_declaredVars->insert(pd.name);
        if (peek().type == TOK_COMMA) advance();
      }
      if (!expect(TOK_RPAREN, QStringLiteral("expected ')' after parameters"))) return false;
      // 强制返回类型注解
      if (peek().type != TOK_COLON) {
        *m_error = QStringLiteral(
                       "function expression requires a return type annotation (e.g. : Type) at "
                       "line %1")
                       .arg(peek().line);
        return false;
      }
      advance();
      expr.funcExpr.returnType = parseType();
      if (!parseBlock(expr.funcExpr.body)) return false;
      return true;
    }

    default:
      *m_error =
          QStringLiteral("unexpected token '%1' at line %2").arg(t.text, QString::number(t.line));
      return false;
  }
}

bool AcParser::parseObject(Expr &expr) {
  expr.kind = Expr::kObject;
  advance();
  while (peek().type != TOK_RBRACE && peek().type != TOK_EOF) {
    if (!isPropertyName(peek().type)) {
      *m_error = QStringLiteral("expected key in object at line %1").arg(peek().line);
      return false;
    }
    ObjectEntry entry;
    entry.key = advance().text;
    if (!expect(TOK_COLON, QStringLiteral("expected ':'"))) return false;
    entry.value = std::make_unique<Expr>();
    if (!parseExpr(*entry.value)) {
      entry.value = nullptr;
      return false;
    }
    expr.objEntries.append(entry);
    if (peek().type == TOK_COMMA) advance();
  }
  return expect(TOK_RBRACE, QStringLiteral("expected '}'"));
}

bool AcParser::parseArray(Expr &expr) {
  expr.kind = Expr::kArray;
  advance();
  while (peek().type != TOK_RBRACKET && peek().type != TOK_EOF) {
    auto item = std::make_unique<Expr>();
    if (!parseExpr(*item)) {
      return false;
    }
    expr.arrItems.push_back(std::move(item));
    if (peek().type == TOK_COMMA) advance();
  }
  return expect(TOK_RBRACKET, QStringLiteral("expected ']'"));
}

bool AcParser::parseFuncCall(const QString &name, Expr &expr) {
  expr.kind = Expr::kFuncCall;
  expr.funcCall.name = name;
  expr.line = peek().line;
  advance();
  while (peek().type != TOK_RPAREN && peek().type != TOK_EOF) {
    auto arg = std::make_unique<Expr>();
    if (!parseLogicalOr(*arg)) {
      return false;
    }
    expr.funcCall.args.push_back(std::move(arg));
    if (peek().type == TOK_COMMA) advance();
  }
  return expect(TOK_RPAREN, QStringLiteral("expected ')'"));
}

bool AcParser::parseTemplateString(Expr &expr) {
  Token tok = advance();
  QString raw = tok.text;

  std::vector<std::unique_ptr<Expr>> parts;

  int i = 0;
  int n = raw.size();
  QString currentStr;

  while (i < n) {
    if (raw[i] == '$' && i + 1 < n && raw[i + 1] == '{') {
      if (!currentStr.isEmpty()) {
        auto strExpr = std::make_unique<Expr>();
        strExpr->kind = Expr::kString;
        strExpr->strVal = currentStr;
        parts.push_back(std::move(strExpr));
        currentStr.clear();
      }
      i += 2;
      int depth = 1;
      int start = i;
      while (i < n && depth > 0) {
        if (raw[i] == '{') ++depth;
        if (raw[i] == '}') --depth;
        if (depth > 0) ++i;
      }
      QString exprText = raw.mid(start, i - start);
      ++i;

      QVector<Token> exprTokens = AcLexer::tokenize(exprText, *m_error);
      if (!m_error->isEmpty()) return false;

      int savedPos = m_pos;
      QVector<Token> savedTokens = m_tokens;
      m_tokens = exprTokens;
      m_pos = 0;

      auto subExpr = std::make_unique<Expr>();
      if (!parseExpr(*subExpr)) {
        m_tokens = savedTokens;
        m_pos = savedPos;
        return false;
      }
      parts.push_back(std::move(subExpr));

      m_tokens = savedTokens;
      m_pos = savedPos;
    } else if (raw[i] == '\\' && i + 1 < n) {
      QChar next = raw[i + 1];
      if (next == 'n') {
        currentStr += QLatin1Char('\n');
      } else if (next == 't') {
        currentStr += QLatin1Char('\t');
      } else if (next == '$') {
        currentStr += QLatin1Char('$');
      } else if (next == '`') {
        currentStr += QLatin1Char('`');
      } else if (next == '\\') {
        currentStr += QLatin1Char('\\');
      } else {
        currentStr += next;
      }
      i += 2;
    } else {
      currentStr += raw[i];
      ++i;
    }
  }

  if (!currentStr.isEmpty()) {
    auto strExpr = std::make_unique<Expr>();
    strExpr->kind = Expr::kString;
    strExpr->strVal = currentStr;
    parts.push_back(std::move(strExpr));
  }

  if (parts.empty()) {
    expr.kind = Expr::kString;
    expr.strVal = QString();
    return true;
  }

  if (parts.size() == 1) {
    expr = std::move(*parts[0]);
    return true;
  }

  auto result = std::move(parts[0]);
  for (int j = 1; j < parts.size(); ++j) {
    auto binary = std::make_unique<Expr>();
    binary->kind = Expr::kBinary;
    binary->binOp = Expr::kAdd;
    binary->left = std::move(result);
    binary->right = std::move(parts[j]);
    result = std::move(binary);
  }
  expr = std::move(*result);
  return true;
}

// ── 类型解析 ──

AcType AcParser::parseType() {
  // 类型必须以标识符开头（内建类型名或自定义类名）
  if (peek().type != TOK_IDENT) {
    *m_error = QStringLiteral("expected type name at line %1").arg(peek().line);
    return AcType::any();
  }
  QString name = advance().text;

  // 支持 Type[] 数组语法
  if (peek().type == TOK_LBRACKET) {
    advance();  // 跳过 '['
    if (!expect(TOK_RBRACKET, QStringLiteral("expected ']' after '[' in array type")))
      return AcType::any();
    return AcType::arrayOf(resolveTypeName(name));
  }

  return resolveTypeName(name);
}

AcType AcParser::resolveTypeName(const QString &name) {
  if (name == QStringLiteral("Number")) return AcType::number();
  if (name == QStringLiteral("String")) return AcType::string();
  if (name == QStringLiteral("Bool")) return AcType::boolean();
  if (name == QStringLiteral("Any")) return AcType::any();
  if (name == QStringLiteral("Void")) return AcType::voidType();
  // 否则视为自定义类名
  return AcType::classType(name);
}