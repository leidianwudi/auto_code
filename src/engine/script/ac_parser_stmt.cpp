/**
 * @file ac_parser_stmt.cpp
 * @brief 语句解析实现文件
 */

#include "../ac_language.h"
#include "ac_parser.h"

// ── 语句解析 ──

bool AcParser::parseStmt(Block::Stmt &stmt) {
  Token t = peek();
  stmt.loc = t.loc;            // 记录语句起始行号
  stmt.filePath = m_filePath;  // 记录语句所属源文件

  // ── import { A, B } from "file" ──
  if (t.type == TokenType::kImport) {
    advance();
    stmt.kind = Block::Stmt::kImport;
    return parseImportStmt(stmt.importStmt);
  }

  // ── export let / export class / export function / export interface ──
  if (t.type == TokenType::kExport) {
    advance();
    Token next = peek();

    if (next.type == TokenType::kLet) {
      advance();
      if (peek().type != TokenType::kIdent) {
        m_error = QStringLiteral("expected variable name after 'export let' at line %1")
                      .arg(peek().loc.line);
        return false;
      }
      if (!declareVar(peek().text, peek().loc.line)) return false;
      stmt.kind = Block::Stmt::kAssign;
      if (!parseAssignStmt(stmt.assign)) return false;
      stmt.assign.isExported = true;
      stmt.assign.isDeclaration = true;
      return true;
    }

    if (next.type == TokenType::kClass) {
      advance();
      stmt.kind = Block::Stmt::kClassDef;
      if (!parseClassDef(stmt.classDef)) return false;
      stmt.classDef.isExported = true;
      return true;
    }

    if (next.type == TokenType::kFunction) {
      advance();
      stmt.kind = Block::Stmt::kFuncDef;
      if (!parseMethodDef(stmt.funcDef)) return false;
      stmt.funcDef.isExported = true;
      return true;
    }

    if (next.type == TokenType::kInterface) {
      advance();
      stmt.kind = Block::Stmt::kInterfaceDef;
      if (!parseInterfaceDef(stmt.interfaceDef)) return false;
      stmt.interfaceDef.isExported = true;
      return true;
    }

    if (next.type == TokenType::kEnum) {
      advance();
      stmt.kind = Block::Stmt::kEnumDef;
      if (!parseEnumDef(stmt.enumDef)) return false;
      stmt.enumDef.isExported = true;
      return true;
    }

    m_error =
        QStringLiteral(
            "expected 'let', 'class', 'function', 'interface', or 'enum' after 'export' at line %1")
            .arg(next.loc.line);
    return false;
  }

  if (t.type == TokenType::kClass) {
    advance();
    stmt.kind = Block::Stmt::kClassDef;
    return parseClassDef(stmt.classDef);
  }

  if (t.type == TokenType::kInterface) {
    advance();
    stmt.kind = Block::Stmt::kInterfaceDef;
    return parseInterfaceDef(stmt.interfaceDef);
  }

  if (t.type == TokenType::kEnum) {
    advance();
    stmt.kind = Block::Stmt::kEnumDef;
    return parseEnumDef(stmt.enumDef);
  }

  if (t.type == TokenType::kFunction) {
    advance();
    stmt.kind = Block::Stmt::kFuncDef;
    return parseMethodDef(stmt.funcDef);
  }

  if (t.type == TokenType::kReturn) {
    advance();
    stmt.kind = Block::Stmt::kReturn;
    return parseReturnStmt(stmt.returnValue);
  }

  if (t.type == TokenType::kIdent && t.text == QString::fromLatin1(AcKeyword::kCall)) {
    advance();
    stmt.kind = Block::Stmt::kCall;
    return parseCallStmt(stmt.call);
  }

  if (t.type == TokenType::kFor) {
    advance();
    stmt.kind = Block::Stmt::kFor;
    stmt.forStmt.loc = t.loc;
    return parseForStmt(stmt.forStmt);
  }

  if (t.type == TokenType::kIf) {
    advance();
    stmt.kind = Block::Stmt::kIf;
    return parseIfStmt(stmt.ifStmt);
  }

  if (t.type == TokenType::kWhile) {
    advance();
    stmt.kind = Block::Stmt::kWhile;
    return parseWhileStmt(stmt.whileStmt);
  }

  if (t.type == TokenType::kSwitch) {
    advance();
    stmt.kind = Block::Stmt::kSwitch;
    return parseSwitchStmt(stmt.switchStmt);
  }

  if (t.type == TokenType::kBreak) {
    advance();
    stmt.kind = Block::Stmt::kBreak;
    return true;
  }

  if (t.type == TokenType::kContinue) {
    advance();
    stmt.kind = Block::Stmt::kContinue;
    return true;
  }

  if (t.type == TokenType::kUsing) {
    advance();
    if (peek().type != TokenType::kIdent) {
      m_error =
          QStringLiteral("expected variable name after 'using' at line %1").arg(peek().loc.line);
      return false;
    }
    stmt.usingStmt.varName = advance().text;
    stmt.usingStmt.loc = t.loc;  // using 关键字所在行（引用/重命名定位用）
    if (!declareVar(stmt.usingStmt.varName, t.loc.line)) return false;
    if (!expect(TokenType::kEquals, QStringLiteral("expected '=' after 'using varName'")))
      return false;
    stmt.kind = Block::Stmt::kUsing;
    stmt.usingStmt.value = std::make_unique<Expr>();
    return parseExpr(*stmt.usingStmt.value);
  }

  if (t.type == TokenType::kLet) {
    advance();
    if (peek().type != TokenType::kIdent) {
      m_error =
          QStringLiteral("expected variable name after 'let' at line %1").arg(peek().loc.line);
      return false;
    }
    if (!peek().text.isEmpty() && peek().text[0].isDigit()) {
      m_error =
          QStringLiteral("variable name cannot start with a digit at line %1").arg(peek().loc.line);
      return false;
    }
    if (!declareVar(peek().text, t.loc.line)) return false;
    stmt.kind = Block::Stmt::kAssign;
    if (!parseAssignStmt(stmt.assign)) return false;
    stmt.assign.loc = t.loc;
    stmt.assign.isDeclaration = true;
    return true;
  }

  if (t.type == TokenType::kConst) {
    advance();
    if (peek().type != TokenType::kIdent) {
      m_error =
          QStringLiteral("expected variable name after 'const' at line %1").arg(peek().loc.line);
      return false;
    }
    if (!declareVar(peek().text, t.loc.line)) return false;
    stmt.kind = Block::Stmt::kAssign;
    if (!parseAssignStmt(stmt.assign)) return false;
    stmt.assign.loc = t.loc;
    stmt.assign.isDeclaration = true;
    stmt.assign.isConst = true;
    return true;
  }

  if (t.type == TokenType::kDo) {
    // do…while：先执行循环体，再判断条件（结尾必须有分号）
    advance();
    stmt.kind = Block::Stmt::kWhile;
    stmt.whileStmt.isDoWhile = true;
    if (!parseBlockOrStmt(stmt.whileStmt.body)) return false;
    if (!expect(TokenType::kWhile, QStringLiteral("expected 'while' after 'do' block")))
      return false;
    if (!expect(TokenType::kLParen, QStringLiteral("expected '(' after 'while'"))) return false;
    if (!parseExpr(stmt.whileStmt.condition)) return false;
    if (!expect(TokenType::kRParen, QStringLiteral("expected ')' after while condition")))
      return false;
    return true;
  }

  if (t.type == TokenType::kTry) {
    advance();
    return parseTryStmt(stmt);
  }

  if (t.type == TokenType::kThrow) {
    advance();
    stmt.kind = Block::Stmt::kThrow;
    return parseExpr(stmt.returnValue);  // 抛出值复用 returnValue 成员
  }

  if (t.type == TokenType::kLBrace) {
    // 独立块作用域：{ stmts }
    stmt.kind = Block::Stmt::kBlock;
    return parseBlock(stmt.blockBody);
  }

  if (t.type == TokenType::kSemi) {
    // 空语句：单独的分号无效果（do…while 结尾分号、多余分号容错）
    advance();
    stmt.kind = Block::Stmt::kBlock;
    return true;
  }

  if (t.type == TokenType::kIdent) {
    return parseIdentStmt(stmt, t);
  }

  if (t.type == TokenType::kThis) {
    return parseThisStmt(stmt);
  }

  // 默认：表达式语句
  // 独立的值字面量（数字/字符串/布尔等）作为语句没有任何效果，视为错误并定位到该行。
  // 若不在此拦截，`11111111111` 会被当作合法表达式，随后在“缺分号”检查时
  // 用下一个 token 的行号报错，导致波浪线画到错误的行上。
  if (t.type == TokenType::kNumber || t.type == TokenType::kString || t.type == TokenType::kTrue ||
      t.type == TokenType::kFalse || t.type == TokenType::kNull ||
      t.type == TokenType::kUndefined || t.type == TokenType::kTemplateString) {
    m_error = QStringLiteral("独立的字面值语句没有效果 at line %1").arg(t.loc.line);
    return false;
  }
  stmt.kind = Block::Stmt::kExpr;
  return parseExpr(stmt.exprStmt);
}

bool AcParser::parseTryStmt(Block::Stmt &stmt) {
  stmt.kind = Block::Stmt::kTry;
  if (!parseBlockOrStmt(stmt.tryStmt.tryBody)) return false;

  bool hasHandler = false;
  if (peek().type == TokenType::kCatch) {
    advance();
    hasHandler = true;
    stmt.tryStmt.hasCatch = true;
    if (peek().type == TokenType::kLParen) {  // catch (e) — 错误变量可选
      advance();
      if (peek().type != TokenType::kIdent) {
        m_error = QStringLiteral("expected catch variable name at line %1").arg(peek().loc.line);
        return false;
      }
      stmt.tryStmt.catchVar = advance().text;
      if (!expect(TokenType::kRParen, QStringLiteral("expected ')' after catch variable")))
        return false;
    }
    if (!parseBlockOrStmt(stmt.tryStmt.catchBody)) return false;
  }
  if (peek().type == TokenType::kFinally) {
    advance();
    hasHandler = true;
    stmt.tryStmt.hasFinally = true;
    if (!parseBlockOrStmt(stmt.tryStmt.finallyBody)) return false;
  }
  if (!hasHandler) {
    m_error =
        QStringLiteral("expected 'catch' or 'finally' after 'try' at line %1").arg(peek().loc.line);
    return false;
  }
  return true;
}

bool AcParser::tryParseStaticAssign(Block::Stmt &stmt) {
  if (m_pos + 3 < m_tokens.size() && m_tokens[m_pos + 1].type == TokenType::kScope &&
      m_tokens[m_pos + 2].type == TokenType::kIdent &&
      m_tokens[m_pos + 3].type == TokenType::kEquals) {
    stmt.kind = Block::Stmt::kAssign;
    stmt.assign.isStatic = true;
    stmt.assign.staticClassName = advance().text;
    advance();                          // skip ::
    stmt.assign.name = advance().text;  // member name
    if (!expect(TokenType::kEquals, QStringLiteral("expected '='"))) return false;
    return parseExpr(stmt.assign.value);
  }
  return false;
}

// indexTargetFollowedByIncDec — 前瞻 ident[...] 语句形式：跳到与开头 '[' 匹配的 ']'，
// 其后紧跟 ++/-- 时返回 true（索引自增/自减语句需走表达式路径）。不消耗 token。
bool AcParser::indexTargetFollowedByIncDec() const {
  int depth = 0;
  int i = m_pos + 1;  // m_pos+1 是 '['（调用方已确认）
  for (; i < m_tokens.size(); ++i) {
    const TokenType ty = m_tokens[i].type;
    if (ty == TokenType::kLBracket) {
      ++depth;
    } else if (ty == TokenType::kRBracket) {
      --depth;
      if (depth == 0) {
        ++i;
        break;
      }
    } else if (ty == TokenType::kEof) {
      return false;
    }
  }
  return i < m_tokens.size() &&
         (m_tokens[i].type == TokenType::kPlusPlus || m_tokens[i].type == TokenType::kMinusMinus);
}

bool AcParser::parseIdentStmt(Block::Stmt &stmt, const Token &t) {
  // ClassName::prop = value  — 静态属性赋值
  if (tryParseStaticAssign(stmt)) return true;
  // ClassName::member() 或 ClassName::member  — 静态访问表达式语句
  if (m_pos + 1 < m_tokens.size() && m_tokens[m_pos + 1].type == TokenType::kScope) {
    stmt.kind = Block::Stmt::kExpr;
    return parseExpr(stmt.exprStmt);
  }
  if (m_pos + 1 < m_tokens.size() && (m_tokens[m_pos + 1].type == TokenType::kEquals ||
                                      m_tokens[m_pos + 1].type == TokenType::kPlusEq ||
                                      m_tokens[m_pos + 1].type == TokenType::kMinusEq ||
                                      m_tokens[m_pos + 1].type == TokenType::kMulEq ||
                                      m_tokens[m_pos + 1].type == TokenType::kDivEq ||
                                      m_tokens[m_pos + 1].type == TokenType::kModEq)) {
    if (!m_declaredVars->contains(t.text)) {
      m_error = QStringLiteral(
                    "variable '%1' must be declared with 'let' "
                    "before assignment at line %2")
                    .arg(t.text, QString::number(t.loc.line));
      return false;
    }
    stmt.kind = Block::Stmt::kAssign;
    return parseAssignStmt(stmt.assign);
  }
  if (m_pos + 1 < m_tokens.size() && m_tokens[m_pos + 1].type == TokenType::kLBracket) {
    // m["k"]++ / arr[i]-- : 索引自增/自减走表达式路径（kIndexAssign 仅接受 = 与复合赋值）
    if (indexTargetFollowedByIncDec()) {
      stmt.kind = Block::Stmt::kExpr;
      return parseExpr(stmt.exprStmt);
    }
    stmt.kind = Block::Stmt::kIndexAssign;
    return parseIndexAssignStmt(stmt.indexAssign);
  }
  if (m_pos + 1 < m_tokens.size() && m_tokens[m_pos + 1].type == TokenType::kLParen) {
    stmt.kind = Block::Stmt::kExpr;
    QString name = advance().text;
    return parseFuncCall(name, stmt.exprStmt);
  }
  if (m_pos + 1 < m_tokens.size() && m_tokens[m_pos + 1].type == TokenType::kDot) {
    // 检查是否为 ident.prop[key] = value（链式索引赋值）
    if (m_pos + 2 < m_tokens.size() && m_tokens[m_pos + 2].type == TokenType::kIdent &&
        m_pos + 3 < m_tokens.size() && m_tokens[m_pos + 3].type == TokenType::kLBracket) {
      stmt.kind = Block::Stmt::kIndexAssign;
      stmt.indexAssign.objectExpr.kind = Expr::kPropAccess;
      stmt.indexAssign.objectExpr.ident = advance().text;
      stmt.indexAssign.objectExpr.loc = t.loc;
      advance();  // skip .
      stmt.indexAssign.objectExpr.prop = advance().text;
      advance();  // skip [
      if (!parseExpr(stmt.indexAssign.indexExpr)) return false;
      if (!expect(TokenType::kRBracket, QStringLiteral("expected ']'"))) return false;
      if (!expect(TokenType::kEquals, QStringLiteral("expected '='"))) return false;
      return parseExpr(stmt.indexAssign.value);
    }
    // 检查是否为 ident.prop = value（属性赋值）
    if (m_pos + 2 < m_tokens.size() && m_tokens[m_pos + 2].type == TokenType::kIdent &&
        m_pos + 3 < m_tokens.size() &&
        (m_tokens[m_pos + 3].type == TokenType::kEquals ||
         m_tokens[m_pos + 3].type == TokenType::kPlusEq ||
         m_tokens[m_pos + 3].type == TokenType::kMinusEq ||
         m_tokens[m_pos + 3].type == TokenType::kMulEq ||
         m_tokens[m_pos + 3].type == TokenType::kDivEq ||
         m_tokens[m_pos + 3].type == TokenType::kModEq)) {
      stmt.kind = Block::Stmt::kPropAssign;
      stmt.propAssign.objectExpr.kind = Expr::kIdent;
      stmt.propAssign.objectExpr.ident = advance().text;
      stmt.propAssign.objectExpr.loc = t.loc;
      advance();  // skip .
      stmt.propAssign.prop = advance().text;
      CompoundOp op = parseCompoundOp();
      if (op != CompoundOp::kNone) {
        stmt.propAssign.compoundOp = op;
      } else {
        if (!expect(TokenType::kEquals, QStringLiteral("expected '='"))) return false;
      }
      return parseExpr(stmt.propAssign.value);
    }
    // 检查是否为 ident.prop[expr] = value（链式索引赋值，走表达式）
    // 或者 ident.prop.method()（方法调用，走表达式）
    stmt.kind = Block::Stmt::kExpr;
    return parseExpr(stmt.exprStmt);
  }
  // 默认：表达式语句
  stmt.kind = Block::Stmt::kExpr;
  return parseExpr(stmt.exprStmt);
}

bool AcParser::parseThisStmt(Block::Stmt &stmt) {
  const int thisLine = peek().loc.line;  // 'this' token 所在行（供赋值语句报错定位）
  if (m_pos + 1 < m_tokens.size() && m_tokens[m_pos + 1].type == TokenType::kDot) {
    int savedPos = m_pos;
    advance();  // skip this
    advance();  // skip .
    if (!isPropertyName(peek().type)) {
      m_error =
          QStringLiteral("expected property name after 'this.' at line %1").arg(peek().loc.line);
      return false;
    }
    QString prop = peek().text;
    TokenType assignOp = peek(1).type;
    bool isCompoundAssign = (assignOp == TokenType::kPlusEq || assignOp == TokenType::kMinusEq ||
                             assignOp == TokenType::kMulEq || assignOp == TokenType::kDivEq ||
                             assignOp == TokenType::kModEq);
    // 检查 this.prop 后面是否跟 '=' 或复合赋值运算符
    if (m_pos + 2 < m_tokens.size() && m_tokens[m_pos + 1].type == TokenType::kDot &&
        m_tokens[m_pos + 2].type == TokenType::kIdent && m_pos + 3 < m_tokens.size() &&
        (m_tokens[m_pos + 3].type == TokenType::kEquals || isCompoundAssign)) {
      advance();  // skip property name
      stmt.assign.thisProp = prop;
      stmt.assign.loc.line = thisLine;  // 行号供类型检查器报错定位（此前缺失导致 "at line 0"）
      CompoundOp op = parseCompoundOp();
      if (op != CompoundOp::kNone) {
        stmt.assign.compoundOp = op;
      } else {
        if (!expect(TokenType::kEquals,
                    QStringLiteral("expected '=' after 'this.%1'").arg(stmt.assign.thisProp)))
          return false;
      }
      stmt.kind = Block::Stmt::kAssign;
      return parseExpr(stmt.assign.value);
    }
    // this.prop [=|+=|-=|*=/=] value 的简单检测
    if (m_pos + 1 < m_tokens.size() &&
        (m_tokens[m_pos + 1].type == TokenType::kEquals || isCompoundAssign)) {
      advance();  // skip property name
      stmt.assign.thisProp = prop;
      stmt.assign.loc.line = thisLine;  // 行号供类型检查器报错定位（此前缺失导致 "at line 0"）
      CompoundOp op = parseCompoundOp();
      if (op != CompoundOp::kNone) {
        stmt.assign.compoundOp = op;
      } else {
        if (!expect(TokenType::kEquals,
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

// ── 语句辅助解析函数 ──

bool AcParser::parseCallStmt(CallStmt &cs) {
  if (!expect(TokenType::kLParen, QStringLiteral("expected '(' after 'call'"))) return false;
  if (!parseExpr(cs.className)) return false;
  if (!expect(TokenType::kComma, QStringLiteral("expected ','"))) return false;
  if (!parseExpr(cs.funcName)) return false;
  if (peek().type == TokenType::kComma) {
    advance();
    if (!parseExpr(cs.args)) return false;
  }
  if (!expect(TokenType::kRParen, QStringLiteral("expected ')' after call args"))) return false;
  return true;
}

CompoundOp AcParser::parseCompoundOp() {
  Token opToken = peek();
  if (opToken.type == TokenType::kPlusEq) {
    advance();
    return CompoundOp::kAdd;
  }
  if (opToken.type == TokenType::kMinusEq) {
    advance();
    return CompoundOp::kSub;
  }
  if (opToken.type == TokenType::kMulEq) {
    advance();
    return CompoundOp::kMul;
  }
  if (opToken.type == TokenType::kDivEq) {
    advance();
    return CompoundOp::kDiv;
  }
  if (opToken.type == TokenType::kModEq) {
    advance();
    return CompoundOp::kMod;
  }
  return CompoundOp::kNone;
}

bool AcParser::parseTypeAnnotation(AcType &outType) {
  if (peek().type != TokenType::kColon) return false;
  advance();  // 消耗 ':'
  outType = parseType();
  return true;
}

bool AcParser::parseAssignStmt(AssignStmt &as) {
  Token nameToken = peek();
  as.name = advance().text;
  as.loc = nameToken.loc;
  // 解析类型注解：let x: Type = ...
  AcType typeAnnotation;
  if (parseTypeAnnotation(typeAnnotation)) {
    as.typeAnnotation = typeAnnotation;
    as.hasTypeAnnotation = true;
  }
  CompoundOp op = parseCompoundOp();
  if (op != CompoundOp::kNone) {
    as.compoundOp = op;
  } else {
    if (!expect(TokenType::kEquals, QStringLiteral("expected '='"))) return false;
  }
  return parseExpr(as.value);
}

bool AcParser::parseIndexAssignStmt(IndexAssignStmt &ias) {
  // 手动解析对象表达式（不包括 [] 后缀）
  Token t = peek();
  if (t.type == TokenType::kIdent) {
    ias.objectExpr.kind = Expr::kIdent;
    ias.objectExpr.ident = advance().text;
    ias.objectExpr.loc = t.loc;
  } else if (t.type == TokenType::kThis) {
    ias.objectExpr.kind = Expr::kThis;
    advance();
  } else if (t.type == TokenType::kSuper) {
    ias.objectExpr.kind = Expr::kIdent;
    ias.objectExpr.ident = QString::fromLatin1(AcKeyword::kSuper);
    advance();
  } else if (t.type == TokenType::kLParen) {
    advance();
    if (!parseExpr(ias.objectExpr)) return false;
    if (!expect(TokenType::kRParen, QStringLiteral("expected ')'"))) return false;
  } else {
    m_error =
        QStringLiteral("expected identifier or expression before '[' at line %1").arg(t.loc.line);
    return false;
  }
  // 处理 .prop 和 .method() 后缀，遇到 [ 则停止
  while (peek().type == TokenType::kDot) {
    advance();
    if (!isPropertyName(peek().type)) {
      m_error = QStringLiteral("expected property name after '.' at line %1").arg(peek().loc.line);
      return false;
    }
    QString memberName = advance().text;
    if (peek().type == TokenType::kLParen) {
      advance();
      Expr chained;
      chained.kind = Expr::kMethodCall;
      chained.loc.line = peek().loc.line;
      chained.methodCall.methodName = memberName;
      if (ias.objectExpr.kind == Expr::kIdent) {
        chained.methodCall.objName = ias.objectExpr.ident;
      }
      chained.methodCall.object = std::make_unique<Expr>(std::move(ias.objectExpr));
      while (peek().type != TokenType::kRParen && peek().type != TokenType::kEof) {
        auto arg = std::make_unique<Expr>();
        if (!parseLogicalOr(*arg)) return false;
        chained.methodCall.args.push_back(std::move(arg));
        if (peek().type == TokenType::kComma) advance();
      }
      if (!expect(TokenType::kRParen, QStringLiteral("expected ')' after method call")))
        return false;
      ias.objectExpr = std::move(chained);
    } else {
      Expr propAccess;
      propAccess.kind = Expr::kPropAccess;
      propAccess.loc.line = peek().loc.line;
      propAccess.prop = memberName;
      propAccess.propObject = std::make_unique<Expr>(std::move(ias.objectExpr));
      ias.objectExpr = std::move(propAccess);
    }
  }
  // 现在应该是 [
  if (!expect(TokenType::kLBracket, QStringLiteral("expected '[' for index assignment")))
    return false;
  if (!parseExpr(ias.indexExpr)) return false;
  if (!expect(TokenType::kRBracket, QStringLiteral("expected ']'"))) return false;
  if (!expect(TokenType::kEquals, QStringLiteral("expected '='"))) return false;
  return parseExpr(ias.value);
}

bool AcParser::parseForStmt(ForStmt &fs) {
  if (!expect(TokenType::kLParen, QStringLiteral("expected '(' after 'for'"))) return false;

  // for 循环变量拥有独立作用域：for (let i = 0; ...) 的 i 仅属于本循环，
  // 多个 for 循环可各自声明同名循环变量，互不干扰。
  ScopeGuard _sg(m_scopes);

  // for (let i = 0; i < 10; i++)
  if (peek().type == TokenType::kLet) {
    advance();
    if (peek().type != TokenType::kIdent) {
      m_error =
          QStringLiteral("expected variable name after 'let' at line %1").arg(peek().loc.line);
      return false;
    }

    fs.varName = advance().text;
    if (!declareVar(fs.varName, fs.loc.line)) return false;
    // 处理类型注解：let i: Number = 0 或 let ch: String in str
    if (peek().type == TokenType::kColon) {
      advance();
      fs.varType = advance().text;
    }
    // for-in: for (let ch: String in str)
    if (peek().type == TokenType::kIn) {
      advance();
      if (!parseExpr(fs.arrayExpr)) return false;
    } else {
      // 标准 for: for (let i = 0; i < n; i++)
      Block::Stmt initStmt;
      initStmt.kind = Block::Stmt::kAssign;
      initStmt.assign.name = fs.varName;
      initStmt.assign.isDeclaration = true;  // let 声明须在最新作用域创建，不覆盖外层同名变量
      if (!expect(TokenType::kEquals, QStringLiteral("expected '='"))) return false;
      if (!parseExpr(initStmt.assign.value)) return false;
      fs.initBlock.stmts.push_back(std::move(initStmt));
      if (!expect(TokenType::kSemi, QStringLiteral("expected ';'"))) return false;
      if (!parseExpr(fs.condition)) return false;
      if (!expect(TokenType::kSemi, QStringLiteral("expected ';'"))) return false;
      if (!parseExpr(fs.updateExpr)) return false;
      fs.isStandard = true;
    }
  }
  // for (item in collection)
  else if (peek().type == TokenType::kIdent) {
    fs.varName = advance().text;
    if (peek().type == TokenType::kColon) {
      advance();
      fs.varType = advance().text;
    }
    if (peek().type == TokenType::kIn) {
      advance();
      if (!parseExpr(fs.arrayExpr)) return false;
    } else {
      // 传统 C 风格 for 循环
      if (!expect(TokenType::kSemi, QStringLiteral("expected ';'"))) return false;
      if (!parseExpr(fs.condition)) return false;
      if (!expect(TokenType::kSemi, QStringLiteral("expected ';'"))) return false;
      if (!parseExpr(fs.updateExpr)) return false;
      fs.isStandard = true;
    }
  }

  if (!expect(TokenType::kRParen, QStringLiteral("expected ')' after for statement"))) return false;
  return parseBlockOrStmt(fs.body);
}

bool AcParser::parseIfStmt(IfStmt &is) {
  if (!expect(TokenType::kLParen, QStringLiteral("expected '(' after 'if'"))) return false;
  if (!parseExpr(is.condition)) return false;
  if (!expect(TokenType::kRParen, QStringLiteral("expected ')' after if condition"))) return false;
  if (!parseBlockOrStmt(is.thenBlock)) return false;

  // else if / else
  if (peek().type == TokenType::kElse) {
    advance();
    if (peek().type == TokenType::kIf) {
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
  imp.loc.line = peek().loc.line;  // import 关键字所在行
  if (!expect(TokenType::kLBrace, QStringLiteral("expected '{' after 'import'"))) return false;
  while (peek().type != TokenType::kRBrace && peek().type != TokenType::kEof) {
    if (peek().type != TokenType::kIdent) {
      m_error =
          QStringLiteral("expected identifier in import list at line %1").arg(peek().loc.line);
      return false;
    }
    const Token nameTok = advance();
    QString name = nameTok.text;
    imp.nameLines.insert(name, nameTok.loc.line);
    QString alias = name;
    if (peek().type == TokenType::kAs) {
      advance();
      if (peek().type != TokenType::kIdent) {
        m_error = QStringLiteral("expected alias after 'as' at line %1").arg(peek().loc.line);
        return false;
      }
      const Token aliasTok = advance();
      alias = aliasTok.text;
      imp.aliasLines.insert(alias, aliasTok.loc.line);
    }
    imp.names.append(name);
    if (alias != name) imp.aliases.insert(name, alias);
    if (peek().type == TokenType::kComma) advance();
  }
  if (!expect(TokenType::kRBrace, QStringLiteral("expected '}' after import list"))) return false;
  if (!expect(TokenType::kFrom, QStringLiteral("expected 'from' after import list"))) return false;
  if (peek().type != TokenType::kString) {
    m_error =
        QStringLiteral("expected file path string after 'from' at line %1").arg(peek().loc.line);
    return false;
  }
  imp.filePath = advance().text;
  return true;
}

bool AcParser::parseWhileStmt(WhileStmt &ws) {
  if (!expect(TokenType::kLParen, QStringLiteral("expected '(' after 'while'"))) return false;
  if (!parseExpr(ws.condition)) return false;
  if (!expect(TokenType::kRParen, QStringLiteral("expected ')' after while condition")))
    return false;
  return parseBlockOrStmt(ws.body);
}

bool AcParser::parseSwitchStmt(SwitchStmt &ss) {
  if (!expect(TokenType::kLParen, QStringLiteral("expected '(' after 'switch'"))) return false;
  if (!parseExpr(ss.expr)) return false;
  if (!expect(TokenType::kRParen, QStringLiteral("expected ')' after switch expression")))
    return false;
  if (!expect(TokenType::kLBrace, QStringLiteral("expected '{' after switch"))) return false;

  while (peek().type != TokenType::kRBrace && peek().type != TokenType::kEof) {
    if (peek().type == TokenType::kCase) {
      advance();
      SwitchCase sc;
      sc.isDefault = false;
      if (!parseExpr(sc.value)) return false;
      if (!expect(TokenType::kColon, QStringLiteral("expected ':' after case value"))) return false;
      while (peek().type != TokenType::kCase && peek().type != TokenType::kDefault &&
             peek().type != TokenType::kRBrace && peek().type != TokenType::kEof) {
        Block::Stmt stmt;
        if (!parseStmt(stmt)) return false;
        sc.body.stmts.append(stmt);
        if (stmtNeedsSemi(stmt, /*blockAllowed=*/false)) {
          if (!expectSemi(QStringLiteral("expected ';' after statement"), stmt.loc.line))
            return false;
        }
      }
      ss.cases.append(sc);
    } else if (peek().type == TokenType::kDefault) {
      advance();
      if (!expect(TokenType::kColon, QStringLiteral("expected ':' after 'default'"))) return false;
      SwitchCase sc;
      sc.isDefault = true;
      while (peek().type != TokenType::kCase && peek().type != TokenType::kDefault &&
             peek().type != TokenType::kRBrace && peek().type != TokenType::kEof) {
        Block::Stmt stmt;
        if (!parseStmt(stmt)) return false;
        sc.body.stmts.append(stmt);
        if (stmtNeedsSemi(stmt, /*blockAllowed=*/false)) {
          if (!expectSemi(QStringLiteral("expected ';' after statement"), stmt.loc.line))
            return false;
        }
      }
      ss.cases.append(sc);
    } else {
      m_error = QStringLiteral("expected 'case' or 'default' at line %1").arg(peek().loc.line);
      return false;
    }
  }
  return expect(TokenType::kRBrace, QStringLiteral("expected '}' after switch"));
}

bool AcParser::parseClassDef(ClassDef &cd) {
  if (peek().type != TokenType::kIdent) {
    m_error = QStringLiteral("expected class name at line %1").arg(peek().loc.line);
    return false;
  }
  cd.name = advance().text;

  // extends ParentClass
  if (peek().type == TokenType::kExtends) {
    advance();
    if (peek().type != TokenType::kIdent) {
      m_error = QStringLiteral("expected parent class name after 'extends' at line %1")
                    .arg(peek().loc.line);
      return false;
    }
    cd.baseClass = advance().text;
  }

  // implements Interface1, Interface2
  if (peek().type == TokenType::kImplements) {
    advance();
    while (peek().type == TokenType::kIdent) {
      cd.interfaces.append(advance().text);
      if (peek().type == TokenType::kComma) advance();
    }
  }

  if (!expect(TokenType::kLBrace, QStringLiteral("expected '{' after class name"))) return false;

  while (peek().type != TokenType::kRBrace && peek().type != TokenType::kEof) {
    int loopStartPos = m_pos;  // 记录迭代起点，用于死循环检测
    // 访问修饰符
    AccessLevel access = AccessLevel::kPublic;
    if (peek().type == TokenType::kPublic) {
      advance();
      access = AccessLevel::kPublic;
    } else if (peek().type == TokenType::kProtected) {
      advance();
      access = AccessLevel::kProtected;
    } else if (peek().type == TokenType::kPrivate) {
      advance();
      access = AccessLevel::kPrivate;
    }

    if (peek().type == TokenType::kStatic) {
      advance();
      if (peek().type == TokenType::kFunction) {
        advance();
        MethodDef md;
        if (!parseMethodDef(md)) return false;
        md.access = access;
        md.isStatic = true;
        cd.methods.append(md);
      } else if (peek().type == TokenType::kLet || peek().type == TokenType::kIdent) {
        if (!parseClassProperty(cd, access, true)) return false;
      }
    } else if (peek().type == TokenType::kConstructor) {
      // constructor(param: Type, ...) { ... }
      advance();
      MethodDef md;
      md.name = QStringLiteral("constructor");
      if (!expect(TokenType::kLParen, QStringLiteral("expected '(' after 'constructor'")))
        return false;
      if (!parseParamList(md.params, /*requireType=*/false, /*allowDefault=*/true,
                          /*declareVars=*/true))
        return false;
      if (!expect(TokenType::kRParen, QStringLiteral("expected ')' after parameters")))
        return false;
      // 支持声明-only 语法：constructor(params);（无函数体，用于 .d.ac 声明文件）
      if (peek().type == TokenType::kSemi) {
        advance();
        md.isDeclaration = true;
      } else {
        if (!parseBlock(md.body)) return false;
      }
      md.access = access;
      cd.methods.append(md);
    } else if (peek().type == TokenType::kFunction) {
      advance();
      MethodDef md;
      if (!parseMethodDef(md)) return false;
      md.access = access;
      cd.methods.append(md);
    } else if (peek().type == TokenType::kOverride) {
      // override function name(): Type { ... }
      advance();
      if (!expect(TokenType::kFunction, QStringLiteral("expected 'function' after 'override'")))
        return false;
      MethodDef md;
      if (!parseMethodDef(md)) return false;
      md.access = access;
      md.isOverride = true;
      cd.methods.append(md);
    } else if (peek().type == TokenType::kLet || peek().type == TokenType::kIdent) {
      if (!parseClassProperty(cd, access, false)) return false;
    }

    // 方法体以 } 结束，不需要分号；属性声明以 ; 结束，跳过即可
    if (peek().type == TokenType::kSemi) advance();

    // 死循环兜底：若本轮未消费任何 token，说明遇到无法识别的类成员，
    // 直接报错退出，避免 m_pos 停滞导致的无限循环（如漏写 function 的方法）
    if (m_pos == loopStartPos) {
      m_error = QStringLiteral(
                    "unexpected token '%1' in class body at line %2 (possible missing "
                    "'function' keyword or invalid member)")
                    .arg(peek().text, QString::number(peek().loc.line));
      return false;
    }
  }
  return expect(TokenType::kRBrace, QStringLiteral("expected '}' after class body"));
}

bool AcParser::parseInterfaceDef(InterfaceDef &iface) {
  if (peek().type != TokenType::kIdent) {
    m_error = QStringLiteral("expected interface name at line %1").arg(peek().loc.line);
    return false;
  }
  iface.name = advance().text;

  if (!expect(TokenType::kLBrace, QStringLiteral("expected '{' after interface name")))
    return false;

  while (peek().type != TokenType::kRBrace && peek().type != TokenType::kEof) {
    if (peek().type == TokenType::kLet) {
      // 对象形状属性契约：let name: Type; / let name?: Type;
      advance();
      ParamDef prop;
      if (peek().type != TokenType::kIdent) {
        m_error = QStringLiteral("expected property name at line %1").arg(peek().loc.line);
        return false;
      }
      prop.name = advance().text;
      if (peek().type == TokenType::kQuestion) {
        advance();
        prop.isOptional = true;
      }
      if (!expect(TokenType::kColon, QStringLiteral("expected ':' after property name")))
        return false;
      prop.type = parseType();
      iface.properties.append(prop);
    } else if (peek().type == TokenType::kFunction) {
      advance();
      InterfaceMethod im;
      if (peek().type != TokenType::kIdent) {
        m_error = QStringLiteral("expected method name at line %1").arg(peek().loc.line);
        return false;
      }
      im.name = advance().text;
      if (!expect(TokenType::kLParen, QStringLiteral("expected '(' after method name")))
        return false;
      // 接口方法：类型注解必须、无默认值、参数名不注入作用域
      if (!parseParamList(im.params, /*requireType=*/true, /*allowDefault=*/false,
                          /*declareVars=*/false))
        return false;
      if (!expect(TokenType::kRParen, QStringLiteral("expected ')' after parameters")))
        return false;
      if (peek().type == TokenType::kColon) {
        advance();
        im.returnType = parseType();
      }
      iface.methods.append(im);
    }
    if (peek().type != TokenType::kRBrace) {
      if (!expect(TokenType::kSemi, QStringLiteral("expected ';' after interface member")))
        return false;
    }
  }
  return expect(TokenType::kRBrace, QStringLiteral("expected '}' after interface body"));
}

bool AcParser::parseEnumDef(EnumDef &ed) {
  if (peek().type != TokenType::kIdent) {
    m_error = QStringLiteral("expected enum name at line %1").arg(peek().loc.line);
    return false;
  }
  ed.name = advance().text;

  if (!expect(TokenType::kLBrace, QStringLiteral("expected '{' after enum name"))) return false;

  int autoValue = 0;
  while (peek().type != TokenType::kRBrace && peek().type != TokenType::kEof) {
    if (peek().type != TokenType::kIdent) {
      m_error = QStringLiteral("expected enum member name at line %1").arg(peek().loc.line);
      return false;
    }
    QString memberName = advance().text;
    EnumMember member;
    member.name = memberName;
    if (peek().type == TokenType::kEquals) {
      advance();
      if (peek().type == TokenType::kNumber) {
        member.value = QJsonValue(advance().text.toDouble());
        autoValue = safeJsonToInt(member.value) + 1;
      } else {
        m_error =
            QStringLiteral("expected number value for enum member at line %1").arg(peek().loc.line);
        return false;
      }
      member.hasValue = true;
    } else {
      member.value = QJsonValue(autoValue);
      member.hasValue = false;
      ++autoValue;
    }
    ed.members.append(member);
    if (peek().type == TokenType::kComma) advance();
  }
  return expect(TokenType::kRBrace, QStringLiteral("expected '}' after enum body"));
}

bool AcParser::parseParamDefault(QJsonValue &out) {
  const Token tok = peek();
  switch (tok.type) {
    case TokenType::kNumber:
      advance();
      out = QJsonValue(tok.text.toDouble());
      return true;
    case TokenType::kMinus: {
      // 负数字面量：- Number
      advance();
      if (peek().type != TokenType::kNumber) {
        m_error = QStringLiteral("expected number after '-' in parameter default value at line %1")
                      .arg(peek().loc.line);
        return false;
      }
      out = QJsonValue(-advance().text.toDouble());
      return true;
    }
    case TokenType::kString:
      advance();
      out = QJsonValue(tok.text);
      return true;
    case TokenType::kTrue:
      advance();
      out = QJsonValue(true);
      return true;
    case TokenType::kFalse:
      advance();
      out = QJsonValue(false);
      return true;
    case TokenType::kNull:
      // 显式 = null：Null 类型（区别于 Undefined「未声明默认值」）
      advance();
      out = QJsonValue();
      return true;
    default:
      m_error =
          QStringLiteral(
              "parameter default value must be a literal (number/string/bool/null) at line %1")
              .arg(tok.loc.line);
      return false;
  }
}

bool AcParser::parseParamList(QVector<ParamDef> &out, bool requireType, bool allowDefault,
                              bool declareVars) {
  while (isParamName(peek().type)) {
    ParamDef pd;
    const Token nameTok = advance();
    pd.name = nameTok.text;
    pd.loc = nameTok.loc;
    if (peek().type == TokenType::kQuestion) {
      advance();
      pd.isOptional = true;
    }
    if (peek().type == TokenType::kColon) {
      advance();
      pd.type = parseType();
    } else if (requireType) {
      m_error =
          QStringLiteral("parameter '%1' requires a type annotation (e.g. %1: Type) at line %2")
              .arg(pd.name)
              .arg(peek().loc.line);
      return false;
    }
    // = 字面量 默认值（自动视为可选参数）
    if (allowDefault && peek().type == TokenType::kEquals) {
      advance();
      if (!parseParamDefault(pd.defaultValue)) return false;
      pd.isOptional = true;
    }
    out.append(pd);
    if (declareVars) m_declaredVars->insert(pd.name);
    if (peek().type == TokenType::kComma) advance();
  }
  return true;
}

bool AcParser::parseMethodDef(MethodDef &md) {
  // dispose 是语言关键字（TokenType::kDispose），但允许作为方法名使用（using/dispose 模式）
  if (peek().type != TokenType::kIdent && peek().type != TokenType::kDispose) {
    m_error = QStringLiteral("expected method name at line %1").arg(peek().loc.line);
    return false;
  }
  md.loc.line = peek().loc.line;  // 记录方法名所在行号
  md.name = advance().text;

  if (!expect(TokenType::kLParen, QStringLiteral("expected '(' after method name"))) return false;

  if (!parseParamList(md.params, /*requireType=*/true, /*allowDefault=*/true,
                      /*declareVars=*/true))
    return false;

  if (!expect(TokenType::kRParen, QStringLiteral("expected ')' after parameters"))) return false;

  // P0: 强制函数返回值类型注解（构造函数除外）
  if (md.name != QStringLiteral("constructor")) {
    if (peek().type != TokenType::kColon) {
      m_error =
          QStringLiteral("function '%1' requires return type annotation (e.g. ): Type) at line %2")
              .arg(md.name)
              .arg(peek().loc.line);
      return false;
    }
    advance();
    md.returnType = parseType();
  } else {
    // 构造函数：可选的返回类型
    if (peek().type == TokenType::kColon) {
      advance();
      md.returnType = parseType();
    }
  }

  // 支持声明-only 语法：function name(params): Type;（无函数体，用于 .d.ac 声明文件）
  if (peek().type == TokenType::kSemi) {
    advance();
    md.isDeclaration = true;
    return true;
  }

  return parseBlock(md.body);
}

bool AcParser::parseClassProperty(ClassDef &cd, AccessLevel access, bool isStatic) {
  if (peek().type == TokenType::kLet) {
    advance();
  }
  if (peek().type != TokenType::kIdent) {
    m_error = QStringLiteral("expected property name at line %1").arg(peek().loc.line);
    return false;
  }
  Token nameToken = advance();
  // 属性名后紧跟 '(' 说明是漏写了 function 关键字的方法定义（如 line2UpLow(...)），
  // 直接报错并返回，避免下方解析把 '(' 遗留下来导致 parseClassDef 死循环
  if (peek().type == TokenType::kLParen) {
    m_error = QStringLiteral(
                  "method '%1' requires the 'function' keyword (e.g. function "
                  "%2(...)) at line %3")
                  .arg(nameToken.text, nameToken.text)
                  .arg(nameToken.loc.line);
    return false;
  }
  ObjectEntry prop;
  prop.key = nameToken.text;
  prop.loc = nameToken.loc;  // 记录属性名所在行号
  prop.isStatic = isStatic;
  prop.access = access;
  // 解析类型注解：let prop: Type = ...
  AcType propType;
  const bool hasTypeAnnotation = parseTypeAnnotation(propType);
  if (hasTypeAnnotation) {
    prop.type = propType;
  }
  const bool hasValue = (peek().type == TokenType::kEquals);
  if (hasValue) {
    advance();
    prop.value = std::make_unique<Expr>();
    if (!parseExpr(*prop.value)) return false;
  }
  // 既无类型注解也无初始值：非法属性声明（例如类体内误写的裸标识符 aaaaa），
  // 必须报错，避免被静默当作无类型属性接受
  if (!hasTypeAnnotation && !hasValue) {
    m_error = QStringLiteral(
                  "invalid class member '%1' — property requires a type annotation "
                  "or an initial value (e.g. %2: Type) at line %3")
                  .arg(nameToken.text, nameToken.text)
                  .arg(nameToken.loc.line);
    return false;
  }
  cd.properties.append(prop);
  return true;
}

bool AcParser::parseReturnStmt(Expr &retVal) {
  if (peek().type == TokenType::kSemi || peek().type == TokenType::kRBrace ||
      peek().type == TokenType::kEof) {
    retVal.kind = Expr::kNull;
    return true;
  }
  return parseExpr(retVal);
}