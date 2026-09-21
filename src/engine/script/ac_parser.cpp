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
  return {TokenType::kEof, {}, 0};
}

Token AcParser::peek(int offset) {
  int pos = m_pos + offset;
  if (pos >= 0 && pos < m_tokens.size()) return m_tokens[pos];
  return {TokenType::kEof, {}, 0};
}

Token AcParser::advance() {
  if (m_pos < m_tokens.size()) return m_tokens[m_pos++];
  return {TokenType::kEof, {}, 0};
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
  reportError(AcDiagCode::kSyntaxExpected, msg, peek().loc);
  return false;
}

bool AcParser::expectSemi(const QString &msg, int stmtLine) {
  if (peek().type == TokenType::kSemi) {
    advance();
    return true;
  }
  // 分号缺失时，错误定位到语句所在行而不是下一个 token 的行，
  // 避免错误行号跳到后续行（如下一行是空行/注释/另一条语句）导致波浪线画错位置
  const int line = stmtLine > 0 ? stmtLine : peek().loc.line;
  reportError(AcDiagCode::kSyntaxMissingSemi, msg, AcLoc{line, 0, 0});
  return false;
}

void AcParser::reportError(const QString &code, const QString &msg, const AcLoc &loc) {
  m_error = QStringLiteral("%1 at line %2").arg(msg).arg(loc.line);
  if (m_diags) m_diags->error(code, msg, m_filePath, loc);
}

bool AcParser::isPropertyName(TokenType t) const {
  return t == TokenType::kIdent || t == TokenType::kString || t == TokenType::kDefault || t == TokenType::kCase || t == TokenType::kNull ||
         t == TokenType::kUndefined || t == TokenType::kWhile || t == TokenType::kBreak || t == TokenType::kContinue ||
         t == TokenType::kSwitch || t == TokenType::kFor || t == TokenType::kIf || t == TokenType::kElse || t == TokenType::kReturn ||
         t == TokenType::kClass || t == TokenType::kFunction || t == TokenType::kStatic || t == TokenType::kPublic ||
         t == TokenType::kProtected || t == TokenType::kPrivate || t == TokenType::kExtends || t == TokenType::kOverride ||
         t == TokenType::kInterface || t == TokenType::kImplements || t == TokenType::kSuper || t == TokenType::kExport ||
         t == TokenType::kImport || t == TokenType::kFrom || t == TokenType::kNew || t == TokenType::kLet || t == TokenType::kIn ||
         t == TokenType::kTrue || t == TokenType::kFalse || t == TokenType::kThis || t == TokenType::kEnum || t == TokenType::kUsing ||
         t == TokenType::kDispose;
}

bool AcParser::isParamName(TokenType t) const {
  // from 是关键字（import ... from），但允许用作参数名（builtin.d.ac 中 indexOf/lastIndexOf）
  return t == TokenType::kIdent || t == TokenType::kFrom;
}

// ── 解析入口 ──

bool AcParser::parse(const QVector<Token> &tokens, Block &program, QSet<QString> &declaredVars) {
  m_tokens = tokens;
  m_pos = 0;
  m_error.clear();
  m_declaredVars = &declaredVars;
  m_scopes.clear();
  m_scopes.append(QSet<QString>());  // 全局作用域
  program = Block();
  bool ok = parseProgram(program);
  // 恢复模式：m_error 仅作 legacy 单串，不作为失败判据（全部错误已进诊断收集器）
  if (m_recoveryMode) return true;
  // parseType() 的类型名错误（如大小写错误 string/number）会设置 m_error 但返回 any() 继续解析，
  // 此处兜底：解析过程中只要设置了错误消息，即使 parseProgram 返回 true 也视为解析失败
  if (ok && !m_error.isEmpty()) return false;
  return ok;
}

bool AcParser::declareVar(const QString &name, int line) {
  Q_ASSERT(!m_scopes.isEmpty());
  if (m_scopes.last().contains(name)) {
    reportError(AcDiagCode::kSyntaxDupDecl,
                QStringLiteral("变量 '%1' 重复声明").arg(name), AcLoc{line, 0, 0});
    return false;
  }
  m_scopes.last().insert(name);
  m_declaredVars->insert(name);
  return true;
}

bool AcParser::parseProgram(Block &block) {
  while (peek().type != TokenType::kEof) {
    Token t = peek();

    // import { A, B } from "file"
    if (t.type == TokenType::kImport) {
      advance();
      Block::Stmt stmt;
      stmt.loc = t.loc;
      stmt.filePath = m_filePath;
      stmt.kind = Block::Stmt::kImport;
      if (!parseImportStmt(stmt.importStmt)) {
        if (!recoverStatement(stmt, block, t.loc)) return false;
        continue;
      }
      block.stmts.append(stmt);
      if (!expectSemi(QStringLiteral("expected ';' after import statement"), t.loc.line)) {
        if (!recoverSemi(block)) return false;
        continue;
      }
      continue;
    }

    // export let / export class / export function / export interface / export enum
    if (t.type == TokenType::kExport) {
      Block::Stmt stmt;
      if (!parseStmt(stmt)) {
        if (!recoverStatement(stmt, block, t.loc)) return false;
        continue;
      }
      block.stmts.append(stmt);
      if (stmt.kind != Block::Stmt::kClassDef && stmt.kind != Block::Stmt::kInterfaceDef &&
          stmt.kind != Block::Stmt::kEnumDef && stmt.kind != Block::Stmt::kFuncDef) {
        if (!expectSemi(QStringLiteral("expected ';' after statement"), stmt.loc.line)) {
          if (!recoverSemi(block)) return false;
          continue;
        }
      }
      continue;
    }

    if (t.type == TokenType::kClass) {
      advance();
      Block::Stmt stmt;
      stmt.loc = t.loc;
      stmt.filePath = m_filePath;
      stmt.kind = Block::Stmt::kClassDef;
      if (!parseClassDef(stmt.classDef)) {
        if (!recoverStatement(stmt, block, t.loc)) return false;
        continue;
      }
      block.stmts.append(stmt);
    } else if (t.type == TokenType::kInterface) {
      advance();
      Block::Stmt stmt;
      stmt.loc = t.loc;
      stmt.filePath = m_filePath;
      stmt.kind = Block::Stmt::kInterfaceDef;
      if (!parseInterfaceDef(stmt.interfaceDef)) {
        if (!recoverStatement(stmt, block, t.loc)) return false;
        continue;
      }
      block.stmts.append(stmt);
    } else if (t.type == TokenType::kEnum) {
      advance();
      Block::Stmt stmt;
      stmt.loc = t.loc;
      stmt.filePath = m_filePath;
      stmt.kind = Block::Stmt::kEnumDef;
      if (!parseEnumDef(stmt.enumDef)) {
        if (!recoverStatement(stmt, block, t.loc)) return false;
        continue;
      }
      block.stmts.append(stmt);
    } else if (t.type == TokenType::kFunction) {
      advance();
      Block::Stmt stmt;
      stmt.loc = t.loc;
      stmt.filePath = m_filePath;
      stmt.kind = Block::Stmt::kFuncDef;
      if (!parseMethodDef(stmt.funcDef)) {
        if (!recoverStatement(stmt, block, t.loc)) return false;
        continue;
      }
      block.stmts.append(stmt);
    } else if (t.type == TokenType::kLet) {
      advance();
      if (peek().type != TokenType::kIdent) {
        reportError(AcDiagCode::kSyntaxExpected, QStringLiteral("expected variable name after 'let'"),
                    peek().loc);
        Block::Stmt hole;
        if (!recoverStatement(hole, block, t.loc)) return false;
        continue;
      }
      if (!declareVar(peek().text, peek().loc.line)) {
        Block::Stmt hole;
        if (!recoverStatement(hole, block, t.loc)) return false;
        continue;
      }
      Block::Stmt stmt;
      stmt.loc = t.loc;
      stmt.filePath = m_filePath;
      stmt.kind = Block::Stmt::kAssign;
      if (!parseAssignStmt(stmt.assign)) {
        if (!recoverStatement(stmt, block, t.loc)) return false;
        continue;
      }
      stmt.assign.loc = t.loc;
      block.stmts.append(stmt);
      if (!expectSemi(QStringLiteral("expected ';' after statement"), stmt.loc.line)) {
        if (!recoverSemi(block)) return false;
        continue;
      }
    } else if (t.type == TokenType::kIdent && t.text == QString::fromLatin1(AcKeyword::kMain)) {
      advance();
      if (!parseBlock(block)) {
        if (!m_recoveryMode) return false;
        recoverToStatementBoundary(0);
        continue;
      }
    } else {
      // 顶层语句（函数调用、赋值、控制流等）——顶层即隐式 main 函数体，
      // 与 parseBlock 相同：以 } 结尾的语句不要求分号
      Block::Stmt stmt;
      if (!parseStmt(stmt)) {
        if (!recoverStatement(stmt, block, t.loc)) return false;
        continue;
      }
      block.stmts.append(stmt);
      if (stmtNeedsSemi(stmt, /*blockAllowed=*/true)) {
        if (!expectSemi(QStringLiteral("expected ';' after statement"), stmt.loc.line)) {
          if (!recoverSemi(block)) return false;
          continue;
        }
      }
      continue;
    }
  }
  return true;
}

bool AcParser::parseBlock(Block &block) {
  if (!expect(TokenType::kLBrace, QStringLiteral("expected '{'"))) {
    if (m_recoveryMode) {
      recoverToStatementBoundary(1);
      return true;
    }
    return false;
  }
  ScopeGuard _sg(m_scopes);
  while (peek().type != TokenType::kRBrace && peek().type != TokenType::kEof) {
    Block::Stmt stmt;
    const Token st = peek();
    if (!parseStmt(stmt)) {
      if (!recoverStatement(stmt, block, st.loc)) return false;
      continue;
    }
    block.stmts.append(stmt);
    if (stmtNeedsSemi(stmt, /*blockAllowed=*/true)) {
      if (!expectSemi(QStringLiteral("expected ';' after statement"), stmt.loc.line)) {
        if (!recoverSemi(block)) return false;
        continue;
      }
    }
  }
  return expect(TokenType::kRBrace, QStringLiteral("expected '}'")) ||
         m_recoveryMode;  // 恢复模式下缺 } 不视为致命
}

/// 语句起始关键字集合（错误恢复同步点）：可在此处安全开始一条新语句
static const QSet<TokenType> &statementStartKeywords() {
  static const QSet<TokenType> s = {
      TokenType::kLet,
      TokenType::kConst,
      TokenType::kClass,
      TokenType::kInterface,
      TokenType::kEnum,
      TokenType::kFunction,
      TokenType::kIf,
      TokenType::kFor,
      TokenType::kWhile,
      TokenType::kReturn,
      TokenType::kImport,
      TokenType::kExport,
      TokenType::kUsing,
      TokenType::kTry,
      TokenType::kSwitch,
      TokenType::kThrow,
      TokenType::kBreak,
      TokenType::kContinue};
  return s;
}

bool AcParser::recoverStatement(Block::Stmt &stmt, Block &block, const AcLoc &loc) {
  if (!m_recoveryMode) return false;
  // 残缺占位：kExpr + Expr::kError，下游（类型检查/编译器）按 isRecovered 跳过
  stmt = Block::Stmt();
  stmt.kind = Block::Stmt::kExpr;
  stmt.loc = loc;
  stmt.filePath = m_filePath;
  stmt.isRecovered = true;
  Expr err;
  err.kind = Expr::kError;
  err.loc = loc;
  stmt.exprStmt = err;
  block.stmts.append(stmt);
  recoverToStatementBoundary(0);
  return true;
}

bool AcParser::recoverSemi(Block &block) {
  (void)block;
  if (!m_recoveryMode) return false;
  recoverToStatementBoundary(0);
  return true;
}

void AcParser::recoverToStatementBoundary(int baseBraceDepth) {
  const int start = m_pos;
  int braceDepth = 0;
  const QSet<TokenType> &starts = statementStartKeywords();
  while (m_pos < m_tokens.size()) {
    const TokenType ty = m_tokens[m_pos].type;
    if (ty == TokenType::kEof) break;
    if (braceDepth <= baseBraceDepth) {
      // 当前构造同层/浅层：; 消耗并停止；} 交回外层；语句起始关键字/块不消耗即停
      if (ty == TokenType::kSemi) {
        ++m_pos;
        break;
      }
      if (ty == TokenType::kRBrace) break;
      if (starts.contains(ty) || ty == TokenType::kLBrace) break;
      ++m_pos;
      continue;
    }
    // 深括号内：仅维护深度
    if (ty == TokenType::kLBrace) ++braceDepth;
    else if (ty == TokenType::kRBrace) --braceDepth;
    ++m_pos;
  }
  // 前进保证：错误片段整体无法同步时强制消费 1 个 token，防止外层死循环
  if (m_pos <= start && start < m_tokens.size()) ++m_pos;
}

// ═════════════════════════════════════════════════════════════════════════════
//  语句分类辅助
// ═════════════════════════════════════════════════════════════════════════════

bool AcParser::stmtNeedsSemi(const Block::Stmt &stmt, bool blockAllowed) const {
  // class/interface/enum/function 定义以 } 结尾，不需要分号
  // if/for/while/switch/try 也以 } 结尾，不需要分号
  if (stmt.kind == Block::Stmt::kClassDef || stmt.kind == Block::Stmt::kInterfaceDef ||
      stmt.kind == Block::Stmt::kEnumDef || stmt.kind == Block::Stmt::kFuncDef ||
      stmt.kind == Block::Stmt::kIf || stmt.kind == Block::Stmt::kFor ||
      stmt.kind == Block::Stmt::kWhile || stmt.kind == Block::Stmt::kSwitch ||
      stmt.kind == Block::Stmt::kTry) {
    return false;
  }
  // 独立块语句 { }（blockAllowed=false 时用于单语句位置，保持原行为要求分号）
  if (blockAllowed && stmt.kind == Block::Stmt::kBlock) return false;
  return true;
}

bool AcParser::parseBlockOrStmt(Block &block) {
  if (peek().type == TokenType::kLBrace) {
    return parseBlock(block);
  }
  Block::Stmt stmt;
  const Token st = peek();
  if (!parseStmt(stmt)) {
    if (!recoverStatement(stmt, block, st.loc)) return false;
    return true;
  }
  block.stmts.append(stmt);
  if (stmtNeedsSemi(stmt, /*blockAllowed=*/false)) {
    if (!expectSemi(QStringLiteral("expected ';' after statement"), stmt.loc.line)) {
      return recoverSemi(block);
    }
  }
  return true;
}

// ── 类型解析 ──

AcType AcParser::parseType() {
  // 类型必须以标识符开头（内建类型名或自定义类名）
  if (peek().type != TokenType::kIdent) {
    reportError(AcDiagCode::kSyntaxExpected, QStringLiteral("expected type name"), peek().loc);
    return AcType::any();
  }

  Token typeToken = advance();
  QString typeName = typeToken.text;

  // 泛型参数：Array<T>
  if (peek().type == TokenType::kLt) {
    advance();
    auto elementType = std::make_shared<AcType>(parseType());
    auto type = AcType::arrayOf(*elementType);
    if (!expect(TokenType::kGt, QStringLiteral("expected '>' after type arguments"))) {
      return AcType::any();
    }
    return type;
  }

  // 简单类型名映射
  AcType baseType;
  if (typeName == AcTypeName::kNumber || typeName == AcTypeName::kInt ||
      typeName == AcTypeName::kFloat || typeName == AcTypeName::kDouble) {
    baseType = AcType::number();
  } else if (typeName == AcTypeName::kString) {
    baseType = AcType::string();
  } else if (typeName == AcTypeName::kBool || typeName == AcTypeName::kBoolean) {
    baseType = AcType::boolean();
  } else if (typeName == AcTypeName::kAny) {
    baseType = AcType::any();
  } else if (typeName == AcTypeName::kVoid) {
    baseType = AcType::voidType();
  } else if (typeName == AcTypeName::kArray) {
    // P1b: 禁止弱类型 Array，必须使用 Array<Type> 或 Type[]
    reportError(AcDiagCode::kSyntaxBadType,
                QStringLiteral("bare 'Array' type requires element type: use Array<Type> or Type[]"),
                peek().loc);
    return AcType::any();
  } else if (typeName == AcTypeName::kObject) {
    baseType = AcType::classType(QString::fromLatin1(AcTypeName::kObject));
  } else {
    // 内建类型名必须严格区分大小写：若匹配到内建类型但大小写不同（如 string、number），
    // 报错并提示正确写法，避免被静默当作自定义类名
    QString suggestion;
    if (typeName.compare(AcTypeName::kNumber, Qt::CaseInsensitive) == 0 ||
        typeName.compare(AcTypeName::kInt, Qt::CaseInsensitive) == 0 ||
        typeName.compare(AcTypeName::kFloat, Qt::CaseInsensitive) == 0 ||
        typeName.compare(AcTypeName::kDouble, Qt::CaseInsensitive) == 0) {
      suggestion = QString::fromLatin1(AcTypeName::kNumber);
    } else if (typeName.compare(AcTypeName::kString, Qt::CaseInsensitive) == 0) {
      suggestion = QString::fromLatin1(AcTypeName::kString);
    } else if (typeName.compare(AcTypeName::kBool, Qt::CaseInsensitive) == 0 ||
               typeName.compare(AcTypeName::kBoolean, Qt::CaseInsensitive) == 0) {
      suggestion = QString::fromLatin1(AcTypeName::kBool);
    } else if (typeName.compare(AcTypeName::kAny, Qt::CaseInsensitive) == 0) {
      suggestion = QString::fromLatin1(AcTypeName::kAny);
    } else if (typeName.compare(AcTypeName::kVoid, Qt::CaseInsensitive) == 0) {
      suggestion = QString::fromLatin1(AcTypeName::kVoid);
    } else if (typeName.compare(AcTypeName::kArray, Qt::CaseInsensitive) == 0) {
      suggestion = QString::fromLatin1(AcTypeName::kArray);
    } else if (typeName.compare(AcTypeName::kObject, Qt::CaseInsensitive) == 0) {
      suggestion = QString::fromLatin1(AcTypeName::kObject);
    }
    if (!suggestion.isEmpty()) {
      // 此处消息格式特殊（行号在中间），legacy 串保持原样，诊断走干净消息
      m_error = QStringLiteral(
                    "unknown type '%1' at line %2 — type names are case-sensitive, did you mean "
                    "'%3'?")
                    .arg(typeName)
                    .arg(typeToken.loc.line)
                    .arg(suggestion);
      if (m_diags) {
        m_diags->error(AcDiagCode::kSyntaxBadType,
                       QStringLiteral(
                           "unknown type '%1' — type names are case-sensitive, did you mean '%2'?")
                           .arg(typeName)
                           .arg(suggestion),
                       m_filePath, typeToken.loc);
      }
      return AcType::any();
    }
    // 自定义类类型
    baseType = AcType::classType(typeName);
  }

  // TypeScript 风格数组后缀：Type[]（支持多维 Type[][]）
  while (peek().type == TokenType::kLBracket && peek(1).type == TokenType::kRBracket) {
    advance();  // [
    advance();  // ]
    baseType = AcType::arrayOf(baseType);
  }

  return baseType;
}
