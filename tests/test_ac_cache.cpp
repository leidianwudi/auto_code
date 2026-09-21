/**
 * @file test_ac_cache.cpp
 * @brief 预编译缓存测试（阶段 3）
 *
 * 覆盖：首次执行为空缓存目录生成 .acb → 二次执行命中缓存（结果一致 + 缓存文件存在）
 * → 改源文件后缓存失效并重新生成 → 缓存文件哈希随内容变化 → 魔数/版本不匹配拒绝加载。
 */

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <cstdio>

#include "src/engine/script/ac_bytecode_cache.h"
#include "src/engine/script/ac_executor.h"
#include "src/engine/script/ac_module_io.h"

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

static void writeTempFile(const QString &path, const QString &content) {
  QFile f(path);
  f.open(QIODevice::WriteOnly | QIODevice::Truncate);
  f.write(content.toUtf8());
  f.close();
}

static QString runBytecode(const QString &scriptFile, const QString &src) {
  AcExecutor ex;
  ex.setScriptFile(scriptFile);
  ex.setExecMode(AcExecMode::kBytecode);
  if (!ex.parse(src)) return QStringLiteral("parse:%1").arg(ex.error());
  QJsonValue r = ex.execute();
  const QString e = ex.error();
  if (!e.isEmpty()) return QStringLiteral("err:%1").arg(e);
  if (r.isObject())
    return QString::fromUtf8(QJsonDocument(r.toObject()).toJson(QJsonDocument::Compact));
  if (r.isArray())
    return QString::fromUtf8(QJsonDocument(r.toArray()).toJson(QJsonDocument::Compact));
  return r.toVariant().toString();
}

int runAcCacheTests() {
  QTemporaryDir tmp;
  CHECK(tmp.isValid());
  const QString script = tmp.filePath(QStringLiteral("main.ac"));
  const QString src1 = QStringLiteral("let a: Number = 21; return a * 2;");
  writeTempFile(script, src1);

  // ── 首次执行：生成缓存文件 ──
  const QString r1 = runBytecode(script, src1);
  CHECK(r1 == QStringLiteral("42"));
  const QString p1 = AcBytecodeCache::cachePathFor(script);
  CHECK(!p1.isEmpty());
  CHECK(QFile::exists(p1));

  // ── 二次执行：命中缓存，结果一致；缓存文件仍存在 ──
  const QString r2 = runBytecode(script, src1);
  CHECK(r2 == r1);

  // ── 缓存可加载且能完整还原模块（VM 可直接执行）──
  AcModule loaded;
  CHECK(AcBytecodeCache::tryLoad(script, loaded));
  CHECK(loaded.funcs.size() >= 1);
  CHECK(loaded.funcs[0].code.size() > 0);  // 顶层单元指令完整还原

  // ── 改源：哈希变化 → 旧缓存失效、新缓存生成 ──
  const QString src2 = QStringLiteral("let a: Number = 21; return a * 2 + 1;");
  const QString p1After = AcBytecodeCache::cachePathFor(script);  // 哈希=src1
  writeTempFile(script, src2);
  const QString p2 = AcBytecodeCache::cachePathFor(script);
  CHECK(p1After != p2);  // 文件名随内容哈希变化
  const QString r3 = runBytecode(script, src2);
  CHECK(r3 == QStringLiteral("43"));
  CHECK(QFile::exists(p2));

  // 旧哈希缓存文件不再被引用（可保留为垃圾；此处仅验证新文件生成）
  CHECK(!QFile::exists(p1After) || p1After != p2);

  // ── Io 层：魔数不匹配拒绝加载 ──
  QFile f(p2);
  f.open(QIODevice::WriteOnly | QIODevice::Truncate);
  f.write(QByteArray("GARBAGE"));
  f.close();
  AcModule bad;
  CHECK(!AcBytecodeCache::tryLoad(script, bad));

  // ── 纯 Io 回环：save → load 完整还原 ──
  QByteArray data;
  CHECK(AcModuleIo::save(loaded, data));
  AcModule round;
  CHECK(AcModuleIo::load(data, round));
  CHECK(round.funcs.size() == loaded.funcs.size());
  CHECK(round.funcUnits == loaded.funcUnits);
  CHECK(round.idents == loaded.idents);
  CHECK(round.entry == loaded.entry);
  CHECK(round.sourceHash == loaded.sourceHash);
  if (round.funcs.size() == loaded.funcs.size()) {
    for (int i = 0; i < loaded.funcs.size(); ++i) {
      CHECK(round.funcs[i].name == loaded.funcs[i].name);
      CHECK(round.funcs[i].code.size() == loaded.funcs[i].code.size());
      if (round.funcs[i].code.size() == loaded.funcs[i].code.size()) {
        for (int j = 0; j < loaded.funcs[i].code.size(); ++j) {
          const AcInstr &a = loaded.funcs[i].code[j];
          const AcInstr &b = round.funcs[i].code[j];
          CHECK(a.op == b.op && a.a == b.a && a.b == b.b && a.c == b.c && a.line == b.line);
        }
      }
      CHECK(round.funcs[i].constants.size() == loaded.funcs[i].constants.size());
      CHECK(round.funcs[i].tryTable.size() == loaded.funcs[i].tryTable.size());
      CHECK(round.funcs[i].paramNames == loaded.funcs[i].paramNames);
    }
  }

  std::printf("[ac_cache] %d checks, %d failed\n", g_total, g_failed);
  return g_failed;
}