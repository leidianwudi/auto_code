/**
 * @file test_ac_recovery.cpp
 * @brief Parser 错误恢复测试（阶段 5）
 *
 * 覆盖：多语法错误逐个收集 ≥2 条诊断 → 残缺 AST 不崩溃（验证器/符号/类型检查可继续
 * 于其上运行）→ 恢复后合法语句仍被解析（后续语句的未声明/类型错误仍正确报告）→
 * Parser 层 isRecovered 标记与 Expr::kError 占位。
 */

#include <cstdio>

#include "src/engine/script/ac_diagnostic.h"
#include "src/engine/script/ac_lexer.h"
#include "src/engine/script/ac_parser.h"
#include "src/engine/script/ac_validator.h"

static int g_total = 0;
static int g_failed = 0;

#define CHECK(cond)                                               \
  do {                                                            \
    ++g_total;                                                    \
    if (!(cond)) {                                                \
      ++g_failed;                                                 \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    }                                                             \
  } while (0)

/// Parser 层：恢复模式产出 isRecovered 占位 + 后续语句照常解析
static void testParserRecovery() {
  QString lexErr;
  const QVector<Token> tokens = AcLexer::tokenize(
      QStringLiteral("let a: = 1; let b: Number = 7; return b;"), lexErr);
  AcDiagCollector diags;
  AcParser parser;
  parser.setFilePath(QString());
  parser.setDiagCollector(&diags);
  parser.setRecoveryMode(true);
  Block program;
  QSet<QString> declaredVars;
  const bool ok = parser.parse(tokens, program, declaredVars);
  CHECK(ok);
  CHECK(program.stmts.size() == 3);
  if (program.stmts.size() == 3) {
    CHECK(program.stmts[0].isRecovered);
    CHECK(program.stmts[0].kind == Block::Stmt::kExpr);
    CHECK(program.stmts[0].exprStmt.kind == Expr::kError);
    CHECK(!program.stmts[1].isRecovered);
    CHECK(program.stmts[1].kind == Block::Stmt::kAssign);
    CHECK(!program.stmts[2].isRecovered);
    CHECK(program.stmts[2].kind == Block::Stmt::kReturn);
  }
  CHECK(diags.size() >= 1);  // 语法错误已收集（残缺语句的行号定位到 'let' 行）
}

/// 块内错误恢复：块内错误后块内后续语句仍被解析
static bool hasAssignInStmts(const Block &block, const QString &name) {
  for (const auto &s : block.stmts) {
    if (!s.isRecovered && s.kind == Block::Stmt::kAssign && s.assign.name == name) return true;
    // 递归进入复合语句体（块/if/循环/switch/try）
    if (s.kind == Block::Stmt::kBlock && hasAssignInStmts(s.blockBody, name)) return true;
    if (s.kind == Block::Stmt::kIf) {
      if (hasAssignInStmts(s.ifStmt.thenBlock, name)) return true;
      if (s.ifStmt.hasElse && hasAssignInStmts(s.ifStmt.elseBlock, name)) return true;
      if (s.ifStmt.elseIfBranch &&
          hasAssignInStmts(s.ifStmt.elseIfBranch->thenBlock, name))
        return true;
    }
    if (s.kind == Block::Stmt::kFor) {
      if (hasAssignInStmts(s.forStmt.body, name)) return true;
      if (hasAssignInStmts(s.forStmt.initBlock, name)) return true;
    }
    if (s.kind == Block::Stmt::kWhile && hasAssignInStmts(s.whileStmt.body, name)) return true;
    if (s.kind == Block::Stmt::kSwitch) {
      for (const auto &cs : s.switchStmt.cases) {
        if (hasAssignInStmts(cs.body, name)) return true;
      }
    }
    if (s.kind == Block::Stmt::kTry) {
      if (hasAssignInStmts(s.tryStmt.tryBody, name)) return true;
      if (hasAssignInStmts(s.tryStmt.catchBody, name)) return true;
      if (hasAssignInStmts(s.tryStmt.finallyBody, name)) return true;
    }
  }
  return false;
}

static void testRecoveryInsideBlock() {
  QString lexErr;
  const QVector<Token> tokens = AcLexer::tokenize(
      QStringLiteral("{ let x: = 1; let y: Number = 2; } let z: Number = y + x;"), lexErr);
  AcDiagCollector diags;
  AcParser parser;
  parser.setDiagCollector(&diags);
  parser.setRecoveryMode(true);
  Block program;
  QSet<QString> declaredVars;
  const bool ok = parser.parse(tokens, program, declaredVars);
  CHECK(ok);
  CHECK(hasAssignInStmts(program, QStringLiteral("y")));  // 块内错误后 y 声明仍被解析
  CHECK(diags.size() >= 1);
}

/// 验证器：多语法错误逐个收集；恢复后合法语句的错误仍被报告
static void testValidatorMultiDiagnostics() {
  AcValidator validator;

  // 两个独立行的语法错误都收集，且后续合法语句无级联假错
  const QString src = QStringLiteral(
      "let q: = 1;\n"
      "let w: = 2;\n"
      "let z: Number = 3;\n"
      "return z;\n");
  const auto r1 = validator.validate(src);
  CHECK(r1.size() >= 2);
  bool hasLine2Syntax = false;
  for (const auto &r : r1) {
    if (r.line == 2 && r.message.contains(QStringLiteral("expected"))) hasLine2Syntax = true;
  }
  CHECK(hasLine2Syntax);

  // 恢复后合法语句的错误仍被报告（未声明变量在第 5 行）
  const QString src2 = QStringLiteral(
      "let q: = 1;\n"
      "let z: Number = 3;\n"
      "return missingSymbol;\n");
  const auto r2 = validator.validate(src2);
  bool hasMissing = false;
  for (const auto &r : r2) {
    if (r.message.contains(QStringLiteral("missingSymbol"))) hasMissing = true;
  }
  CHECK(hasMissing);
}

/// 残缺 AST 不崩溃：类体内部错误后，后续顶层语句仍可收集与检查
static void testRecoveredAstNoCrash() {
  AcValidator validator;
  const QString src = QStringLiteral(
      "class Broken { let x: = 1; }\n"
      "let ok: Number = 5;\n"
      "return ok;\n");
  const auto results = validator.validate(src);
  CHECK(results.size() >= 1);  // 类体内语法错误有诊断，且全程不崩溃
}

/// A6：恢复模式下声明失败回滚 — 被丢弃的声明不再残留 declaredVars
static void testRecoveryRollbackDeclaredVars() {
  // Parser 层：let 声明失败被恢复 → 名字从 declaredVars 回滚；成功声明保留
  QString lexErr;
  const QVector<Token> tokens = AcLexer::tokenize(
      QStringLiteral("let bad: = 1; let good: Number = 2; return good;"), lexErr);
  AcParser parser;
  parser.setFilePath(QString());
  parser.setRecoveryMode(true);
  Block program;
  QSet<QString> declaredVars;
  CHECK(parser.parse(tokens, program, declaredVars));
  CHECK(declaredVars.contains(QStringLiteral("good")));
  CHECK(!declaredVars.contains(QStringLiteral("bad")));  // 失败声明已回滚
  if (program.stmts.size() >= 2) {
    CHECK(program.stmts[0].isRecovered);
    CHECK(!program.stmts[1].isRecovered);
  }

  // 端到端：回滚后，后续使用 bad 被正确识别为未声明（若无回滚则被当作已声明）
  AcValidator validator;
  const QString src = QStringLiteral(
      "let bad: = 1;\n"
      "let good: Number = 2;\n"
      "return bad + good;\n");
  bool foundUndeclaredBad = false;
  for (const auto &r : validator.validate(src)) {
    if (r.message.contains(QStringLiteral("bad"))) foundUndeclaredBad = true;
  }
  CHECK(foundUndeclaredBad);
}

int runAcRecoveryTests() {
  testParserRecovery();
  testRecoveryInsideBlock();
  testValidatorMultiDiagnostics();
  testRecoveredAstNoCrash();
  testRecoveryRollbackDeclaredVars();
  std::printf("[ac_recovery] %d checks, %d failed\n", g_total, g_failed);
  return g_failed;
}