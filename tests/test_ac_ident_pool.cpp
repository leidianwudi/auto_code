/**
 * @file test_ac_ident_pool.cpp
 * @brief 标识符驻留池单元测试（纯 QtCore，无 GUI）
 *
 * 覆盖：
 *  - 驻留幂等：同名标识符多次 intern 返回同一 id
 *  - id 唯一性：不同名字获得不同 id；id → 名字反查正确
 *  - 无效值：空串/0 号 id 的语义
 *  - 反查安全：越界 id 不崩溃，返回空串
 *  - 词法集成：AcLexer 对标识符 token 填充 identId
 */

#include <QSet>
#include <cstdio>

#include "src/core/common/ac_ident_pool.h"
#include "src/engine/script/ac_lexer.h"

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

/// 驻留幂等 + 唯一性 + 反查
static void testInternBasics() {
  accore::AcIdentPool &pool = accore::AcIdentPool::ins();

  const accore::AcIdent a1 = pool.intern(QStringLiteral("userName"));
  const accore::AcIdent a2 = pool.intern(QStringLiteral("userName"));
  CHECK(a1 != accore::kInvalidIdent);
  CHECK(a1 == a2);  // 驻留幂等：同一名字同一 id

  const accore::AcIdent b = pool.intern(QStringLiteral("orderList"));
  CHECK(b != a1);  // 不同名字不同 id

  CHECK(pool.name(a1) == QStringLiteral("userName"));
  CHECK(pool.name(b) == QStringLiteral("orderList"));
}

/// 无效值语义：空串不驻留；0 号 id 反查为空；越界 id 安全
static void testInvalidAndBounds() {
  accore::AcIdentPool &pool = accore::AcIdentPool::ins();

  CHECK(pool.intern(QString()) == accore::kInvalidIdent);  // 空串不进池
  CHECK(pool.name(accore::kInvalidIdent).isEmpty());       // 0 号 id 无名字

  const accore::AcIdent valid = pool.intern(QStringLiteral("z_limit_probe"));
  CHECK(pool.name(valid + 1000).isEmpty());  // 越界 id 返回空串，不崩溃
}

/// 词法集成：标识符 token 带驻留 id，同名 token id 相同
static void testLexerIntegration() {
  QString err;
  const QVector<Token> tokens = AcLexer::tokenize(
      QStringLiteral("let alpha = 1; let beta = alpha + alpha;"), err);
  CHECK(err.isEmpty());
  CHECK(!tokens.isEmpty());
  if (tokens.isEmpty()) return;

  accore::AcIdentPool &pool = accore::AcIdentPool::ins();
  QHash<QString, accore::AcIdent> seen;
  for (const Token &t : tokens) {
    if (t.type != TokenType::kIdent) {
      CHECK(t.identId == accore::kInvalidIdent);  // 非标识符无 id
      continue;
    }
    CHECK(t.identId != accore::kInvalidIdent);
    CHECK(pool.name(t.identId) == t.text);  // id 反查等于原文
    const auto it = seen.constFind(t.text);
    if (it != seen.constEnd()) {
      CHECK(it.value() == t.identId);  // 同名 token 同 id
    } else {
      seen.insert(t.text, t.identId);
    }
  }
  CHECK(seen.contains(QStringLiteral("alpha")));
  CHECK(seen.contains(QStringLiteral("beta")));
}

int runAcIdentPoolTests() {
  testInternBasics();
  testInvalidAndBounds();
  testLexerIntegration();
  std::printf("[ac_ident_pool] %d checks, %d failed\n", g_total, g_failed);
  return g_failed;
}
