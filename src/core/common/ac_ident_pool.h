/**
 * @file ac_ident_pool.h
 * @brief 标识符驻留池 — 标识符字符串一次性入池，换出稳定 32 位 id
 *
 * 编译器经典基础设施（对标 LLVM StringRef/StringMap 的驻留思想）：
 * - 同名标识符全生命周期只存一份字符串，id 即身份
 * - id 之间比较为整数比较 O(1)；id → 名字反查 O(1)
 * - 词法期驻留一次，Token/AST/符号表/IR 各阶段共享同一 id
 *
 * 当前接入点：AcLexer 对每个标识符 token 驻留（Token::ident）。
 * 后续演进（见 docs/architecture.md "标识符驻留"）：
 * - AST 的 name 字段批量切换为 AcIdent，符号表/类型检查/重命名以 id 为键
 * - 引入字节码/IR 时符号直接使用 id
 *
 * 线程安全：intern 可多线程并发调用（QMutex 保护）；id→名反查无锁安全
 * （QString 具名值语义，池只增不改）。
 */

#pragma once

#include <QHash>
#include <QMutex>
#include <QString>
#include <vector>

namespace accore {

/// 标识符 id（0 为无效值，便于零初始化判空）
using AcIdent = unsigned int;
inline constexpr AcIdent kInvalidIdent = 0;

class AcIdentPool {
public:
  /// 全局池（词法/解析/索引多阶段共享同一身份空间）
  static AcIdentPool &ins() {
    static AcIdentPool pool;
    return pool;
  }

  /// 驻留标识符：已存在返回原 id，否则创建新 id（1 起递增）
  AcIdent intern(const QString &name) {
    if (name.isEmpty()) return kInvalidIdent;
    QMutexLocker lock(&m_mutex);
    const auto it = m_ids.constFind(name);
    if (it != m_ids.constEnd()) return it.value();
    const AcIdent id = static_cast<AcIdent>(m_names.size() + 1);
    m_ids.insert(name, id);
    m_names.push_back(name);
    return id;
  }

  /// id → 名字（无效 id 返回空串）；池只增不改，无需加锁
  const QString &name(AcIdent id) const {
    static const QString kEmpty;
    if (id == kInvalidIdent || id > m_names.size()) return kEmpty;
    return m_names[id - 1];
  }

  /// 已驻留的标识符数量
  int size() const {
    QMutexLocker lock(&m_mutex);
    return static_cast<int>(m_names.size());
  }

private:
  AcIdentPool() = default;
  ~AcIdentPool() = default;
  AcIdentPool(const AcIdentPool &) = delete;
  AcIdentPool &operator=(const AcIdentPool &) = delete;

  mutable QMutex m_mutex;
  QHash<QString, AcIdent> m_ids;   ///< 名字 → id
  std::vector<QString> m_names;    ///< id → 名字（下标 = id-1）
};

}  // namespace accore
