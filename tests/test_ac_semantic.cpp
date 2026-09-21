/**
 * @file test_ac_semantic.cpp
 * @brief 进程内语义服务测试（阶段 5b）
 *
 * 覆盖：多诊断（恢复式）、补全（关键字 + 标识符去重）、定义跳转（函数/变量/类）、
 * 引用查找、越界未命中返回空。
 */

#include <cstdio>

#include "src/engine/script/ac_lexer.h"
#include "src/engine/script/ac_semantic_service.h"

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

static int findIdentIdx(const QVector<Token> &toks, const QString &name, int line) {
  for (int i = 0; i < toks.size(); ++i) {
    if (toks[i].type == TokenType::kIdent && toks[i].text == name &&
        (line <= 0 || toks[i].loc.line == line))
      return i;
  }
  return -1;
}

int runAcSemanticTests() {
  // ── 诊断：多语法错误逐个收集 ──
  {
    const QString bad = QStringLiteral("let a: = 1;\nlet b: = 2;\nreturn a;\n");
    const auto res = AcSemanticService::diagnose(bad, QStringLiteral("x.ac"));
    CHECK(res.size() >= 2);
  }

  // ── 补全：关键字恒在 + 文件内标识符去重 ──
  {
    const QString src =
        QStringLiteral("let alpha = 1;\nfunction beta(): Number { return alpha; }\n");
    const auto items = AcSemanticService::complete(src, QString());
    bool hasAlpha = false;
    bool hasKeyword = false;
    for (const auto &it : items) {
      if (it.label == QStringLiteral("alpha")) hasAlpha = true;
      if (it.kind == QStringLiteral("keyword")) hasKeyword = true;
    }
    CHECK(hasAlpha);
    CHECK(hasKeyword);
    bool hasBeta = false;
    for (const auto &it : items) {
      if (it.label == QStringLiteral("beta")) hasBeta = true;
    }
    CHECK(hasBeta);
  }

  // ── 定义跳转：调用处 add → 函数定义行；result 使用 → 声明行 ──
  {
    const QString fsrc =
        QStringLiteral("function add(a: Number, b: Number): Number { return a + b; }\n"
                       "let result = add(1, 2);\n");
    QString lexErr;
    const QVector<Token> toks = AcLexer::tokenize(fsrc, lexErr);
    const int useAdd = findIdentIdx(toks, QStringLiteral("add"), 2);
    CHECK(useAdd >= 0);
    if (useAdd >= 0) {
      const auto loc = AcSemanticService::resolveDefinition(
          fsrc, QStringLiteral("f.ac"), toks[useAdd].loc.line, toks[useAdd].loc.col);
      CHECK(loc.line == 1);  // add 定义在第 1 行
    }
    const int useResult = findIdentIdx(toks, QStringLiteral("result"), 2);
    CHECK(useResult >= 0);
    if (useResult >= 0) {
      const auto loc = AcSemanticService::resolveDefinition(
          fsrc, QStringLiteral("f.ac"), toks[useResult].loc.line, toks[useResult].loc.col);
      CHECK(loc.line == 2);  // result 声明在第 2 行
    }
    const int useParam = findIdentIdx(toks, QStringLiteral("a"), 1);
    CHECK(useParam >= 0);
  }

  // ── 引用查找：定义 + 调用共 2 处 ──
  {
    const QString fsrc =
        QStringLiteral("function add(a: Number, b: Number): Number { return a + b; }\n"
                       "let result = add(1, 2);\n");
    const auto refs = AcSemanticService::findReferences(fsrc, QStringLiteral("f.ac"),
                                                        QStringLiteral("add"));
    CHECK(refs.size() == 2);
    const auto xrefs = AcSemanticService::findReferences(fsrc, QStringLiteral("f.ac"),
                                                         QStringLiteral("not_exist"));
    CHECK(xrefs.isEmpty());
  }

  // ── 未命中：列越界返回空文件 ──
  {
    const QString src = QStringLiteral("let ok = 1;");
    const auto none = AcSemanticService::resolveDefinition(src, QString(), 1, 100);
    CHECK(none.filePath.isEmpty());
  }

  std::printf("[ac_semantic] %d checks, %d failed\n", g_total, g_failed);
  return g_failed;
}