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

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

#include "src/engine/function/fun_mgr.h"
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
  result = exec.execute().toQJsonValue();
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
  CHECK(
      runScript(QStringLiteral("class Counter { let n: Number = 0; "
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
  QJsonValue r = exec.execute().toQJsonValue();
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

// ── 新语言特性：== 语义 / sort / map/filter / ?. / ?? / const / 对象方法 / do-while / try-catch ──

/// 对象相等：值语义结构化比较；实例按引用（objId）比较
static void testObjectEqualitySemantics() {
  QJsonValue r;
  QString err;
  CHECK(runScript(QStringLiteral("let a = {x: 1, y: \"s\"}; let b = {x: 1, y: \"s\"};") +
                      QStringLiteral("return a == b;"),
                  r));
  CHECK(r.toBool() == true);  // 结构一致即相等（值语义）
  CHECK(runScript(QStringLiteral("let a = {x: 1}; let b = {x: 2}; return a == b;"), r));
  CHECK(r.toBool() == false);
  CHECK(runScript(QStringLiteral("let a = [1, 2]; return a == [1, 2] && a != [2, 1];"), r));
  CHECK(r.toBool() == true);
  // 实例：引用相等 —— 同一实例相等，不同实例即便属性一致也不相等
  CHECK(runScript(QStringLiteral("class P { let v = 1; }") +
                      QStringLiteral("let a = new P(); let b = new P();") +
                      QStringLiteral("return a == b || a != b;"),
                  r, &err));
  CHECK(r.toBool() == true);  // a==b 为 false，a!=b 为 true → true
}

/// 数组 sort：默认排序 + 比较函数
static void testArraySort() {
  QJsonValue r;
  CHECK(runScript(QStringLiteral("let a = [3, 1, 2]; a.sort(); return a[0] + \",\" + a[2];"), r));
  CHECK(r.toString() == QStringLiteral("1,3"));
  CHECK(runScript(QStringLiteral("let a = [\"b\", \"a\", \"c\"]; a.sort(); return a.join(\"\");"),
                  r));
  CHECK(r.toString() == QStringLiteral("abc"));
  CHECK(runScript(
      QStringLiteral("let a = [10, 1, 5];") +
          QStringLiteral("a.sort(function(x: Number, y: Number): Number { return y - x; });") +
          QStringLiteral("return a[0];"),
      r));
  CHECK(r.toDouble() == 10.0);  // 比较函数降序
}

/// map/filter/forEach 高阶方法（回调为 function 表达式）
static void testArrayHigherOrder() {
  QJsonValue r;
  CHECK(runScript(
      QStringLiteral("let a = [1, 2, 3];") +
          QStringLiteral("let b = a.map(function(e: Number): Number { return e * 2; });") +
          QStringLiteral("return b.join(\",\");"),
      r));
  CHECK(r.toString() == QStringLiteral("2,4,6"));
  CHECK(runScript(
      QStringLiteral("let a = [1, 2, 3, 4];") +
          QStringLiteral("let b = a.filter(function(e: Number): Bool { return e > 2; });") +
          QStringLiteral("return b.length;"),
      r));
  CHECK(r.toDouble() == 2.0);
  CHECK(runScript(
      QStringLiteral("let s = 0; let a = [1, 2, 3];") +
          QStringLiteral("a.forEach(function(e: Number): Void { s = s + e; }); return s;"),
      r));
  CHECK(r.toDouble() == 6.0);
}

/// ?. 可选链：null 时短路为 null，非 null 正常访问；?? 空值合并
static void testOptionalChainAndCoalesce() {
  QJsonValue r;
  QString err;
  CHECK(runScript(QStringLiteral("let o = null; return o?.x;"), r, &err));
  CHECK(r.isNull());
  CHECK(err.isEmpty());  // 短路不报错
  CHECK(runScript(QStringLiteral("let o = {x: {y: 7}}; return o?.x.y;"), r));
  CHECK(r.toDouble() == 7.0);
  CHECK(runScript(QStringLiteral("let o = null; return o?.x?.y ?? 42;"), r));
  CHECK(r.toDouble() == 42.0);  // 链式短路 + ?? 兜底
  CHECK(runScript(QStringLiteral("let o = {a: 1}; return o?.size();"), r));
  CHECK(r.toDouble() == 1.0);  // 非空对象：?. 方法调用走内置方法
  CHECK(runScript(QStringLiteral("let o = null; return o?.size() ?? 5;"), r));
  CHECK(r.toDouble() == 5.0);  // null 短路 + ?? 兜底
  // ?? 与 || 的区别：false/0 不触发 ??
  CHECK(runScript(QStringLiteral("return (false ?? \"dflt\");"), r));
  CHECK(r.toBool() == false);
  CHECK(runScript(QStringLiteral("return (null ?? 0) == 0;"), r));
  CHECK(r.toBool() == true);
}

/// const 常量：声明后赋值报错
static void testConstDeclaration() {
  QJsonValue r;
  QString err;
  CHECK(runScript(QStringLiteral("const N = 10; return N + 1;"), r));
  CHECK(r.toDouble() == 11.0);
  err.clear();
  CHECK(!runScript(QStringLiteral("const N = 10; N = 20; return N;"), r, &err));
  CHECK(err.contains(QStringLiteral("const")));
}

/// 对象内置方法 keys/values/has/size
static void testObjectBuiltinMethods() {
  QJsonValue r;
  CHECK(runScript(QStringLiteral("let o = {a: 1, b: 2}; return o.keys().join(\",\");"), r));
  CHECK(r.toString() == QStringLiteral("a,b"));
  CHECK(runScript(QStringLiteral("let o = {a: 1, b: 2}; return o.values().join(\",\");"), r));
  CHECK(r.toString() == QStringLiteral("1,2"));
  CHECK(runScript(QStringLiteral("let o = {a: 1}; return (o.has(\"a\") ? 1 : 0) + o.size();"), r));
  CHECK(r.toDouble() == 2.0);
}

/// do…while：至少执行一次；条件在循环体后判断
static void testDoWhile() {
  QJsonValue r;
  CHECK(runScript(QStringLiteral("let i = 10; let n = 0;") +
                      QStringLiteral("do { n = n + 1; i = i + 1; } while (i < 5);") +
                      QStringLiteral("return n;"),
                  r));
  CHECK(r.toDouble() == 1.0);  // 条件先假也执行一次
  CHECK(runScript(QStringLiteral("let n = 0; let i = 0;") +
                      QStringLiteral("do { n = n + i; i = i + 1; } while (i < 4);") +
                      QStringLiteral("return n;"),
                  r));
  CHECK(r.toDouble() == 6.0);  // 0+1+2+3
}

/// try/catch/finally/throw
static void testTryCatchThrow() {
  QJsonValue r;
  QString err;
  CHECK(runScript(QStringLiteral("let msg = \"\";") +
                      QStringLiteral("try { throw \"boom\"; } catch (e) { msg = e; }") +
                      QStringLiteral("return msg;"),
                  r, &err));
  CHECK(r.toString() == QStringLiteral("boom"));
  CHECK(
      runScript(QStringLiteral("let out = \"a\";") +
                    QStringLiteral("try { out = out + \"b\"; } catch (e) { out = out + \"X\"; }") +
                    QStringLiteral("return out;"),
                r));
  CHECK(r.toString() == QStringLiteral("ab"));  // 无错误不进 catch
  CHECK(runScript(QStringLiteral("let order = \"\";") +
                      QStringLiteral("try { throw \"e1\"; } catch (e) { order = order + \"c\"; }") +
                      QStringLiteral("finally { order = order + \"f\"; }") +
                      QStringLiteral("return order;"),
                  r));
  CHECK(r.toString() == QStringLiteral("cf"));  // finally 在 catch 后执行
  CHECK(runScript(QStringLiteral("let x = 0;") +
                      QStringLiteral("try { x = 1; } finally { x = x + 10; }") +
                      QStringLiteral("return x;"),
                  r));
  CHECK(r.toDouble() == 11.0);  // 只有 finally
  // catch 后不再向上传播
  err.clear();
  CHECK(runScript(QStringLiteral("try { throw \"x\"; } catch (e) { } return \"done\";"), r, &err));
  CHECK(r.toString() == QStringLiteral("done"));
  CHECK(err.isEmpty());
}

/// 对象形状 interface：类 implements 时必须提供属性且类型兼容（可选属性可缺失）
static void testInterfacePropertyContract() {
  QJsonValue r;
  QString err;
  CHECK(runScript(
      QStringLiteral("interface Pt { let x: Number; let y: Number; }") +
          QStringLiteral("class P implements Pt { let x: Number = 1; let y: Number = 2; }") +
          QStringLiteral("let p: Pt = new P(); return p.x + p.y;"),
      r, &err));
  CHECK(r.toDouble() == 3.0);
  err.clear();
  CHECK(!runScript(QStringLiteral("interface Pt { let x: Number; let y: Number; }") +
                       QStringLiteral("class P implements Pt { let x: Number = 1; }") +
                       QStringLiteral("return 1;"),
                   r, &err));
  CHECK(err.contains(QStringLiteral("property")));
  err.clear();
  CHECK(!runScript(QStringLiteral("interface Pt { let x: Number; }") +
                       QStringLiteral("class P implements Pt { let x: String = \"s\"; }") +
                       QStringLiteral("return 1;"),
                   r, &err));
  CHECK(err.contains(QStringLiteral("incompatible")));
  err.clear();
  // 可选属性缺失合法
  CHECK(runScript(QStringLiteral("interface Cfg { let name: String; let tag?: String; }") +
                      QStringLiteral("class C implements Cfg { let name: String = \"n\"; }") +
                      QStringLiteral("return 1;"),
                  r, &err));
  CHECK(err.isEmpty());
}

/// FunMgr 线程安全：注册表并发读 + thread_local 错误通道隔离（多线程并行 worker 的前置保障）
static void testFunMgrThreadSafety() {
  // 注册所有内置函数（幂等：同名重复注册仅覆盖）；FunDb::init 内部
  // mysql_library_init 显式化，消除并行 worker 首次 new DB() 的隐式初始化竞态
  FunMgr::init();

  // ── 1) 注册表并发调用：4 线程 × 300 次 str.toUpperCase/contains，共享读不崩、结果正确 ──
  constexpr int kThreads = 4;
  constexpr int kIters = 300;
  std::atomic<int> bad{0};
  std::vector<std::thread> workers;
  for (int t = 0; t < kThreads; ++t) {
    workers.emplace_back([&bad, t]() {
      for (int i = 0; i < kIters; ++i) {
        if (!FunMgr::ins().contains(QStringLiteral("str"), QStringLiteral("toUpperCase"))) {
          ++bad;
          continue;
        }
        accore::AcJsonValue args = accore::AcJsonValue::makeArray();
        args.append(accore::AcJsonValue(QStringLiteral("abc") + QString::number(t)));
        const accore::AcJsonValue r =
            FunMgr::ins().call(QStringLiteral("str"), QStringLiteral("toUpperCase"), args);
        if (r.toString() != QStringLiteral("ABC") + QString::number(t)) ++bad;
      }
    });
  }
  for (auto &w : workers) w.join();
  CHECK(bad.load() == 0);

  // ── 2) 错误通道隔离：worker 先 setError，主线程再 takeError ──
  // 时序由两个原子标志确定性编排：主线程的 takeError 必然发生在 worker 的 setError 之后。
  // 旧实现（全局静态 s_lastError）：主线程拿到 "worker-error"（串扰）→ 此用例失败；
  // thread_local 实现：主线程拿到的仍是自己的 "main-error"
  FunMgr::setError(QStringLiteral("main-error"));
  std::atomic<bool> workerSet{false};
  std::atomic<bool> mainDone{false};
  QString workerTaken;
  std::thread worker([&]() {
    FunMgr::setError(QStringLiteral("worker-error"));
    workerSet.store(true);
    while (!mainDone.load()) std::this_thread::yield();  // 等主线程完成 takeError
    workerTaken = FunMgr::takeError();
  });
  while (!workerSet.load()) std::this_thread::yield();
  const QString mainTaken = FunMgr::takeError();
  mainDone.store(true);
  worker.join();
  CHECK(mainTaken == QStringLiteral("main-error"));
  CHECK(workerTaken == QStringLiteral("worker-error"));
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
  testObjectEqualitySemantics();
  testArraySort();
  testArrayHigherOrder();
  testOptionalChainAndCoalesce();
  testConstDeclaration();
  testObjectBuiltinMethods();
  testDoWhile();
  testTryCatchThrow();
  testInterfacePropertyContract();
  testFunMgrThreadSafety();
  std::printf("[ac_interpreter] %d checks, %d failed\n", g_total, g_failed);
  return g_failed;
}
