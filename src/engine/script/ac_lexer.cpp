/**
 * @file ac_lexer.cpp
 * @brief 词法分析器实现文件
 */

#include "ac_lexer.h"

#include <QHash>

#include "../ac_language.h"

/// @brief 跳过行注释（// 到行尾）
void AcLexer::skipLineComment(const QString &source, int &pos) {
  while (pos < source.size() && source[pos] != '\n') ++pos;
}

/// @brief 跳过块注释（/* 到 */）
bool AcLexer::skipBlockComment(const QString &source, int &pos, int &line, int &lineStart,
                               QString &error) {
  pos += 2;
  while (pos < source.size()) {
    if (source[pos] == '*' && pos + 1 < source.size() && source[pos + 1] == '/') {
      pos += 2;
      return true;
    }
    if (source[pos] == '\n') {
      ++line;
      lineStart = pos + 1;
    }
    ++pos;
  }
  error = QStringLiteral("unterminated block comment at line %1").arg(line);
  return false;
}

Token AcLexer::parseStringLiteral(const QString &source, int &pos, const AcLoc &startLoc,
                                  QString &error) {
  int start = ++pos;
  int n = source.size();
  while (pos < n && source[pos] != '"') {
    if (source[pos] == '\\' && pos + 1 < n) ++pos;
    ++pos;
  }
  if (pos >= n) {
    error = QStringLiteral("unterminated string at line %1").arg(startLoc.line);
    return {TokenType::kEof, {}, startLoc};
  }
  QString val = source.mid(start, pos - start);
  val.replace(QStringLiteral("\\\""), QStringLiteral("\""));
  val.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
  val.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
  ++pos;
  return {TokenType::kString, val, startLoc};
}

Token AcLexer::parseTemplateStringLiteral(const QString &source, int &pos, int &line,
                                          int &lineStart, QString &error) {
  int start = ++pos;
  int n = source.size();
  int depth = 0;
  while (pos < n) {
    if (source[pos] == '`' && depth == 0) break;
    if (source[pos] == '\\' && pos + 1 < n) {
      pos += 2;
      continue;
    }
    if (source[pos] == '$' && pos + 1 < n && source[pos + 1] == '{') {
      ++depth;
      pos += 2;
      continue;
    }
    if (source[pos] == '{' && depth > 0) {
      ++depth;
      ++pos;
      continue;
    }
    if (source[pos] == '}' && depth > 0) {
      --depth;
      ++pos;
      continue;
    }
    if (source[pos] == '\n') {
      ++line;
      lineStart = pos + 1;
    }
    ++pos;
  }
  if (pos >= n) {
    error = QStringLiteral("unterminated template string at line %1").arg(line);
    return {TokenType::kEof, {}, {line, pos - lineStart + 1, pos}};
  }
  QString val = source.mid(start, pos - start);
  ++pos;
  return {TokenType::kTemplateString, val, {line, pos - lineStart + 1, pos}};
}

Token AcLexer::parseNumberLiteral(const QString &source, int &pos, const AcLoc &startLoc) {
  int start = pos;
  int n = source.size();
  while (pos < n && source[pos].isDigit()) ++pos;
  if (pos + 1 < n && source[pos] == '.' && source[pos + 1].isDigit()) {
    ++pos;
    while (pos < n && source[pos].isDigit()) ++pos;
  }
  return {TokenType::kNumber, source.mid(start, pos - start), startLoc};
}

/// @brief 关键字到 Token 类型的映射表
static const QHash<QString, TokenType> &keywordMap() {
  static const QHash<QString, TokenType> map = {
      {QString::fromLatin1(AcKeyword::kFor), TokenType::kFor},
      {QString::fromLatin1(AcKeyword::kIn), TokenType::kIn},
      {QString::fromLatin1(AcKeyword::kIf), TokenType::kIf},
      {QString::fromLatin1(AcKeyword::kElse), TokenType::kElse},
      {QString::fromLatin1(AcKeyword::kLet), TokenType::kLet},
      {QString::fromLatin1(AcKeyword::kClass), TokenType::kClass},
      {QString::fromLatin1(AcKeyword::kFunction), TokenType::kFunction},
      {QString::fromLatin1(AcKeyword::kNew), TokenType::kNew},
      {QString::fromLatin1(AcKeyword::kThis), TokenType::kThis},
      {QString::fromLatin1(AcKeyword::kReturn), TokenType::kReturn},
      {QString::fromLatin1(AcKeyword::kTrue), TokenType::kTrue},
      {QString::fromLatin1(AcKeyword::kFalse), TokenType::kFalse},
      {QString::fromLatin1(AcKeyword::kStatic), TokenType::kStatic},
      {QString::fromLatin1(AcKeyword::kPublic), TokenType::kPublic},
      {QString::fromLatin1(AcKeyword::kProtected), TokenType::kProtected},
      {QString::fromLatin1(AcKeyword::kPrivate), TokenType::kPrivate},
      {QString::fromLatin1(AcKeyword::kExtends), TokenType::kExtends},
      {QString::fromLatin1(AcKeyword::kOverride), TokenType::kOverride},
      {QString::fromLatin1(AcKeyword::kInterface), TokenType::kInterface},
      {QString::fromLatin1(AcKeyword::kImplements), TokenType::kImplements},
      {QString::fromLatin1(AcKeyword::kSuper), TokenType::kSuper},
      {QString::fromLatin1(AcKeyword::kExport), TokenType::kExport},
      {QString::fromLatin1(AcKeyword::kImport), TokenType::kImport},
      {QString::fromLatin1(AcKeyword::kFrom), TokenType::kFrom},
      {QString::fromLatin1(AcKeyword::kNull), TokenType::kNull},
      {QString::fromLatin1(AcKeyword::kUndefined), TokenType::kUndefined},
      {QString::fromLatin1(AcKeyword::kWhile), TokenType::kWhile},
      {QString::fromLatin1(AcKeyword::kBreak), TokenType::kBreak},
      {QString::fromLatin1(AcKeyword::kContinue), TokenType::kContinue},
      {QString::fromLatin1(AcKeyword::kSwitch), TokenType::kSwitch},
      {QString::fromLatin1(AcKeyword::kCase), TokenType::kCase},
      {QString::fromLatin1(AcKeyword::kDefault), TokenType::kDefault},
      {QString::fromLatin1(AcKeyword::kEnum), TokenType::kEnum},
      {QString::fromLatin1(AcKeyword::kConstructor), TokenType::kConstructor},
      {QString::fromLatin1(AcKeyword::kUsing), TokenType::kUsing},
      {QString::fromLatin1(AcKeyword::kDispose), TokenType::kDispose},
      {QString::fromLatin1(AcKeyword::kAs), TokenType::kAs},
      {QString::fromLatin1(AcKeyword::kConst), TokenType::kConst},
      {QString::fromLatin1(AcKeyword::kDo), TokenType::kDo},
      {QString::fromLatin1(AcKeyword::kTry), TokenType::kTry},
      {QString::fromLatin1(AcKeyword::kCatch), TokenType::kCatch},
      {QString::fromLatin1(AcKeyword::kFinally), TokenType::kFinally},
      {QString::fromLatin1(AcKeyword::kThrow), TokenType::kThrow},
  };
  return map;
}

Token AcLexer::parseIdentifier(const QString &source, int &pos, const AcLoc &startLoc) {
  int start = pos;
  int n = source.size();
  while (pos < n &&
         ((source[pos].isLetterOrNumber() && source[pos].unicode() < 128) || source[pos] == '_'))
    ++pos;
  QString word = source.mid(start, pos - start);
  auto it = keywordMap().constFind(word);
  Token tok;
  tok.loc = startLoc;
  tok.text = word;
  tok.type = (it != keywordMap().constEnd()) ? it.value() : TokenType::kIdent;
  if (tok.type == TokenType::kIdent) {
    // 词法期驻留：同名标识符全局共享一个 id（供符号表/后续 IR 阶段使用）
    tok.identId = accore::AcIdentPool::ins().intern(word);
  }
  return tok;
}

/// @brief 将源码字符串拆分为 token 序列
QVector<Token> AcLexer::tokenize(const QString &source, QString &error) {
  QVector<Token> tokens;
  int i = 0;
  int line = 1;
  int lineStart = 0;  ///< 当前行首个字符的偏移（列计算基准）
  int n = source.size();

  /// @brief 计算当前位置的源码位置（行/列/偏移）
  auto locAt = [&](int pos) { return AcLoc{line, pos - lineStart + 1, pos}; };

  while (i < n) {
    QChar c = source[i];

    if (c == '\n') {
      ++line;
      lineStart = i + 1;
      ++i;
      continue;
    }
    if (c.isSpace()) {
      ++i;
      continue;
    }
    if (c == '/' && i + 1 < n && source[i + 1] == '/') {
      skipLineComment(source, i);
      continue;
    }

    // 检测块注释 /* ... */
    if (c == '/' && i + 1 < n && source[i + 1] == '*') {
      if (!skipBlockComment(source, i, line, lineStart, error)) {
        return {};  // 块注释未闭合，返回错误
      }
      continue;
    }

    switch (c.unicode()) {
      case '{':
        tokens.append({TokenType::kLBrace, QStringLiteral("{"), locAt(i)});
        ++i;
        break;
      case '}':
        tokens.append({TokenType::kRBrace, QStringLiteral("}"), locAt(i)});
        ++i;
        break;
      case '(':
        tokens.append({TokenType::kLParen, QStringLiteral("("), locAt(i)});
        ++i;
        break;
      case ')':
        tokens.append({TokenType::kRParen, QStringLiteral(")"), locAt(i)});
        ++i;
        break;
      case '[':
        tokens.append({TokenType::kLBracket, QStringLiteral("["), locAt(i)});
        ++i;
        break;
      case ']':
        tokens.append({TokenType::kRBracket, QStringLiteral("]"), locAt(i)});
        ++i;
        break;
      case ',':
        tokens.append({TokenType::kComma, QStringLiteral(","), locAt(i)});
        ++i;
        break;
      case ':':
        if (i + 1 < n && source[i + 1] == ':') {
          tokens.append({TokenType::kScope, QStringLiteral("::"), locAt(i)});
          i += 2;
        } else {
          tokens.append({TokenType::kColon, QStringLiteral(":"), locAt(i)});
          ++i;
        }
        break;
      case '.':
        tokens.append({TokenType::kDot, QStringLiteral("."), locAt(i)});
        ++i;
        break;
      case '+':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TokenType::kPlusEq, QStringLiteral("+="), locAt(i)});
          i += 2;
        } else if (i + 1 < n && source[i + 1] == '+') {
          tokens.append({TokenType::kPlusPlus, QStringLiteral("++"), locAt(i)});
          i += 2;
        } else {
          tokens.append({TokenType::kPlus, QStringLiteral("+"), locAt(i)});
          ++i;
        }
        break;
      case '-':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TokenType::kMinusEq, QStringLiteral("-="), locAt(i)});
          i += 2;
        } else if (i + 1 < n && source[i + 1] == '-') {
          tokens.append({TokenType::kMinusMinus, QStringLiteral("--"), locAt(i)});
          i += 2;
        } else {
          tokens.append({TokenType::kMinus, QStringLiteral("-"), locAt(i)});
          ++i;
        }
        break;
      case '*':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TokenType::kMulEq, QStringLiteral("*="), locAt(i)});
          i += 2;
        } else {
          tokens.append({TokenType::kMul, QStringLiteral("*"), locAt(i)});
          ++i;
        }
        break;
      case '/':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TokenType::kDivEq, QStringLiteral("/="), locAt(i)});
          i += 2;
        } else {
          tokens.append({TokenType::kDiv, QStringLiteral("/"), locAt(i)});
          ++i;
        }
        break;
      case '%':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TokenType::kModEq, QStringLiteral("%="), locAt(i)});
          i += 2;
        } else {
          tokens.append({TokenType::kMod, QStringLiteral("%"), locAt(i)});
          ++i;
        }
        break;
      case '|':
        if (i + 1 < n && source[i + 1] == '|') {
          tokens.append({TokenType::kOr, QStringLiteral("||"), locAt(i)});
          i += 2;
        } else {
          error = QStringLiteral("unexpected character '|' at line %1").arg(line);
          return {};
        }
        break;
      case '&':
        if (i + 1 < n && source[i + 1] == '&') {
          tokens.append({TokenType::kAnd, QStringLiteral("&&"), locAt(i)});
          i += 2;
        } else {
          error = QStringLiteral("unexpected character '&' at line %1").arg(line);
          return {};
        }
        break;
      case '!':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TokenType::kNeq, QStringLiteral("!="), locAt(i)});
          i += 2;
        } else {
          tokens.append({TokenType::kNot, QStringLiteral("!"), locAt(i)});
          ++i;
        }
        break;
      case '=':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TokenType::kEq, QStringLiteral("=="), locAt(i)});
          i += 2;
        } else {
          tokens.append({TokenType::kEquals, QStringLiteral("="), locAt(i)});
          ++i;
        }
        break;
      case '<':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TokenType::kLte, QStringLiteral("<="), locAt(i)});
          i += 2;
        } else {
          tokens.append({TokenType::kLt, QStringLiteral("<"), locAt(i)});
          ++i;
        }
        break;
      case '>':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TokenType::kGte, QStringLiteral(">="), locAt(i)});
          i += 2;
        } else {
          tokens.append({TokenType::kGt, QStringLiteral(">"), locAt(i)});
          ++i;
        }
        break;
      case ';':
        tokens.append({TokenType::kSemi, QStringLiteral(";"), locAt(i)});
        ++i;
        break;
      case '?':
        if (i + 1 < n && source[i + 1] == '?') {
          tokens.append({TokenType::kQuestionQuestion, QStringLiteral("??"), locAt(i)});
          i += 2;
        } else if (i + 1 < n && source[i + 1] == '.' &&
                   !(i + 2 < n && source[i + 2].isDigit())) {
          // `?.` 可选链；后跟数字时按三元处理（如 a ?.5 : 1，与 JS 规范一致）
          tokens.append({TokenType::kQuestionDot, QStringLiteral("?."), locAt(i)});
          i += 2;
        } else {
          tokens.append({TokenType::kQuestion, QStringLiteral("?"), locAt(i)});
          ++i;
        }
        break;
      case '"': {
        Token tok = parseStringLiteral(source, i, locAt(i), error);
        if (!error.isEmpty()) return {};
        tokens.append(tok);
        break;
      }
      case '`': {
        Token tok = parseTemplateStringLiteral(source, i, line, lineStart, error);
        if (!error.isEmpty()) return {};
        tokens.append(tok);
        break;
      }
      default:
        if (c.isDigit()) {
          tokens.append(parseNumberLiteral(source, i, locAt(i)));
        } else if ((c.isLetter() && c.unicode() < 128) || c == '_') {
          tokens.append(parseIdentifier(source, i, locAt(i)));
        } else if (c.unicode() > 127) {
          error = QStringLiteral("unexpected non-ASCII character '%1' at line %2")
                      .arg(c, QString::number(line));
          return {};
        } else {
          error =
              QStringLiteral("unexpected character '%1' at line %2").arg(c, QString::number(line));
          return {};
        }
        break;
    }
  }

  tokens.append({TokenType::kEof, {}, locAt(i)});
  return tokens;
}
