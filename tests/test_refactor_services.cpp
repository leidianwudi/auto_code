/** @file test_refactor_services.cpp
 * @brief 原子保存、配置错误、结构服务和执行上下文隔离的回归测试。
 */
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <atomic>
#include <cstdio>
#include <thread>

#include "src/engine/function/fun_builtin.h"
#include "src/engine/function/fun_mgr.h"
#include "src/engine/script/ac_executor.h"
#include "src/ui/json_source/json_source_model.h"
#include "src/ui/main_dev/pending_change_store.h"
#include "src/ui/json_source/json_upload_model.h"
#include "src/ui/json_source/table_schema_service.h"
#include "src/util/common/util_file.h"
#include "src/util/common/util_json.h"

namespace {
int total = 0;
int failed = 0;
#define CHECK(condition)                                              \
  do {                                                                \
    ++total;                                                          \
    if (!(condition)) {                                               \
      ++failed;                                                       \
      std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); \
    }                                                                 \
  } while (0)

/// 已有文件完整替换、UTF-8 与空文件可用；目标无法提交时不改写其他原文件。
void testAtomicWrite() {
  QTemporaryDir dir;
  CHECK(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("子目录/配置.json"));
  QString error;
  CHECK(UtilFile::writeUtf8(path, QStringLiteral("原始内容"), &error, true));
  CHECK(error.isEmpty());
  CHECK(UtilFile::writeUtf8(path, QStringLiteral("新内容\n第二行"), &error));
  CHECK(UtilFile::readUtf8(path) == QStringLiteral("新内容\n第二行"));
  // 文件不能被当成目录；失败路径不得截断上一级已有文件。
  CHECK(!UtilFile::writeUtf8(path + QStringLiteral("/bad.txt"), QStringLiteral("错误"), &error));
  CHECK(!error.isEmpty());
  CHECK(UtilFile::readUtf8(path) == QStringLiteral("新内容\n第二行"));
  CHECK(UtilFile::writeUtf8(path, QString(), &error));
  CHECK(QFile(path).size() == 0);
}

/// 解析失败必须可区分于新建空配置，且失败不得替换调用方已有对象。
void testPendingSaveFailure() {
  QTemporaryDir dir;
  PendingChangeStore store;
  const QString good = dir.filePath(QStringLiteral("good.txt"));
  const QString blocked = dir.filePath(QStringLiteral("missing/file.txt"));
  store.set(good, QStringLiteral("已保存"));
  store.set(blocked, QStringLiteral("失败后保留"));
  const QStringList errors = store.flush();
  CHECK(errors.size() == 1);
  CHECK(!store.contains(good));
  CHECK(store.contains(blocked));
  CHECK(store.value(blocked) == QStringLiteral("失败后保留"));
  CHECK(UtilFile::readUtf8(good) == QStringLiteral("已保存"));
  CHECK(QDir().mkpath(dir.filePath(QStringLiteral("missing"))));
  CHECK(store.flush().isEmpty());
  CHECK(store.isEmpty());
  CHECK(UtilFile::readUtf8(blocked) == QStringLiteral("失败后保留"));
}

void testConfigErrors() {
  QString error = QStringLiteral("旧错误");
  QJsonObject root{{QStringLiteral("keep"), true}};
  CHECK(!UtilJson::parseObject(QStringLiteral("{sources:"), &root, &error));
  CHECK(!error.isEmpty());
  CHECK(root.value(QStringLiteral("keep")).toBool());
  CHECK(!UtilJson::parseObject(QStringLiteral("[]"), &root, &error));
  CHECK(UtilJson::parseObject(QStringLiteral("{sources: [],}"), &root, &error));
  CHECK(error.isEmpty());
  JsonSourceConfig::fromJsonString(QStringLiteral("{sources:"), &error);
  CHECK(!error.isEmpty());
  JsonSourceConfig::fromJsonString(QStringLiteral("{sources:[]}"), &error);
  CHECK(error.isEmpty());
  JsonUploadConfig::fromJsonString(QStringLiteral("[]"), &error);
  CHECK(!error.isEmpty());
  JsonTableConfig::fromJsonString(QStringLiteral("{"), &error);
  CHECK(!error.isEmpty());
  jsonTableFromString(QStringLiteral("{table:[]}"), &error);
  CHECK(!error.isEmpty());
}

/// 结构比较、删除引用与 DDL 都可脱离控件验证，避免重构改变业务结果。
void testSchemaOperations() {
  JsonTableTable table;
  table.tableName = QStringLiteral("人`员");
  table.tableComment = QStringLiteral("退休'人员");
  JsonTableColumn id;
  id.name = QStringLiteral("id");
  id.mysqlType = QStringLiteral("bigint");
  id.isPrimary = true;
  id.isAutoInc = true;
  JsonTableColumn name;
  name.name = QStringLiteral("name");
  name.mysqlType = QStringLiteral("varchar(100)");
  name.defaultValue = QStringLiteral("O'Neil");
  table.columns = {id, name};
  QString error;
  CHECK(TableSchema::validateCrossRefs(table, {}, {}, table.tableName, &error));
  const QString ddl =
      TableSchema::createDdlText(table.tableName, table.tableComment, table.columns, {});
  CHECK(ddl.contains(QStringLiteral("`人``员`")));
  CHECK(ddl.contains(QStringLiteral("'退休''人员'")));
  CHECK(ddl.contains(QStringLiteral("DEFAULT 'O''Neil'")));
  const auto diff = TableSchema::buildImportDiffEntries(table.columns, {id}, {});
  CHECK(diff.size() == 1);
  CHECK(diff.first().kind == TableSchema::ImportDiffKind::Removed);
  JsonTableSelCol selected;
  selected.ref = name.name;
  table.selCols.append(selected);
  const auto cleanup = TableSchema::cleanRemovedColumnRefs(&table, {name.name});
  CHECK(cleanup.selCols == 1);
  CHECK(table.selCols.isEmpty());
  table.columns.append(id);
  CHECK(!TableSchema::validateCrossRefs(table, {}, {}, table.tableName, &error));
  CHECK(error.contains(QStringLiteral("重复")));
}

QString scriptDir() {
  return FunMgr::ins()
      .call(QStringLiteral("builtin"), QStringLiteral("scriptDir"),
            accore::AcJsonValue::makeArray())
      .toString();
}

/// 同线程嵌套与不同线程交错，都不能污染外层路径或产物列表。
void testContextIsolation() {
  FunMgr::init();
  {
    FunBuiltin::ContextScope outer({QStringLiteral("outer"), {}, {}, nullptr, 0});
    CHECK(scriptDir() == QStringLiteral("outer"));
    {
      FunBuiltin::ContextScope inner({QStringLiteral("inner"), {}, {}, nullptr, 0});
      CHECK(scriptDir() == QStringLiteral("inner"));
    }
    CHECK(scriptDir() == QStringLiteral("outer"));
    // 实际 VM/解释器执行也必须在返回后恢复外层绑定。
    for (auto mode : {AcExecMode::kInterpreter, AcExecMode::kBytecode}) {
      AcExecutor executor;
      executor.setExecMode(mode);
      executor.setScriptDir(QStringLiteral("nested"));
      CHECK(executor.parse(QStringLiteral("let value: String = scriptDir();")));
      executor.execute();
      CHECK(executor.error().isEmpty());
      CHECK(scriptDir() == QStringLiteral("outer"));
    }
  }
  CHECK(scriptDir().isEmpty());
  std::atomic<int> ready{0};
  std::atomic<int> errors{0};
  auto run = [&](const QString &path) {
    FunBuiltin::ContextScope context({path, {}, {}, nullptr, 0});
    ++ready;
    while (ready.load() != 2) std::this_thread::yield();
    for (int i = 0; i < 1000; ++i) {
      if (scriptDir() != path) ++errors;
      std::this_thread::yield();
    }
  };
  std::thread first(run, QStringLiteral("first"));
  std::thread second(run, QStringLiteral("second"));
  first.join();
  second.join();
  CHECK(errors.load() == 0);
}
}  // namespace

int runRefactorServiceTests() {
  testAtomicWrite();
  testPendingSaveFailure();
  testConfigErrors();
  testSchemaOperations();
  testContextIsolation();
  std::printf("[refactor services] %d checks, %d failed\n", total, failed);
  return failed;
}
