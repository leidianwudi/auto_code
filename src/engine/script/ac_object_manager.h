/**
 * @file ac_object_manager.h
 * @brief 对象管理器 — 引用计数 + 自动析构
 *
 * 为 ac 脚本中 new 创建的实例对象提供类似 TS/JS 的自动释放能力：
 * - 局部变量离开作用域时自动 release
 * - 赋值给属性/数组时自动 retain
 * - 引用计数归零时标记为待析构，由 AcInterpreter 统一调用 __destruct__
 *
 * 不处理循环引用（ac 脚本无闭包，不存在循环引用场景）。
 */

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QString>
#include <QVector>

class AcObjectManager {
public:
  /// 引用计数归零的实例信息（由 AcInterpreter 在 release 后检查并执行析构）
  struct DestructInfo {
    QJsonObject instance;  ///< 实例数据
    QString className;     ///< 类名
  };

  static AcObjectManager &ins();

  /**
   * @brief 注册新实例（new 时调用）
   * @param instance 构造后的实例对象
   * @param className 类名
   * @return 带 __objId__ 的实例对象
   */
  QJsonObject registerInstance(const QJsonObject &instance, const QString &className);

  /**
   * @brief 引用计数 +1（赋值给变量/属性/数组时调用）
   * @param objId 实例唯一ID
   */
  void retain(const QString &objId);

  /**
   * @brief 引用计数 -1（变量离开作用域/被覆盖时调用）
   *
   * 引用计数归零时将实例加入待析构列表，不直接调用析构器。
   * @param objId 实例唯一ID
   * @return true 表示引用计数归零（需要析构），false 表示仍有引用
   */
  bool release(const QString &objId);

  /**
   * @brief 判断 QJsonValue 是否为受管理的实例对象
   */
  static bool isManagedInstance(const QJsonValue &val);

  /**
   * @brief 从 QJsonValue 中提取 objId
   */
  static QString getObjId(const QJsonValue &val);

  /**
   * @brief 取出所有待析构的实例（由 AcInterpreter 调用后执行 __destruct__）
   *
   * 调用后清空待析构列表。
   */
  QVector<DestructInfo> takePendingDestructs();

  // ── 标记-清扫（处理循环引用） ──

  /// @brief 清除所有标记
  void clearMarks();

  /// @brief 标记实例为"可达"
  void mark(const QString &objId);

  /// @brief 检查实例是否已被标记
  bool isMarked(const QString &objId) const;

  /// @brief 检查 objId 是否仍在管理器中
  bool contains(const QString &objId) const;

  /// @brief 获取实例数据（用于遍历属性）
  QJsonObject getObject(const QString &objId) const;

  /**
   * @brief 收集所有未标记的实例（循环引用垃圾）
   *
   * 从管理器中移除未标记的实例并返回，由 AcInterpreter 执行析构。
   * 不经过 pendingDestructs，直接返回列表。
   */
  QVector<DestructInfo> collectUnmarked();

  /**
   * @brief 获取实例数量
   */
  int objectCount() const { return m_objects.size(); }

  /**
   * @brief 清理所有实例（程序退出时调用）
   */
  void cleanup();

private:
  AcObjectManager() = default;
  ~AcObjectManager() = default;
  AcObjectManager(const AcObjectManager &) = delete;
  AcObjectManager &operator=(const AcObjectManager &) = delete;

  QHash<QString, int> m_refCount;            ///< objId → 引用计数
  QHash<QString, QJsonObject> m_objects;     ///< objId → 实例数据
  QHash<QString, QString> m_classNames;      ///< objId → 类名
  QVector<DestructInfo> m_pendingDestructs;  ///< 待析构列表
  QSet<QString> m_marked;                    ///< 标记-清扫的标记集合
};