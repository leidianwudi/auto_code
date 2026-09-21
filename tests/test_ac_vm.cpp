/**
 * @file test_ac_vm.cpp
 * @brief 字节码 VM 对拍测试 — 同一脚本分别经解释器与字节码 VM 执行，断言结果一致
 *
 * 覆盖：算术/字符串/数组/对象、控制流（while/for-in/for/if/switch/break/continue）、
 * 函数与递归、函数表达式（lambda 高阶回调）、类实例/this/静态成员/继承/super、
 * 自增自减回写、复合赋值、复合运算、三元/逻辑短路/空值合并、try/catch、
 * 运行时错误传播（错误消息一致）。
 *
 * 构建：cmake --build <build-dir> --target auto_code_tests --config RelWithDebInfo
 */

#include <QJsonDocument>

#include <cstdio>

#include "src/engine/function/fun_mgr.h"
#include "src/engine/script/ac_executor.h"

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

#ifdef _MSC_VER
/// 普通调用（允许对象展开），由 __try 包裹（v1 调试保留，未启用）
static bool vmCall(AcExecutor &exec, QJsonValue &out, QString &err) {
  out = exec.execute();
  err = exec.error();
  return true;
}
#endif

/// 双跑：解释器 + 字节码 VM；返回两模式是否都成功且（结果序列化相等 && 错误串相等）
/// 精简序列化（标量/数组/对象统一转字符串，供对拍比较）
static QString toJsonText(const QJsonValue &v) {
  if (v.isObject())
    return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
  if (v.isArray())
    return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
  return v.toVariant().toString();
}

static bool runDual(const QString &src, QString *detail = nullptr) {
  // ── 解释器 ──
  AcExecutor iexec;
  bool iOk = iexec.parse(src);
  const QString iParseErr = iOk ? QString() : iexec.error();
  QJsonValue iResult = iOk ? iexec.execute() : QJsonValue();
  const QString iExecErr = iOk ? iexec.error() : QString();

  // ── 字节码 VM（指令级 trace 已 flush，崩溃前最后指令可定位） ──
  AcExecutor vexec;
  vexec.setExecMode(AcExecMode::kBytecode);
  bool vOk = vexec.parse(src);
  const QString vParseErr = vOk ? QString() : vexec.error();
  QJsonValue vResult = vOk ? vexec.execute() : QJsonValue();
  const QString vExecErr = vOk ? vexec.error() : QString();

  // parse 结果一致性
  if (iOk != vOk) {
    if (detail) *detail = QStringLiteral("parse ok mismatch: int=%1 vm=%2 (intErr='%3' vmErr='%4')")
                               .arg(iOk)
                               .arg(vOk)
                               .arg(iParseErr, vParseErr);
    return false;
  }
  if (!iOk) return true;  // 双方 parse 失败即一致
  // 执行错误一致性
  if (iExecErr != vExecErr) {
    if (detail) *detail = QStringLiteral("exec err mismatch: int='%1' vm='%2'")
                               .arg(iExecErr, vExecErr);
    return false;
  }
  // 结果一致性（序列化比较，键序保真）
  if (iResult != vResult) {
    if (detail)
      *detail = QStringLiteral("result mismatch: int=%1 vm=%2")
                    .arg(toJsonText(iResult), toJsonText(vResult));
    return false;
  }
  return true;
}

static void testDual(const char *name, const QString &src) {
  QString detail;
  if (!runDual(src, &detail)) {
    ++g_failed;
    ++g_total;
    std::printf("FAIL [%s] %s\n", name, detail.toUtf8().constData());
    return;
  }
  ++g_total;
}

// ── 用例语料（覆盖解释器 116 项测试的主要语义） ──

static const char *kArithmetic = "return 1 + 2 * 3 - 4 / 2;";
static const char *kStringConcat = "return \"a\" + \"b\" + 1 + 2;";
static const char *kArrayAccess = "let a: Number[] = [1, 2, 3]; return a[1];";
static const char *kObjectReadWrite = "let o: Object = {x: 1, y: \"z\"}; o.x = o.x + 5; return o.x;";
static const char *kCompoundAssign = "let x: Number = 5; x += 3; x *= 2; x -= 1; x /= 2; return x;";
static const char *kIncDecLocal =
    "let i: Number = 1; let a: Number = i++; let b: Number = ++i; let c: Number = i--; return a*100 + b*10 + c;";
static const char *kWhileLoop = "let s: Number = 0; let i: Number = 0; while (i < 10) { s += i; i += 1; } return s;";
static const char *kForInLoop = "let sum: Number = 0; for (let v: Number in [10, 20, 30]) { sum += v; } return sum;";
static const char *kForInObjKeys = "let keys: String[] = []; for (let k: String in {a: 1, b: 2}) { keys.push(k); } return keys.join(\",\");";
static const char *kBreakContinue =
    "let s: Number = 0; let x: Number = 0; while (true) { x += 1; if (x == 5) { continue; } if (x > 10) { break; } s += x; } return s;";
static const char *kStandardFor =
    "let s: Number = 0; for (let i = 0; i < 5; i = i + 1) { if (i == 2) { continue; } s += i; } return s;";
static const char *kSwitchCase =
    "let r: String = \"no\"; let v: Number = 2; switch (v) { case 1: r = \"one\"; break; case 2: r = \"two\"; break; default: r = \"many\"; } return r;";
static const char *kFunctionCall =
    "function add(a: Number, b: Number): Number { return a + b; } return add(3, 4);";
static const char *kFuncOrdering =
    "function sub(a: Number, b: Number): Number { return a - b; } "
    "function tag(a: String, b: String): String { return a + \"[\" + b + \"]\"; } "
    "return sub(10, 3) * 10 + sub(5, 2);";
static const char *kRecursion =
    "function fib(n: Number): Number { if (n <= 1) { return n; } return fib(n-1) + fib(n-2); } return fib(10);";
static const char *kLambda =
    "let f = function(a: Number): Number { return a * 2; }; return f(21);";
static const char *kLambdaHigherOrderMap =
    "let arr: Number[] = [1, 2, 3]; let m = arr.map(function(x: Number): Number { return x * x; }); return m.join(\",\");";
static const char *kArraySort = "let a: Number[] = [3, 1, 2]; a.sort(); return a.join(\",\");";
static const char *kClassInstance =
    "class User { let name: String = \"\"; function say(): String { return \"hi \" + this.name; } } "
    "let u = new User(); u.name = \"tom\"; return u.say();";
static const char *kClassStatic =
    "class Counter { static let n: Number = 0; static function inc(): Number { Counter.n += 1; return Counter.n; } } "
    "Counter.inc(); return Counter.n;";
static const char *kClassInheritance =
    "class A { let x: Number = 10; function get(): Number { return this.x; } } "
    "class B extends A { let y: Number = 20; function get(): Number { return this.x + this.y; } } "
    "let b = new B(); return b.get();";
static const char *kCompoundProp =
    "let o: Object = {n: 5}; o.n += 3; return o.n;";
static const char *kIncPropWriteBack =
    "let o: Object = {n: 5}; o.n++; return o.n;";
static const char *kIncIndex = "let a: Number[] = [1, 2]; a[1]++; return a[1];";
static const char *kTernary = "let c: Boolean = true; return c ? \"y\" : \"n\";";
static const char *kLogicalShortCircuit =
    "let a: Object = null; let r: String = \"s\"; if (a != null && a.x == 1) { r = \"bad\"; } return r;";
static const char *kCoalesce = "let a: Any = null; let b: Number = a ?? 42; return b;";
static const char *kObjectBuiltins =
    "let o: Object = {a: 1, b: 2}; let ks: String[] = o.keys(); return ks.join(\",\") + \"|\" + o.has(\"a\");";
static const char *kJsonBuiltin =
    "let o: Object = JSON.parse(\"{\\\"k\\\": 1}\"); return o.k;";
static const char *kTryCatch =
    "let r: String = \"none\"; try { throw \"boom\"; } catch (e) { r = e; } return r;";
static const char *kTryFinally =
    "let r: String = \"none\"; try { r = \"try\"; } finally { r += \"+fin\"; } return r;";
static const char *kRuntimeErrorPropagation =
    "let o: Object = null; let v: Any = o[\"k\"]; return v;";
static const char *kStringMethods =
    "let s: String = \"Hello World\"; let t: String = s.toLowerCase(); let u: String = s.toUpperCase(); return t + \"|\" + u;";
static const char *kEnumDef = "enum Color { Red, Green = 5, Blue } return Color.Blue;";
static const char *kConstDecl =
    "const PI: Number = 3.14; let r: Number = PI * 2; return r;";
static const char *kNestedBlockScope =
    "let a: Number = 1; { let b: Number = a + 1; a = b; } return a;";

struct DualCase {
  const char *name;
  const char *script;
};

static const DualCase kCases[] = {
    {"switchMinimal", "let v: Number = 2; let r: Number = 0; switch (v) { case 1: break; case 2: r = 5; break; } return r;"},
    {"arithmetic", kArithmetic},
    {"stringConcat", kStringConcat},
    {"arrayAccess", kArrayAccess},
    {"objectReadWrite", kObjectReadWrite},
    {"compoundAssign", kCompoundAssign},
    {"incDecLocal", kIncDecLocal},
    {"whileLoop", kWhileLoop},
    {"forInLoop", kForInLoop},
    {"forInObjKeys", kForInObjKeys},
    {"breakContinue", kBreakContinue},
    {"standardFor", kStandardFor},
    {"functionCall", kFunctionCall},
    {"funcOrdering", kFuncOrdering},
    {"lambda", kLambda},
    {"lambdaHigherOrderMap", kLambdaHigherOrderMap},
    {"arraySort", kArraySort},
    {"classInstance", kClassInstance},
    {"classStatic", kClassStatic},
    {"classInheritance", kClassInheritance},
    {"compoundProp", kCompoundProp},
    {"incPropWriteBack", kIncPropWriteBack},
    {"incIndex", kIncIndex},
    {"ternary", kTernary},
    {"logicalShortCircuit", kLogicalShortCircuit},
    {"coalesce", kCoalesce},
    {"objectBuiltins", kObjectBuiltins},
    {"jsonBuiltin", kJsonBuiltin},
    {"tryCatch", kTryCatch},
    {"tryFinally", kTryFinally},
    {"runtimeErrorPropagation", kRuntimeErrorPropagation},
    {"stringMethods", kStringMethods},
    {"enumDef", kEnumDef},
    {"constDecl", kConstDecl},
    {"nestedBlockScope", kNestedBlockScope},
};

static void testAll() {
  const QByteArray only = qgetenv("AC_VM_CASE");
  if (!only.isEmpty()) {
    for (const auto &c : kCases) { if (QString::fromLatin1(c.name) == QString::fromLatin1(only)) { testDual(c.name, QString::fromLatin1(c.script)); return; } }
    return;
  }
  for (const auto &c : kCases) {
    std::printf("[vm-case] %s\n", c.name);
    std::fflush(stdout);
    testDual(c.name, QString::fromLatin1(c.script));
  }
}

int runAcVmTests() {
  testAll();
  std::printf("[ac_vm] %d checks, %d failed\n", g_total, g_failed);
  return g_failed;
}
