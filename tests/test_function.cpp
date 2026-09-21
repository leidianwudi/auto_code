/**
 * @file test_function.cpp
 * @brief 内置函数系统测试 — FunMgr 注册/调用/参数校验/错误通道的行为锁定
 *
 * 覆盖：
 * - str 域 6 个函数的正常路径与参数错误路径
 * - builtin 域纯函数（basename/fileName/fileExists/merge/formatPath）
 * - 参数校验失败：返回 Null + thread_local 错误通道（takeError）
 * - 未注册类/函数：返回 Null
 *
 * 说明：file/db 域涉及磁盘与网络副作用，留给端到端 golden 测试覆盖。
 */

#include <cstdio>

#include "src/engine/function/fun_mgr.h"

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

/// 构造参数数组（从字符串/数字列表）
static accore::AcJsonValue makeArgs(std::initializer_list<accore::AcJsonValue> items) {
  accore::AcJsonValue args = accore::AcJsonValue::makeArray();
  for (const auto &v : items) args.append(v);
  return args;
}

/// 调用并断言返回字符串等于期望
static void checkStr(const QString &cls, const QString &fn,
                     std::initializer_list<accore::AcJsonValue> items, const QString &expect) {
  const accore::AcJsonValue r = FunMgr::ins().call(cls, fn, makeArgs(items));
  CHECK(r.isString());
  CHECK(r.toString() == expect);
}

/// 调用并断言参数校验失败：返回 Null 且 takeError 携带可定位消息
static void checkArgError(const QString &cls, const QString &fn,
                          std::initializer_list<accore::AcJsonValue> items,
                          const QString &expectMsgPart) {
  (void)FunMgr::takeError();  // 清空旧错误，防残留造成假阳/假阴
  const accore::AcJsonValue r = FunMgr::ins().call(cls, fn, makeArgs(items));
  CHECK(r.isNull());
  const QString err = FunMgr::takeError();
  CHECK(!err.isEmpty());
  CHECK(err.contains(expectMsgPart));
}

// ── str 域：正常路径 ──
static void testStrFunctions() {
  checkStr(QStringLiteral("str"), QStringLiteral("toLowerCase"),
           {QStringLiteral("HeLLo")}, QStringLiteral("hello"));
  checkStr(QStringLiteral("str"), QStringLiteral("toUpperCase"),
           {QStringLiteral("HeLLo")}, QStringLiteral("HELLO"));
  checkStr(QStringLiteral("str"), QStringLiteral("trim"),
           {QStringLiteral("  x  ")}, QStringLiteral("x"));
  checkStr(QStringLiteral("str"), QStringLiteral("capitalize"),
           {QStringLiteral("hello world")}, QStringLiteral("Hello world"));
  checkStr(QStringLiteral("str"), QStringLiteral("capitalize"),
           {QStringLiteral("")}, QStringLiteral(""));
  checkStr(QStringLiteral("str"), QStringLiteral("substring"),
           {QStringLiteral("hello"), 1}, QStringLiteral("ello"));
  checkStr(QStringLiteral("str"), QStringLiteral("substring"),
           {QStringLiteral("hello"), 1, 3}, QStringLiteral("ell"));
  checkStr(QStringLiteral("str"), QStringLiteral("replace"),
           {QStringLiteral("a,b"), QStringLiteral(","), QStringLiteral("-")},
           QStringLiteral("a-b"));
}

// ── str 域：参数校验错误路径 ──
static void testStrArgErrors() {
  checkArgError(QStringLiteral("str"), QStringLiteral("toLowerCase"), {},
                QStringLiteral("toLowerCase"));
  checkArgError(QStringLiteral("str"), QStringLiteral("substring"),
                {QStringLiteral("hello")},
                QStringLiteral("substring"));
  checkArgError(QStringLiteral("str"), QStringLiteral("replace"),
                {QStringLiteral("a"), QStringLiteral("b")},
                QStringLiteral("replace"));
}

// ── builtin 域：路径类纯函数 ──
static void testBuiltinPathFunctions() {
  checkStr(QStringLiteral("builtin"), QStringLiteral("basename"),
           {QStringLiteral("/a/b/c.txt")}, QStringLiteral("c"));
  checkStr(QStringLiteral("builtin"), QStringLiteral("fileName"),
           {QStringLiteral("/a/b/c.txt")}, QStringLiteral("c.txt"));

  // fileExists：不存在的路径 false；当前目录 true
  const accore::AcJsonValue no = FunMgr::ins().call(
      QStringLiteral("builtin"), QStringLiteral("fileExists"),
      makeArgs({QStringLiteral("___no_such_file___.xyz")}));
  CHECK(no.isBool());
  CHECK(no.toBool() == false);
  const accore::AcJsonValue yes = FunMgr::ins().call(
      QStringLiteral("builtin"), QStringLiteral("fileExists"),
      makeArgs({QStringLiteral(".")}));
  CHECK(yes.toBool() == true);
}

// ── builtin 域：merge（键序保真 + 原位置更新） ──
static void testBuiltinMerge() {
  accore::AcJsonValue a = accore::AcJsonValue::makeObject();
  a.set(QStringLiteral("a"), accore::AcJsonValue(1));
  a.set(QStringLiteral("b"), accore::AcJsonValue(2));
  accore::AcJsonValue b = accore::AcJsonValue::makeObject();
  b.set(QStringLiteral("b"), accore::AcJsonValue(9));
  b.set(QStringLiteral("c"), accore::AcJsonValue(3));

  const accore::AcJsonValue merged = FunMgr::ins().call(
      QStringLiteral("builtin"), QStringLiteral("merge"), makeArgs({a, b}));
  CHECK(merged.isObject());
  CHECK(merged.size() == 3);
  // 键插入序：a, b, c（b 原位置更新而非重插）
  const QStringList expectedKeys{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
  CHECK(merged.keys() == expectedKeys);
  CHECK(merged.value(QStringLiteral("b")).toDouble() == 9.0);
}

// ── builtin 域：formatPath（{key} 占位符替换） ──
static void testBuiltinFormatPath() {
  accore::AcJsonValue data = accore::AcJsonValue::makeObject();
  data.set(QStringLiteral("base"), accore::AcJsonValue(QStringLiteral("src")));
  data.set(QStringLiteral("name"), accore::AcJsonValue(QStringLiteral("user")));
  checkStr(QStringLiteral("builtin"), QStringLiteral("formatPath"),
           {QStringLiteral("{base}/{name}.entity.ts"), data},
           QStringLiteral("src/user.entity.ts"));
}

// ── 未注册的类/函数：返回 Null（不崩溃、不污染错误通道） ──
static void testUnregistered() {
  const accore::AcJsonValue r1 = FunMgr::ins().call(
      QStringLiteral("__no_such_class__"), QStringLiteral("fn"), makeArgs({}));
  CHECK(r1.isNull());

  const accore::AcJsonValue r2 = FunMgr::ins().call(
      QStringLiteral("str"), QStringLiteral("__no_such_fn__"), makeArgs({}));
  CHECK(r2.isNull());

  CHECK(FunMgr::ins().contains(QStringLiteral("str")));
  CHECK(FunMgr::ins().contains(QStringLiteral("str"), QStringLiteral("trim")));
  CHECK(!FunMgr::ins().contains(QStringLiteral("str"), QStringLiteral("__no_such_fn__")));
  CHECK(!FunMgr::ins().contains(QStringLiteral("__no_such_class__")));
}

int runFunctionTests() {
  FunMgr::init();  // 幂等：重复注册安全

  testStrFunctions();
  testStrArgErrors();
  testBuiltinPathFunctions();
  testBuiltinMerge();
  testBuiltinFormatPath();
  testUnregistered();

  std::printf("[function] %d checks, %d failed\n", g_total, g_failed);
  return g_failed;
}
