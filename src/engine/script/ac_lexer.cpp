/**
 * @file ac_lexer.cpp
 * @brief 词法分析器实现文件
 */

#include "ac_lexer.h"
#include "../ac_language.h"

#include <QHash>

/// @brief 跳过行注释（// 到行尾）
void AcLexer::skipLineComment(const QString &source, int &pos) {
  while (pos < source.size() && source[pos] != '\n')
    ++pos;
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
  };
  return map;
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
      tokens.append({TOK_COLON, QStringLiteral(":"), line});
      ++i;
      break;
    case '.':
      tokens.append({TOK_DOT, QStringLiteral("."), line});
      ++i;
      break;
    case '=':
      tokens.append({TOK_EQUALS, QStringLiteral("="), line});
      ++i;
      break;
    case '+':
      tokens.append({TOK_PLUS, QStringLiteral("+"), line});
      ++i;
      break;
    case '-':
      tokens.append({TOK_MINUS, QStringLiteral("-"), line});
      ++i;
      break;
    case '*':
      tokens.append({TOK_MUL, QStringLiteral("*"), line});
      ++i;
      break;
    case '/':
      tokens.append({TOK_DIV, QStringLiteral("/"), line});
      ++i;
      break;
    case ';':
      tokens.append({TOK_SEMI, QStringLiteral(";"), line});
      ++i;
      break;
    case '"': {
      int start = ++i;
      while (i < n && source[i] != '"') {
        if (source[i] == '\\' && i + 1 < n)
          ++i;
        ++i;
      }
      if (i >= n) {
        error = QStringLiteral("unterminated string at line %1").arg(line);
        return {};
      }
      QString val = source.mid(start, i - start);
      val.replace(QStringLiteral("\\\""), QStringLiteral("\""));
      val.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
      val.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
      tokens.append({TOK_STRING, val, line});
      ++i;
      break;
    }
    default:
      if (c.isDigit()) {
        int start = i;
        while (i < n && source[i].isDigit())
          ++i;
        if (i + 1 < n && source[i] == '.' && source[i + 1].isDigit()) {
          ++i;
          while (i < n && source[i].isDigit())
            ++i;
        }
        tokens.append({TOK_NUMBER, source.mid(start, i - start), line});
      } else if (c.isLetter() || c == '_') {
        int start = i;
        while (i < n && (source[i].isLetterOrNumber() || source[i] == '_'))
          ++i;
        QString word = source.mid(start, i - start);
        auto it = keywordMap().constFind(word);
        if (it != keywordMap().constEnd())
          tokens.append({*it, word, line});
        else
          tokens.append({TOK_IDENT, word, line});
      } else {
        error = QStringLiteral("unexpected character '%1' at line %2")
                    .arg(c, QString::number(line));
        return {};
      }
      break;
    }
  }

  tokens.append({TOK_EOF, {}, line});
  return tokens;
}