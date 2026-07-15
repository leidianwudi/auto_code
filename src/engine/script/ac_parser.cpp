/**
 * @file ac_parser.cpp
 * @brief 语法分析器实现文件
 */

#include "ac_parser.h"

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

    // export let / export class / export function / export interface
    if (t.type == TOK_EXPORT) {
      Block::Stmt stmt;
      if (!parseStmt(stmt)) return false;
      block.stmts.append(stmt);
      if (stmt.kind != Block::Stmt::kClassDef && stmt.kind != Block::Stmt::kInterfaceDef &&
          stmt.kind != Block::Stmt::kFuncDef) {
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
      *m_error = QStringLiteral(
                     "expected 'import', 'export', 'class', 'interface', 'function', 'let', or "
                     "'main' at top level at line %1")
                     .arg(peek().line);
      return false;
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
    // class/interface/function 定义以 } 结尾，不需要分号
    if (stmt.kind != Block::Stmt::kClassDef && stmt.kind != Block::Stmt::kInterfaceDef &&
        stmt.kind != Block::Stmt::kFuncDef && stmt.kind != Block::Stmt::kIf &&
        stmt.kind != Block::Stmt::kFor && stmt.kind != Block::Stmt::kWhile &&
        stmt.kind != Block::Stmt::kSwitch) {
      if (!expect(TOK_SEMI, QStringLiteral("expected ';' after statement"))) return false;
    }
  }
  return expect(TOK_RBRACE, QStringLiteral("expected '}'"));
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

    *m_error = QStringLiteral(
                   "expected 'let', 'class', 'function', or 'interface' after 'export' at line %1")
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
         m_tokens[m_pos + 1].type == TOK_DIVEQ)) {
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
      // 检查是否为 ident.prop = value（属性赋值）
      if (m_pos + 2 < m_tokens.size() && m_tokens[m_pos + 2].type == TOK_IDENT &&
          m_pos + 3 < m_tokens.size() &&
          (m_tokens[m_pos + 3].type == TOK_EQUALS || m_tokens[m_pos + 3].type == TOK_PLUSEQ ||
           m_tokens[m_pos + 3].type == TOK_MINUSEQ || m_tokens[m_pos + 3].type == TOK_MULEQ ||
           m_tokens[m_pos + 3].type == TOK_DIVEQ)) {
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
      if (peek().type != TOK_IDENT) {
        *m_error =
            QStringLiteral("expected property name after 'this.' at line %1").arg(peek().line);
        return false;
      }
      QString prop = peek().text;
      TokenType assignOp = peek(1).type;
      bool isCompoundAssign = (assignOp == TOK_PLUSEQ || assignOp == TOK_MINUSEQ ||
                               assignOp == TOK_MULEQ || assignOp == TOK_DIVEQ);
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
  if (peek().type != TOK_IDENT) {
    *m_error = QStringLiteral("expected variable name in for at line %1").arg(peek().line);
    return false;
  }
  fs.varName = advance().text;
  if (!expect(TOK_IN, QStringLiteral("expected 'in'"))) return false;
  if (!parseExpr(fs.arrayExpr)) return false;
  if (!expect(TOK_RPAREN, QStringLiteral("expected ')'"))) return false;
  return parseBlock(fs.body);
}

bool AcParser::parseIfStmt(IfStmt &is) {
  advance();
  if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after if"))) return false;
  if (!parseExpr(is.condition)) return false;
  if (!expect(TOK_RPAREN, QStringLiteral("expected ')'"))) return false;
  if (!parseBlock(is.thenBlock)) return false;

  // 检查 else 或 else if
  if (peek().type == TOK_ELSE) {
    advance();  // skip else
    is.hasElse = true;

    // else if 链式写法
    if (peek().type == TOK_IF) {
      is.elseIfBranch = new IfStmt();
      is.elseIfBranch->isElseIf = true;
      return parseIfStmt(*is.elseIfBranch);
    }

    // 普通 else { }
    return parseBlock(is.elseBlock);
  }
  return true;
}

bool AcParser::parseImportStmt(ImportStmt &imp) {
  // import { A, B } from "file"
  if (!expect(TOK_LBRACE, QStringLiteral("expected '{' after 'import'"))) return false;

  do {
    if (peek().type != TOK_IDENT) {
      *m_error = QStringLiteral("expected identifier in import list at line %1").arg(peek().line);
      return false;
    }
    imp.names.append(advance().text);
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
  return parseBlock(ws.body);
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
      prop.value = new Expr();
      if (peek().type == TOK_EQUALS) {
        advance();
        if (!parseExpr(*prop.value)) {
          delete prop.value;
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
      prop.value = new Expr();
      if (peek().type == TOK_EQUALS) {
        advance();
        if (!parseExpr(*prop.value)) {
          delete prop.value;
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

bool AcParser::parseExpr(Expr &expr) { return parseTernary(expr); }

bool AcParser::parseTernary(Expr &expr) {
  if (!parseLogicalOr(expr)) return false;
  if (peek().type == TOK_QUESTION) {
    int ternaryLine = peek().line;
    advance();
    Expr *cond = new Expr(std::move(expr));
    Expr *trueExpr = new Expr();
    if (!parseLogicalOr(*trueExpr)) {
      delete cond;
      delete trueExpr;
      return false;
    }
    if (!expect(TOK_COLON, QStringLiteral("expected ':' in ternary expression"))) {
      delete cond;
      delete trueExpr;
      return false;
    }
    Expr *falseExpr = new Expr();
    if (!parseTernary(*falseExpr)) {
      delete cond;
      delete trueExpr;
      delete falseExpr;
      return false;
    }
    expr.kind = Expr::kTernary;
    expr.line = ternaryLine;
    expr.left = cond;
    expr.right = trueExpr;
    expr.operand = falseExpr;
    return true;
  }
  return true;
}

bool AcParser::parseLogicalOr(Expr &expr) {
  if (!parseLogicalAnd(expr)) return false;
  while (peek().type == TOK_OR) {
    Token opToken = peek();
    advance();
    Expr *left = new Expr(std::move(expr));
    Expr *right = new Expr();
    if (!parseLogicalAnd(*right)) {
      delete left;
      delete right;
      return false;
    }
    Expr binary;
    binary.kind = Expr::kBinary;
    binary.line = opToken.line;
    binary.binOp = Expr::kOr;
    binary.left = left;
    binary.right = right;
    expr = std::move(binary);
  }
  return true;
}

bool AcParser::parseLogicalAnd(Expr &expr) {
  if (!parseComparison(expr)) return false;
  while (peek().type == TOK_AND) {
    Token opToken = peek();
    advance();
    Expr *left = new Expr(std::move(expr));
    Expr *right = new Expr();
    if (!parseComparison(*right)) {
      delete left;
      delete right;
      return false;
    }
    Expr binary;
    binary.kind = Expr::kBinary;
    binary.line = opToken.line;
    binary.binOp = Expr::kAnd;
    binary.left = left;
    binary.right = right;
    expr = std::move(binary);
  }
  return true;
}

bool AcParser::parseComparison(Expr &expr) {
  if (!parseAddSub(expr)) return false;
  while (peek().type == TOK_EQ || peek().type == TOK_NEQ || peek().type == TOK_LT ||
         peek().type == TOK_GT || peek().type == TOK_LTE || peek().type == TOK_GTE) {
    Token opToken = peek();
    Expr::BinaryOp op;
    switch (peek().type) {
      case TOK_EQ:
        op = Expr::kEq;
        break;
      case TOK_NEQ:
        op = Expr::kNeq;
        break;
      case TOK_LT:
        op = Expr::kLt;
        break;
      case TOK_GT:
        op = Expr::kGt;
        break;
      case TOK_LTE:
        op = Expr::kLte;
        break;
      case TOK_GTE:
        op = Expr::kGte;
        break;
      default:
        break;
    }
    advance();
    Expr *left = new Expr(std::move(expr));
    Expr *right = new Expr();
    if (!parseAddSub(*right)) {
      delete left;
      delete right;
      return false;
    }
    Expr binary;
    binary.kind = Expr::kBinary;
    binary.line = opToken.line;
    binary.binOp = op;
    binary.left = left;
    binary.right = right;
    expr = std::move(binary);
  }
  return true;
}

bool AcParser::parseAddSub(Expr &expr) {
  if (!parseMulDiv(expr)) return false;
  while (peek().type == TOK_PLUS || peek().type == TOK_MINUS) {
    Token opToken = peek();
    Expr::BinaryOp op;
    if (peek().type == TOK_PLUS) {
      op = Expr::kAdd;
      advance();
    } else {
      op = Expr::kSub;
      advance();
    }
    Expr *left = new Expr(std::move(expr));
    Expr *right = new Expr();
    if (!parseMulDiv(*right)) {
      delete left;
      delete right;
      return false;
    }
    Expr binary;
    binary.kind = Expr::kBinary;
    binary.line = opToken.line;
    binary.binOp = op;
    binary.left = left;
    binary.right = right;
    expr = std::move(binary);
  }
  return true;
}

bool AcParser::parseMulDiv(Expr &expr) {
  if (!parsePrimary(expr)) return false;
  while (peek().type == TOK_MUL || peek().type == TOK_DIV) {
    Token opToken = peek();
    Expr::BinaryOp op;
    if (peek().type == TOK_MUL) {
      op = Expr::kMul;
      advance();
    } else {
      op = Expr::kDiv;
      advance();
    }
    Expr *left = new Expr(std::move(expr));
    Expr *right = new Expr();
    if (!parsePrimary(*right)) {
      delete left;
      delete right;
      return false;
    }
    Expr binary;
    binary.kind = Expr::kBinary;
    binary.line = opToken.line;
    binary.binOp = op;
    binary.left = left;
    binary.right = right;
    expr = std::move(binary);
  }
  // 处理链式访问：expr.prop 或 expr.method()
  if (peek().type == TOK_DOT) {
    advance();
    if (peek().type != TOK_IDENT) {
      *m_error = QStringLiteral("expected property name after '.' at line %1").arg(peek().line);
      return false;
    }
    QString memberName = advance().text;
    if (peek().type == TOK_LPAREN) {
      // 链式方法调用：expr.method()
      advance();
      Expr chained;
      chained.kind = Expr::kMethodCall;
      chained.line = peek().line;
      chained.methodCall.methodName = memberName;
      chained.methodCall.object = new Expr(std::move(expr));
      while (peek().type != TOK_RPAREN && peek().type != TOK_EOF) {
        auto *arg = new Expr();
        if (!parseLogicalOr(*arg)) {
          delete arg;
          return false;
        }
        chained.methodCall.args.append(arg);
        if (peek().type == TOK_COMMA) advance();
      }
      if (!expect(TOK_RPAREN,
                  QStringLiteral("expected ')' after method call at line %1").arg(peek().line)))
        return false;
      expr = std::move(chained);
    } else {
      // 链式属性访问：expr.prop（暂不支持深层链式）
      *m_error = QStringLiteral("chained property access is not supported yet at line %1")
                     .arg(peek().line);
      return false;
    }
  }
  return true;
}

bool AcParser::parsePrimary(Expr &expr) {
  Token t = peek();

  if (t.type == TOK_NOT) {
    advance();
    Expr *operand = new Expr();
    if (!parsePrimary(*operand)) {
      delete operand;
      return false;
    }
    expr.kind = Expr::kUnary;
    expr.line = t.line;
    expr.unaryOp = Expr::kNot;
    expr.operand = operand;
    return true;
  }

  if (t.type == TOK_MINUS) {
    advance();
    if (peek().type == TOK_NUMBER) {
      expr.kind = Expr::kNumber;
      expr.numVal = -advance().text.toDouble();
      return true;
    }
    Expr *right = new Expr();
    if (!parsePrimary(*right)) {
      delete right;
      return false;
    }
    Expr binary;
    binary.kind = Expr::kBinary;
    binary.binOp = Expr::kSub;
    binary.left = new Expr();
    binary.left->kind = Expr::kNumber;
    binary.left->numVal = 0;
    binary.right = right;
    expr = std::move(binary);
    return true;
  }

  if (t.type == TOK_THIS) {
    advance();
    if (peek().type == TOK_DOT) {
      advance();
      if (peek().type != TOK_IDENT) {
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
          auto *arg = new Expr();
          if (!parseLogicalOr(*arg)) {
            delete arg;
            return false;
          }
          expr.methodCall.args.append(arg);
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
      expr.left = new Expr();
      expr.left->kind = Expr::kThis;
      expr.right = new Expr();
      if (!parseExpr(*expr.right)) return false;
      return expect(TOK_RBRACKET, QStringLiteral("expected ']'"));
    }
    expr.kind = Expr::kThis;
    return true;
  }

  if (t.type == TOK_SUPER) {
    advance();
    if (peek().type != TOK_DOT) {
      *m_error = QStringLiteral("expected '.' after 'super' at line %1").arg(peek().line);
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
        auto *arg = new Expr();
        if (!parseLogicalOr(*arg)) {
          delete arg;
          return false;
        }
        expr.methodCall.args.append(arg);
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
    advance();
    if (peek().type != TOK_IDENT) {
      *m_error = QStringLiteral("expected class name after 'new' at line %1").arg(peek().line);
      return false;
    }
    expr.kind = Expr::kNewInstance;
    expr.className = advance().text;
    if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after class name"))) return false;
    // 解析构造参数列表（支持任意多个参数）
    if (peek().type != TOK_RPAREN) {
      do {
        Expr *arg = new Expr();
        if (!parseLogicalOr(*arg)) {
          delete arg;
          return false;
        }
        expr.constructorArgs.append(arg);
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
            auto *arg = new Expr();
            if (!parseLogicalOr(*arg)) {
              delete arg;
              return false;
            }
            expr.funcCall.args.append(arg);
            if (peek().type == TOK_COMMA) advance();
          }
          return expect(TOK_RPAREN, QStringLiteral("expected ')' after static method call"));
        }
        return true;
      }
      if (peek().type == TOK_DOT) {
        advance();
        if (peek().type != TOK_IDENT) {
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
            auto *arg = new Expr();
            if (!parseLogicalOr(*arg)) {
              delete arg;
              return false;
            }
            expr.methodCall.args.append(arg);
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
        expr.left = new Expr();
        expr.left->kind = Expr::kIdent;
        expr.left->ident = name;
        expr.left->line = t.line;
        expr.right = new Expr();
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
    if (peek().type != TOK_IDENT) {
      *m_error = QStringLiteral("expected key in object at line %1").arg(peek().line);
      return false;
    }
    ObjectEntry entry;
    entry.key = advance().text;
    if (!expect(TOK_COLON, QStringLiteral("expected ':'"))) return false;
    entry.value = new Expr();
    if (!parseExpr(*entry.value)) {
      delete entry.value;
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
    auto *item = new Expr();
    if (!parseExpr(*item)) {
      delete item;
      return false;
    }
    expr.arrItems.append(item);
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
    auto *arg = new Expr();
    if (!parseLogicalOr(*arg)) {
      delete arg;
      return false;
    }
    expr.funcCall.args.append(arg);
    if (peek().type == TOK_COMMA) advance();
  }
  return expect(TOK_RPAREN, QStringLiteral("expected ')'"));
}

bool AcParser::parseTemplateString(Expr &expr) {
  Token tok = advance();
  QString raw = tok.text;

  QVector<Expr *> parts;

  int i = 0;
  int n = raw.size();
  QString currentStr;

  while (i < n) {
    if (raw[i] == '$' && i + 1 < n && raw[i + 1] == '{') {
      if (!currentStr.isEmpty()) {
        Expr *strExpr = new Expr();
        strExpr->kind = Expr::kString;
        strExpr->strVal = currentStr;
        parts.append(strExpr);
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

      Expr *subExpr = new Expr();
      if (!parseExpr(*subExpr)) {
        delete subExpr;
        m_tokens = savedTokens;
        m_pos = savedPos;
        return false;
      }
      parts.append(subExpr);

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
    Expr *strExpr = new Expr();
    strExpr->kind = Expr::kString;
    strExpr->strVal = currentStr;
    parts.append(strExpr);
  }

  if (parts.isEmpty()) {
    expr.kind = Expr::kString;
    expr.strVal = QString();
    return true;
  }

  if (parts.size() == 1) {
    expr = std::move(*parts[0]);
    delete parts[0];
    return true;
  }

  Expr *result = parts[0];
  for (int j = 1; j < parts.size(); ++j) {
    Expr *binary = new Expr();
    binary->kind = Expr::kBinary;
    binary->binOp = Expr::kAdd;
    binary->left = result;
    binary->right = parts[j];
    result = binary;
  }
  expr = std::move(*result);
  delete result;
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