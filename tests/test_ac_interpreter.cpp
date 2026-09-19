/**
 * @file test_ac_interpreter.cpp
 * @brief AC 脚本解释器直接单测（纯 QtCore，无 GUI）
 *
 * 通过 AcExecutor（parse + execute）驱动解释器，直接断言脚本返回值与错误信息，
 * 补齐此前仅靠 .ac 语料 + golden 进程测试的覆盖断层：
 *  - 算术/字符串/数组/对象基础求值
 *  - 控制流（while/for-in/if/switch/break/continue）
 *  - 函数与递归、函数表达式（lambda）
 *  - 类实例、this 方法、静态成员
 *  - 自增/自减回写（局部变量 / 属性 / 索引 —— 属性与索引自增为本次修复的新行为）
 *  - 运行时错误传播（错误信息带行号）
 *
 * 构建：cmake --build <build-dir> --target auto_code_tests
 */

#include <cstdio>

#include "src/engine/script/ac_executor.h"

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

/// 执行脚本：成功时返回 true 并把 main 返回值写入 result；失败时 error 含解释器/检查器信息
static bool runScript(const QString &src, QJsonValue &result, QString *error = nullptr) {
  AcExecutor exec;
  if (!exec.parse(src)) {
    if (error) *error = exec.error();
    std::printf("  [diag] parse error: %s\n", exec.error().toUtf8().constData());
    return false;
  }
  result = exec.execute();
  if (!exec.error().isEmpty()) {
    if (error) *error = exec.error();
    std::printf("  [diag] exec error: %s\n", exec.error().toUtf8().constData());
    return false;
  }
  return true;
}

// ── 基础求值 ──

static void testArithmeticPrecedence() {
  QJsonValue r;
  CHECK(runScript(QStringLiteral("return 1 + 2 * 3;"), r));
  CHECK(r.toDouble() == 7.0);
}

static void testStringConcat() {
  QJsonValue r;
  CHECK(runScript(QStringLiteral("return \"foo\" + \"bar\";"), r));
  CHECK(r.toString() == QStringLiteral("foobar"));
}

static void testArrayAccess() {
  QJsonValue r;
  CHECK(runScript(QStringLiteral("let a = [10, 20, 30]; return a[1] + a.length;"), r));
  CHECK(r.toDouble() == 23.0);
}

static void testObjectPropertyReadWrite() {
  QJsonValue r;
  CHECK(runScript(QStringLiteral("let o = {x: 10, y: \"s\"}; o.x = o.x + 5; return o.x;"), r));
  CHECK(r.toDouble() == 15.0);
}

static void testCompoundAssign() {
  QJsonValue r;
  CHECK(runScript(QStringLiteral("let n = 10; n += 5; n *= 2; return n;"), r));
  CHECK(r.toDouble() == 30.0);
}

// ── 自增/自减回写 ──

static void testIncDecLocal() {
  QJsonValue r;
  // 前置两次 +1 → 7
  CHECK(runScript(QStringLiteral("let i = 5; i++; ++i; return i;"), r));
  CHECK(r.toDouble() == 7.0);
}

static void testIncDecPostReturnsOld() {
  QJsonValue r;
  // 后置返回旧值：j = 5，i 自增后 = 6 → j*10 + i = 56
  CHECK(runScript(QStringLiteral("let i = 5; let j = i++; return j * 10 + i;"), r));
  CHECK(r.toDouble() == 56.0);
}

static void testIncDecPropertyWriteBack() {
  // 属性自增回写（修复点：此前 o.n++ 静默不回写）
  QJsonValue r;
  CHECK(runScript(QStringLiteral("let o = {n: 1}; o.n++; return o.n;"), r));
  CHECK(r.toDouble() == 2.0);
}

static void testIncDecIndexWriteBack() {
  // 索引自增回写（修复点：此前 obj[key]++ 静默不回写）
  QJsonValue r;
  CHECK(runScript(QStringLiteral("let m = {}; m[\"k\"] = 10; m[\"k\"]++; return m[\"k\"];"), r));
  CHECK(r.toDouble() == 11.0);
}

static void testDecPropertyWriteBack() {
  QJsonValue r;
  CHECK(runScript(QStringLiteral("let o = {n: 3}; --o.n; return o.n;"), r));
  CHECK(r.toDouble() == 2.0);
}

// ── 控制流 ──

static void testWhileLoop() {
  QJsonValue r;
  CHECK(runScript(
      QStringLiteral("let s = 0; let i = 1; while (i <= 10) { s += i; i++; } return s;"), r));
  CHECK(r.toDouble() == 55.0);
}

static void testForInLoop() {
  QJsonValue r;
  CHECK(runScript(QStringLiteral("let s = 0; for (v in [1, 2, 3, 4]) { s += v; } return s;"), r));
  CHECK(r.toDouble() == 10.0);
}

static void testBreakContinue() {
  QJsonValue r;
  CHECK(
      runScript(QStringLiteral("let s = 0; let i = 0; while (true) { i++; if (i > 100) { break; } "
                               "if (i % 2 == 0) { continue; } s += i; } return s;"),
                r));
  // 1..100 内奇数之和 = 2500（i>100 时 break）
  CHECK(r.toDouble() == 2500.0);
}

static void testSwitchCase() {
  QJsonValue r;
  CHECK(runScript(
      QStringLiteral("let x = 2; let s = \"\"; switch (x) { case 1: s = \"one\"; break; "
                     "case 2: s = \"two\"; break; default: s = \"other\"; break; } return s;"),
      r));
  CHECK(r.toString() == QStringLiteral("two"));
}

// ── 函数 ──

static void testFunctionCall() {
  QJsonValue r;
  CHECK(runScript(QStringLiteral("function add(a: Number, b: Number): Number { return a + b; } "
                                 "return add(2, 3);"),
                  r));
  CHECK(r.toDouble() == 5.0);
}

static void testRecursion() {
  QJsonValue r;
  CHECK(runScript(QStringLiteral("function fib(n: Number): Number { if (n <= 1) { return n; } "
                                 "return fib(n - 1) + fib(n - 2); } return fib(10);"),
                  r));
  CHECK(r.toDouble() == 55.0);
}

static void testLambda() {
  QJsonValue r;
  CHECK(runScript(
      QStringLiteral("let f = function(a: Number): Number { return a * 2; }; return f(21);"), r));
  CHECK(r.toDouble() == 42.0);
}

// ── 类与实例 ──

static void testClassInstanceAndThis() {
  QJsonValue r;
  CHECK(runScript(QStringLiteral("class Counter { let n: Number = 0; "
                                 "constructor(base: Number) { this.n = base; } "
                                 "function add(v: Number): Number { this.n += v; return this.n; } } "
                                 "let c = new Counter(10); c.add(5); return c.n;"),
                r));
  CHECK(r.toDouble() == 15.0);
}

static void testClassStaticMember() {
  QJsonValue r;
  CHECK(runScript(QStringLiteral("class Config { static let version: Number = 7; "
                                 "static function get(): Number "
                                 "{ return Config.version; } } "
                                 "return Config.get();"),
                  r));
  CHECK(r.toDouble() == 7.0);
}

// ── 错误传播 ──

static void testRuntimeErrorPropagates() {
  // 调用字符串上不存在的方法 → 运行时错误，execute 返回 false 且错误信息非空
  AcExecutor exec;
  const bool parsed = exec.parse(QStringLiteral("let s = \"a\"; return s.notAMethod();"));
  CHECK(parsed);
  if (!parsed) return;
  QJsonValue r = exec.execute();
  CHECK(exec.error().isEmpty() == false);
  CHECK(r.isNull());
}

static void testRuntimeErrorHasLineNumber() {
  // 错误发生在第 3 行：错误信息必须带 "at line 3"（行号传播格式统一）
  AcExecutor exec;
  const bool parsed =
      exec.parse(QStringLiteral("let ok = 1;\nlet s = \"x\";\nreturn s.notAMethod();\n"));
  CHECK(parsed);
  if (!parsed) return;
  exec.execute();
  CHECK(exec.error().contains(QStringLiteral("line 3")));
}

static void testSyntaxErrorRejected() {
  // 语法错误（缺右括号）→ parse 失败，错误信息非空
  AcExecutor exec;
  const bool parsed = exec.parse(QStringLiteral("let a = [1, 2; return a;"));
  CHECK(!parsed);
  CHECK(!exec.error().isEmpty());
}

/// 运行全部用例，返回失败数（0 = 全部通过）；由 test_json_utils.cpp 的 main 调用
int runAcInterpreterTests() {
  testArithmeticPrecedence();
  testStringConcat();
  testArrayAccess();
  testObjectPropertyReadWrite();
  testCompoundAssign();
  testIncDecLocal();
  testIncDecPostReturnsOld();
  testIncDecPropertyWriteBack();
  testIncDecIndexWriteBack();
  testDecPropertyWriteBack();
  testWhileLoop();
  testForInLoop();
  testBreakContinue();
  testSwitchCase();
  testFunctionCall();
  testRecursion();
  testLambda();
  testClassInstanceAndThis();
  testClassStaticMember();
  testRuntimeErrorPropagates();
  testRuntimeErrorHasLineNumber();
  testSyntaxErrorRejected();
  std::printf("[ac_interpreter] %d checks, %d failed\n", g_total, g_failed);
  return g_failed;
}
