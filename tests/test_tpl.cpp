/**
 * @file test_tpl.cpp
 * @brief 模板引擎测试 — 语法核心能力与 FunMgr 集成的行为锁定
 *
 * 覆盖（语法模式取自 file/test/engine_sample.tpl 生产模板）：
 * - 变量替换 / 嵌套属性路径
 * - each 循环与元变量（item_index / item.name）
 * - if / else 条件、等值比较（== / !=）
 * - 算术表达式（优先级）
 * - str 域函数调用（含引号内逗号参数）
 * - builtin 域函数调用（fileExists）
 * - 注释块（${# ...}）剔除
 * - 渲染成功时 lastError 为空
 *
 * 构建：cmake --build <build-dir> --target auto_code_tests --config Debug
 */

#include <cstdio>

#include "src/engine/function/fun_mgr.h"
#include "src/engine/tpl/tpl_engine.h"

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

/// 渲染并返回结果（数据用 JSON5 文本构造，兼带顺序敏感性）
static QString renderTpl(const QString &tmpl, const char *dataJson5) {
  bool ok = false;
  const accore::AcJsonValue data = accore::AcJsonValue::parse(
      QString::fromUtf8(dataJson5), &ok);
  CHECK(ok);
  TplEngine engine;
  const QString out = engine.render(tmpl, data);
  if (!engine.lastError().isEmpty()) {
    std::printf("  [tpl] render error: %s\n", engine.lastError().toUtf8().constData());
  }
  CHECK(engine.lastError().isEmpty());
  return out;
}

/// ── 变量替换 / 嵌套路径 ──
static void testVariableSubstitution() {
  const QString out = renderTpl(
      QStringLiteral("name=${name}\nurl=${meta.dataUrl}\ndeep=${meta.nested.key}\n"),
      "{name: 'AutoCode', meta: {dataUrl: '/api/v1', nested: {key: 'v9'}}}");
  CHECK(out.contains(QStringLiteral("name=AutoCode")));
  CHECK(out.contains(QStringLiteral("url=/api/v1")));
  CHECK(out.contains(QStringLiteral("deep=v9")));
}

/// ── each 循环与元变量 ──
static void testEachLoop() {
  const QString out = renderTpl(
      QStringLiteral("${each item in items}${item_index}:${item.name};${/each}"),
      "{items: [{name: 'apple'}, {name: 'banana'}]}");
  CHECK(out.contains(QStringLiteral("0:apple;1:banana;")));
}

/// ── if / else 与等值比较 ──
static void testConditionals() {
  const QString tmpl = QStringLiteral(
      "${if flag}ON${else}OFF${/if}|${if empty}A${else}B${/if}"
      "|${if name == \"AutoCode\"}EQ${/if}|${if name != \"other\"}NE${/if}");
  const QString out = renderTpl(tmpl, "{flag: true, empty: false, name: 'AutoCode'}");
  CHECK(out.contains(QStringLiteral("ON|B")));
  CHECK(out.contains(QStringLiteral("|EQ|NE")));
}

/// ── 算术表达式（优先级） ──
static void testArithmetic() {
  const QString out = renderTpl(QStringLiteral("sum=${a + b}|prod=${a * b + 2}"),
                                "{a: 3, b: 4}");
  CHECK(out.contains(QStringLiteral("sum=7")));
  CHECK(out.contains(QStringLiteral("prod=14")));
}

/// ── str 域函数调用（FunMgr 集成；含引号内逗号参数） ──
static void testStrFunctionCalls() {
  const QString out = renderTpl(
      QStringLiteral("upper=${str.toUpperCase(name)}|repl=${str.replace(s, \"a,b\", \"X\")}"),
      "{name: 'ac', s: '1a,b2'}");
  CHECK(out.contains(QStringLiteral("upper=AC")));
  CHECK(out.contains(QStringLiteral("repl=1X2")));
}

/// ── builtin 域函数调用（fileExists 对不存在路径返回 false） ──
static void testBuiltinFunctionCall() {
  const QString out =
      renderTpl(QStringLiteral("notexist=${fileExists(\"___no_such_file___.xyz\")}"), "{}");
  CHECK(out.contains(QStringLiteral("notexist=false")));
}

/// ── 注释块剔除（${# ...} 不进入输出） ──
static void testCommentBlock() {
  const QString out = renderTpl(
      QStringLiteral("${# 这是注释 should not appear}\nvalue=${v}"),
      "{v: 42}");
  CHECK(!out.contains(QStringLiteral("should not appear")));
  CHECK(out.contains(QStringLiteral("value=42")));
}

/// ── 生产样例模板（file/test/engine_sample.tpl 同款）行为锁定 ──
static void testEngineSampleShape() {
  const QString tmpl = QStringLiteral(
      "name=${name}\n"
      "count=${count}\n"
      "${each item in items}${item_index}:${item.name};${/each}\n"
      "${if flag}FLAG_ON${else}FLAG_OFF${/if}\n"
      "${if name == \"AutoCode\"}NAME_EQ${/if}\n"
      "sum=${a + b}\n"
      "upper=${str.toUpperCase(name)}\n");
  const QString out = renderTpl(
      tmpl,
      "{name: 'AutoCode', count: 3, items: [{name: 'x'}, {name: 'y'}], flag: true, a: 2, b: 5}");
  CHECK(out.contains(QStringLiteral("name=AutoCode")));
  CHECK(out.contains(QStringLiteral("count=3")));
  CHECK(out.contains(QStringLiteral("0:x;1:y;")));
  CHECK(out.contains(QStringLiteral("FLAG_ON")));
  CHECK(out.contains(QStringLiteral("NAME_EQ")));
  CHECK(out.contains(QStringLiteral("sum=7")));
  CHECK(out.contains(QStringLiteral("upper=AUTOCODE")));
}

int runTplTests() {
  // str. / builtin. 域函数调用需要注册表（幂等：重复 init 安全）
  FunMgr::init();

  testVariableSubstitution();
  testEachLoop();
  testConditionals();
  testArithmetic();
  testStrFunctionCalls();
  testBuiltinFunctionCall();
  testCommentBlock();
  testEngineSampleShape();

  std::printf("[tpl] %d checks, %d failed\n", g_total, g_failed);
  return g_failed;
}
