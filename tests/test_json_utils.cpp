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
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <cstdio>

#include "src/engine/schema_validator.h"
#include "src/ui/json_source/json_source_finder.h"
#include "src/ui/json_source/json_table_model.h"
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

/// AC 结构化诊断系统测试（tests/test_ac_diagnostic.cpp），返回失败数
int runAcDiagnosticTests();

/// 字节码 VM 对拍测试（tests/test_ac_vm.cpp），返回失败数
int runAcVmTests();

/// 预编译缓存测试（tests/test_ac_cache.cpp），返回失败数
int runAcCacheTests();

/// Parser 错误恢复测试（tests/test_ac_recovery.cpp），返回失败数
int runAcRecoveryTests();

/// 进程内语义服务测试（tests/test_ac_semantic.cpp），返回失败数
int runAcSemanticTests();

/// 端到端 golden 脚本测试（tests/test_golden_script.cpp），返回失败数
int runGoldenScriptTests();

/// 模板引擎测试（tests/test_tpl.cpp），返回失败数
int runTplTests();

/// 内置函数系统测试（tests/test_function.cpp），返回失败数
int runFunctionTests();

/// 语义引用收集测试（tests/test_rename.cpp），返回失败数
int runRenameTests();

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

/// .jsontable 作用域查找：与 jsonsource/jsonupload 共用实现，验证后缀参数化后行为一致
static void testProjectScopedJsontableFinder() {
  const QString fileRoot =
      QDir::cleanPath(QStringLiteral(PROJECT_SOURCE_DIR) + QStringLiteral("/file"));

  // 构造临时项目：file/__test_proj__/{project.acproj, api/a.jsontable, sub/b.jsontable}
  const QString proj = fileRoot + QStringLiteral("/__test_proj__");
  QDir().mkpath(proj + QStringLiteral("/api"));
  QDir().mkpath(proj + QStringLiteral("/sub"));
  QFile marker(proj + QStringLiteral("/project.acproj"));
  CHECK(marker.open(QIODevice::WriteOnly | QIODevice::Text));
  marker.write("{}");
  marker.close();
  QFile f1(proj + QStringLiteral("/api/a.jsontable"));
  f1.open(QIODevice::WriteOnly | QIODevice::Text);
  f1.close();
  QFile f2(proj + QStringLiteral("/sub/b.jsontable"));
  f2.open(QIODevice::WriteOnly | QIODevice::Text);
  f2.close();

  // 1) 项目作用域：只收集该项目根下的 .jsontable
  const QStringList scoped = findJsontableFiles(proj + QStringLiteral("/sub"));
  CHECK(scoped.size() == 2);
  CHECK(scoped.contains(QDir::cleanPath(proj + QStringLiteral("/api/a.jsontable"))));
  CHECK(scoped.contains(QDir::cleanPath(proj + QStringLiteral("/sub/b.jsontable"))));

  // 2) 查找结果互不混入：jsonsource 查找不含 .jsontable 文件
  const QStringList srcScoped = findJsonsourceFiles(proj + QStringLiteral("/sub"));
  CHECK(!srcScoped.contains(QDir::cleanPath(proj + QStringLiteral("/api/a.jsontable"))));

  // 3) 无标记目录（file/ 根未设项目时）→ 回退全局：能找到临时项目的 .jsontable
  if (!PathResolver::isProjectRoot(fileRoot)) {
    const QStringList all = findJsontableFiles(fileRoot + QStringLiteral("/news_admin"));
    CHECK(all.contains(QDir::cleanPath(proj + QStringLiteral("/api/a.jsontable"))));
  }

  // 清理临时项目目录
  QDir(proj).removeRecursively();
}

/// JsonTableConfig 序列化往返：逐字段保真 + 未知键（i18n / meta 未知键）不丢失
static void testJsonTableConfigRoundTrip() {
  // 构造 meta + 2 张表（覆盖主键/自增/unsigned/nullable/默认值/前端角色/唯一索引）
  JsonTableConfig cfg;
  cfg.metaProject = QStringLiteral("shop");
  cfg.dbHost = QStringLiteral("127.0.0.1");
  cfg.dbPort = 3307;
  cfg.dbUser = QStringLiteral("root");
  cfg.dbPassword = QStringLiteral("secret");
  cfg.dbDatabase = QStringLiteral("shop_db");
  cfg.goBasePath = QStringLiteral("D:/work/github/shop/shop_api");
  cfg.webBasePath = QStringLiteral("D:/work/github/shop/shop_admin");

  JsonTableTable t1;
  t1.tableName = QStringLiteral("shop");
  t1.modelName = QStringLiteral("shop");
  t1.tableComment = QStringLiteral("商品表");
  t1.manualUpdateTime = true;

  JsonTableColumn id;
  id.name = QStringLiteral("id");
  id.mysqlType = QStringLiteral("bigint");
  id.isUnsigned = true;
  id.isPrimary = true;
  id.isAutoInc = true;
  id.comment = QStringLiteral("主键");
  id.form = QStringLiteral("none");
  t1.columns.append(id);

  JsonTableColumn name;
  name.name = QStringLiteral("name");
  name.mysqlType = QStringLiteral("varchar(64)");
  name.nullable = true;  // 边界：nullable/unsigned 与主键列取反
  name.comment = QStringLiteral("商品名");
  name.search = true;
  name.required = true;
  t1.columns.append(name);

  JsonTableColumn price;
  price.name = QStringLiteral("price");
  price.mysqlType = QStringLiteral("decimal(10,2)");
  price.isUnsigned = true;
  price.defaultValue = QStringLiteral("0.00");
  price.form = QStringLiteral("number");
  t1.columns.append(price);

  JsonTableIndex ukName;
  ukName.name = QStringLiteral("uk_name");
  ukName.cols = QStringList{QStringLiteral("name")};
  ukName.unique = true;
  t1.indexes.append(ukName);
  cfg.tables.append(t1);

  JsonTableTable t2;
  t2.tableName = QStringLiteral("category");
  t2.modelName = QStringLiteral("category");
  JsonTableColumn cid;
  cid.name = QStringLiteral("id");
  cid.mysqlType = QStringLiteral("int");
  cid.isPrimary = true;
  t2.columns.append(cid);
  JsonTableColumn cname;
  cname.name = QStringLiteral("title");
  cname.list = false;  // 边界：前端角色翻转
  t2.columns.append(cname);
  cfg.tables.append(t2);

  const QString jsonStr = cfg.toJsonString();
  const JsonTableConfig back = JsonTableConfig::fromJsonString(jsonStr);

  CHECK(back.metaProject == QStringLiteral("shop"));
  CHECK(back.dbHost == QStringLiteral("127.0.0.1"));
  CHECK(back.dbPort == 3307);
  CHECK(back.dbUser == QStringLiteral("root"));
  CHECK(back.dbPassword == QStringLiteral("secret"));
  CHECK(back.dbDatabase == QStringLiteral("shop_db"));
  CHECK(back.goBasePath == QStringLiteral("D:/work/github/shop/shop_api"));
  CHECK(back.webBasePath == QStringLiteral("D:/work/github/shop/shop_admin"));
  CHECK(back.tables.size() == 2);

  // 表 1：逐字段保真（operator== 覆盖全部列字段）
  const JsonTableTable &r1 = back.tables[0];
  CHECK(r1.tableName == QStringLiteral("shop"));
  CHECK(r1.modelName == QStringLiteral("shop"));
  CHECK(r1.tableComment == QStringLiteral("商品表"));
  CHECK(r1.manualUpdateTime);
  CHECK(r1.columns.size() == 3);
  CHECK(r1.columns[0] == id);
  CHECK(r1.columns[1] == name);
  CHECK(r1.columns[1].defaultValue.isEmpty());  // 边界：空 default 往返后仍为空（无默认值）
  CHECK(r1.columns[2] == price);
  CHECK(r1.indexes.size() == 1);
  CHECK(r1.indexes[0].name == QStringLiteral("uk_name"));
  CHECK(r1.indexes[0].cols == QStringList{QStringLiteral("name")});
  CHECK(r1.indexes[0].unique);

  // 列顺序保持（建表顺序语义）
  CHECK(r1.columns[0].name == QStringLiteral("id"));
  CHECK(r1.columns[1].name == QStringLiteral("name"));
  CHECK(r1.columns[2].name == QStringLiteral("price"));

  // 表 2：list=false 翻转保持，其余走默认值
  const JsonTableTable &r2 = back.tables[1];
  CHECK(r2.tableName == QStringLiteral("category"));
  CHECK(r2.columns.size() == 2);
  CHECK(r2.columns[1].list == false);
  CHECK(r2.columns[1].search == false);
  CHECK(r2.columns[1].form == QStringLiteral("input"));
  CHECK(r2.indexes.isEmpty());

  // 未知键保真：表级 i18n 进阶节点 + meta 未知键，往返后原样写回
  QJsonObject raw = cfg.toJsonObject();
  QJsonObject meta = raw.value(QStringLiteral("meta")).toObject();
  meta.insert(QStringLiteral("customFutureKey"), QStringLiteral("kept"));
  raw[QStringLiteral("meta")] = meta;
  QJsonObject t1Obj = raw.value(QStringLiteral("tables")).toArray().at(0).toObject();
  t1Obj.insert(QStringLiteral("i18n"),
               QJsonObject{{QStringLiteral("table"), QStringLiteral("shop0")},
                           {QStringLiteral("extKey"), QStringLiteral("ext_id")}});
  QJsonArray arr = raw.value(QStringLiteral("tables")).toArray();
  arr[0] = t1Obj;
  raw[QStringLiteral("tables")] = arr;
  const JsonTableConfig back2 = JsonTableConfig::fromJsonString(JsonTableConfig::toJsonString(raw));
  CHECK(back2.tables.size() == 2);
  CHECK(back2.tables[0].tableName == QStringLiteral("shop"));  // 已知字段正常解析
  const QJsonObject i18n = back2.tables[0].extra.value(QStringLiteral("i18n")).toObject();
  CHECK(i18n.value(QStringLiteral("table")).toString() == QStringLiteral("shop0"));
  CHECK(i18n.value(QStringLiteral("extKey")).toString() == QStringLiteral("ext_id"));
  CHECK(back2.extra.value(QStringLiteral("customFutureKey")).toString() == QStringLiteral("kept"));

  // 边界：db 缺省 port → 默认 3306；列 default 为 null/数字字面量的兼容
  const JsonTableConfig minimal = JsonTableConfig::fromJsonString(
      QStringLiteral("{\"meta\":{\"project\":\"p\",\"db\":{\"host\":\"h\"}},"
                     "\"tables\":[{\"tableName\":\"t\",\"columns\":["
                     "{\"name\":\"c\",\"default\":null,\"mysqlType\":\"int\"},"
                     "{\"name\":\"n\",\"default\":0}]}]}"));
  CHECK(minimal.dbPort == 3306);
  CHECK(minimal.tables.size() == 1);
  CHECK(minimal.tables[0].columns[0].defaultValue.isEmpty());  // null → 无默认值
  CHECK(minimal.tables[0].columns[0].list);                    // 缺省 true
  CHECK(minimal.tables[0].columns[0].form == QStringLiteral("input"));
  CHECK(minimal.tables[0].columns[1].defaultValue == QStringLiteral("0"));  // 数字默认值显式转串
}

/// 性能探针（诊断 .jsontable 编辑卡顿）：对真实 shop.jsontable + 其 schema，
/// 分别计时 load / validateDocument / completions 的批量耗时，
/// 输出每次操作的平均毫秒数——定位"每键全量重验"卡顿的真实热点。
static void testSchemaValidatePerfProbe() {
  const QString jsonPath = QDir::cleanPath(QStringLiteral(PROJECT_SOURCE_DIR) +
                                           QStringLiteral("/file/go_pure/shop/shop.jsontable"));
  const QString schemaPath =
      QDir::cleanPath(QStringLiteral(PROJECT_SOURCE_DIR) +
                      QStringLiteral("/file/crud_gin/template/tool/param.schema.json"));
  QFile f(jsonPath);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    std::printf("[perf-probe] shop.jsontable 不存在，跳过\n");
    return;
  }
  const QString text = QString::fromUtf8(f.readAll());
  f.close();

  QElapsedTimer timer;
  // 1) load（缓存命中路径，模拟每键校验）
  timer.start();
  for (int i = 0; i < 200; ++i) {
    SchemaValidator v;
    if (!v.load(schemaPath)) {
      std::printf("[perf-probe] schema load 失败\n");
      return;
    }
  }
  std::printf("[perf-probe] load x200: %lld ms（平均 %.3f ms/次）\n", timer.elapsed(),
              timer.elapsed() / 200.0);

  // 2) validateDocument（模拟每键全量校验）
  SchemaValidator v;
  v.load(schemaPath);
  const QJsonDocument doc = UtilJson::fromJson(text);
  timer.start();
  int totalErrs = 0;
  for (int i = 0; i < 200; ++i) {
    totalErrs = v.validateDocument(accore::AcJsonValue::fromQJsonValue(doc.object())).size();
  }
  std::printf("[perf-probe] validateDocument x200: %lld ms（平均 %.3f ms/次，错误数 %d）\n",
              timer.elapsed(), timer.elapsed() / 200.0, totalErrs);

  // 3) completions（模拟每键补全扫描）
  timer.start();
  for (int i = 0; i < 200; ++i) {
    v.completions(text, text.size() - 2);
  }
  std::printf("[perf-probe] completions x200: %lld ms（平均 %.3f ms/次）\n", timer.elapsed(),
              timer.elapsed() / 200.0);
}

int main() {
  // 无缓冲输出：崩溃前也能看到进度（stdout 重定向到文件是块缓冲，崩溃会丢缓冲）
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  // 所测接口均不依赖 QCoreApplication 实例，无需构造应用对象
  testBuiltinParamSchemaLoads();
  testFormatCodeJsonRoundTrip();
  testJson5Parsing();
  testFingerprint();
  testProjectScopedJsonsourceFinder();
  testProjectScopedJsonuploadFinder();
  testJsonUploadConfigRoundTrip();
  testProjectScopedJsontableFinder();
  testJsonTableConfigRoundTrip();
  testSchemaValidatePerfProbe();
  const int extraFailed = runAcParamDefaultTests();
  const int jsonValueFailed = runAcJsonValueTests();
  const int identPoolFailed = runAcIdentPoolTests();
  const int interpreterFailed = runAcInterpreterTests();
  const int diagnosticFailed = runAcDiagnosticTests();
  const int vmFailed = runAcVmTests();
  const int cacheFailed = runAcCacheTests();
  const int recoveryFailed = runAcRecoveryTests();
  const int semanticFailed = runAcSemanticTests();
  const int tplFailed = runTplTests();
  const int functionFailed = runFunctionTests();
  const int renameFailed = runRenameTests();
  const int goldenFailed = runGoldenScriptTests();

  const int totalFailed = g_failed + extraFailed + jsonValueFailed + identPoolFailed +
                          interpreterFailed + diagnosticFailed + vmFailed + cacheFailed +
                          recoveryFailed + semanticFailed + tplFailed + functionFailed +
                          renameFailed + goldenFailed;
  std::printf("%d checks, %d failed\n", g_total, totalFailed);
  return totalFailed == 0 ? 0 : 1;
}
