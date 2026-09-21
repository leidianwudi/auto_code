/**
 * @file test_golden_script.cpp
 * @brief 端到端 golden 测试 — 通过真实可执行程序（--run headless 模式）执行 AC 脚本
 *
 * 锁定 M2 迁移的核心行为（对象键插入序）：
 *  1. 对象字面量 {zebra, apple, mango} 经 for-in 迭代按声明序产出（非字母序）
 *  2. JSON.parse('{"y":1,"a":2}') 经 for-in 迭代按原文键序产出
 *
 * 通过 QProcess 以子进程方式调用 auto_code.exe --run（GUI 程序带管道句柄，
 * stdout 可被子进程捕获 —— 与 ac_headless_runner.h 设计的调用方式一致）。
 */

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QProcess>
#include <QStringList>
#include <QTemporaryDir>
#include <cstdio>

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

/// 端到端 golden：对象字面量与 JSON.parse 的 for-in 键序（插入序）
static void testGoldenScriptOrder() {
  // QProcess 需要应用实例（局部创建，函数结束销毁）
  int argc = 1;
  char arg0[] = "auto_code_tests";
  char *argv[] = {arg0};
  QCoreApplication app(argc, argv);

  const QString exe = QCoreApplication::applicationDirPath() + QStringLiteral("/auto_code.exe");
  if (!QFileInfo::exists(exe)) {
    std::printf("SKIP golden: main exe not found at %s\n", qPrintable(exe));
    return;
  }

  QTemporaryDir tmp;
  CHECK(tmp.isValid());

  // golden 脚本：对象字面量（乱序键）+ JSON.parse（乱序键），经函数内 for-in 拼接键序后打印
  // （顶层 for 语句解析器不支持，按语言现状包在函数内）
  const QString script = QStringLiteral(
      "function joinKeys(o: Object): String {\n"
      "  let s = \"\";\n"
      "  for (let k: String in o) {\n"
      "    s = s + k + \",\";\n"
      "  }\n"
      "  return s;\n"
      "}\n"
      "printLog(joinKeys({zebra: 1, apple: 2, mango: 3}));\n"
      "printLog(joinKeys(JSON.parse(\"{\\\"y\\\": 1, \\\"a\\\": 2}\")));\n");
  const QString scriptPath = tmp.path() + QStringLiteral("/golden.ac");
  {
    QFile f(scriptPath);
    CHECK(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(script.toUtf8());
    f.close();
  }

  QProcess proc;
  proc.start(exe, QStringList(
                      {QStringLiteral("--run"), scriptPath, QStringLiteral("--root"), tmp.path()}));
  CHECK(proc.waitForStarted(10000));
  CHECK(proc.waitForFinished(120000));
  const QString out = QString::fromUtf8(proc.readAllStandardOutput());

  // golden 断言：键按插入序（迁移前 QJsonObject 为字母序：apple,mango,zebra / a,y）
  CHECK(out.contains(QStringLiteral("zebra,apple,mango,")));
  CHECK(out.contains(QStringLiteral("y,a,")));
  CHECK(proc.exitStatus() == QProcess::NormalExit);
  CHECK(proc.exitCode() == 0);
}

/// 端到端负向用例：运行时非法操作必须以非零退出码报错中断（错误传播语义护栏）
static void testGoldenScriptErrors() {
  int argc = 1;
  char arg0[] = "auto_code_tests";
  char *argv[] = {arg0};
  QCoreApplication app(argc, argv);

  const QString exe = QCoreApplication::applicationDirPath() + QStringLiteral("/auto_code.exe");
  if (!QFileInfo::exists(exe)) {
    std::printf("SKIP golden errors: main exe not found\n");
    return;
  }

  struct ErrorCase {
    QString script;
    QString expectError;
  };
  const QList<ErrorCase> cases = {
      // 索引赋值目标为 null：对齐 JS TypeError，必须报错中断
      {QStringLiteral("let o: Object = null;\n"
                      "o[\"k\"] = 1;\n"
                      "printLog(\"unreachable\");\n"),
       QStringLiteral("cannot index-assign on value")},
      // 属性赋值目标为 null：必须报错中断
      {QStringLiteral("let o: Object = null;\n"
                      "o.p = 1;\n"
                      "printLog(\"unreachable\");\n"),
       QStringLiteral("cannot set property 'p' on value")},
      // 索引读取目标为 null：必须报错中断
      {QStringLiteral("let o: Object = null;\n"
                      "let v: Any = o[\"k\"];\n"
                      "printLog(\"unreachable\");\n"),
       QStringLiteral("cannot access index on value")},
      // 对 null 调用方法：必须报错中断
      {QStringLiteral("let s: String = null;\n"
                      "s.trim();\n"
                      "printLog(\"unreachable\");\n"),
       QStringLiteral("cannot call method")},
  };

  QTemporaryDir tmp;
  CHECK(tmp.isValid());
  for (int i = 0; i < cases.size(); ++i) {
    const QString scriptPath = tmp.path() + QStringLiteral("/err_%1.ac").arg(i);
    {
      QFile f(scriptPath);
      CHECK(f.open(QIODevice::WriteOnly | QIODevice::Text));
      f.write(cases[i].script.toUtf8());
      f.close();
    }
    QProcess proc;
    proc.start(exe, QStringList({QStringLiteral("--run"), scriptPath, QStringLiteral("--root"),
                                 tmp.path()}));
    CHECK(proc.waitForStarted(10000));
    CHECK(proc.waitForFinished(120000));
    const QString out = QString::fromUtf8(proc.readAllStandardOutput());
    CHECK(proc.exitStatus() == QProcess::NormalExit);
    CHECK(proc.exitCode() != 0);                          // 非法操作必须导致脚本失败
    CHECK(out.contains(cases[i].expectError));            // 错误消息可定位
    CHECK(!out.contains(QStringLiteral("unreachable")));  // 报错后必须立即中断
  }
}

/// 端到端对拍：同一脚本分别以解释器与字节码 VM（--vm）执行，stdout 与退出码必须一致。
/// 进程内对拍见 test_ac_vm.cpp；本用例覆盖 headless 真实入口（FunMgr 集成、
/// 日志输出、退出码传播）下的双模式一致性，是 VM 切默认前的持续验证护栏。
static void testGoldenScriptDualMode() {
  int argc = 1;
  char arg0[] = "auto_code_tests";
  char *argv[] = {arg0};
  QCoreApplication app(argc, argv);

  const QString exe = QCoreApplication::applicationDirPath() + QStringLiteral("/auto_code.exe");
  if (!QFileInfo::exists(exe)) {
    std::printf("SKIP golden dual-mode: main exe not found\n");
    return;
  }

  // 综合脚本：类实例/递归/对象键序/数组方法/try-catch/字符串方法/空值合并
  //（语法取自 test_ac_vm.cpp 已验证语料，printLog 输出到 stdout 供对拍比较）
  const QString okScript = QStringLiteral(
      "class User { let name: String = \"\"; function say(): String { return \"hi \" + this.name; } }\n"
      "function fib(n: Number): Number { if (n <= 1) { return n; } return fib(n-1) + fib(n-2); }\n"
      "function joinKeys(o: Object): String {\n"
      "  let s = \"\";\n"
      "  for (let k: String in o) { s = s + k + \",\"; }\n"
      "  return s;\n"
      "}\n"
      "let u = new User(); u.name = \"tom\"; printLog(u.say());\n"
      "printLog(\"\" + fib(10));\n"  // printLog 签名要求 String，Number 需拼接转换
      "printLog(joinKeys({zebra: 1, apple: 2, mango: 3}));\n"
      "let arr: Number[] = [3, 1, 2]; arr.sort(); printLog(arr.join(\",\"));\n"
      "let r: String = \"none\"; try { throw \"boom\"; } catch (e) { r = e; } printLog(r);\n"
      "let s: String = \"Hello World\"; printLog(s.toLowerCase() + \"|\" + s.toUpperCase());\n"
      "let a: Any = null; let b: Number = a ?? 42; printLog(\"\" + b);\n");
  // 错误脚本：运行时非法操作（两模式错误消息与退出码必须一致，错误传播语义护栏）
  const QString errScript = QStringLiteral(
      "let o: Object = null;\n"
      "o[\"k\"] = 1;\n"
      "printLog(\"unreachable\");\n");

  QTemporaryDir tmp;
  CHECK(tmp.isValid());
  const QString okPath = tmp.path() + QStringLiteral("/dual_ok.ac");
  const QString errPath = tmp.path() + QStringLiteral("/dual_err.ac");
  {
    QFile f(okPath);
    CHECK(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(okScript.toUtf8());
    f.close();
  }
  {
    QFile f(errPath);
    CHECK(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(errScript.toUtf8());
    f.close();
  }

  // 子进程执行 helper：返回 stdout 与退出码
  auto runOnce = [&](const QString &scriptPath, bool useVm, int *exitCode) {
    QStringList args{QStringLiteral("--run"), scriptPath, QStringLiteral("--root"), tmp.path()};
    if (useVm) args.append(QStringLiteral("--vm"));
    QProcess proc;
    proc.start(exe, args);
    proc.waitForStarted(10000);
    proc.waitForFinished(120000);
    *exitCode = proc.exitCode();
    CHECK(proc.exitStatus() == QProcess::NormalExit);
    return QString::fromUtf8(proc.readAllStandardOutput());
  };

  // 成功脚本：两模式输出逐字节一致，退出码均为 0
  {
    int codeInt = -1, codeVm = -1;
    const QString outInt = runOnce(okPath, false, &codeInt);
    const QString outVm = runOnce(okPath, true, &codeVm);
    CHECK(codeInt == 0);
    CHECK(codeVm == 0);
    CHECK(outInt == outVm);
    // 关键输出抽查（防止两模式同时"安静失败"的假一致）
    CHECK(outInt.contains(QStringLiteral("hi tom")));
    CHECK(outInt.contains(QStringLiteral("zebra,apple,mango,")));
    CHECK(outInt.contains(QStringLiteral("boom")));
  }

  // 错误脚本：两模式错误输出与退出码一致，均为非 0 且中断执行
  {
    int codeInt = -1, codeVm = -1;
    const QString outInt = runOnce(errPath, false, &codeInt);
    const QString outVm = runOnce(errPath, true, &codeVm);
    CHECK(codeInt != 0);
    CHECK(codeVm != 0);
    CHECK(codeInt == codeVm);
    CHECK(outInt == outVm);
    CHECK(outInt.contains(QStringLiteral("cannot index-assign on value")));
    CHECK(!outInt.contains(QStringLiteral("unreachable")));
  }
}

int runGoldenScriptTests() {
  testGoldenScriptOrder();
  testGoldenScriptErrors();
  testGoldenScriptDualMode();
  return g_failed;
}
