/**
 * @file ac_parser_expr.cpp
 * @brief 表达式解析实现文件
 */

#include <vector>

#include "../ac_language.h"
#include "ac_parser.h"

// ── 表达式解析入口 ──

bool AcParser::parseExpr(Expr &expr) {
  if (!parseTernary(expr)) return false;
  if (peek().type == TOK_PLUSPLUS) {
    advance();
    auto operand = std::make_unique<Expr>(std::move(expr));
    expr.kind = Expr::kPostInc;
    expr.operand = std::move(operand);
    return true;
  }
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
        if (expr.kind == Expr::kIdent ||
            (expr.kind == Expr::kPropAccess && !expr.ident.isEmpty())) {
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