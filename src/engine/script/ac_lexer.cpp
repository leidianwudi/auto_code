/**
 * @file ac_lexer.cpp
 * @brief 词法分析器实现文件
 */

#include "ac_lexer.h"

/// @brief 跳过行注释（// 到行尾）
void AcLexer::skipLineComment(const QString &source, int &pos) {
  while (pos < source.size() && source[pos] != '\n')
    ++pos;
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
        if (word == QStringLiteral("for"))
          tokens.append({TOK_FOR, word, line});
        else if (word == QStringLiteral("in"))
          tokens.append({TOK_IN, word, line});
        else if (word == QStringLiteral("if"))
          tokens.append({TOK_IF, word, line});
        else if (word == QStringLiteral("else"))
          tokens.append({TOK_ELSE, word, line});
        else if (word == QStringLiteral("let"))
          tokens.append({TOK_LET, word, line});
        else if (word == QStringLiteral("class"))
          tokens.append({TOK_CLASS, word, line});
        else if (word == QStringLiteral("function"))
          tokens.append({TOK_FUNCTION, word, line});
        else if (word == QStringLiteral("new"))
          tokens.append({TOK_NEW, word, line});
        else if (word == QStringLiteral("this"))
          tokens.append({TOK_THIS, word, line});
        else if (word == QStringLiteral("return"))
          tokens.append({TOK_RETURN, word, line});
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