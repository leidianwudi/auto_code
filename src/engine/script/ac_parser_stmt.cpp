/**
 * @file ac_parser_stmt.cpp
 * @brief 语句解析实现文件
 */

#include "../ac_language.h"
#include "ac_parser.h"

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
    advance();
    stmt.kind = Block::Stmt::kFor;
    return parseForStmt(stmt.forStmt);
  }

  if (t.type == TOK_IF) {
    advance();
    stmt.kind = Block::Stmt::kIf;
    return parseIfStmt(stmt.ifStmt);
  }

  if (t.type == TOK_WHILE) {
    advance();
    stmt.kind = Block::Stmt::kWhile;
    return parseWhileStmt(stmt.whileStmt);
  }

  if (t.type == TOK_SWITCH) {
    advance();
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
      int savedPos = m_pos;
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
      // 检查 this.prop 后面是否跟 '=' 或复合赋值运算符
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
          if (!expect(TOK_EQUALS,
                      QStringLiteral("expected '=' after 'this.%1'").arg(stmt.assign.thisProp)))
            return false;
        }
        stmt.kind = Block::Stmt::kAssign;
        return parseExpr(stmt.assign.value);
      }
      // 回退到表达式语句（恢复位置，让 parseExpr 从 this 开始解析）
      m_pos = savedPos;
      stmt.kind = Block::Stmt::kExpr;
      return parseExpr(stmt.exprStmt);
    }
    // this 作为表达式语句
    stmt.kind = Block::Stmt::kExpr;
    return parseExpr(stmt.exprStmt);
  }

  // 默认：表达式语句
  stmt.kind = Block::Stmt::kExpr;
  return parseExpr(stmt.exprStmt);
}

// ── 语句辅助解析函数 ──

bool AcParser::parseCallStmt(CallStmt &cs) {
  if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after 'call'"))) return false;
  if (!parseExpr(cs.className)) return false;
  if (!expect(TOK_COMMA, QStringLiteral("expected ','"))) return false;
  if (!parseExpr(cs.funcName)) return false;
  if (peek().type == TOK_COMMA) {
    advance();
    if (!parseExpr(cs.args)) return false;
  }
  if (!expect(TOK_RPAREN, QStringLiteral("expected ')' after call args"))) return false;
  return true;
}

bool AcParser::parseAssignStmt(AssignStmt &as) {
  as.name = advance().text;
  Token opToken = peek();
  if (opToken.type == TOK_PLUSEQ) {
    as.compoundOp = 1;
    advance();
  } else if (opToken.type == TOK_MINUSEQ) {
    as.compoundOp = 2;
    advance();
  } else if (opToken.type == TOK_MULEQ) {
    as.compoundOp = 3;
    advance();
  } else if (opToken.type == TOK_DIVEQ) {
    as.compoundOp = 4;
    advance();
  } else if (opToken.type == TOK_MODEQ) {
    as.compoundOp = 5;
    advance();
  } else {
    if (!expect(TOK_EQUALS, QStringLiteral("expected '='"))) return false;
  }
  return parseExpr(as.value);
}

bool AcParser::parseIndexAssignStmt(IndexAssignStmt &ias) {
  // 手动解析对象表达式（不包括 [] 后缀）
  Token t = peek();
  if (t.type == TOK_IDENT) {
    ias.objectExpr.kind = Expr::kIdent;
    ias.objectExpr.ident = advance().text;
    ias.objectExpr.line = t.line;
  } else if (t.type == TOK_THIS) {
    ias.objectExpr.kind = Expr::kThis;
    advance();
  } else if (t.type == TOK_SUPER) {
    ias.objectExpr.kind = Expr::kIdent;
    ias.objectExpr.ident = QString::fromLatin1(AcKeyword::kSuper);
    advance();
  } else if (t.type == TOK_LPAREN) {
    advance();
    if (!parseExpr(ias.objectExpr)) return false;
    if (!expect(TOK_RPAREN, QStringLiteral("expected ')'"))) return false;
  } else {
    *m_error =
        QStringLiteral("expected identifier or expression before '[' at line %1").arg(t.line);
    return false;
  }
  // 处理 .prop 和 .method() 后缀，遇到 [ 则停止
  while (peek().type == TOK_DOT) {
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
      if (ias.objectExpr.kind == Expr::kIdent) {
        chained.methodCall.objName = ias.objectExpr.ident;
      }
      chained.methodCall.object = std::make_unique<Expr>(std::move(ias.objectExpr));
      while (peek().type != TOK_RPAREN && peek().type != TOK_EOF) {
        auto arg = std::make_unique<Expr>();
        if (!parseLogicalOr(*arg)) return false;
        chained.methodCall.args.push_back(std::move(arg));
        if (peek().type == TOK_COMMA) advance();
      }
      if (!expect(TOK_RPAREN, QStringLiteral("expected ')' after method call"))) return false;
      ias.objectExpr = std::move(chained);
    } else {
      Expr propAccess;
      propAccess.kind = Expr::kPropAccess;
      propAccess.line = peek().line;
      propAccess.prop = memberName;
      propAccess.propObject = std::make_unique<Expr>(std::move(ias.objectExpr));
      ias.objectExpr = std::move(propAccess);
    }
  }
  // 现在应该是 [
  if (!expect(TOK_LBRACKET, QStringLiteral("expected '[' for index assignment"))) return false;
  if (!parseExpr(ias.indexExpr)) return false;
  if (!expect(TOK_RBRACKET, QStringLiteral("expected ']'"))) return false;
  if (!expect(TOK_EQUALS, QStringLiteral("expected '='"))) return false;
  return parseExpr(ias.value);
}

bool AcParser::parseForStmt(ForStmt &fs) {
  if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after 'for'"))) return false;

  // for (let i = 0; i < 10; i++)
  if (peek().type == TOK_LET) {
    advance();
    if (peek().type != TOK_IDENT) {
      *m_error = QStringLiteral("expected variable name after 'let' at line %1").arg(peek().line);
      return false;
    }
    fs.varName = advance().text;
    m_declaredVars->insert(fs.varName);
    // 处理类型注解：let i: Number = 0 或 let ch: String in str
    if (peek().type == TOK_COLON) {
      advance();
      fs.varType = advance().text;
    }
    // for-in: for (let ch: String in str)
    if (peek().type == TOK_IN) {
      advance();
      if (!parseExpr(fs.arrayExpr)) return false;
    } else {
      // 标准 for: for (let i = 0; i < n; i++)
      Block::Stmt initStmt;
      initStmt.kind = Block::Stmt::kAssign;
      initStmt.assign.name = fs.varName;
      if (!expect(TOK_EQUALS, QStringLiteral("expected '='"))) return false;
      if (!parseExpr(initStmt.assign.value)) return false;
      fs.initBlock.stmts.push_back(std::move(initStmt));
      if (!expect(TOK_SEMI, QStringLiteral("expected ';'"))) return false;
      if (!parseExpr(fs.condition)) return false;
      if (!expect(TOK_SEMI, QStringLiteral("expected ';'"))) return false;
      if (!parseExpr(fs.updateExpr)) return false;
      fs.isStandard = true;
    }
  }
  // for (item in collection)
  else if (peek().type == TOK_IDENT) {
    fs.varName = advance().text;
    if (peek().type == TOK_COLON) {
      advance();
      fs.varType = advance().text;
    }
    if (peek().type == TOK_IN) {
      advance();
      if (!parseExpr(fs.arrayExpr)) return false;
    } else {
      // 传统 C 风格 for 循环
      if (!expect(TOK_SEMI, QStringLiteral("expected ';'"))) return false;
      if (!parseExpr(fs.condition)) return false;
      if (!expect(TOK_SEMI, QStringLiteral("expected ';'"))) return false;
      if (!parseExpr(fs.updateExpr)) return false;
      fs.isStandard = true;
    }
  }

  if (!expect(TOK_RPAREN, QStringLiteral("expected ')' after for statement"))) return false;
  return parseBlockOrStmt(fs.body);
}

bool AcParser::parseIfStmt(IfStmt &is) {
  if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after 'if'"))) return false;
  if (!parseExpr(is.condition)) return false;
  if (!expect(TOK_RPAREN, QStringLiteral("expected ')' after if condition"))) return false;
  if (!parseBlockOrStmt(is.thenBlock)) return false;

  // else if / else
  if (peek().type == TOK_ELSE) {
    advance();
    if (peek().type == TOK_IF) {
      // else if 链
      advance();
      is.hasElse = true;
      is.elseIfBranch = std::make_unique<IfStmt>();
      is.elseIfBranch->isElseIf = true;
      return parseIfStmt(*is.elseIfBranch);
    } else {
      // 普通 else
      is.hasElse = true;
      return parseBlockOrStmt(is.elseBlock);
    }
  }
  return true;
}

bool AcParser::parseImportStmt(ImportStmt &imp) {
  if (!expect(TOK_LBRACE, QStringLiteral("expected '{' after 'import'"))) return false;
  while (peek().type != TOK_RBRACE && peek().type != TOK_EOF) {
    if (peek().type != TOK_IDENT) {
      *m_error = QStringLiteral("expected identifier in import list at line %1").arg(peek().line);
      return false;
    }
    QString name = advance().text;
    QString alias = name;
    if (peek().type == TOK_AS) {
      advance();
      if (peek().type != TOK_IDENT) {
        *m_error = QStringLiteral("expected alias after 'as' at line %1").arg(peek().line);
        return false;
      }
      alias = advance().text;
    }
    imp.names.append(name);
    if (alias != name) imp.aliases.insert(name, alias);
    if (peek().type == TOK_COMMA) advance();
  }
  if (!expect(TOK_RBRACE, QStringLiteral("expected '}' after import list"))) return false;
  if (!expect(TOK_FROM, QStringLiteral("expected 'from' after import list"))) return false;
  if (peek().type != TOK_STRING) {
    *m_error = QStringLiteral("expected file path string after 'from' at line %1").arg(peek().line);
    return false;
  }
  imp.filePath = advance().text;
  return true;
}

bool AcParser::parseWhileStmt(WhileStmt &ws) {
  if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after 'while'"))) return false;
  if (!parseExpr(ws.condition)) return false;
  if (!expect(TOK_RPAREN, QStringLiteral("expected ')' after while condition"))) return false;
  return parseBlockOrStmt(ws.body);
}

bool AcParser::parseSwitchStmt(SwitchStmt &ss) {
  if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after 'switch'"))) return false;
  if (!parseExpr(ss.expr)) return false;
  if (!expect(TOK_RPAREN, QStringLiteral("expected ')' after switch expression"))) return false;
  if (!expect(TOK_LBRACE, QStringLiteral("expected '{' after switch"))) return false;

  while (peek().type != TOK_RBRACE && peek().type != TOK_EOF) {
    if (peek().type == TOK_CASE) {
      advance();
      SwitchCase sc;
      sc.isDefault = false;
      if (!parseExpr(sc.value)) return false;
      if (!expect(TOK_COLON, QStringLiteral("expected ':' after case value"))) return false;
      while (peek().type != TOK_CASE && peek().type != TOK_DEFAULT && peek().type != TOK_RBRACE &&
             peek().type != TOK_EOF) {
        Block::Stmt stmt;
        if (!parseStmt(stmt)) return false;
        sc.body.stmts.append(stmt);
        if (stmt.kind != Block::Stmt::kIf && stmt.kind != Block::Stmt::kFor &&
            stmt.kind != Block::Stmt::kWhile && stmt.kind != Block::Stmt::kSwitch &&
            stmt.kind != Block::Stmt::kClassDef && stmt.kind != Block::Stmt::kInterfaceDef &&
            stmt.kind != Block::Stmt::kEnumDef && stmt.kind != Block::Stmt::kFuncDef) {
          if (!expect(TOK_SEMI, QStringLiteral("expected ';' after statement"))) return false;
        }
      }
      ss.cases.append(sc);
    } else if (peek().type == TOK_DEFAULT) {
      advance();
      if (!expect(TOK_COLON, QStringLiteral("expected ':' after 'default'"))) return false;
      SwitchCase sc;
      sc.isDefault = true;
      while (peek().type != TOK_CASE && peek().type != TOK_DEFAULT && peek().type != TOK_RBRACE &&
             peek().type != TOK_EOF) {
        Block::Stmt stmt;
        if (!parseStmt(stmt)) return false;
        sc.body.stmts.append(stmt);
        if (stmt.kind != Block::Stmt::kIf && stmt.kind != Block::Stmt::kFor &&
            stmt.kind != Block::Stmt::kWhile && stmt.kind != Block::Stmt::kSwitch &&
            stmt.kind != Block::Stmt::kClassDef && stmt.kind != Block::Stmt::kInterfaceDef &&
            stmt.kind != Block::Stmt::kEnumDef && stmt.kind != Block::Stmt::kFuncDef) {
          if (!expect(TOK_SEMI, QStringLiteral("expected ';' after statement"))) return false;
        }
      }
      ss.cases.append(sc);
    } else {
      *m_error = QStringLiteral("expected 'case' or 'default' at line %1").arg(peek().line);
      return false;
    }
  }
  return expect(TOK_RBRACE, QStringLiteral("expected '}' after switch"));
}

bool AcParser::parseClassDef(ClassDef &cd) {
  if (peek().type != TOK_IDENT) {
    *m_error = QStringLiteral("expected class name at line %1").arg(peek().line);
    return false;
  }
  cd.name = advance().text;

  // extends ParentClass
  if (peek().type == TOK_EXTENDS) {
    advance();
    if (peek().type != TOK_IDENT) {
      *m_error =
          QStringLiteral("expected parent class name after 'extends' at line %1").arg(peek().line);
      return false;
    }
    cd.baseClass = advance().text;
  }

  // implements Interface1, Interface2
  if (peek().type == TOK_IMPLEMENTS) {
    advance();
    while (peek().type == TOK_IDENT) {
      cd.interfaces.append(advance().text);
      if (peek().type == TOK_COMMA) advance();
    }
  }

  if (!expect(TOK_LBRACE, QStringLiteral("expected '{' after class name"))) return false;

  while (peek().type != TOK_RBRACE && peek().type != TOK_EOF) {
    // 访问修饰符
    AccessLevel access = AccessLevel::kPublic;
    if (peek().type == TOK_PUBLIC) {
      advance();
      access = AccessLevel::kPublic;
    } else if (peek().type == TOK_PROTECTED) {
      advance();
      access = AccessLevel::kProtected;
    } else if (peek().type == TOK_PRIVATE) {
      advance();
      access = AccessLevel::kPrivate;
    }

    if (peek().type == TOK_STATIC) {
      advance();
      if (peek().type == TOK_FUNCTION) {
        advance();
        MethodDef md;
        if (!parseMethodDef(md)) return false;
        md.access = access;
        md.isStatic = true;
        cd.methods.append(md);
      } else if (peek().type == TOK_LET) {
        // static let property: Type [= value];
        advance();
        QString name = advance().text;
        ObjectEntry prop;
        prop.key = name;
        prop.isStatic = true;
        prop.access = access;
        if (peek().type == TOK_COLON) {
          advance();
          advance();  // skip type name
        }
        if (peek().type == TOK_EQUALS) {
          advance();
          prop.value = std::make_unique<Expr>();
          if (!parseExpr(*prop.value)) return false;
        }
        cd.properties.append(prop);
      } else if (peek().type == TOK_IDENT) {
        QString name = advance().text;
        if (peek().type == TOK_EQUALS) {
          advance();
          ObjectEntry prop;
          prop.key = name;
          prop.isStatic = true;
          prop.access = access;
          prop.value = std::make_unique<Expr>();
          if (!parseExpr(*prop.value)) return false;
          cd.properties.append(prop);
        }
      }
    } else if (peek().type == TOK_CONSTRUCTOR) {
      // constructor(param: Type, ...) { ... }
      advance();
      MethodDef md;
      md.name = QStringLiteral("constructor");
      if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after 'constructor'"))) return false;
      while (peek().type == TOK_IDENT) {
        ParamDef pd;
        pd.name = advance().text;
        if (peek().type == TOK_COLON) {
          advance();
          pd.type = parseType();
        }
        md.params.append(pd);
        m_declaredVars->insert(pd.name);
        if (peek().type == TOK_COMMA) advance();
      }
      if (!expect(TOK_RPAREN, QStringLiteral("expected ')' after parameters"))) return false;
      if (!parseBlock(md.body)) return false;
      md.access = access;
      cd.methods.append(md);
    } else if (peek().type == TOK_FUNCTION) {
      advance();
      MethodDef md;
      if (!parseMethodDef(md)) return false;
      md.access = access;
      cd.methods.append(md);
    } else if (peek().type == TOK_OVERRIDE) {
      // override function name(): Type { ... }
      advance();
      if (!expect(TOK_FUNCTION, QStringLiteral("expected 'function' after 'override'")))
        return false;
      MethodDef md;
      if (!parseMethodDef(md)) return false;
      md.access = access;
      md.isOverride = true;
      cd.methods.append(md);
    } else if (peek().type == TOK_LET) {
      // let property: Type [= value];
      advance();
      QString name = advance().text;
      ObjectEntry prop;
      prop.key = name;
      if (peek().type == TOK_COLON) {
        advance();
        advance();  // skip type name
      }
      if (peek().type == TOK_EQUALS) {
        advance();
        prop.value = std::make_unique<Expr>();
        if (!parseExpr(*prop.value)) return false;
      }
      cd.properties.append(prop);
    } else if (peek().type == TOK_IDENT) {
      QString name = advance().text;
      if (peek().type == TOK_EQUALS) {
        advance();
        ObjectEntry prop;
        prop.key = name;
        prop.value = std::make_unique<Expr>();
        if (!parseExpr(*prop.value)) return false;
        cd.properties.append(prop);
      }
    }

    // 方法体以 } 结束，不需要分号；属性声明以 ; 结束，跳过即可
    if (peek().type == TOK_SEMI) advance();
  }
  return expect(TOK_RBRACE, QStringLiteral("expected '}' after class body"));
}

bool AcParser::parseInterfaceDef(InterfaceDef &iface) {
  if (peek().type != TOK_IDENT) {
    *m_error = QStringLiteral("expected interface name at line %1").arg(peek().line);
    return false;
  }
  iface.name = advance().text;

  if (!expect(TOK_LBRACE, QStringLiteral("expected '{' after interface name"))) return false;

  while (peek().type != TOK_RBRACE && peek().type != TOK_EOF) {
    if (peek().type == TOK_FUNCTION) {
      advance();
      InterfaceMethod im;
      if (peek().type != TOK_IDENT) {
        *m_error = QStringLiteral("expected method name at line %1").arg(peek().line);
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
      if (peek().type == TOK_COLON) {
        advance();
        im.returnType = parseType();
      }
      iface.methods.append(im);
    }
    if (peek().type != TOK_RBRACE) {
      if (!expect(TOK_SEMI, QStringLiteral("expected ';' after interface member"))) return false;
    }
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

  int autoValue = 0;
  while (peek().type != TOK_RBRACE && peek().type != TOK_EOF) {
    if (peek().type != TOK_IDENT) {
      *m_error = QStringLiteral("expected enum member name at line %1").arg(peek().line);
      return false;
    }
    QString memberName = advance().text;
    EnumMember member;
    member.name = memberName;
    if (peek().type == TOK_EQUALS) {
      advance();
      if (peek().type == TOK_NUMBER) {
        member.value = QJsonValue(advance().text.toDouble());
        autoValue = member.value.toInt() + 1;
      } else {
        *m_error =
            QStringLiteral("expected number value for enum member at line %1").arg(peek().line);
        return false;
      }
      member.hasValue = true;
    } else {
      member.value = QJsonValue(autoValue);
      member.hasValue = false;
      ++autoValue;
    }
    ed.members.append(member);
    if (peek().type == TOK_COMMA) advance();
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
    if (peek().type != TOK_COLON) {
      *m_error =
          QStringLiteral("parameter '%1' requires a type annotation (e.g. %1: Type) at line %2")
              .arg(pd.name)
              .arg(peek().line);
      return false;
    }
    advance();
    pd.type = parseType();
    md.params.append(pd);
    m_declaredVars->insert(pd.name);
    if (peek().type == TOK_COMMA) advance();
  }

  if (!expect(TOK_RPAREN, QStringLiteral("expected ')' after parameters"))) return false;

  if (peek().type == TOK_COLON) {
    advance();
    md.returnType = parseType();
  }

  return parseBlock(md.body);
}

bool AcParser::parseReturnStmt(Expr &retVal) {
  if (peek().type == TOK_SEMI || peek().type == TOK_RBRACE || peek().type == TOK_EOF) {
    retVal.kind = Expr::kNull;
    return true;
  }
  return parseExpr(retVal);
}