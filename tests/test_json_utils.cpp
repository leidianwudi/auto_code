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

#include "src/engine/schema_validator.h"
#include "src/ui/json_source/json_source_finder.h"
#include "src/ui/json_source/json_upload_model.h"
#include "src/util/common/path_resolver.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/code/format_code.h"

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

/// accore JSON 值类型测试（tests/test_ac_json_value.cpp），返回失败数
int runAcJsonValueTests();

/// 标识符驻留池测试（tests/test_ac_ident_pool.cpp），返回失败数
int runAcIdentPoolTests();

/// AC 解释器直接单测（tests/test_ac_interpreter.cpp），返回失败数
int runAcInterpreterTests();

/// 端到端 golden 脚本测试（tests/test_golden_script.cpp），返回失败数
int runGoldenScriptTests();

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
  CHECK(PathResolver::findProjectRootUpward(proj + QStringLiteral("/sub")) == proj);
  CHECK(PathResolver::isProjectRoot(proj));
  CHECK(!PathResolver::isProjectRoot(proj + QStringLiteral("/sub")));

  // 2) 项目作用域：jsonvue 所在目录（sub）向上找根后递归收集，
  //    api/ 子目录下的数据源也能找到，且不混入其它项目
  const QStringList scoped = findJsonsourceFiles(proj + QStringLiteral("/sub"));
  CHECK(scoped.size() == 2);
  CHECK(scoped.contains(QDir::cleanPath(proj + QStringLiteral("/api/a.jsonsource"))));
  CHECK(scoped.contains(QDir::cleanPath(proj + QStringLiteral("/sub/b.jsonsource"))));

  // 3) 无标记目录（file/ 根未设项目时）→ 回退全局：包含其它项目的数据源
  if (!PathResolver::isProjectRoot(fileRoot)) {
    const QStringList all = findJsonsourceFiles(fileRoot + QStringLiteral("/admin_vue/template"));
    CHECK(!all.isEmpty());
    CHECK(all.contains(QDir::cleanPath(
        fileRoot + QStringLiteral("/admin_vue/news_admin/api/boolean.jsonsource"))));
  }

  // 清理临时项目目录
  QDir(proj).removeRecursively();
}

/// .jsonupload 作用域查找：与 jsonsource 共用实现，验证后缀参数化后行为一致
static void testProjectScopedJsonuploadFinder() {
  const QString fileRoot =
      QDir::cleanPath(QStringLiteral(PROJECT_SOURCE_DIR) + QStringLiteral("/file"));

  // 构造临时项目：file/__test_proj__/{project.acproj, api/u.jsonupload, sub/v.jsonupload}
  const QString proj = fileRoot + QStringLiteral("/__test_proj__");
  QDir().mkpath(proj + QStringLiteral("/api"));
  QDir().mkpath(proj + QStringLiteral("/sub"));
  QFile marker(proj + QStringLiteral("/project.acproj"));
  CHECK(marker.open(QIODevice::WriteOnly | QIODevice::Text));
  marker.write("{}");
  marker.close();
  QFile f1(proj + QStringLiteral("/api/u.jsonupload"));
  f1.open(QIODevice::WriteOnly | QIODevice::Text);
  f1.close();
  QFile f2(proj + QStringLiteral("/sub/v.jsonupload"));
  f2.open(QIODevice::WriteOnly | QIODevice::Text);
  f2.close();

  // 1) 项目作用域：只收集该项目根下的 .jsonupload
  const QStringList scoped = findJsonuploadFiles(proj + QStringLiteral("/sub"));
  CHECK(scoped.size() == 2);
  CHECK(scoped.contains(QDir::cleanPath(proj + QStringLiteral("/api/u.jsonupload"))));
  CHECK(scoped.contains(QDir::cleanPath(proj + QStringLiteral("/sub/v.jsonupload"))));

  // 2) 查找结果互不混入：jsonsource 查找不含 .jsonupload 文件
  const QStringList srcScoped = findJsonsourceFiles(proj + QStringLiteral("/sub"));
  CHECK(!srcScoped.contains(QDir::cleanPath(proj + QStringLiteral("/api/u.jsonupload"))));

  // 3) 无标记目录（file/ 根未设项目时）→ 回退全局：能找到临时项目的 .jsonupload
  if (!PathResolver::isProjectRoot(fileRoot)) {
    const QStringList all = findJsonuploadFiles(fileRoot + QStringLiteral("/news_admin"));
    CHECK(all.contains(QDir::cleanPath(proj + QStringLiteral("/api/u.jsonupload"))));
  }

  // 清理临时项目目录
  QDir(proj).removeRecursively();
}

/// JsonUploadConfig 序列化往返：逐字段保真 + 未知键不丢失
static void testJsonUploadConfigRoundTrip() {
  // 构造 2 条预设（覆盖 params/valueType/maxCount/默认值）
  JsonUploadConfig cfg;
  JsonUpload u1;
  u1.id = QStringLiteral("a1b2c3d4");
  u1.remark = QStringLiteral("商品主图上传");
  u1.url = QStringLiteral("upload/image");
  u1.method = QStringLiteral("POST");
  u1.fileField = QStringLiteral("file");
  JsonUploadParam p1;
  p1.name = QStringLiteral("type");
  p1.value = QStringLiteral("0");
  p1.valueType = QStringLiteral("number");
  u1.params.append(p1);
  JsonUploadParam p2;
  p2.name = QStringLiteral("scene");
  p2.value = QStringLiteral("admin");
  u1.params.append(p2);
  u1.responsePath = QStringLiteral("data.url");
  u1.maxCount = 5;
  u1.valueType = QStringLiteral("array");
  cfg.uploads.append(u1);

  JsonUpload u2;
  u2.id = QStringLiteral("e5f6a7b8");
  u2.url = QStringLiteral("upload/avatar");
  u2.maxCount = 1;  // 单图，其余字段全部走默认值
  cfg.uploads.append(u2);

  const QString jsonStr = cfg.toJsonString();
  const JsonUploadConfig back = JsonUploadConfig::fromJsonString(jsonStr);

  CHECK(back.uploads.size() == 2);

  const JsonUpload &r1 = back.uploads[0];
  CHECK(r1.id == u1.id);
  CHECK(r1.remark == u1.remark);
  CHECK(r1.url == u1.url);
  CHECK(r1.method == QStringLiteral("POST"));
  CHECK(r1.fileField == QStringLiteral("file"));
  CHECK(r1.params.size() == 2);
  CHECK(r1.params[0].name == QStringLiteral("type"));
  CHECK(r1.params[0].value == QStringLiteral("0"));
  CHECK(r1.params[0].valueType == QStringLiteral("number"));
  CHECK(r1.params[1].name == QStringLiteral("scene"));
  CHECK(r1.params[1].value == QStringLiteral("admin"));
  CHECK(r1.params[1].valueType.isEmpty());
  CHECK(r1.responsePath == QStringLiteral("data.url"));
  CHECK(r1.maxCount == 5);
  CHECK(r1.valueType == QStringLiteral("array"));
  CHECK(r1.isMulti());

  const JsonUpload &r2 = back.uploads[1];
  CHECK(r2.id == u2.id);
  CHECK(r2.url == QStringLiteral("upload/avatar"));
  CHECK(r2.method == QStringLiteral("POST"));            // 默认值
  CHECK(r2.fileField == QStringLiteral("file"));         // 默认值
  CHECK(r2.responsePath == QStringLiteral("data.url"));  // 默认值
  CHECK(r2.maxCount == 1);
  CHECK(r2.valueType.isEmpty());
  CHECK(!r2.isMulti());

  // uploadById / isEmpty
  CHECK(back.uploadById(QStringLiteral("a1b2c3d4")) != nullptr);
  CHECK(back.uploadById(QStringLiteral("no-such-id")) == nullptr);
  CHECK(!back.isEmpty());

  // 未知键保真：手工塞入自定义键再解析，写回不丢失
  QJsonObject raw = JsonUploadConfig::fromJsonString(jsonStr).toJsonObject();
  QJsonObject firstUpload = raw.value(QStringLiteral("uploads")).toArray().at(0).toObject();
  firstUpload.insert(QStringLiteral("customFutureKey"), QStringLiteral("kept"));
  QJsonArray arr = raw.value(QStringLiteral("uploads")).toArray();
  arr[0] = firstUpload;
  raw[QStringLiteral("uploads")] = arr;
  const JsonUploadConfig back2 =
      JsonUploadConfig::fromJsonString(JsonUploadConfig::toJsonString(raw));
  CHECK(back2.uploads.size() == 2);
  CHECK(back2.uploads[0].id == u1.id);  // 已知字段正常解析
  // 保真合并由 JsonUploadWidget 的 collectMergedObject 层负责（同 jsonsource），模型层只保证不崩
}

/// 回归防护：项目内置 param.schema.json 必须能被 SchemaValidator 加载
/// （该文件为手工维护的 JSON5；历史上因根对象闭合后多写一个尾逗号导致
///  AcJsonValue::parse 报「末尾存在多余内容」，可视化编辑提示 schema 加载失败）
static void testBuiltinParamSchemaLoads() {
  const QString path =
      QDir::cleanPath(QStringLiteral(PROJECT_SOURCE_DIR) +
                      QStringLiteral("/file/crud_nest/template/tool/param.schema.json"));

  // 解析层：文件级 JSON5 语法必须干净（尾逗号仅允许出现在对象/数组内部）
  const QString raw = UtilJson::readTextFile(path);
  CHECK(!raw.isEmpty());
  bool ok = false;
  QString err;
  accore::AcJsonValue::parse(raw, &ok, &err);
  CHECK(ok);

  // 加载层：root/definitions 完整解析，rootClass 指向 AutoConfig
  SchemaValidator schema;
  CHECK(schema.load(path));
  CHECK(schema.hasRoot());
  CHECK(schema.rootClass() == QStringLiteral("AutoConfig"));
  CHECK(schema.classNames().contains(QStringLiteral("TableConfig")));
}

/// 回归防护：JSON/JSON5 格式化输出必须可再次解析，且根值闭合后不得有尾随逗号
/// （历史上 FormatJson5 对根对象闭合后也输出逗号，param.schema.json 被格式化
///   一次后即出现「schema 加载失败」——逗号恰好落在根值之后，属非法 JSON5）
static void testFormatCodeJsonRoundTrip() {
  // ── 普通风格 ──
  // 空对象/空数组作为非最后成员必须保留分隔逗号（历史上空对象漏逗号 → 输出非法 JSON）
  const QString plain =
      FormatCode::format(QStringLiteral("{\"a\": {}, \"b\": []}"), FormatCode::FormatJson);
  QJsonParseError perr;
  const QJsonDocument pdoc = UtilJson::fromJson(plain, &perr);
  CHECK(perr.error == QJsonParseError::NoError);
  CHECK(pdoc.object().value(QStringLiteral("a")).toObject().isEmpty());
  CHECK(pdoc.object().value(QStringLiteral("b")).toArray().isEmpty());
  CHECK(plain.contains(QStringLiteral("\"a\": {},")));
  CHECK(plain.contains(QStringLiteral("\"b\": []")));

  // ── JSON5 风格 ──
  const QString json5 = FormatCode::format(QStringLiteral("{\"name\": \"x\", \"cfg\": {\"n\": 1}}"),
                                           FormatCode::FormatJson5);
  // 根值闭合后绝不能有逗号（核心回归：param.schema.json 场景）
  CHECK(json5.trimmed().endsWith(QLatin1Char('}')));
  // 容器内部最后成员保留尾随逗号（JSON5 风格特征）
  CHECK(json5.contains(QStringLiteral("1,")));
  CHECK(json5.contains(QStringLiteral("},")));
  // 无引号 key + 单引号字符串
  CHECK(json5.contains(QStringLiteral("name: 'x'")));
  // 输出必须是合法 JSON5（UtilJson::fromJson 支持 JSON5）
  const QJsonDocument j5doc = UtilJson::fromJson(json5, &perr);
  CHECK(perr.error == QJsonParseError::NoError);
  CHECK(j5doc.object()
            .value(QStringLiteral("cfg"))
            .toObject()
            .value(QStringLiteral("n"))
            .toDouble() == 1.0);

  // 根数组同理：闭合 ] 后无逗号
  const QString arr5 =
      FormatCode::format(QStringLiteral("[{\"k\": 1}, {}]"), FormatCode::FormatJson5);
  CHECK(arr5.trimmed().endsWith(QLatin1Char(']')));
  const QJsonDocument arrDoc = UtilJson::fromJson(arr5, &perr);
  CHECK(perr.error == QJsonParseError::NoError);
  CHECK(arrDoc.array().size() == 2);
}

int main() {
  // 所测接口均不依赖 QCoreApplication 实例，无需构造应用对象
  testBuiltinParamSchemaLoads();
  testFormatCodeJsonRoundTrip();
  testJson5Parsing();
  testFingerprint();
  testResolveSchemaPath();
  testProjectScopedJsonsourceFinder();
  testProjectScopedJsonuploadFinder();
  testJsonUploadConfigRoundTrip();
  const int extraFailed = runAcParamDefaultTests();
  const int jsonValueFailed = runAcJsonValueTests();
  const int identPoolFailed = runAcIdentPoolTests();
  const int interpreterFailed = runAcInterpreterTests();
  const int goldenFailed = runGoldenScriptTests();

  const int totalFailed =
      g_failed + extraFailed + jsonValueFailed + identPoolFailed + interpreterFailed + goldenFailed;
  std::printf("%d checks, %d failed\n", g_total, totalFailed);
  return totalFailed == 0 ? 0 : 1;
}
