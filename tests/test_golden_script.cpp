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

int runGoldenScriptTests() {
  testGoldenScriptOrder();
  testGoldenScriptErrors();
  return g_failed;
}
