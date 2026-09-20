/**
 * @file ac_parser_expr.cpp
 * @brief 表达式解析实现文件
 */

#include <vector>

#include "../ac_language.h"
#include "ac_parser.h"

// ── 表达式解析入口 ──

/// 表达式嵌套深度上限：括号/数组/对象字面量会递归回到 parseExpr，
/// 无上限时机器生成的深嵌套代码会打爆 C++ 栈。
/// 实测每层嵌套约消耗 9KB 栈（parseExpr→…→parsePrimary 一条链 16 个递归帧），
/// 64 层 ≈ 590KB，为 1MB 线程栈留足余量 —— 上限不可随意调大
static constexpr int kMaxExprDepth = 64;

bool AcParser::parseExpr(Expr &expr) {
  if (m_exprDepth >= kMaxExprDepth) {
    m_error = QStringLiteral("表达式嵌套过深（上限 %1 层）at line %2")
                  .arg(kMaxExprDepth)
                  .arg(peek().loc.line);
    return false;
  }
  ++m_exprDepth;
  struct DepthPop {
    int &d;
    ~DepthPop() { --d; }
  } pop{m_exprDepth};
  if (!parseTernary(expr)) return false;
  // 赋值表达式: lhs = rhs
  if (peek().type == TokenType::kEquals) {
    int assignLine = peek().loc.line;
    advance();
    auto left = std::make_unique<Expr>(std::move(expr));
    auto right = std::make_unique<Expr>();
    if (!parseExpr(*right)) return false;
    expr.kind = Expr::kAssign;
    expr.loc.line = assignLine;
    expr.left = std::move(left);
    expr.right = std::move(right);
    return true;
  }
  if (peek().type == TokenType::kPlusPlus) {
    advance();
    auto operand = std::make_unique<Expr>(std::move(expr));
    expr.kind = Expr::kPostInc;
    expr.operand = std::move(operand);
    return true;
  }
  if (peek().type == TokenType::kMinusMinus) {
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
  // 空值合并 a ?? b（左结合链；左为 null/undefined 时取右）
  while (peek().type == TokenType::kQuestionQuestion) {
    const int line = peek().loc.line;
    advance();
    Expr node;
    node.kind = Expr::kCoalesce;
    node.loc.line = line;
    node.left = std::make_unique<Expr>(std::move(expr));
    auto right = std::make_unique<Expr>();
    if (!parseLogicalOr(*right)) return false;
    node.right = std::move(right);
    expr = std::move(node);
  }
  if (peek().type == TokenType::kQuestion) {
    int ternaryLine = peek().loc.line;
    advance();
    auto cond = std::make_unique<Expr>(std::move(expr));
    auto trueExpr = std::make_unique<Expr>();
    if (!parseLogicalOr(*trueExpr)) {
      return false;
    }
    if (!expect(TokenType::kColon, QStringLiteral("expected ':' in ternary expression"))) {
      return false;
    }
    auto falseExpr = std::make_unique<Expr>();
    if (!parseTernary(*falseExpr)) {
      return false;
    }
    expr.kind = Expr::kTernary;
    expr.loc.line = ternaryLine;
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
    binary.loc = opToken.loc;
    binary.binOp = matchedOp;
    binary.left = std::move(left);
    binary.right = std::move(right);
    expr = std::move(binary);
  }
  return true;
}

bool AcParser::parseLogicalOr(Expr &expr) {
  return parseBinary<&AcParser::parseLogicalAnd>(expr, {{TokenType::kOr, Expr::kOr}});
}

bool AcParser::parseLogicalAnd(Expr &expr) {
  return parseBinary<&AcParser::parseComparison>(expr, {{TokenType::kAnd, Expr::kAnd}});
}

bool AcParser::parseComparison(Expr &expr) {
  return parseBinary<&AcParser::parseAddSub>(expr, {{TokenType::kEq, Expr::kEq},
                                                    {TokenType::kNeq, Expr::kNeq},
                                                    {TokenType::kLt, Expr::kLt},
                                                    {TokenType::kGt, Expr::kGt},
                                                    {TokenType::kLte, Expr::kLte},
                                                    {TokenType::kGte, Expr::kGte}});
}

bool AcParser::parseAddSub(Expr &expr) {
  return parseBinary<&AcParser::parseMulDiv>(
      expr, {{TokenType::kPlus, Expr::kAdd}, {TokenType::kMinus, Expr::kSub}});
}

bool AcParser::parseMulDiv(Expr &expr) {
  return parseBinary<&AcParser::parseUnary>(expr, {{TokenType::kMul, Expr::kMul},
                                                   {TokenType::kDiv, Expr::kDiv},
                                                   {TokenType::kMod, Expr::kMod}});
}

bool AcParser::parseUnary(Expr &expr) {
  Token t = peek();

  if (t.type == TokenType::kPlusPlus) {
    advance();
    auto operand = std::make_unique<Expr>();
    if (!parseUnary(*operand)) {
      return false;
    }
    expr.kind = Expr::kPreInc;
    expr.loc = t.loc;
    expr.operand = std::move(operand);
    return true;
  }

  if (t.type == TokenType::kMinusMinus) {
    advance();
    auto operand = std::make_unique<Expr>();
    if (!parseUnary(*operand)) {
      return false;
    }
    expr.kind = Expr::kPreDec;
    expr.loc = t.loc;
    expr.operand = std::move(operand);
    return true;
  }

  if (t.type == TokenType::kNot) {
    advance();
    auto operand = std::make_unique<Expr>();
    if (!parseUnary(*operand)) {
      return false;
    }
    expr.kind = Expr::kUnary;
    expr.loc = t.loc;
    expr.unaryOp = Expr::kNot;
    expr.operand = std::move(operand);
    return true;
  }

  if (t.type == TokenType::kMinus) {
    advance();
    if (peek().type == TokenType::kNumber) {
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
    if (peek().type == TokenType::kDot) {
      advance();
      if (!isPropertyName(peek().type)) {
        m_error =
            QStringLiteral("expected property name after '.' at line %1").arg(peek().loc.line);
        return false;
      }
      QString memberName = advance().text;
      if (peek().type == TokenType::kLParen) {
        advance();
        Expr chained;
        chained.kind = Expr::kMethodCall;
        chained.loc.line = peek().loc.line;
        chained.methodCall.methodName = memberName;
        if (expr.kind == Expr::kIdent ||
            (expr.kind == Expr::kPropAccess && !expr.ident.isEmpty())) {
          chained.methodCall.objName = expr.ident;
        }
        chained.methodCall.object = std::make_unique<Expr>(std::move(expr));
        while (peek().type != TokenType::kRParen && peek().type != TokenType::kEof) {
          auto arg = std::make_unique<Expr>();
          if (!parseLogicalOr(*arg)) {
            return false;
          }
          chained.methodCall.args.push_back(std::move(arg));
          if (peek().type == TokenType::kComma) advance();
        }
        if (!expect(
                TokenType::kRParen,
                QStringLiteral("expected ')' after method call at line %1").arg(peek().loc.line)))
          return false;
        expr = std::move(chained);
      } else {
        Expr propAccess;
        propAccess.kind = Expr::kPropAccess;
        propAccess.loc.line = peek().loc.line;
        propAccess.prop = memberName;
        propAccess.propObject = std::make_unique<Expr>(std::move(expr));
        expr = std::move(propAccess);
      }
    } else if (peek().type == TokenType::kQuestionDot) {
      // 可选链 ?.prop / ?.method(...)：对象为 null 时整体短路为 null
      advance();
      if (!isPropertyName(peek().type)) {
        m_error =
            QStringLiteral("expected property name after '?.' at line %1").arg(peek().loc.line);
        return false;
      }
      QString memberName = advance().text;
      if (peek().type == TokenType::kLParen) {
        advance();
        Expr chained;
        chained.kind = Expr::kMethodCall;
        chained.loc.line = peek().loc.line;
        chained.methodCall.methodName = memberName;
        chained.methodCall.isOptional = true;
        chained.methodCall.object = std::make_unique<Expr>(std::move(expr));
        while (peek().type != TokenType::kRParen && peek().type != TokenType::kEof) {
          auto arg = std::make_unique<Expr>();
          if (!parseLogicalOr(*arg)) {
            return false;
          }
          chained.methodCall.args.push_back(std::move(arg));
          if (peek().type == TokenType::kComma) advance();
        }
        if (!expect(
                TokenType::kRParen,
                QStringLiteral("expected ')' after method call at line %1").arg(peek().loc.line)))
          return false;
        expr = std::move(chained);
      } else {
        Expr propAccess;
        propAccess.kind = Expr::kPropAccess;
        propAccess.loc.line = peek().loc.line;
        propAccess.prop = memberName;
        propAccess.isOptional = true;
        propAccess.propObject = std::make_unique<Expr>(std::move(expr));
        expr = std::move(propAccess);
      }
    } else if (peek().type == TokenType::kLBracket) {
      advance();
      auto idxExpr = std::make_unique<Expr>();
      if (!parseLogicalOr(*idxExpr)) {
        return false;
      }
      if (!expect(TokenType::kRBracket, QStringLiteral("expected ']' after index expression"))) {
        return false;
      }
      Expr idxAccess;
      idxAccess.kind = Expr::kIndexAccess;
      idxAccess.loc.line = peek().loc.line;
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

  if (t.type == TokenType::kThis) {
    advance();
    if (peek().type == TokenType::kDot) {
      advance();
      if (!isPropertyName(peek().type)) {
        m_error =
            QStringLiteral("expected property name after 'this.' at line %1").arg(peek().loc.line);
        return false;
      }
      QString propName = advance().text;
      if (peek().type == TokenType::kLParen) {
        expr.kind = Expr::kMethodCall;
        expr.loc.line = peek().loc.line;
        expr.methodCall.objName = QString::fromLatin1(AcKeyword::kThis);
        expr.methodCall.methodName = propName;
        advance();
        while (peek().type != TokenType::kRParen && peek().type != TokenType::kEof) {
          auto arg = std::make_unique<Expr>();
          if (!parseLogicalOr(*arg)) {
            return false;
          }
          expr.methodCall.args.push_back(std::move(arg));
          if (peek().type == TokenType::kComma) advance();
        }
        return expect(TokenType::kRParen, QStringLiteral("expected ')'"));
      }
      expr.kind = Expr::kPropAccess;
      expr.loc = t.loc;
      expr.ident = QString::fromLatin1(AcKeyword::kThis);
      expr.prop = propName;
      return true;
    }
    if (peek().type == TokenType::kLBracket) {
      advance();
      expr.kind = Expr::kIndexAccess;
      expr.left = std::make_unique<Expr>();
      expr.left->kind = Expr::kThis;
      expr.right = std::make_unique<Expr>();
      if (!parseExpr(*expr.right)) return false;
      return expect(TokenType::kRBracket, QStringLiteral("expected ']'"));
    }
    expr.kind = Expr::kThis;
    return true;
  }

  if (t.type == TokenType::kSuper) {
    advance();
    if (peek().type == TokenType::kLParen) {
      expr.kind = Expr::kMethodCall;
      expr.loc.line = peek().loc.line;
      expr.methodCall.objName = QString::fromLatin1(AcKeyword::kSuper);
      expr.methodCall.methodName = QStringLiteral("constructor");
      advance();
      while (peek().type != TokenType::kRParen && peek().type != TokenType::kEof) {
        auto arg = std::make_unique<Expr>();
        if (!parseLogicalOr(*arg)) {
          return false;
        }
        expr.methodCall.args.push_back(std::move(arg));
        if (peek().type == TokenType::kComma) advance();
      }
      return expect(TokenType::kRParen, QStringLiteral("expected ')'"));
    }
    if (peek().type != TokenType::kDot) {
      m_error = QStringLiteral("expected '.' or '(' after 'super' at line %1").arg(peek().loc.line);
      return false;
    }
    advance();
    if (peek().type != TokenType::kIdent) {
      m_error =
          QStringLiteral("expected method name after 'super.' at line %1").arg(peek().loc.line);
      return false;
    }
    QString methodName = advance().text;
    if (peek().type == TokenType::kLParen) {
      expr.kind = Expr::kMethodCall;
      expr.loc.line = peek().loc.line;
      expr.methodCall.objName = QString::fromLatin1(AcKeyword::kSuper);
      expr.methodCall.methodName = methodName;
      advance();
      while (peek().type != TokenType::kRParen && peek().type != TokenType::kEof) {
        auto arg = std::make_unique<Expr>();
        if (!parseLogicalOr(*arg)) {
          return false;
        }
        expr.methodCall.args.push_back(std::move(arg));
        if (peek().type == TokenType::kComma) advance();
      }
      return expect(TokenType::kRParen, QStringLiteral("expected ')'"));
    }
    expr.kind = Expr::kPropAccess;
    expr.loc = t.loc;
    expr.ident = QString::fromLatin1(AcKeyword::kSuper);
    expr.prop = methodName;
    return true;
  }

  if (t.type == TokenType::kNew) {
    int newLine = t.loc.line;
    advance();
    if (peek().type != TokenType::kIdent) {
      m_error = QStringLiteral("expected class name after 'new' at line %1").arg(peek().loc.line);
      return false;
    }
    expr.kind = Expr::kNewInstance;
    expr.loc.line = newLine;
    expr.className = advance().text;
    if (!expect(TokenType::kLParen, QStringLiteral("expected '(' after class name"))) return false;
    if (peek().type != TokenType::kRParen) {
      do {
        auto arg = std::make_unique<Expr>();
        if (!parseLogicalOr(*arg)) {
          return false;
        }
        expr.constructorArgs.push_back(std::move(arg));
        if (peek().type != TokenType::kComma) break;
        advance();
      } while (true);
    }
    return expect(TokenType::kRParen, QStringLiteral("expected ')'"));
  }

  switch (t.type) {
    case TokenType::kString:
      expr.kind = Expr::kString;
      expr.strVal = advance().text;
      return true;

    case TokenType::kNumber:
      expr.kind = Expr::kNumber;
      expr.numVal = advance().text.toDouble();
      return true;

    case TokenType::kTrue:
      expr.kind = Expr::kBool;
      expr.boolVal = true;
      advance();
      return true;

    case TokenType::kFalse:
      expr.kind = Expr::kBool;
      expr.boolVal = false;
      advance();
      return true;

    case TokenType::kNull:
      expr.kind = Expr::kNull;
      advance();
      return true;

    case TokenType::kUndefined:
      expr.kind = Expr::kUndefined;
      advance();
      return true;

    case TokenType::kTemplateString:
      return parseTemplateString(expr);

    case TokenType::kLParen: {
      advance();
      if (!parseExpr(expr)) return false;
      return expect(TokenType::kRParen, QStringLiteral("expected ')'"));
    }

    case TokenType::kIdent: {
      int identLine = peek().loc.line;
      QString name = advance().text;
      if (peek().type == TokenType::kScope) {
        int scopeLine = peek().loc.line;
        advance();
        if (peek().type != TokenType::kIdent) {
          m_error =
              QStringLiteral("expected member name after '::' at line %1").arg(peek().loc.line);
          return false;
        }
        QString member = advance().text;
        expr.kind = Expr::kStaticAccess;
        expr.loc.line = scopeLine;
        expr.className = name;
        expr.prop = member;
        if (peek().type == TokenType::kLParen) {
          advance();
          while (peek().type != TokenType::kRParen && peek().type != TokenType::kEof) {
            auto arg = std::make_unique<Expr>();
            if (!parseLogicalOr(*arg)) {
              return false;
            }
            expr.funcCall.args.push_back(std::move(arg));
            if (peek().type == TokenType::kComma) advance();
          }
          return expect(TokenType::kRParen,
                        QStringLiteral("expected ')' after static method call"));
        }
        return true;
      }
      if (peek().type == TokenType::kDot) {
        advance();
        if (!isPropertyName(peek().type)) {
          m_error =
              QStringLiteral("expected property name after '.' at line %1").arg(peek().loc.line);
          return false;
        }
        QString propName = advance().text;
        if (peek().type == TokenType::kLParen) {
          expr.kind = Expr::kMethodCall;
          expr.loc.line = peek().loc.line;
          expr.methodCall.objName = name;
          expr.methodCall.methodName = propName;
          advance();
          while (peek().type != TokenType::kRParen && peek().type != TokenType::kEof) {
            auto arg = std::make_unique<Expr>();
            if (!parseLogicalOr(*arg)) {
              return false;
            }
            expr.methodCall.args.push_back(std::move(arg));
            if (peek().type == TokenType::kComma) advance();
          }
          return expect(TokenType::kRParen, QStringLiteral("expected ')'"));
        }
        expr.kind = Expr::kPropAccess;
        expr.loc.line = identLine;
        expr.ident = name;
        expr.prop = propName;
        return true;
      }
      if (peek().type == TokenType::kLParen) {
        return parseFuncCall(name, expr);
      }
      if (peek().type == TokenType::kLBracket) {
        advance();
        expr.kind = Expr::kIndexAccess;
        expr.left = std::make_unique<Expr>();
        expr.left->kind = Expr::kIdent;
        expr.left->ident = name;
        expr.left->loc = t.loc;
        expr.right = std::make_unique<Expr>();
        if (!parseExpr(*expr.right)) return false;
        return expect(TokenType::kRBracket, QStringLiteral("expected ']'"));
      }
      expr.kind = Expr::kIdent;
      expr.ident = name;
      expr.loc = t.loc;
      return true;
    }

    case TokenType::kLBrace:
      return parseObject(expr);

    case TokenType::kLBracket:
      return parseArray(expr);

    case TokenType::kFunction: {
      advance();
      expr.kind = Expr::kFuncExpr;
      if (peek().type == TokenType::kIdent) {
        expr.funcExpr.name = advance().text;
      } else {
        expr.funcExpr.name = QStringLiteral("__anonymous__");
      }
      if (!expect(TokenType::kLParen, QStringLiteral("expected '(' in function expression")))
        return false;
      if (!parseParamList(expr.funcExpr.params, /*requireType=*/true, /*allowDefault=*/true,
                          /*declareVars=*/true))
        return false;
      if (!expect(TokenType::kRParen, QStringLiteral("expected ')' after parameters")))
        return false;
      if (peek().type != TokenType::kColon) {
        m_error = QStringLiteral(
                      "function expression requires a return type annotation (e.g. : Type) at "
                      "line %1")
                      .arg(peek().loc.line);
        return false;
      }
      advance();
      expr.funcExpr.returnType = parseType();
      if (!parseBlock(expr.funcExpr.body)) return false;
      return true;
    }

    default:
      m_error = QStringLiteral("unexpected token '%1' at line %2")
                    .arg(t.text, QString::number(t.loc.line));
      return false;
  }
}

bool AcParser::parseObject(Expr &expr) {
  expr.kind = Expr::kObject;
  advance();
  while (peek().type != TokenType::kRBrace && peek().type != TokenType::kEof) {
    if (!isPropertyName(peek().type)) {
      m_error = QStringLiteral("expected key in object at line %1").arg(peek().loc.line);
      return false;
    }
    ObjectEntry entry;
    entry.key = advance().text;
    if (!expect(TokenType::kColon, QStringLiteral("expected ':'"))) return false;
    entry.value = std::make_unique<Expr>();
    if (!parseExpr(*entry.value)) {
      entry.value = nullptr;
      return false;
    }
    expr.objEntries.append(entry);
    if (peek().type == TokenType::kComma) advance();
  }
  return expect(TokenType::kRBrace, QStringLiteral("expected '}'"));
}

bool AcParser::parseArray(Expr &expr) {
  expr.kind = Expr::kArray;
  advance();
  while (peek().type != TokenType::kRBracket && peek().type != TokenType::kEof) {
    auto item = std::make_unique<Expr>();
    if (!parseExpr(*item)) {
      return false;
    }
    expr.arrItems.push_back(std::move(item));
    if (peek().type == TokenType::kComma) advance();
  }
  return expect(TokenType::kRBracket, QStringLiteral("expected ']'"));
}

bool AcParser::parseFuncCall(const QString &name, Expr &expr) {
  expr.kind = Expr::kFuncCall;
  expr.funcCall.name = name;
  expr.loc.line = peek().loc.line;
  advance();
  while (peek().type != TokenType::kRParen && peek().type != TokenType::kEof) {
    auto arg = std::make_unique<Expr>();
    if (!parseLogicalOr(*arg)) {
      return false;
    }
    expr.funcCall.args.push_back(std::move(arg));
    if (peek().type == TokenType::kComma) advance();
  }
  return expect(TokenType::kRParen, QStringLiteral("expected ')'"));
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

      QVector<Token> exprTokens = AcLexer::tokenize(exprText, m_error);
      if (!m_error.isEmpty()) return false;
      // 插值片段是独立 tokenize 的，token 行号从片段内起始（常为 1），
      // 需偏移回源文件真实行号，否则 ${var} 里的标识符其引用/校验/报错都定位不到正确行。
      // 片段起始行 = 模板 token 行 + 片段之前的换行数；把片段内第 1 行映射到该行。
      const int baseLine = tok.loc.line + raw.left(start).count(QLatin1Char('\n'));
      const int lineOffset = baseLine - 1;
      for (Token &epTok : exprTokens) epTok.loc.line += lineOffset;

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