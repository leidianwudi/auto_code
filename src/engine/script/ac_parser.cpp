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
  m_error = QStringLiteral("%1 at line %2").arg(msg).arg(peek().loc.line);
  return false;
}

bool AcParser::expectSemi(const QString &msg, int stmtLine) {
  if (peek().type == TokenType::kSemi) {
    advance();
    return true;
  }
  // 分号缺失时，错误定位到语句所在行而不是下一个 token 的行，
  // 避免错误行号跳到后续行（如下一行是空行/注释/另一条语句）导致波浪线画错位置
  m_error = QStringLiteral("%1 at line %2").arg(msg).arg(stmtLine > 0 ? stmtLine : peek().loc.line);
  return false;
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
  // parseType() 的类型名错误（如大小写错误 string/number）会设置 m_error 但返回 any() 继续解析，
  // 此处兜底：解析过程中只要设置了错误消息，即使 parseProgram 返回 true 也视为解析失败
  if (ok && !m_error.isEmpty()) return false;
  return ok;
}

bool AcParser::declareVar(const QString &name, int line) {
  Q_ASSERT(!m_scopes.isEmpty());
  if (m_scopes.last().contains(name)) {
    m_error = QStringLiteral("变量 '%1' 重复声明 at line %2").arg(name).arg(line);
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
      if (!parseImportStmt(stmt.importStmt)) return false;
      block.stmts.append(stmt);
      if (!expectSemi(QStringLiteral("expected ';' after import statement"), t.loc.line)) return false;
      continue;
    }

    // export let / export class / export function / export interface / export enum
    if (t.type == TokenType::kExport) {
      Block::Stmt stmt;
      if (!parseStmt(stmt)) return false;
      block.stmts.append(stmt);
      if (stmt.kind != Block::Stmt::kClassDef && stmt.kind != Block::Stmt::kInterfaceDef &&
          stmt.kind != Block::Stmt::kEnumDef && stmt.kind != Block::Stmt::kFuncDef) {
        if (!expectSemi(QStringLiteral("expected ';' after statement"), stmt.loc.line)) return false;
      }
      continue;
    }

    if (t.type == TokenType::kClass) {
      advance();
      Block::Stmt stmt;
      stmt.loc = t.loc;
      stmt.filePath = m_filePath;
      stmt.kind = Block::Stmt::kClassDef;
      if (!parseClassDef(stmt.classDef)) return false;
      block.stmts.append(stmt);
    } else if (t.type == TokenType::kInterface) {
      advance();
      Block::Stmt stmt;
      stmt.loc = t.loc;
      stmt.filePath = m_filePath;
      stmt.kind = Block::Stmt::kInterfaceDef;
      if (!parseInterfaceDef(stmt.interfaceDef)) return false;
      block.stmts.append(stmt);
    } else if (t.type == TokenType::kEnum) {
      advance();
      Block::Stmt stmt;
      stmt.loc = t.loc;
      stmt.filePath = m_filePath;
      stmt.kind = Block::Stmt::kEnumDef;
      if (!parseEnumDef(stmt.enumDef)) return false;
      block.stmts.append(stmt);
    } else if (t.type == TokenType::kFunction) {
      advance();
      Block::Stmt stmt;
      stmt.loc = t.loc;
      stmt.filePath = m_filePath;
      stmt.kind = Block::Stmt::kFuncDef;
      if (!parseMethodDef(stmt.funcDef)) return false;
      block.stmts.append(stmt);
    } else if (t.type == TokenType::kLet) {
      advance();
      if (peek().type != TokenType::kIdent) {
        m_error = QStringLiteral("expected variable name after 'let' at line %1").arg(peek().loc.line);
        return false;
      }
      if (!declareVar(peek().text, peek().loc.line)) return false;
      Block::Stmt stmt;
      stmt.loc = t.loc;
      stmt.filePath = m_filePath;
      stmt.kind = Block::Stmt::kAssign;
      if (!parseAssignStmt(stmt.assign)) return false;
      stmt.assign.loc = t.loc;
      block.stmts.append(stmt);
      if (!expectSemi(QStringLiteral("expected ';' after statement"), stmt.loc.line)) return false;
    } else if (t.type == TokenType::kIdent && t.text == QString::fromLatin1(AcKeyword::kMain)) {
      advance();
      if (!parseBlock(block)) return false;
    } else {
      // 顶层语句（函数调用、赋值、控制流等）——顶层即隐式 main 函数体，
      // 与 parseBlock 相同：以 } 结尾的语句不要求分号
      Block::Stmt stmt;
      if (!parseStmt(stmt)) return false;
      block.stmts.append(stmt);
      if (stmtNeedsSemi(stmt, /*blockAllowed=*/true)) {
        if (!expectSemi(QStringLiteral("expected ';' after statement"), stmt.loc.line)) return false;
      }
      continue;
    }
  }
  return true;
}

bool AcParser::parseBlock(Block &block) {
  if (!expect(TokenType::kLBrace, QStringLiteral("expected '{'"))) return false;
  ScopeGuard _sg(m_scopes);
  while (peek().type != TokenType::kRBrace && peek().type != TokenType::kEof) {
    Block::Stmt stmt;
    if (!parseStmt(stmt)) return false;
    block.stmts.append(stmt);
    if (stmtNeedsSemi(stmt, /*blockAllowed=*/true)) {
      if (!expectSemi(QStringLiteral("expected ';' after statement"), stmt.loc.line)) return false;
    }
  }
  return expect(TokenType::kRBrace, QStringLiteral("expected '}'"));
}

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
  if (!parseStmt(stmt)) return false;
  block.stmts.append(stmt);
  if (stmtNeedsSemi(stmt, /*blockAllowed=*/false)) {
    if (!expectSemi(QStringLiteral("expected ';' after statement"), stmt.loc.line)) return false;
  }
  return true;
}

// ── 类型解析 ──

AcType AcParser::parseType() {
  // 类型必须以标识符开头（内建类型名或自定义类名）
  if (peek().type != TokenType::kIdent) {
    m_error = QStringLiteral("expected type name at line %1").arg(peek().loc.line);
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
    m_error = QStringLiteral(
                  "bare 'Array' type requires element type: use Array<Type> or Type[] at line %1")
                  .arg(peek().loc.line);
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
      m_error = QStringLiteral(
                    "unknown type '%1' at line %2 — type names are case-sensitive, did you mean "
                    "'%3'?")
                    .arg(typeName)
                    .arg(typeToken.loc.line)
                    .arg(suggestion);
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