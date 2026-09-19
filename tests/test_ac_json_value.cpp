/**
 * @file test_ac_json_value.cpp
 * @brief accore JSON 值类型单元测试（纯 QtCore，无 GUI）
 *
 * 覆盖：
 *  - 对象键插入序：keys()/members()/serialize 均保序
 *  - 已存在键赋值 = 原位置更新（JS 语义）；remove 后重插 = 尾部追加
 *  - 写时复制：拷贝后修改不影响原值（数组/对象）
 *  - 深拷贝隔离：clone 后互不影响
 *  - 解析：严格 JSON + JSON5 超集（注释/单引号/无引号键/尾逗号）+ 转义
 *  - 序列化：紧凑/格式化、数字 JS 语义、字符串转义
 *  - QJsonValue 边界互转
 */

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <cstdio>

#include "src/core/json/ac_json_value.h"

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

using accore::AcJsonValue;

/// 对象键插入序：set 顺序 = keys/members/serialize 顺序
static void testObjectInsertionOrder() {
  AcJsonValue o = AcJsonValue::makeObject();
  o.set(QStringLiteral("code"), AcJsonValue(0));
  o.set(QStringLiteral("msg"), AcJsonValue(QStringLiteral("ok")));
  o.set(QStringLiteral("data"), AcJsonValue::makeArray());
  CHECK(o.keys() ==
        QStringList({QStringLiteral("code"), QStringLiteral("msg"), QStringLiteral("data")}));
  CHECK(o.serialize() == QStringLiteral("{\"code\":0,\"msg\":\"ok\",\"data\":[]}"));
  CHECK(o.size() == 3);
  CHECK(o.members().at(0).key == QStringLiteral("code"));
  CHECK(o.members().at(2).value.isArray());
}

/// 已存在键赋值 = 原位置更新（JS 语义）；remove 后重插 = 尾部追加
static void testObjectUpdateAndReinsert() {
  AcJsonValue o = AcJsonValue::makeObject();
  o.set(QStringLiteral("a"), AcJsonValue(1));
  o.set(QStringLiteral("b"), AcJsonValue(2));
  o.set(QStringLiteral("a"), AcJsonValue(99));  // 原位置更新
  CHECK(o.keys() == QStringList({QStringLiteral("a"), QStringLiteral("b")}));
  CHECK(o.value(QStringLiteral("a")).toDouble() == 99);

  o.remove(QStringLiteral("a"));
  CHECK(!o.has(QStringLiteral("a")));
  CHECK(o.keys() == QStringList({QStringLiteral("b")}));
  o.set(QStringLiteral("a"), AcJsonValue(7));  // 重插 = 尾部
  CHECK(o.keys() == QStringList({QStringLiteral("b"), QStringLiteral("a")}));
}

/// 写时复制：拷贝共享读、写入时分离
static void testCopyOnWrite() {
  AcJsonValue a = AcJsonValue::makeObject();
  a.set(QStringLiteral("x"), AcJsonValue(1));
  AcJsonValue b = a;
  b.set(QStringLiteral("x"), AcJsonValue(2));
  b.set(QStringLiteral("y"), AcJsonValue(3));
  CHECK(a.value(QStringLiteral("x")).toDouble() == 1);
  CHECK(!a.has(QStringLiteral("y")));
  CHECK(b.value(QStringLiteral("x")).toDouble() == 2);

  AcJsonValue arr = AcJsonValue::makeArray();
  arr.append(AcJsonValue(1));
  AcJsonValue arr2 = arr;
  arr2.append(AcJsonValue(2));
  CHECK(arr.size() == 1);
  CHECK(arr2.size() == 2);

  // 数组内嵌对象：拷贝后改内嵌对象不影响原值
  AcJsonValue row = AcJsonValue::makeObject();
  row.set(QStringLiteral("k"), AcJsonValue(QStringLiteral("v1")));
  AcJsonValue rows = AcJsonValue::makeArray();
  rows.append(row);
  AcJsonValue rows2 = rows;
  rows2.at(0);  // 只读访问
  AcJsonValue inner = rows2.at(0);
  inner.set(QStringLiteral("k"), AcJsonValue(QStringLiteral("v2")));
  CHECK(rows.at(0).value(QStringLiteral("k")).toString() == QStringLiteral("v1"));
}

/// clone 深拷贝：嵌套结构完全隔离
/// 注意：value() 返回副本（与 QJsonValue 值语义一致），对副本修改需重新 set 写回
static void testCloneDeep() {
  AcJsonValue inner = AcJsonValue::makeObject();
  inner.set(QStringLiteral("n"), AcJsonValue(1));
  AcJsonValue src = AcJsonValue::makeObject();
  src.set(QStringLiteral("inner"), inner);
  src.set(QStringLiteral("list"), AcJsonValue::makeArray());
  src.value(QStringLiteral("list")).append(AcJsonValue(QStringLiteral("x")));

  AcJsonValue dst = src.clone();
  AcJsonValue inner2 = dst.value(QStringLiteral("inner"));
  inner2.set(QStringLiteral("n"), AcJsonValue(2));
  dst.set(QStringLiteral("inner"), inner2);  // 写回
  CHECK(src.value(QStringLiteral("inner")).value(QStringLiteral("n")).toDouble() == 1);
  CHECK(dst.value(QStringLiteral("inner")).value(QStringLiteral("n")).toDouble() == 2);
}

/// 数组操作：at 越界返回 Null、replace、removeLast
static void testArrayOps() {
  AcJsonValue arr = AcJsonValue::makeArray();
  arr.append(AcJsonValue(1));
  arr.append(AcJsonValue(2));
  arr.append(AcJsonValue(3));
  CHECK(arr.size() == 3);
  CHECK(arr.at(1).toDouble() == 2);
  CHECK(arr.at(9).isNull());
  arr.replace(1, AcJsonValue(QStringLiteral("two")));
  CHECK(arr.at(1).toString() == QStringLiteral("two"));
  arr.removeLast();
  CHECK(arr.size() == 2);
  // 非数组调用数组接口应安全无效果
  AcJsonValue num = AcJsonValue(5);
  num.append(AcJsonValue(1));
  CHECK(num.size() == 0);
}

/// 缺失与显式 null 的区分
static void testNullAndMissing() {
  AcJsonValue o = AcJsonValue::makeObject();
  o.set(QStringLiteral("n"), AcJsonValue());
  CHECK(!o.has(QStringLiteral("missing")));
  CHECK(o.value(QStringLiteral("missing")).isNull());
  CHECK(o.has(QStringLiteral("n")));
  CHECK(o.value(QStringLiteral("n")).isNull());
  // 键序含 null 值成员
  CHECK(o.keys() == QStringList({QStringLiteral("n")}));
}

/// 解析严格 JSON，对象键按原文顺序
static void testParseStrict() {
  bool ok = false;
  AcJsonValue v = AcJsonValue::parse(
      QStringLiteral("{\"code\":0,\"data\":{\"list\":[{\"b\":1,\"a\":2}]},\"msg\":\"ok\"}"), &ok);
  CHECK(ok);
  CHECK(v.keys() ==
        QStringList({QStringLiteral("code"), QStringLiteral("data"), QStringLiteral("msg")}));
  const AcJsonValue row = v.value(QStringLiteral("data")).value(QStringLiteral("list")).at(0);
  CHECK(row.keys() == QStringList({QStringLiteral("b"), QStringLiteral("a")}));
  CHECK(row.value(QStringLiteral("b")).toDouble() == 1);
  // 顶层标量也是合法 JSON
  AcJsonValue n = AcJsonValue::parse(QStringLiteral("42"), &ok);
  CHECK(ok && n.toDouble() == 42);
  AcJsonValue s = AcJsonValue::parse(QStringLiteral("\"hi\""), &ok);
  CHECK(ok && s.toString() == QStringLiteral("hi"));
}

/// 解析 JSON5 超集：注释、单引号、无引号键、尾逗号
static void testParseJson5() {
  bool ok = false;
  const QString text = QStringLiteral(
      "// 行注释\n"
      "{ /* 块注释 */\n"
      "  unquoted: '单引号',\n"
      "  'quoted key': true,\n"
      "  list: [1, 2, 3,],\n"
      "}\n");
  AcJsonValue v = AcJsonValue::parse(text, &ok);
  CHECK(ok);
  CHECK(v.value(QStringLiteral("unquoted")).toString() == QStringLiteral("单引号"));
  CHECK(v.value(QStringLiteral("quoted key")).toBool() == true);
  CHECK(v.value(QStringLiteral("list")).size() == 3);
  // 无引号键同样保序
  CHECK(v.keys().first() == QStringLiteral("unquoted"));
}

/// 解析转义：标准转义 + \uXXXX + 反斜杠续行
static void testParseEscapes() {
  bool ok = false;
  AcJsonValue v = AcJsonValue::parse(QStringLiteral("{\"s\":\"a\\n\\t\\\"q\\\\b\\u0041\"}"), &ok);
  CHECK(ok);
  CHECK(v.value(QStringLiteral("s")).toString() == QStringLiteral("a\n\t\"q\\bA"));
  // 单引号内可含未转义双引号
  AcJsonValue v2 = AcJsonValue::parse(QStringLiteral("{'k':'say \"hi\"'}"), &ok);
  CHECK(ok);
  CHECK(v2.value(QStringLiteral("k")).toString() == QStringLiteral("say \"hi\""));
}

/// 解析失败：未闭合/多余内容/空文本
static void testParseErrors() {
  bool ok = true;
  QString err;
  AcJsonValue::parse(QStringLiteral("{\"a\":1"), &ok, &err);
  CHECK(!ok);
  CHECK(!err.isEmpty());
  AcJsonValue::parse(QStringLiteral("[1, 2"), &ok);
  CHECK(!ok);
  AcJsonValue::parse(QStringLiteral("{} trailing"), &ok);
  CHECK(!ok);
  AcJsonValue::parse(QString(), &ok);
  CHECK(!ok);
}

/// 序列化：数字 JS 语义、字符串转义、紧凑与格式化
static void testSerialize() {
  CHECK(AcJsonValue(3).serialize() == QStringLiteral("3"));
  CHECK(AcJsonValue(3.5).serialize() == QStringLiteral("3.5"));
  CHECK(AcJsonValue(-0.25).serialize() == QStringLiteral("-0.25"));
  CHECK(AcJsonValue(1e21).serialize() == QStringLiteral("1e+21"));

  AcJsonValue o = AcJsonValue::makeObject();
  o.set(QStringLiteral("s"), AcJsonValue(QStringLiteral("a\"b\\c\nd")));
  CHECK(o.serialize() == QStringLiteral("{\"s\":\"a\\\"b\\\\c\\nd\"}"));

  AcJsonValue nested = AcJsonValue::makeObject();
  AcJsonValue list = AcJsonValue::makeArray();
  list.append(AcJsonValue(1));
  list.append(AcJsonValue(2));
  nested.set(QStringLiteral("list"), list);
  nested.set(QStringLiteral("empty"), AcJsonValue::makeObject());
  const QString pretty = nested.serialize(true);
  CHECK(pretty == QStringLiteral("{\n  \"list\": [\n    1,\n    2\n  ],\n  \"empty\": {}\n}"));
  // 空数组/空对象紧凑输出
  CHECK(AcJsonValue::makeArray().serialize() == QStringLiteral("[]"));
  CHECK(AcJsonValue::makeObject().serialize() == QStringLiteral("{}"));
}

/// QJsonValue 边界互转：数组保序精确；对象经 QJsonObject 后仅值语义保留（键序必然丢失）
static void testQJsonValueRoundTrip() {
  // 数组往返：完全一致
  AcJsonValue arr = AcJsonValue::makeArray();
  arr.append(AcJsonValue(1));
  arr.append(AcJsonValue(QStringLiteral("s")));
  arr.append(AcJsonValue(true));
  const QJsonValue q = arr.toQJsonValue();
  CHECK(q.isArray());
  const AcJsonValue back = AcJsonValue::fromQJsonValue(q);
  CHECK(back.serialize() == arr.serialize());

  // 对象往返：值保留（键序经 QJsonObject 已排序，属已知边界限制）
  AcJsonValue o = AcJsonValue::makeObject();
  o.set(QStringLiteral("k"), AcJsonValue(1.5));
  o.set(QStringLiteral("t"), AcJsonValue(true));
  const QJsonValue qo = o.toQJsonValue();
  CHECK(qo.isObject());
  const AcJsonValue ob = AcJsonValue::fromQJsonValue(qo);
  CHECK(ob.value(QStringLiteral("k")).toDouble() == 1.5);
  CHECK(ob.value(QStringLiteral("t")).toBool() == true);
  // 与 QJsonDocument 序列化结果一致（同为字母序）
  const QJsonObject qobj = qo.toObject();
  CHECK(ob.serialize() == QString::fromUtf8(QJsonDocument(qobj).toJson(QJsonDocument::Compact)));
}

/// toInt 语义：仅整值可转，否则默认值
static void testToIntSemantics() {
  CHECK(AcJsonValue(3).toInt() == 3);
  CHECK(AcJsonValue(3.0).toInt() == 3);
  CHECK(AcJsonValue(3.5).toInt(-1) == -1);
  CHECK(AcJsonValue(QStringLiteral("3")).toInt(-1) == -1);
  CHECK(AcJsonValue().toInt(-2) == -2);
}

/// accore 核心层依赖卫生（M4 防债护栏）：
/// - QJson 值类型仅允许出现在值类型适配文件 ac_json_value.h（边界互转接口），
///   其余核心文件一律禁止（QJsonObject 键序有语义缺陷）
/// - GUI 模块与上层目录（engine/util/ui）禁止被核心引用
static void testCoreIncludeHygiene() {
  const QDir coreDir(QStringLiteral(PROJECT_SOURCE_DIR) + QStringLiteral("/src/core"));
  CHECK(coreDir.exists());
  const QStringList forbidden = {
      QStringLiteral("#include <QJson"),     QStringLiteral("#include <QtGui"),
      QStringLiteral("#include <QtWidgets"), QStringLiteral("#include \"src/engine"),
      QStringLiteral("#include \"src/ui"),   QStringLiteral("#include \"src/util"),
  };
  const QString qjsonAdapterFile = QStringLiteral("ac_json_value.h");

  QStringList violations;
  QDirIterator it(coreDir.absolutePath(), {QStringLiteral("*.h"), QStringLiteral("*.cpp")},
                  QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString path = it.next();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
    const QString text = QString::fromUtf8(f.readAll());
    for (const QString &bad : forbidden) {
      if (!text.contains(bad)) continue;
      // 例外：值类型适配单元（.h 接口 + .cpp 实现）自身的 QJson 边界互转
      if (bad == QStringLiteral("#include <QJson") &&
          (QFileInfo(path).fileName() == qjsonAdapterFile ||
           QFileInfo(path).fileName() == QStringLiteral("ac_json_value.cpp"))) {
        continue;
      }
      violations.append(path + QStringLiteral(" => ") + bad);
    }
  }
  for (const QString &v : violations) {
    std::printf("HYGIENE VIOLATION %s\n", v.toUtf8().constData());
  }
  CHECK(violations.isEmpty());
}

int runAcJsonValueTests() {
  testObjectInsertionOrder();
  testObjectUpdateAndReinsert();
  testCopyOnWrite();
  testCloneDeep();
  testArrayOps();
  testNullAndMissing();
  testParseStrict();
  testParseJson5();
  testParseEscapes();
  testParseErrors();
  testSerialize();
  testQJsonValueRoundTrip();
  testToIntSemantics();
  testCoreIncludeHygiene();
  return g_failed;
}
