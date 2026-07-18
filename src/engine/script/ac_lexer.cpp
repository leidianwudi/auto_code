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
bool AcLexer::skipBlockComment(const QString &source, int &pos, int &line, QString &error) {
  pos += 2;
  while (pos < source.size()) {
    if (source[pos] == '*' && pos + 1 < source.size() && source[pos + 1] == '/') {
      pos += 2;
      return true;
    }
    if (source[pos] == '\n') ++line;
    ++pos;
  }
  error = QStringLiteral("unterminated block comment at line %1").arg(line);
  return false;
}

Token AcLexer::parseStringLiteral(const QString &source, int &pos, int line, QString &error) {
  int start = ++pos;
  int n = source.size();
  while (pos < n && source[pos] != '"') {
    if (source[pos] == '\\' && pos + 1 < n) ++pos;
    ++pos;
  }
  if (pos >= n) {
    error = QStringLiteral("unterminated string at line %1").arg(line);
    return {TOK_EOF, {}, line};
  }
  QString val = source.mid(start, pos - start);
  val.replace(QStringLiteral("\\\""), QStringLiteral("\""));
  val.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
  val.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
  ++pos;
  return {TOK_STRING, val, line};
}

Token AcLexer::parseTemplateStringLiteral(const QString &source, int &pos, int &line,
                                          QString &error) {
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
    if (source[pos] == '\n') ++line;
    ++pos;
  }
  if (pos >= n) {
    error = QStringLiteral("unterminated template string at line %1").arg(line);
    return {TOK_EOF, {}, line};
  }
  QString val = source.mid(start, pos - start);
  ++pos;
  return {TOK_TEMPLATE_STRING, val, line};
}

Token AcLexer::parseNumberLiteral(const QString &source, int &pos, int line) {
  int start = pos;
  int n = source.size();
  while (pos < n && source[pos].isDigit()) ++pos;
  if (pos + 1 < n && source[pos] == '.' && source[pos + 1].isDigit()) {
    ++pos;
    while (pos < n && source[pos].isDigit()) ++pos;
  }
  return {TOK_NUMBER, source.mid(start, pos - start), line};
}

/// @brief 关键字到 Token 类型的映射表
static const QHash<QString, TokenType> &keywordMap() {
  static const QHash<QString, TokenType> map = {
      {QString::fromLatin1(AcKeyword::kFor), TOK_FOR},
      {QString::fromLatin1(AcKeyword::kIn), TOK_IN},
      {QString::fromLatin1(AcKeyword::kIf), TOK_IF},
      {QString::fromLatin1(AcKeyword::kElse), TOK_ELSE},
      {QString::fromLatin1(AcKeyword::kLet), TOK_LET},
      {QString::fromLatin1(AcKeyword::kClass), TOK_CLASS},
      {QString::fromLatin1(AcKeyword::kFunction), TOK_FUNCTION},
      {QString::fromLatin1(AcKeyword::kNew), TOK_NEW},
      {QString::fromLatin1(AcKeyword::kThis), TOK_THIS},
      {QString::fromLatin1(AcKeyword::kReturn), TOK_RETURN},
      {QString::fromLatin1(AcKeyword::kTrue), TOK_TRUE},
      {QString::fromLatin1(AcKeyword::kFalse), TOK_FALSE},
      {QString::fromLatin1(AcKeyword::kStatic), TOK_STATIC},
      {QString::fromLatin1(AcKeyword::kPublic), TOK_PUBLIC},
      {QString::fromLatin1(AcKeyword::kProtected), TOK_PROTECTED},
      {QString::fromLatin1(AcKeyword::kPrivate), TOK_PRIVATE},
      {QString::fromLatin1(AcKeyword::kExtends), TOK_EXTENDS},
      {QString::fromLatin1(AcKeyword::kOverride), TOK_OVERRIDE},
      {QString::fromLatin1(AcKeyword::kInterface), TOK_INTERFACE},
      {QString::fromLatin1(AcKeyword::kImplements), TOK_IMPLEMENTS},
      {QString::fromLatin1(AcKeyword::kSuper), TOK_SUPER},
      {QString::fromLatin1(AcKeyword::kExport), TOK_EXPORT},
      {QString::fromLatin1(AcKeyword::kImport), TOK_IMPORT},
      {QString::fromLatin1(AcKeyword::kFrom), TOK_FROM},
      {QString::fromLatin1(AcKeyword::kNull), TOK_NULL},
      {QString::fromLatin1(AcKeyword::kUndefined), TOK_UNDEFINED},
      {QString::fromLatin1(AcKeyword::kWhile), TOK_WHILE},
      {QString::fromLatin1(AcKeyword::kBreak), TOK_BREAK},
      {QString::fromLatin1(AcKeyword::kContinue), TOK_CONTINUE},
      {QString::fromLatin1(AcKeyword::kSwitch), TOK_SWITCH},
      {QString::fromLatin1(AcKeyword::kCase), TOK_CASE},
      {QString::fromLatin1(AcKeyword::kDefault), TOK_DEFAULT},
      {QString::fromLatin1(AcKeyword::kEnum), TOK_ENUM},
      {QString::fromLatin1(AcKeyword::kConstructor), TOK_CONSTRUCTOR},
      {QString::fromLatin1(AcKeyword::kUsing), TOK_USING},
      {QString::fromLatin1(AcKeyword::kDispose), TOK_DISPOSE},
      {QString::fromLatin1(AcKeyword::kAs), TOK_AS},
  };
  return map;
}

Token AcLexer::parseIdentifier(const QString &source, int &pos, int line) {
  int start = pos;
  int n = source.size();
  while (pos < n &&
         ((source[pos].isLetterOrNumber() && source[pos].unicode() < 128) || source[pos] == '_'))
    ++pos;
  QString word = source.mid(start, pos - start);
  auto it = keywordMap().constFind(word);
  Token tok;
  tok.line = line;
  tok.text = word;
  tok.type = (it != keywordMap().constEnd()) ? it.value() : TOK_IDENT;
  return tok;
}

/// @brief 将源码字符串拆分为 token 序列
QVector<Token> AcLexer::tokenize(const QString &source, QString &error) {
  QVector<Token> tokens;
  int i = 0;
  int line = 1;
  int n = source.size();

  while (i < n) {
    QChar c = source[i];

    if (c == '\n') {
      ++line;
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
      if (!skipBlockComment(source, i, line, error)) {
        return {};  // 块注释未闭合，返回错误
      }
      continue;
    }

    switch (c.unicode()) {
      case '{':
        tokens.append({TOK_LBRACE, QStringLiteral("{"), line});
        ++i;
        break;
      case '}':
        tokens.append({TOK_RBRACE, QStringLiteral("}"), line});
        ++i;
        break;
      case '(':
        tokens.append({TOK_LPAREN, QStringLiteral("("), line});
        ++i;
        break;
      case ')':
        tokens.append({TOK_RPAREN, QStringLiteral(")"), line});
        ++i;
        break;
      case '[':
        tokens.append({TOK_LBRACKET, QStringLiteral("["), line});
        ++i;
        break;
      case ']':
        tokens.append({TOK_RBRACKET, QStringLiteral("]"), line});
        ++i;
        break;
      case ',':
        tokens.append({TOK_COMMA, QStringLiteral(","), line});
        ++i;
        break;
      case ':':
        if (i + 1 < n && source[i + 1] == ':') {
          tokens.append({TOK_SCOPE, QStringLiteral("::"), line});
          i += 2;
        } else {
          tokens.append({TOK_COLON, QStringLiteral(":"), line});
          ++i;
        }
        break;
      case '.':
        tokens.append({TOK_DOT, QStringLiteral("."), line});
        ++i;
        break;
      case '+':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TOK_PLUSEQ, QStringLiteral("+="), line});
          i += 2;
        } else if (i + 1 < n && source[i + 1] == '+') {
          tokens.append({TOK_PLUSPLUS, QStringLiteral("++"), line});
          i += 2;
        } else {
          tokens.append({TOK_PLUS, QStringLiteral("+"), line});
          ++i;
        }
        break;
      case '-':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TOK_MINUSEQ, QStringLiteral("-="), line});
          i += 2;
        } else if (i + 1 < n && source[i + 1] == '-') {
          tokens.append({TOK_MINUSMINUS, QStringLiteral("--"), line});
          i += 2;
        } else {
          tokens.append({TOK_MINUS, QStringLiteral("-"), line});
          ++i;
        }
        break;
      case '*':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TOK_MULEQ, QStringLiteral("*="), line});
          i += 2;
        } else {
          tokens.append({TOK_MUL, QStringLiteral("*"), line});
          ++i;
        }
        break;
      case '/':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TOK_DIVEQ, QStringLiteral("/="), line});
          i += 2;
        } else {
          tokens.append({TOK_DIV, QStringLiteral("/"), line});
          ++i;
        }
        break;
      case '%':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TOK_MODEQ, QStringLiteral("%="), line});
          i += 2;
        } else {
          tokens.append({TOK_MOD, QStringLiteral("%"), line});
          ++i;
        }
        break;
      case '|':
        if (i + 1 < n && source[i + 1] == '|') {
          tokens.append({TOK_OR, QStringLiteral("||"), line});
          i += 2;
        } else {
          error = QStringLiteral("unexpected character '|' at line %1").arg(line);
          return {};
        }
        break;
      case '&':
        if (i + 1 < n && source[i + 1] == '&') {
          tokens.append({TOK_AND, QStringLiteral("&&"), line});
          i += 2;
        } else {
          error = QStringLiteral("unexpected character '&' at line %1").arg(line);
          return {};
        }
        break;
      case '!':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TOK_NEQ, QStringLiteral("!="), line});
          i += 2;
        } else {
          tokens.append({TOK_NOT, QStringLiteral("!"), line});
          ++i;
        }
        break;
      case '=':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TOK_EQ, QStringLiteral("=="), line});
          i += 2;
        } else {
          tokens.append({TOK_EQUALS, QStringLiteral("="), line});
          ++i;
        }
        break;
      case '<':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TOK_LTE, QStringLiteral("<="), line});
          i += 2;
        } else {
          tokens.append({TOK_LT, QStringLiteral("<"), line});
          ++i;
        }
        break;
      case '>':
        if (i + 1 < n && source[i + 1] == '=') {
          tokens.append({TOK_GTE, QStringLiteral(">="), line});
          i += 2;
        } else {
          tokens.append({TOK_GT, QStringLiteral(">"), line});
          ++i;
        }
        break;
      case ';':
        tokens.append({TOK_SEMI, QStringLiteral(";"), line});
        ++i;
        break;
      case '?':
        tokens.append({TOK_QUESTION, QStringLiteral("?"), line});
        ++i;
        break;
      case '"': {
        Token tok = parseStringLiteral(source, i, line, error);
        if (!error.isEmpty()) return {};
        tokens.append(tok);
        break;
      }
      case '`': {
        Token tok = parseTemplateStringLiteral(source, i, line, error);
        if (!error.isEmpty()) return {};
        tokens.append(tok);
        break;
      }
      default:
        if (c.isDigit()) {
          tokens.append(parseNumberLiteral(source, i, line));
        } else if ((c.isLetter() && c.unicode() < 128) || c == '_') {
          tokens.append(parseIdentifier(source, i, line));
        } else if (c.unicode() > 127) {
          ++i;
        } else {
          error =
              QStringLiteral("unexpected character '%1' at line %2").arg(c, QString::number(line));
          return {};
        }
        break;
    }
  }

  tokens.append({TOK_EOF, {}, line});
  return tokens;
}