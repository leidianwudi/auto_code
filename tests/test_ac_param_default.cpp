/**
 * @file test_ac_param_default.cpp
 * @brief AC 脚本参数默认值语法（param: Type = 字面量）解析单元测试（纯 QtCore，无 GUI）
 *
 * 覆盖：
 *  - 顶级函数 / 类方法 / 构造函数 / 函数表达式中的 = 默认值解析
 *  - 数字（含负数）、字符串、布尔、null 四种字面量类型
 *  - 与 ?: 可选参数语法的混用（默认值自动视为可选参数）
 *  - 非字面量默认值（变量 / 表达式）被拒绝
 *
 * 构建：cmake --build <build-dir> --target auto_code_tests
 */

#include <QFile>
#include <QSet>
#include <QStringList>
#include <cstdio>

#include "src/engine/script/ac_lexer.h"
#include "src/engine/script/ac_parser.h"

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

/// 解析源码，成功时填充 program
static bool parseSource(const QString &src, Block &program) {
  QString lexErr;
  const QVector<Token> tokens = AcLexer::tokenize(src, lexErr);
  if (tokens.isEmpty() && !lexErr.isEmpty()) return false;
  QSet<QString> declared;
  AcParser parser;
  if (!parser.parse(tokens, program, declared)) return false;
  return true;
}

/// 顶级函数：四种字面量默认值 + 负数 + 与 ?: 混用
static void testTopLevelFuncDefaults() {
  Block program;
  const bool ok = parseSource(
      QStringLiteral("function f(a: Number = 10, b: String = \"hi\", c: Boolean = true, "
                     "d: Number = -2.5, e: Boolean = null, g?: String): Number { return a; }"),
      program);
  CHECK(ok);
  if (!ok) return;

  CHECK(program.stmts.size() == 1);
  CHECK(program.stmts.first().kind == Block::Stmt::kFuncDef);
  const MethodDef &md = program.stmts.first().funcDef;
  CHECK(md.params.size() == 6);

  // a: Number = 10 → 默认值生效且自动可选
  CHECK(md.params[0].isOptional);
  CHECK(md.params[0].defaultValue.isDouble());
  CHECK(md.params[0].defaultValue.toDouble() == 10.0);

  // b: String = "hi"
  CHECK(md.params[1].defaultValue.isString());
  CHECK(md.params[1].defaultValue.toString() == QStringLiteral("hi"));

  // c: Boolean = true
  CHECK(md.params[2].defaultValue.isBool());
  CHECK(md.params[2].defaultValue.toBool() == true);

  // d: Number = -2.5（负数字面量）
  CHECK(md.params[3].defaultValue.toDouble() == -2.5);

  // e: Boolean = null（显式 null，区别于未声明默认值的 Undefined）
  CHECK(md.params[4].isOptional);
  CHECK(md.params[4].defaultValue.isNull());

  // g?: String（?: 语法，无默认值）
  CHECK(md.params[5].isOptional);
  CHECK(md.params[5].defaultValue.isUndefined());
}

/// 类方法 / 构造函数 / 函数表达式中的默认值
static void testClassMethodAndCtorDefaults() {
  Block program;
  const bool ok =
      parseSource(QStringLiteral("class A {"
                                 "  constructor(p: Number = 5, q: String = \"x\") { let v = p; }"
                                 "  function m(a: Number = 1): Number { return a; }"
                                 "  static function s(a?: Number): Number { return a; }"
                                 "}"
                                 "let lam = function(n: Number = 3): Number { return n; };"),
                  program);
  CHECK(ok);
  if (!ok) return;

  // 类：构造函数 + 方法
  bool foundClass = false;
  for (const auto &stmt : program.stmts) {
    if (stmt.kind != Block::Stmt::kClassDef) continue;
    foundClass = true;
    const ClassDef &cd = stmt.classDef;
    CHECK(cd.methods.size() == 3);

    const MethodDef &ctor = cd.methods[0];
    CHECK(ctor.name == QStringLiteral("constructor"));
    CHECK(ctor.params.size() == 2);
    CHECK(ctor.params[0].isOptional);
    CHECK(ctor.params[0].defaultValue.toDouble() == 5.0);
    CHECK(ctor.params[1].defaultValue.toString() == QStringLiteral("x"));

    const MethodDef &m = cd.methods[1];
    CHECK(m.params[0].isOptional);
    CHECK(m.params[0].defaultValue.toDouble() == 1.0);

    // static + ?（无默认值）
    const MethodDef &s = cd.methods[2];
    CHECK(s.isStatic);
    CHECK(s.params[0].isOptional);
    CHECK(s.params[0].defaultValue.isUndefined());
  }
  CHECK(foundClass);

  // 函数表达式（lambda）默认值
  bool foundLambda = false;
  for (const auto &stmt : program.stmts) {
    if (stmt.kind != Block::Stmt::kAssign) continue;
    if (stmt.assign.value.kind != Expr::kFuncExpr) continue;
    foundLambda = true;
    const MethodDef &fe = stmt.assign.value.funcExpr;
    CHECK(fe.params.size() == 1);
    CHECK(fe.params[0].isOptional);
    CHECK(fe.params[0].defaultValue.toDouble() == 3.0);
  }
  CHECK(foundLambda);
}

/// 非字面量默认值被拒绝：变量、缺省、表达式
static void testNonLiteralRejected() {
  Block program;

  // 默认值为变量标识符 → 解析失败
  CHECK(!parseSource(QStringLiteral("function g(a: Number = someVar): Number { return a; }"),
                     program));

  // = 后为空 → 解析失败
  CHECK(!parseSource(QStringLiteral("function h(a: Number = ): Number { return a; }"), program));

  // 默认值为算术表达式 → 解析失败（字面量后出现意外 token）
  CHECK(
      !parseSource(QStringLiteral("function k(a: Number = 1 + 2): Number { return a; }"), program));
}

/// 关键字 from 可用作参数名（builtin.d.ac 中 indexOf/lastIndexOf 的声明依赖此行为）
static void testFromAsParamName() {
  // 声明-only 语法（.d.ac）
  Block program;
  const bool ok =
      parseSource(QStringLiteral("function indexOf(sub: String, from?: Number): Number;"), program);
  CHECK(ok);
  if (ok) {
    CHECK(program.stmts.size() == 1);
    const MethodDef &md = program.stmts.first().funcDef;
    CHECK(md.isDeclaration);
    CHECK(md.params.size() == 2);
    CHECK(md.params[1].name == QStringLiteral("from"));
    CHECK(md.params[1].isOptional);
    CHECK(md.params[1].defaultValue.isUndefined());
  }

  // 带函数体：from 作普通参数名 + 默认值（注意：函数体内引用 from 变量仍不支持，
  // from 仅在参数名位置被视为标识符）
  Block program2;
  const bool ok2 = parseSource(
      QStringLiteral("function find(from: Number = 0): Number { return 0; }"), program2);
  CHECK(ok2);
  if (ok2) {
    const MethodDef &md = program2.stmts.first().funcDef;
    CHECK(md.params[0].name == QStringLiteral("from"));
    CHECK(md.params[0].defaultValue.toDouble() == 0.0);
  }
}

/// 回归守护：真实 .ac 测试套件必须能被解析（覆盖类/接口/模板字符串/默认值等全部语法）
static void testRealScriptsParse() {
#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif
  const QString root = QStringLiteral(PROJECT_SOURCE_DIR);
  const QStringList scripts = {
      root + QStringLiteral("/file/builtin.d.ac"),  // 原生声明库（关键字 from 作参数名）
      root + QStringLiteral("/file/test/test_suite_main.ac"),
      root + QStringLiteral("/file/test/test_engine_main.ac"),
  };
  for (const QString &path : scripts) {
    QFile f(path);
    const bool opened = f.open(QIODevice::ReadOnly | QIODevice::Text);
    CHECK(opened);
    if (!opened) continue;
    Block program;
    const bool ok = parseSource(QString::fromUtf8(f.readAll()), program);
    if (!ok) {
      QString err;
      // 复跑一次拿具体错误（parseSource 不回传错误）
      AcLexer::tokenize(QString::fromUtf8(f.readAll()), err);
      if (err.isEmpty()) {
        QSet<QString> declared;
        AcParser parser;
        parser.parse(AcLexer::tokenize(QString::fromUtf8(f.readAll()), err), program, declared);
        err = parser.error();
      }
      std::printf("  [diag] %s 解析失败: %s\n", path.toUtf8().constData(),
                  err.toUtf8().constData());
      CHECK(ok);
    }
  }
}

/// 表达式深度防护：超深嵌套显式报错而非打爆 C++ 栈（Debug 下也必须安全）；正常深度不受影响
static void testExprDepthGuard() {
  const int deep = 500;  // 远超 64/40 层上限
  QString src = QStringLiteral("let x = ");
  for (int i = 0; i < deep; ++i) src += u'(';
  src += u'1';
  for (int i = 0; i < deep; ++i) src += u')';
  src += u';';

  Block program;
  const bool ok = parseSource(src, program);
  CHECK(!ok);  // 超深嵌套显式失败（此前会打爆 C++ 栈）
  CHECK(program.stmts.isEmpty());

  // 上限内的正常嵌套仍可解析
  QString fine = QStringLiteral("let y = ");
  for (int i = 0; i < 20; ++i) fine += u'(';
  fine += u'1';
  for (int i = 0; i < 20; ++i) fine += u')';
  fine += u';';
  Block fineProgram;
  CHECK(parseSource(fine, fineProgram));

  // 语句块嵌套同样受限：500 层大括号显式失败而非打爆 C++ 栈
  QString deepBlock;
  for (int i = 0; i < 500; ++i) deepBlock += u'{';
  deepBlock += QStringLiteral("let inner: Number = 1;");
  for (int i = 0; i < 500; ++i) deepBlock += u'}';
  Block blockedProgram;
  CHECK(!parseSource(deepBlock, blockedProgram));

  // 上限内的正常块嵌套仍可解析
  QString fineBlock;
  for (int i = 0; i < 20; ++i) fineBlock += u'{';
  fineBlock += QStringLiteral("let ok: Number = 1;");
  for (int i = 0; i < 20; ++i) fineBlock += u'}';
  Block fineBlockedProgram;
  CHECK(parseSource(fineBlock, fineBlockedProgram));
}

/// 运行全部用例，返回失败数（0 = 全部通过）；由 test_json_utils.cpp 的 main 调用
int runAcParamDefaultTests() {
  testTopLevelFuncDefaults();
  testClassMethodAndCtorDefaults();
  testNonLiteralRejected();
  testFromAsParamName();
  testRealScriptsParse();
  testExprDepthGuard();
  std::printf("[ac_param_default] %d checks, %d failed\n", g_total, g_failed);
  return g_failed;
}