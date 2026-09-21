/**
 * @file test_ac_diagnostic.cpp
 * @brief 结构化诊断系统单测（AcDiagnostic / AcDiagCollector / 各阶段接入）
 *
 * 覆盖：
 *  - 多错误脚本收集 ≥2 条诊断（未声明标识符收集式）
 *  - 诊断错误码 / 行号 / 列号正确
 *  - 语法错误诊断（AC1xxx）+ legacy error() 兼容（"at line N" 后缀）
 *  - 词法错误诊断（AC0xxx）
 *  - 类型错误诊断（AC4xxx）与警告分级（kWarning）
 *  - AcValidator::validate() 直接产出带位置的 ValidationResult（无正则反解）
 *
 * 构建：cmake --build <build-dir> --target auto_code_tests --config Debug
 */

#include <cstdio>

#include "src/engine/function/fun_mgr.h"
#include "src/engine/script/ac_diagnostic.h"
#include "src/engine/script/ac_executor.h"
#include "src/engine/script/ac_validator.h"

static int g_total = 0;
static int g_failed = 0;

/// 极简断言：失败打印位置并计数，不中断后续用例
#define CHECK(cond)                                               \
  do {                                                            \
    ++g_total;                                                    \
    if (!(cond)) {                                                \
      ++g_failed;                                                 \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    }                                                             \
  } while (0)

/// 判断诊断列表中是否存在指定错误码
static bool hasCode(const QVector<AcDiagnostic> &diags, const char *code) {
  for (const auto &d : diags) {
    if (d.code == QLatin1String(code)) return true;
  }
  return false;
}

// ── 收集器基础行为 ──

static void testCollectorBasics() {
  AcDiagCollector c;
  CHECK(!c.hasErrors());
  CHECK(c.size() == 0);

  c.error(AcDiagCode::kSyntaxExpected, QStringLiteral("expected ';'"), QStringLiteral("a.ac"),
          AcLoc{3, 5, 20});
  c.warning(AcDiagCode::kTypeError, QStringLiteral("warning: uses Any"), QStringLiteral("a.ac"),
            AcLoc{7, 1, 100});
  CHECK(c.size() == 2);
  CHECK(c.hasErrors());
  CHECK(c.errors().size() == 1);
  CHECK(c.firstErrorMessage() == QStringLiteral("expected ';'"));
  CHECK(c.all().first().loc.line == 3);
  CHECK(c.all().first().loc.col == 5);
  CHECK(c.all().at(1).isWarning());
  CHECK(!c.all().first().isWarning());

  // toValidationResult 映射
  ValidationResult vr = toValidationResult(c.all().first());
  CHECK(vr.line == 3);
  CHECK(vr.column == 5);
  CHECK(vr.severity == ValidationResult::kError);
  CHECK(vr.message == QStringLiteral("expected ';'"));
  ValidationResult vrWarn = toValidationResult(c.all().at(1));
  CHECK(vrWarn.severity == ValidationResult::kWarning);

  c.clear();
  CHECK(c.size() == 0);
  CHECK(!c.hasErrors());
}

// ── 未声明标识符：多错误收集（AC3001） ──

static void testUndeclaredMultipleDiagnostics() {
  AcExecutor exec;
  const QString src = QStringLiteral(
      "let a: Number = 1;\n"
      "let b: Number = undefinedVar1 + undefinedVar2;\n"
      "return a + b;\n");
  CHECK(exec.parse(src));
  exec.execute();
  const auto &diags = exec.diagnostics().all();
  CHECK(diags.size() >= 2);
  CHECK(hasCode(diags, AcDiagCode::kUndeclaredIdent));
  // 首个未声明错误定位在第 2 行
  bool foundLine2 = false;
  for (const auto &d : diags) {
    if (d.code == QLatin1String(AcDiagCode::kUndeclaredIdent) && d.loc.line == 2) {
      foundLine2 = true;
      break;
    }
  }
  CHECK(foundLine2);
}

// ── 语法错误：诊断 + legacy 兼容 ──

static void testSyntaxDiagnosticAndLegacyError() {
  AcExecutor exec;
  const QString src = QStringLiteral(
      "let a: Number = 1;\n"
      "let b: Number = 2\n"
      "return a;\n");
  CHECK(!exec.parse(src));
  // legacy 单错误串保持 "at line N" 格式
  CHECK(exec.error().contains(QStringLiteral("at line 2")));
  // 结构化诊断同步产出（缺分号 → AC1002）
  const auto &diags = exec.diagnostics().all();
  CHECK(diags.size() >= 1);
  CHECK(hasCode(diags, AcDiagCode::kSyntaxMissingSemi));
  CHECK(diags.first().loc.line == 2);
  // 诊断消息是干净消息（不含行号后缀）
  CHECK(!diags.first().message.contains(QStringLiteral("at line")));
}

// ── 词法错误：未终止字符串（AC0001） ──

static void testLexDiagnostic() {
  AcExecutor exec;
  const QString src = QStringLiteral(
      "let a: String = \"unterminated\n"
      "return a;\n");
  CHECK(!exec.parse(src));
  const auto &diags = exec.diagnostics().all();
  CHECK(diags.size() >= 1);
  CHECK(hasCode(diags, AcDiagCode::kLexUnterminatedString));
  CHECK(diags.first().loc.line == 1);
}

// ── 类型错误：诊断 + 警告分级 ──

static void testTypeDiagnosticAndWarningSeverity() {
  AcExecutor exec;
  // 类型错误：把 String 赋给 Number
  const QString src = QStringLiteral(
      "let a: Number = 0;\n"
      "a = \"not a number\";\n"
      "return a;\n");
  CHECK(exec.parse(src));
  exec.execute();
  CHECK(!exec.error().isEmpty());
  const auto &diags = exec.diagnostics().all();
  CHECK(hasCode(diags, AcDiagCode::kTypeError));
}

// ── AcValidator：validate() 直接产出带位置的 ValidationResult ──

static void testValidatorProducesPositionedResults() {
  AcValidator validator;
  const QString src = QStringLiteral(
      "let a: Number = 1;\n"
      "let b: Number = undefinedVar1 + undefinedVar2;\n"
      "return a + b;\n");
  const QVector<ValidationResult> results = validator.validate(src);
  CHECK(results.size() >= 2);
  // 全部结果带正确行号（未声明错误在第 2 行）
  bool hasLine2 = false;
  for (const auto &r : results) {
    if (r.line == 2 && r.message.contains(QStringLiteral("undefinedVar"))) hasLine2 = true;
  }
  CHECK(hasLine2);
  // 消息是干净消息（不再含 "at line N" 反解残留）
  for (const auto &r : results) {
    CHECK(!r.message.contains(QStringLiteral("at line ")));
  }
}

static void testValidatorSyntaxErrorHasLine() {
  AcValidator validator;
  const QString src = QStringLiteral(
      "let a: Number = 1;\n"
      "let b: Number = 2\n"
      "return a;\n");
  const QVector<ValidationResult> results = validator.validate(src);
  CHECK(results.size() >= 1);
  CHECK(results.first().line == 2);
}

int runAcDiagnosticTests() {
  testCollectorBasics();
  testUndeclaredMultipleDiagnostics();
  testSyntaxDiagnosticAndLegacyError();
  testLexDiagnostic();
  testTypeDiagnosticAndWarningSeverity();
  testValidatorProducesPositionedResults();
  testValidatorSyntaxErrorHasLine();
  std::printf("[ac_diagnostic] %d checks\n", g_total);
  return g_failed;
}
