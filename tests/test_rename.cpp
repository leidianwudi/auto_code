/**
 * @file test_rename.cpp
 * @brief 语义引用收集测试 — 「查找所有引用 / 重命名」共用的跨文件语义（行为锁定）
 *
 * 覆盖（实现：WorkspaceIndex::findReferences，经 collectSymbolReferences 兼容层）：
 * - 顶层导出函数：声明位置（isDeclaration）+ 定义文件内调用
 * - import 别名追踪：b.ac 中 `import { greet as sayHi }` 后的 sayHi() 调用计入引用
 * - 作用域遮蔽：其他文件里同名局部变量不误报
 * - resolveDefinition：别名调用处跳转解析回源定义
 *
 * 说明：单文件源码级引用（AcSemanticService）已在 test_ac_semantic.cpp 覆盖，
 * 本文件专测文件系统级（import 链 / 别名 / 遮蔽）。
 */

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>

#include "src/engine/rename/symbol_rename.h"
#include "src/engine/semantic/workspace_index.h"

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

/// 工作区夹具：三个 .ac 文件（定义方 / 别名使用方 / 同名局部变量遮蔽方）
struct RenameFixture {
  QTemporaryDir dir;
  QString aPath;  // export function greet + 文件内调用
  QString bPath;  // import { greet as sayHi } + sayHi() 调用
  QString cPath;  // 局部变量 greet 遮蔽（不应命中）

  bool setup() {
    if (!dir.isValid()) return false;
    aPath = dir.path() + QStringLiteral("/a.ac");
    bPath = dir.path() + QStringLiteral("/b.ac");
    cPath = dir.path() + QStringLiteral("/c.ac");

    const QString aSrc = QStringLiteral(
        "export function greet(name: String): String {\n"
        "  return \"hi \" + name;\n"
        "}\n"
        "printLog(greet(\"a\"));\n");
    const QString bSrc = QStringLiteral(
        "import { greet as sayHi } from \"a.ac\";\n"
        "printLog(sayHi(\"b\"));\n");
    const QString cSrc = QStringLiteral(
        "function localScope(): String {\n"
        "  let greet: String = \"shadow\";\n"
        "  return greet;\n"
        "}\n"
        "printLog(localScope());\n");

    QFile fa(aPath), fb(bPath), fc(cPath);
    if (!fa.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    fa.write(aSrc.toUtf8());
    fa.close();
    if (!fb.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    fb.write(bSrc.toUtf8());
    fb.close();
    if (!fc.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    fc.write(cSrc.toUtf8());
    fc.close();
    return true;
  }
};

/// 引用列表中是否含指定文件的项
static bool hasRefInFile(const QVector<RenameRef> &refs, const QString &filePath) {
  for (const auto &r : refs) {
    if (r.filePath == filePath) return true;
  }
  return false;
}

static void testCrossFileReferences() {
  RenameFixture fx;
  CHECK(fx.setup());

  // 模块表构建（findReferences 只读索引，不自动重建）
  WorkspaceIndex::ins().rebuild(fx.dir.path());
  CHECK(WorkspaceIndex::ins().isReady());

  // 触发点：a.ac 第 1 行的 greet 声明（triggerColumn 实现侧忽略，给准确值更稳）
  const QVector<RenameRef> refs = collectSymbolReferences(
      fx.dir.path(), fx.aPath, 1, 16, QStringLiteral("greet"));

  // 声明位置必须收集到（isDeclaration 且在 a.ac）
  bool hasDecl = false;
  for (const auto &r : refs) {
    if (r.isDeclaration && r.filePath == fx.aPath) hasDecl = true;
  }
  CHECK(hasDecl);

  // a.ac 文件内调用 + b.ac 别名使用（import 子句与 sayHi() 调用）必须命中
  CHECK(hasRefInFile(refs, fx.aPath));
  CHECK(hasRefInFile(refs, fx.bPath));

  // 遮蔽：c.ac 的局部变量 greet 不属于本符号，不得误报
  CHECK(!hasRefInFile(refs, fx.cPath));
}

static void testResolveDefinitionThroughAlias() {
  RenameFixture fx;
  CHECK(fx.setup());
  WorkspaceIndex::ins().rebuild(fx.dir.path());

  // b.ac 第 2 行 sayHi() 调用处 → 应解析回 a.ac 的 greet 定义
  const SemanticSymbol sym = WorkspaceIndex::ins().resolveDefinition(
      fx.bPath, 2, 9, QStringLiteral("sayHi"));
  CHECK(!sym.key.isEmpty());
  CHECK(sym.name == QStringLiteral("greet"));
  CHECK(sym.filePath == fx.aPath);
  CHECK(sym.kind == QStringLiteral("function"));
}

int runRenameTests() {
  testCrossFileReferences();
  testResolveDefinitionThroughAlias();
  std::printf("[rename] %d checks, %d failed\n", g_total, g_failed);
  return g_failed;
}
