#include "script_parser.h"

#include "engine_tpl.h"
#include "function/fun_const.h"
#include "function/fun_mgr.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

ScriptParser::ScriptParser() = default;

// ── 词法分析（Tokenizer） ──

/// @brief 跳过行注释（// 到行尾）
void ScriptParser::skipLineComment(const QString &source, int &pos) {
  while (pos < source.size() && source[pos] != '\n')
    ++pos;
}

/// @brief 将源码字符串拆分为 token 序列
QVector<ScriptParser::Token> ScriptParser::tokenize(const QString &source) {
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
        m_error = QStringLiteral("unterminated string at line %1").arg(line);
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
        else
          tokens.append({TOK_IDENT, word, line});
      } else {
        m_error = QStringLiteral("unexpected character '%1' at line %2")
                      .arg(c, QString::number(line));
        return {};
      }
      break;
    }
  }

  tokens.append({TOK_EOF, {}, line});
  return tokens;
}

// ── 语法分析（递归下降 Parser） ──

/// @brief 查看当前 token，不移除
ScriptParser::Token ScriptParser::peek() {
  if (m_pos < m_tokens.size())
    return m_tokens[m_pos];
  return {TOK_EOF, {}, 0};
}

/// @brief 消费当前 token 并返回
ScriptParser::Token ScriptParser::advance() {
  if (m_pos < m_tokens.size())
    return m_tokens[m_pos++];
  return {TOK_EOF, {}, 0};
}

/// @brief 如果当前 token 匹配指定类型则消费并返回 true
bool ScriptParser::match(TokenType t) {
  if (peek().type == t) {
    advance();
    return true;
  }
  return false;
}

/// @brief 要求当前 token 匹配指定类型，否则报错
bool ScriptParser::expect(TokenType t, const QString &msg) {
  if (peek().type == t) {
    advance();
    return true;
  }
  m_error = QStringLiteral("%1 at line %2").arg(msg).arg(peek().line);
  return false;
}

/// @brief 解析入口：先调用 tokenize 再 parseProgram
bool ScriptParser::parse(const QString &source) {
  m_tokens = tokenize(source);
  if (!m_error.isEmpty())
    return false;
  m_pos = 0;
  m_program = Block();
  return parseProgram(m_program);
}

/// @brief 解析程序入口 — 跳过可选的 main 关键字后进入 block
bool ScriptParser::parseProgram(Block &block) {
  // 跳过可选的 "main" 关键字
  if (peek().type == TOK_IDENT && peek().text == QStringLiteral("main")) {
    advance();
  }
  return parseBlock(block);
}

/// @brief 解析 { stmt* } 语句块
bool ScriptParser::parseBlock(Block &block) {
  if (!expect(TOK_LBRACE, QStringLiteral("expected '{'")))
    return false;
  while (peek().type != TOK_RBRACE && peek().type != TOK_EOF) {
    Stmt stmt;
    if (!parseStmt(stmt))
      return false;
    block.stmts.append(stmt);
    // 消费可选的 ; 语句结束符
    if (peek().type == TOK_SEMI)
      advance();
  }
  return expect(TOK_RBRACE, QStringLiteral("expected '}'"));
}

/// @brief 根据 token 类型分发到不同的语句解析函数
bool ScriptParser::parseStmt(Stmt &stmt) {
  Token t = peek();

  if (t.type == TOK_IDENT && t.text == QStringLiteral("call")) {
    advance(); // 消费 'call'
    stmt.kind = Stmt::kCall;
    return parseCallStmt(stmt.call);
  }

  if (t.type == TOK_FOR) {
    stmt.kind = Stmt::kFor;
    return parseForStmt(stmt.forStmt);
  }

  if (t.type == TOK_IF) {
    stmt.kind = Stmt::kIf;
    return parseIfStmt(stmt.ifStmt);
  }

  if (t.type == TOK_LET) {
    advance(); // 消费 'let'
    // 必须先有标识符
    if (peek().type != TOK_IDENT) {
      m_error = QStringLiteral("expected variable name after 'let' at line %1")
                    .arg(peek().line);
      return false;
    }
    // 变量名不能以数字开头
    if (!peek().text.isEmpty() && peek().text[0].isDigit()) {
      m_error =
          QStringLiteral("variable name cannot start with a digit at line %1")
              .arg(peek().line);
      return false;
    }
    // 注册为已声明变量
    m_declaredVars.insert(peek().text);
    stmt.kind = Stmt::kAssign;
    return parseAssignStmt(stmt.assign);
  }

  if (t.type == TOK_IDENT) {
    // look ahead: if next is '=', it's assignment
    if (m_pos + 1 < m_tokens.size() && m_tokens[m_pos + 1].type == TOK_EQUALS) {
      // 变量必须先 let 声明才能赋值
      if (!m_declaredVars.contains(t.text)) {
        m_error = QStringLiteral("variable '%1' must be declared with 'let' "
                                 "before assignment at line %2")
                      .arg(t.text, QString::number(t.line));
        return false;
      }
      stmt.kind = Stmt::kAssign;
      return parseAssignStmt(stmt.assign);
    }
    // look ahead: if next is '[', it's indexed assignment obj["key"] = expr
    if (m_pos + 1 < m_tokens.size() &&
        m_tokens[m_pos + 1].type == TOK_LBRACKET) {
      stmt.kind = Stmt::kIndexAssign;
      return parseIndexAssignStmt(stmt.indexAssign);
    }
    // look ahead: if next is '(', it's an expression statement (e.g.
    // write(...))
    if (m_pos + 1 < m_tokens.size() && m_tokens[m_pos + 1].type == TOK_LPAREN) {
      stmt.kind = Stmt::kExpr;
      QString name = advance().text;
      return parseFuncCall(name, stmt.exprStmt);
    }
  }

  m_error = QStringLiteral("unexpected token '%1' at line %2")
                .arg(t.text, QString::number(t.line));
  return false;
}

/// @brief 解析 call("cls", "func", arg)
bool ScriptParser::parseCallStmt(CallStmt &cs) {
  if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after call")))
    return false;
  if (!parseExpr(cs.className))
    return false;
  if (!expect(TOK_COMMA, QStringLiteral("expected ',' after class name")))
    return false;
  if (!parseExpr(cs.funcName))
    return false;
  if (match(TOK_COMMA)) {
    if (!parseExpr(cs.args))
      return false;
  }
  return expect(TOK_RPAREN, QStringLiteral("expected ')'"));
}

/// @brief 解析 name = expr
bool ScriptParser::parseAssignStmt(AssignStmt &as) {
  as.name = advance().text;
  if (!expect(TOK_EQUALS, QStringLiteral("expected '='")))
    return false;
  return parseExpr(as.value);
}

/// @brief 解析 obj["key"] = expr
bool ScriptParser::parseIndexAssignStmt(IndexAssignStmt &ias) {
  ias.objName = advance().text;
  if (!expect(TOK_LBRACKET, QStringLiteral("expected '['")))
    return false;
  if (peek().type != TOK_STRING) {
    m_error =
        QStringLiteral("expected string key in index assignment at line %1")
            .arg(peek().line);
    return false;
  }
  ias.key = advance().text;
  if (!expect(TOK_RBRACKET, QStringLiteral("expected ']'")))
    return false;
  if (!expect(TOK_EQUALS, QStringLiteral("expected '='")))
    return false;
  return parseExpr(ias.value);
}

/// @brief 解析 for (var in arrayExpr) { body }
bool ScriptParser::parseForStmt(ForStmt &fs) {
  advance(); // 跳过 'for'
  if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after for")))
    return false;
  if (peek().type != TOK_IDENT) {
    m_error = QStringLiteral("expected variable name in for at line %1")
                  .arg(peek().line);
    return false;
  }
  fs.varName = advance().text;
  if (!expect(TOK_IN, QStringLiteral("expected 'in'")))
    return false;
  if (!parseExpr(fs.arrayExpr))
    return false;
  if (!expect(TOK_RPAREN, QStringLiteral("expected ')'")))
    return false;
  return parseBlock(fs.body);
}

/// @brief 解析 if (cond) { then } [else { else }]
bool ScriptParser::parseIfStmt(IfStmt &is) {
  advance(); // 跳过 'if'
  if (!expect(TOK_LPAREN, QStringLiteral("expected '(' after if")))
    return false;
  if (!parseExpr(is.condition))
    return false;
  if (!expect(TOK_RPAREN, QStringLiteral("expected ')'")))
    return false;
  if (!parseBlock(is.thenBlock))
    return false;
  // 可选的 else 分支
  if (peek().type == TOK_ELSE) {
    advance();
    is.hasElse = true;
    return parseBlock(is.elseBlock);
  }
  return true;
}

/// @brief 解析表达式入口 — 委托给 parseAddSub 实现运算符优先级
bool ScriptParser::parseExpr(Expr &expr) { return parseAddSub(expr); }

/// @brief 解析加减法（最低优先级），左结合
bool ScriptParser::parseAddSub(Expr &expr) {
  if (!parseMulDiv(expr))
    return false;
  while (peek().type == TOK_PLUS || peek().type == TOK_MINUS) {
    Expr::BinaryOp op;
    if (peek().type == TOK_PLUS) {
      op = Expr::kAdd;
      advance();
    } else {
      op = Expr::kSub;
      advance();
    }
    Expr *left = new Expr(std::move(expr));
    Expr *right = new Expr();
    if (!parseMulDiv(*right)) {
      delete left;
      delete right;
      return false;
    }
    Expr binary;
    binary.kind = Expr::kBinary;
    binary.binOp = op;
    binary.left = left;
    binary.right = right;
    expr = std::move(binary);
  }
  return true;
}

/// @brief 解析乘除法（较高优先级），左结合
bool ScriptParser::parseMulDiv(Expr &expr) {
  if (!parsePrimary(expr))
    return false;
  while (peek().type == TOK_MUL || peek().type == TOK_DIV) {
    Expr::BinaryOp op;
    if (peek().type == TOK_MUL) {
      op = Expr::kMul;
      advance();
    } else {
      op = Expr::kDiv;
      advance();
    }
    Expr *left = new Expr(std::move(expr));
    Expr *right = new Expr();
    if (!parsePrimary(*right)) {
      delete left;
      delete right;
      return false;
    }
    Expr binary;
    binary.kind = Expr::kBinary;
    binary.binOp = op;
    binary.left = left;
    binary.right = right;
    expr = std::move(binary);
  }
  return true;
}

/// @brief 解析基础表达式 — 字符串/数字/标识符/对象/数组/函数调用
bool ScriptParser::parsePrimary(Expr &expr) {
  Token t = peek();

  // ── 一元负号 ──
  if (t.type == TOK_MINUS) {
    advance();
    // 检查后面跟的是数字 → 直接产生负数字面量
    if (peek().type == TOK_NUMBER) {
      expr.kind = Expr::kNumber;
      expr.numVal = -advance().text.toDouble();
      return true;
    }
    // 否则产生 0 - expr 的二元运算（处理 -x 或 -(a+b) 等情况）
    Expr *right = new Expr();
    if (!parsePrimary(*right)) {
      delete right;
      return false;
    }
    Expr binary;
    binary.kind = Expr::kBinary;
    binary.binOp = Expr::kSub;
    binary.left = new Expr();
    binary.left->kind = Expr::kNumber;
    binary.left->numVal = 0;
    binary.right = right;
    expr = std::move(binary);
    return true;
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

  case TOK_LPAREN: {
    // 括号分组 (expr)
    advance(); // 消费 '('
    if (!parseExpr(expr))
      return false;
    return expect(TOK_RPAREN, QStringLiteral("expected ')'"));
  }

  case TOK_IDENT: {
    QString name = advance().text;
    if (peek().type == TOK_DOT) {
      advance();
      if (peek().type != TOK_IDENT) {
        m_error = QStringLiteral("expected property name after '.' at line %1")
                      .arg(peek().line);
        return false;
      }
      expr.kind = Expr::kPropAccess;
      expr.ident = name;
      expr.prop = advance().text;
      return true;
    }
    if (peek().type == TOK_LPAREN) {
      return parseFuncCall(name, expr);
    }
    if (peek().type == TOK_LBRACKET) {
      // 索引访问：obj["key"]
      advance(); // 消费 '['
      if (peek().type != TOK_STRING) {
        m_error = QStringLiteral("expected string key in '[]' at line %1")
                      .arg(peek().line);
        return false;
      }
      expr.kind = Expr::kIndexAccess;
      expr.ident = name;
      expr.indexKey = advance().text;
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

  default:
    m_error = QStringLiteral("unexpected token '%1' at line %2")
                  .arg(t.text, QString::number(t.line));
    return false;
  }
}

/// @brief 解析对象字面量 { key: val, ... }
bool ScriptParser::parseObject(Expr &expr) {
  expr.kind = Expr::kObject;
  advance(); // 跳过 '{'
  while (peek().type != TOK_RBRACE && peek().type != TOK_EOF) {
    if (peek().type != TOK_IDENT) {
      m_error =
          QStringLiteral("expected key in object at line %1").arg(peek().line);
      return false;
    }
    ObjectEntry entry;
    entry.key = advance().text;
    if (!expect(TOK_COLON, QStringLiteral("expected ':'")))
      return false;
    entry.value = new Expr();
    if (!parseExpr(*entry.value)) {
      delete entry.value;
      return false;
    }
    expr.objEntries.append(entry);
    if (peek().type == TOK_COMMA)
      advance();
  }
  return expect(TOK_RBRACE, QStringLiteral("expected '}'"));
}

/// @brief 解析数组字面量 [item, ...]
bool ScriptParser::parseArray(Expr &expr) {
  expr.kind = Expr::kArray;
  advance(); // 跳过 '['
  while (peek().type != TOK_RBRACKET && peek().type != TOK_EOF) {
    auto *item = new Expr();
    if (!parseExpr(*item)) {
      delete item;
      return false;
    }
    expr.arrItems.append(item);
    if (peek().type == TOK_COMMA)
      advance();
  }
  return expect(TOK_RBRACKET, QStringLiteral("expected ']'"));
}

/// @brief 解析函数调用 name(arg1, arg2, ...)
bool ScriptParser::parseFuncCall(const QString &name, Expr &expr) {
  // 校验函数名是否为已知内置函数
  static const QSet<QString> kBuiltins = {
      QStringLiteral("call"),   QStringLiteral("readJson"),
      QStringLiteral("render"), QStringLiteral("write"),
      QStringLiteral("print"),  QStringLiteral("getCheckedFiles"),
      QStringLiteral("merge"),  QStringLiteral("basename")};
  if (!kBuiltins.contains(name)) {
    m_error = QStringLiteral("unknown function '%1' at line %2")
                  .arg(name, QString::number(peek().line));
    return false;
  }

  expr.kind = Expr::kFuncCall;
  expr.funcCall.name = name;
  advance(); // 跳过 '('
  while (peek().type != TOK_RPAREN && peek().type != TOK_EOF) {
    auto *arg = new Expr();
    if (!parseExpr(*arg)) {
      delete arg;
      return false;
    }
    expr.funcCall.args.append(arg);
    if (peek().type == TOK_COMMA)
      advance();
  }
  return expect(TOK_RPAREN, QStringLiteral("expected ')'"));
}

// ── 解释执行（Interpreter） ──

/// @brief 根据表达式类型计算 JSON 值
QJsonValue ScriptParser::evalExpr(const Expr &expr) {
  switch (expr.kind) {
  case Expr::kString:
    return QJsonValue(expr.strVal);

  case Expr::kNumber:
    return QJsonValue(expr.numVal);

  case Expr::kIdent:
    if (m_vars.contains(expr.ident))
      return m_vars[expr.ident];
    if (m_globals.contains(expr.ident))
      return m_globals[expr.ident];
    m_error = QStringLiteral("undefined variable '%1'").arg(expr.ident);
    return QJsonValue();

  case Expr::kPropAccess: {
    QJsonValue obj;
    if (m_vars.contains(expr.ident))
      obj = m_vars[expr.ident];
    else if (m_globals.contains(expr.ident))
      obj = m_globals[expr.ident];
    if (obj.isObject())
      return obj.toObject().value(expr.prop);
    if (obj.isArray()) {
      // support array index access: arr.0, arr.1, etc.
      bool ok = false;
      int idx = expr.prop.toInt(&ok);
      if (ok) {
        QJsonArray arr = obj.toArray();
        if (idx >= 0 && idx < arr.size())
          return arr[idx];
      }
    }
    m_error = QStringLiteral("cannot access property '%1' on '%2'")
                  .arg(expr.prop, expr.ident);
    return QJsonValue();
  }

  case Expr::kIndexAccess: {
    QJsonValue obj;
    if (m_vars.contains(expr.ident))
      obj = m_vars[expr.ident];
    else if (m_globals.contains(expr.ident))
      obj = m_globals[expr.ident];
    if (obj.isObject())
      return obj.toObject().value(expr.indexKey);
    if (obj.isArray()) {
      bool ok = false;
      int idx = expr.indexKey.toInt(&ok);
      if (ok) {
        QJsonArray arr = obj.toArray();
        if (idx >= 0 && idx < arr.size())
          return arr[idx];
      }
    }
    m_error = QStringLiteral("cannot access index '%1' on '%2'")
                  .arg(expr.indexKey, expr.ident);
    return QJsonValue();
  }

  case Expr::kObject: {
    QJsonObject obj;
    for (const auto &e : expr.objEntries)
      obj[e.key] = evalExpr(*e.value);
    return obj;
  }

  case Expr::kArray: {
    QJsonArray arr;
    for (auto *e : expr.arrItems)
      arr.append(evalExpr(*e));
    return arr;
  }

  case Expr::kFuncCall: {
    return callBuiltin(expr.funcCall.name, expr.funcCall.args);
  }

  case Expr::kBinary:
    return evalBinary(expr);
  }

  return QJsonValue();
}

/// @brief 计算二元运算表达式
QJsonValue ScriptParser::evalBinary(const Expr &expr) {
  QJsonValue l = evalExpr(*expr.left);
  QJsonValue r = evalExpr(*expr.right);

  switch (expr.binOp) {
  case Expr::kAdd: {
    // + 支持字符串拼接和数字加法
    // 注意：QJsonValue::toString() 在 Qt 6
    // 中对数值类型不会自动转换（返回空字符串）， 因此必须显式用
    // QString::number() 转换
    if (l.isString() || r.isString()) {
      QString ls = l.isString() ? l.toString() : QString::number(l.toDouble());
      QString rs = r.isString() ? r.toString() : QString::number(r.toDouble());
      return QJsonValue(ls + rs);
    }
    return QJsonValue(l.toDouble() + r.toDouble());
  }
  case Expr::kSub:
    return QJsonValue(l.toDouble() - r.toDouble());
  case Expr::kMul:
    return QJsonValue(l.toDouble() * r.toDouble());
  case Expr::kDiv:
    if (r.toDouble() == 0.0) {
      m_error = QStringLiteral("division by zero");
      return QJsonValue();
    }
    return QJsonValue(l.toDouble() / r.toDouble());
  }
  return QJsonValue();
}

/// @brief 通过 FunMgr 调用 C++ 函数
QJsonValue ScriptParser::callFunMgr(const QString &cls, const QString &func,
                                    const QJsonValue &args) {
  QJsonArray arr;
  if (args.isArray())
    arr = args.toArray();
  else if (!args.isNull() && !args.isUndefined())
    arr.append(args);
  return FunMgr::ins().call(cls, func, arr);
}

/// @brief 执行内置函数：call / readJson / render / write / getCheckedFiles /
/// merge / basename
QJsonValue ScriptParser::callBuiltin(const QString &name,
                                     const QVector<Expr *> &args) {
  // call("className", "funcName", args) → FunMgr::call
  if (name == QStringLiteral("call")) {
    if (args.size() < 2) {
      m_error = QStringLiteral("call() requires at least 2 arguments");
      return QJsonValue();
    }
    QString cls = evalExpr(*args[0]).toString();
    QString func = evalExpr(*args[1]).toString();
    QJsonValue a = args.size() >= 3 ? evalExpr(*args[2]) : QJsonValue();
    return callFunMgr(cls, func, a);
  }

  // readJson(path) → read JSON file
  if (name == QStringLiteral("readJson")) {
    if (args.isEmpty()) {
      m_error = QStringLiteral("readJson() requires a path argument");
      return QJsonValue();
    }
    QString path = evalExpr(*args[0]).toString();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
      return QJsonObject();
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    return doc.isObject() ? QJsonValue(doc.object()) : QJsonValue();
  }

  // render(template, data) → TemplateEngine::render
  if (name == QStringLiteral("render")) {
    if (args.size() < 2) {
      m_error = QStringLiteral("render() requires template and data arguments");
      return QJsonValue();
    }
    QString tplPath = evalExpr(*args[0]).toString();
    // 相对路径相对于脚本目录解析
    QFileInfo tplInfo(tplPath);
    if (tplInfo.isRelative())
      tplPath = m_scriptDir + QStringLiteral("/") + tplPath;

    QFile f(tplPath);
    if (!f.open(QIODevice::ReadOnly))
      return QJsonValue();
    QString tpl = QString::fromUtf8(f.readAll());

    TemplateEngine engine;
    if (m_logCallback)
      engine.setLogCallback(m_logCallback);
    QJsonValue data = evalExpr(*args[1]);
    if (!data.isObject()) {
      m_error = QStringLiteral("render() data must be a JSON object");
      return QJsonValue();
    }
    return QJsonValue(engine.render(tpl, data.toObject()));
  }

  // print(text) → 输出日志到 UI 面板
  if (name == QStringLiteral("print")) {
    if (args.isEmpty()) {
      m_error = QStringLiteral("print() requires at least one argument");
      return QJsonValue();
    }
    QString text = evalExpr(*args[0]).toString();
    if (m_logCallback)
      m_logCallback(text, false);
    return QJsonValue(true);
  }

  // write(path, content) → write file
  if (name == QStringLiteral("write")) {
    if (args.size() < 2) {
      m_error = QStringLiteral("write() requires path and content arguments");
      return QJsonValue(false);
    }
    QString path = evalExpr(*args[0]).toString();
    QString content = evalExpr(*args[1]).toString();
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
      return QJsonValue(false);
    f.write(content.toUtf8());
    m_generatedFiles.append(QDir::toNativeSeparators(path));
    return QJsonValue(true);
  }

  // getCheckedFiles() → read tree.config checked paths
  if (name == QStringLiteral("getCheckedFiles")) {
    QString treePath = m_rootDir.isEmpty()
                           ? m_scriptDir + QStringLiteral("/tree.config")
                           : m_rootDir + QStringLiteral("/tree.config");
    QFile f(treePath);
    QJsonArray result;
    if (f.open(QIODevice::ReadOnly)) {
      QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
      QJsonArray checked =
          doc.object().value(QStringLiteral("checked")).toArray();
      for (const QJsonValue &v : checked) {
        if (v.isString())
          result.append(QDir::cleanPath(m_rootDir.isEmpty()
                                            ? m_scriptDir
                                            : m_rootDir + QStringLiteral("/") +
                                                  v.toString()));
      }
    }
    return result;
  }

  // merge(a, b) → merge two objects
  if (name == QStringLiteral("merge")) {
    if (args.size() < 2) {
      m_error = QStringLiteral("merge() requires two arguments");
      return QJsonValue();
    }
    QJsonValue a = evalExpr(*args[0]);
    QJsonValue b = evalExpr(*args[1]);
    QJsonObject result = a.toObject();
    QJsonObject ob = b.toObject();
    for (auto it = ob.begin(); it != ob.end(); ++it)
      result[it.key()] = it.value();
    return result;
  }

  // basename(path) → file basename without extension
  if (name == QStringLiteral("basename")) {
    if (args.isEmpty())
      return QJsonValue();
    QString path = evalExpr(*args[0]).toString();
    return QJsonValue(QFileInfo(path).completeBaseName());
  }

  m_error = QStringLiteral("unknown function '%1'").arg(name);
  return QJsonValue();
}

/// @brief 执行单条语句：调用/赋值/循环
void ScriptParser::execStmt(const Stmt &stmt) {
  switch (stmt.kind) {
  case Stmt::kCall: {
    QString cls = evalExpr(stmt.call.className).toString();
    QString func = evalExpr(stmt.call.funcName).toString();
    QJsonValue a =
        stmt.call.args.kind != Expr::kString || !stmt.call.args.strVal.isEmpty()
            ? evalExpr(stmt.call.args)
            : QJsonValue();
    callFunMgr(cls, func, a);
    break;
  }

  case Stmt::kAssign: {
    m_vars[stmt.assign.name] = evalExpr(stmt.assign.value);
    break;
  }

  case Stmt::kIndexAssign: {
    QJsonValue objVal = m_vars.value(stmt.indexAssign.objName);
    QJsonObject obj = objVal.toObject();
    obj[stmt.indexAssign.key] = evalExpr(stmt.indexAssign.value);
    m_vars[stmt.indexAssign.objName] = obj;
    break;
  }

  case Stmt::kFor: {
    QJsonValue arrVal = evalExpr(stmt.forStmt.arrayExpr);
    QJsonArray arr = arrVal.toArray();
    if (arr.isEmpty() && arrVal.isArray()) {
      // empty array, nothing to do
      break;
    }
    for (const QJsonValue &v : arr) {
      m_vars[stmt.forStmt.varName] = v;
      execBlock(stmt.forStmt.body);
      if (!m_error.isEmpty())
        return;
    }
    break;
  }

  case Stmt::kIf: {
    QJsonValue cond = evalExpr(stmt.ifStmt.condition);
    bool truthy = false;
    if (cond.isBool())
      truthy = cond.toBool();
    else if (cond.isString())
      truthy = !cond.toString().isEmpty();
    else if (cond.isDouble())
      truthy = cond.toDouble() != 0.0;
    else if (cond.isNull() || cond.isUndefined())
      truthy = false;
    else
      truthy = true; // object/array 均视为 truthy

    if (truthy)
      execBlock(stmt.ifStmt.thenBlock);
    else if (stmt.ifStmt.hasElse)
      execBlock(stmt.ifStmt.elseBlock);
    break;
  }

  case Stmt::kExpr: {
    evalExpr(stmt.exprStmt);
    break;
  }
  }
}

/// @brief 执行语句块中的所有语句
void ScriptParser::execBlock(const Block &block) {
  for (const auto &stmt : block.stmts) {
    execStmt(stmt);
    if (!m_error.isEmpty())
      return;
  }
}

/// @brief 解析后验证：检查 AST 中所有 kIdent 是否已用 let 声明
/// @return 错误信息列表，格式 "undefined variable 'xxx' at line N"
QStringList ScriptParser::validateUndeclaredIdents() const {
  QStringList errors;
  // 初始作用域 = 已用 let 声明的全局变量
  QSet<QString> scopeVars = m_declaredVars;
  validateBlockIdents(m_program, errors, scopeVars);
  return errors;
}

/// @brief 递归验证 Block 中的所有 Stmt
void ScriptParser::validateBlockIdents(const Block &block, QStringList &errors,
                                       const QSet<QString> &scopeVars) const {
  for (const auto &stmt : block.stmts)
    validateStmtIdents(stmt, errors, scopeVars);
}

/// @brief 验证单个 Stmt 中的 kIdent
void ScriptParser::validateStmtIdents(const Stmt &stmt, QStringList &errors,
                                      const QSet<QString> &scopeVars) const {
  switch (stmt.kind) {
  case Stmt::kCall:
    validateExprIdents(stmt.call.className, errors, scopeVars);
    validateExprIdents(stmt.call.funcName, errors, scopeVars);
    validateExprIdents(stmt.call.args, errors, scopeVars);
    break;
  case Stmt::kAssign:
    validateExprIdents(stmt.assign.value, errors, scopeVars);
    break;
  case Stmt::kIndexAssign:
    validateExprIdents(stmt.indexAssign.value, errors, scopeVars);
    break;
  case Stmt::kFor: {
    validateExprIdents(stmt.forStmt.arrayExpr, errors, scopeVars);
    // for 循环变量在 body 内视为已声明
    QSet<QString> bodyScope = scopeVars;
    bodyScope.insert(stmt.forStmt.varName);
    validateBlockIdents(stmt.forStmt.body, errors, bodyScope);
    break;
  }
  case Stmt::kIf:
    validateExprIdents(stmt.ifStmt.condition, errors, scopeVars);
    validateBlockIdents(stmt.ifStmt.thenBlock, errors, scopeVars);
    if (stmt.ifStmt.hasElse)
      validateBlockIdents(stmt.ifStmt.elseBlock, errors, scopeVars);
    break;
  case Stmt::kExpr:
    validateExprIdents(stmt.exprStmt, errors, scopeVars);
    break;
  }
}

/// @brief 递归验证 Expr 树中的 kIdent
void ScriptParser::validateExprIdents(const Expr &expr, QStringList &errors,
                                      const QSet<QString> &scopeVars) const {
  switch (expr.kind) {
  case Expr::kIdent:
    if (!scopeVars.contains(expr.ident)) {
      errors << QStringLiteral("undefined variable '%1' at line %2")
                    .arg(expr.ident, QString::number(expr.line));
    }
    break;
  case Expr::kPropAccess:
    // obj.prop：验证 obj 部分
    if (!scopeVars.contains(expr.ident)) {
      errors << QStringLiteral("undefined variable '%1' at line %2")
                    .arg(expr.ident, QString::number(expr.line));
    }
    break;
  case Expr::kIndexAccess:
    // obj["key"]：验证 obj 部分
    if (!scopeVars.contains(expr.ident)) {
      errors << QStringLiteral("undefined variable '%1' at line %2")
                    .arg(expr.ident, QString::number(expr.line));
    }
    break;
  case Expr::kFuncCall:
    for (const auto *arg : expr.funcCall.args)
      validateExprIdents(*arg, errors, scopeVars);
    break;
  case Expr::kObject:
    for (const auto &entry : expr.objEntries) {
      if (entry.value)
        validateExprIdents(*entry.value, errors, scopeVars);
    }
    break;
  case Expr::kArray:
    for (const auto *item : expr.arrItems)
      validateExprIdents(*item, errors, scopeVars);
    break;
  case Expr::kBinary:
    if (expr.left)
      validateExprIdents(*expr.left, errors, scopeVars);
    if (expr.right)
      validateExprIdents(*expr.right, errors, scopeVars);
    break;
  default:
    // kString / kNumber — 无需检查
    break;
  }
}

/// @brief 执行脚本：重新解析 token 序列并执行
QJsonValue ScriptParser::execute() {
  m_error.clear();
  m_vars.clear();
  m_declaredVars.clear();
  m_program = Block();
  // Re-parse the program from tokens
  m_pos = 0;
  if (!parseProgram(m_program))
    return QJsonValue();
  execBlock(m_program);
  if (!m_error.isEmpty())
    return QJsonValue();
  return QJsonValue(true);
}