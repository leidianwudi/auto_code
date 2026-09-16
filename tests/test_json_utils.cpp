/**
 * @file test_json_utils.cpp
 * @brief UtilJson / PathResolver 单元测试（纯 QtCore，无 GUI）
 *
 * 覆盖可视化编辑重构中抽出的三个公共工具：
 *  - UtilJson::fromJson（JSON5 兼容解析：注释/无引号键/单引号/尾随逗号/十六进制）
 *  - UtilJson::fingerprint（内容指纹，键序无关，用于跳过表单重建）
 *  - PathResolver::resolveSchemaPath（$schema 引用解析的三条规则）
 *
 * 构建：cmake --build <build-dir> --target auto_code_tests
 * 运行：<build-dir>/auto_code_tests.exe（返回 0 = 全部通过）
 */

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <cstdio>

#include "src/ui/json_source/json_source_finder.h"
#include "src/util/common/path_resolver.h"
#include "src/util/common/util_json.h"

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

/// JSON5 兼容解析：注释、无引号键、单引号、尾随逗号、十六进制，且字符串内的 // 不被误剥
static void testJson5Parsing() {
  QJsonDocument doc = UtilJson::fromJson(
      QStringLiteral("// 行注释\n"
                     "/* 块注释 */\n"
                     "{\n"
                     "  name: '模板A',\n"
                     "  count: 0xFF,\n"
                     "  items: [1, 2,],\n"
                     "  url: 'http://a.com/x',  // 字符串内的 // 不能被当注释剥离\n"
                     "}"));
  if (!doc.isObject()) {
    QJsonParseError diag;
    UtilJson::fromJson(
        QStringLiteral("// 行注释\n"
                       "/* 块注释 */\n"
                       "{\n"
                       "  name: '模板A',\n"
                       "  count: 0xFF,\n"
                       "  items: [1, 2,],\n"
                       "  url: 'http://a.com/x',  // 字符串内的 // 不能被当注释剥离\n"
                       "}"),
        &diag);
    std::printf("  [diag] json5 parse error: %s @%d\n", diag.errorString().toUtf8().constData(),
                diag.offset);
  }
  CHECK(doc.isObject());
  const QJsonObject obj = doc.object();
  CHECK(obj.value(QStringLiteral("name")).toString() == QStringLiteral("模板A"));
  CHECK(obj.value(QStringLiteral("count")).toInt() == 0xFF);
  CHECK(obj.value(QStringLiteral("items")).toArray().size() == 2);
  CHECK(obj.value(QStringLiteral("url")).toString() == QStringLiteral("http://a.com/x"));

  // 严格 JSON 同样可用
  QJsonDocument plain = UtilJson::fromJson(QStringLiteral("{\"a\": 1}"));
  CHECK(plain.object().value(QStringLiteral("a")).toInt() == 1);

  // 语法错误：error 非 NoError（半编辑状态判定依赖此行为）
  QJsonParseError err;
  UtilJson::fromJson(QStringLiteral("{\"a\": }"), &err);
  CHECK(err.error != QJsonParseError::NoError);
}

/// 内容指纹：键序无关（QJsonObject 有序存储），内容不同指纹必不同
static void testFingerprint() {
  const QJsonObject a{{"x", 1}, {"y", QStringLiteral("s")}};
  const QJsonObject b{{"y", QStringLiteral("s")}, {"x", 1}};
  const QJsonObject c{{"x", 2}, {"y", QStringLiteral("s")}};
  CHECK(UtilJson::fingerprint(a) == UtilJson::fingerprint(b));
  CHECK(UtilJson::fingerprint(a) != UtilJson::fingerprint(c));
  CHECK(UtilJson::fingerprint(QJsonObject()) != UtilJson::fingerprint(a));
}

/// $schema 引用解析三规则：/ 开头 → 项目源码 file/；相对 → json 所在目录；绝对 → 原样
static void testResolveSchemaPath() {
#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif
  const QString projectRoot = QStringLiteral(PROJECT_SOURCE_DIR);

  // / 开头 → 项目源码目录 file/ 下（公共 schema）
  const QString rootRef = PathResolver::resolveSchemaPath(QStringLiteral("d:/any/where/a.json"),
                                                          QStringLiteral("/tpl/main.schema.json"));
  CHECK(rootRef == QDir::cleanPath(projectRoot + QStringLiteral("/file/tpl/main.schema.json")));

  // 相对路径 → 基于 json 文件所在目录（盘符大小写按 Windows 习惯不敏感）
  const QString rel = PathResolver::resolveSchemaPath(QStringLiteral("d:/work/x/api/a.json"),
                                                      QStringLiteral("main.schema.json"));
  CHECK(rel.compare(QDir::cleanPath(QStringLiteral("d:/work/x/api/main.schema.json")),
                    Qt::CaseInsensitive) == 0);

  // 绝对路径 → 原样（仅做规范化）
  const QString abs = PathResolver::resolveSchemaPath(QStringLiteral("d:/work/x/api/a.json"),
                                                      QStringLiteral("d:/schemas/s.schema.json"));
  CHECK(abs == QStringLiteral("d:/schemas/s.schema.json"));
}

/// AC 参数默认值语法测试（tests/test_ac_param_default.cpp），返回失败数
int runAcParamDefaultTests();

/// .jsonsource 作用域查找：项目根（project.acproj）内严格过滤，无标记目录回退全局
static void testProjectScopedJsonsourceFinder() {
  const QString fileRoot =
      QDir::cleanPath(QStringLiteral(PROJECT_SOURCE_DIR) + QStringLiteral("/file"));

  // 构造临时项目：file/__test_proj__/{project.acproj, api/a.jsonsource, sub/b.jsonsource}
  const QString proj = fileRoot + QStringLiteral("/__test_proj__");
  QDir().mkpath(proj + QStringLiteral("/api"));
  QDir().mkpath(proj + QStringLiteral("/sub"));
  QFile marker(proj + QStringLiteral("/project.acproj"));
  CHECK(marker.open(QIODevice::WriteOnly | QIODevice::Text));
  marker.write("{}");
  marker.close();
  QFile f1(proj + QStringLiteral("/api/a.jsonsource"));
  f1.open(QIODevice::WriteOnly | QIODevice::Text);
  f1.close();
  QFile f2(proj + QStringLiteral("/sub/b.jsonsource"));
  f2.open(QIODevice::WriteOnly | QIODevice::Text);
  f2.close();

  // 1) 向上找根：从子目录 sub 命中项目根 __test_proj__
  CHECK(findProjectRootUpward(proj + QStringLiteral("/sub")) == proj);
  CHECK(isProjectRootDir(proj));
  CHECK(!isProjectRootDir(proj + QStringLiteral("/sub")));

  // 2) 项目作用域：jsonvue 所在目录（sub）向上找根后递归收集，
  //    api/ 子目录下的数据源也能找到，且不混入其它项目
  const QStringList scoped = findJsonsourceFiles(proj + QStringLiteral("/sub"));
  CHECK(scoped.size() == 2);
  CHECK(scoped.contains(QDir::cleanPath(proj + QStringLiteral("/api/a.jsonsource"))));
  CHECK(scoped.contains(QDir::cleanPath(proj + QStringLiteral("/sub/b.jsonsource"))));

  // 3) 无标记目录（file/ 根未设项目时）→ 回退全局：包含其它项目的数据源
  if (!isProjectRootDir(fileRoot)) {
    const QStringList all = findJsonsourceFiles(fileRoot + QStringLiteral("/admin_vue/template"));
    CHECK(!all.isEmpty());
    CHECK(all.contains(QDir::cleanPath(
        fileRoot + QStringLiteral("/admin_vue/news_admin/api/boolean.jsonsource"))));
  }

  // 清理临时项目目录
  QDir(proj).removeRecursively();
}

int main() {
  // 所测接口均不依赖 QCoreApplication 实例，无需构造应用对象
  testJson5Parsing();
  testFingerprint();
  testResolveSchemaPath();
  testProjectScopedJsonsourceFinder();
  const int extraFailed = runAcParamDefaultTests();

  std::printf("%d checks, %d failed\n", g_total, g_failed + extraFailed);
  return (g_failed + extraFailed) == 0 ? 0 : 1;
}
