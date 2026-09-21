/**
 * @file ac_semantic_service.cpp
 * @brief 进程内语义服务实现 — 基于错误恢复解析 + 词法定位的单文件语义助手
 */

#include "ac_semantic_service.h"

#include <QSet>
#include <QStringList>

#include "ac_lexer.h"
#include "ac_validator.h"

namespace {

/// 词法分析（容忍个别词法错误：失败也返回已有 token）
QVector<Token> lex(const QString &source) {
  QString err;
  return AcLexer::tokenize(source, err);
}

/// 定位 (line,col) 处的 token 下标；未命中返回 -1
int tokenAt(const QVector<Token> &toks, int line, int col) {
  for (int i = 0; i < toks.size(); ++i) {
    const Token &t = toks[i];
    if (t.type == TokenType::kEof) continue;
    if (t.loc.line != line) continue;
    if (col >= t.loc.col && col < t.loc.col + t.text.length()) return i;
  }
  return -1;
}

/// 补全关键字表（语言关键字 + 常用内置类）
const QStringList &completionKeywords() {
  static const QStringList k = {QStringLiteral("let"),   QStringLiteral("const"),
                                QStringLiteral("function"), QStringLiteral("class"),
                                QStringLiteral("interface"), QStringLiteral("enum"),
                                QStringLiteral("if"),     QStringLiteral("else"),
                                QStringLiteral("for"),    QStringLiteral("while"),
                                QStringLiteral("return"), QStringLiteral("switch"),
                                QStringLiteral("case"),   QStringLiteral("default"),
                                QStringLiteral("break"),  QStringLiteral("continue"),
                                QStringLiteral("import"), QStringLiteral("export"),
                                QStringLiteral("from"),   QStringLiteral("using"),
                                QStringLiteral("try"),    QStringLiteral("catch"),
                                QStringLiteral("finally"), QStringLiteral("throw"),
                                QStringLiteral("new"),    QStringLiteral("extends"),
                                QStringLiteral("implements"), QStringLiteral("super"),
                                QStringLiteral("this"),   QStringLiteral("static"),
                                QStringLiteral("public"), QStringLiteral("private"),
                                QStringLiteral("protected"), QStringLiteral("override"),
                                QStringLiteral("null"),   QStringLiteral("undefined"),
                                QStringLiteral("true"),   QStringLiteral("false"),
                                QStringLiteral("in"),     QStringLiteral("as"),
                                QStringLiteral("String"), QStringLiteral("Number"),
                                QStringLiteral("Boolean"), QStringLiteral("Object"),
                                QStringLiteral("Array"),  QStringLiteral("Any")};
  return k;
}

}  // namespace

QVector<ValidationResult> AcSemanticService::diagnose(const QString &source,
                                                      const QString &filePath) {
  AcValidator validator;
  validator.setFilePath(filePath);
  return validator.validate(source);
}

QVector<AcCompletionItem> AcSemanticService::complete(const QString &source, const QString &) {
  QVector<AcCompletionItem> items;
  for (const auto &k : completionKeywords()) {
    items.append({k, QStringLiteral("keyword")});
  }
  QSet<QString> seen;
  const QVector<Token> toks = lex(source);
  for (const auto &t : toks) {
    if (t.type != TokenType::kIdent || seen.contains(t.text)) continue;
    // 声明处归类（let/const/function/class/interface/enum 之后的标识符）
    // 简化：一律按 'ident' 返回，编辑器按其字形着色
    seen.insert(t.text);
    items.append({t.text, QStringLiteral("ident")});
  }
  return items;
}

AcSymbolLoc AcSemanticService::resolveDefinition(const QString &source, const QString &filePath,
                                                 int line, int col) {
  const QVector<Token> toks = lex(source);
  const int idx = tokenAt(toks, line, col);
  if (idx < 0 || toks[idx].type != TokenType::kIdent) return AcSymbolLoc();
  const QString name = toks[idx].text;
  // 扫描文件中 name 的声明位置：紧跟声明关键字的同名标识符即定义
  for (int i = 1; i < toks.size(); ++i) {
    const Token &t = toks[i];
    if (t.type != TokenType::kIdent || t.text != name) continue;
    const TokenType prev = toks[i - 1].type;
    const bool isDecl = (prev == TokenType::kLet || prev == TokenType::kConst ||
                         prev == TokenType::kFunction || prev == TokenType::kClass ||
                         prev == TokenType::kInterface || prev == TokenType::kEnum);
    if (isDecl) {
      AcSymbolLoc loc;
      loc.filePath = filePath;
      loc.line = t.loc.line;
      loc.col = t.loc.col;
      return loc;
    }
  }
  return AcSymbolLoc();
}

QVector<AcSymbolLoc> AcSemanticService::findReferences(const QString &source,
                                                       const QString &filePath,
                                                       const QString &name) {
  QVector<AcSymbolLoc> out;
  const QVector<Token> toks = lex(source);
  for (const auto &t : toks) {
    if (t.type != TokenType::kIdent || t.text != name) continue;
    AcSymbolLoc loc;
    loc.filePath = filePath;
    loc.line = t.loc.line;
    loc.col = t.loc.col;
    out.append(loc);
  }
  return out;
}