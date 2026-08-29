/**
 * @file pending_change_store.h
 * @brief 未打开文件的缓冲修改存储（header-only）
 *
 * 重命名等操作对未打开文件：先存缓冲（不写盘），树目录标黄，退出时提示保存（VSCode 行为）。
 * 本类集中管理这份数据：写入、读取、清除、重命名/移动重键、删除清理、快照落盘。
 * 树目录黄色标记与保存按钮状态由 MainDevMgr 负责（本类不依赖 UI）。
 */

#pragma once

#include <QDir>
#include <QHash>
#include <QString>
#include <QStringList>
#include <utility>

/// 未打开文件缓冲修改的存储与路径维护
class PendingChangeStore {
public:
  bool isEmpty() const { return m_pending.isEmpty(); }
  int size() const { return m_pending.size(); }
  bool contains(const QString &filePath) const { return m_pending.contains(filePath); }
  QString value(const QString &filePath) const { return m_pending.value(filePath); }

  /// 写入/更新某文件的缓冲内容
  void set(const QString &filePath, const QString &content) { m_pending.insert(filePath, content); }
  /// 清除某文件（空路径忽略）
  void clear(const QString &filePath) {
    if (!filePath.isEmpty()) m_pending.remove(filePath);
  }
  /// 全部清空
  void clearAll() { m_pending.clear(); }
  /// 当前所有 (路径, 内容) 快照（如需在迭代中清空，先拷贝一份）
  QHash<QString, QString> snapshot() const { return m_pending; }
  /// 当前所有路径
  QStringList filePaths() const { return m_pending.keys(); }

  /// 文件/文件夹重命名或移动后，将缓冲中的旧路径批量改为新路径：
  /// 精确匹配的键 → newPath；子路径（oldPath/...）→ newPath/对应后缀。
  /// 返回发生变化的旧路径 → 新路径映射（为空表示无匹配，调用方据此决定是否更新树黄色标记）。
  QHash<QString, QString> rekeyUnder(const QString &oldPath, const QString &newPath) {
    const QString cleanOld = QDir::cleanPath(oldPath);
    const QString prefix = cleanOld + QLatin1Char('/');
    const QString cleanNew = QDir::cleanPath(newPath);
    // 先收集 旧键 → (新键, 内容)，避免边迭代边修改
    QHash<QString, std::pair<QString, QString>> rekeyed;
    for (auto it = m_pending.cbegin(); it != m_pending.cend(); ++it) {
      const QString key = QDir::cleanPath(it.key());
      if (key == cleanOld) {
        rekeyed.insert(it.key(), {cleanNew, it.value()});
      } else if (key.startsWith(prefix)) {
        rekeyed.insert(it.key(), {cleanNew + key.mid(cleanOld.length()), it.value()});
      }
    }
    if (rekeyed.isEmpty()) return {};

    QHash<QString, QString> mapping;  // 旧键 → 新键（供调用方为新路径补树黄色）
    for (auto it = rekeyed.cbegin(); it != rekeyed.cend(); ++it) {
      m_pending.remove(it.value().first);  // 先移除目标新键可能残留的旧值
      mapping.insert(it.key(), it.value().first);
    }
    // 删除所有旧键（精确 + 前缀）
    for (auto it = m_pending.begin(); it != m_pending.end();) {
      const QString key = QDir::cleanPath(it.key());
      if (key == cleanOld || key.startsWith(prefix))
        it = m_pending.erase(it);
      else
        ++it;
    }
    // 写入新键
    for (auto it = rekeyed.cbegin(); it != rekeyed.cend(); ++it)
      m_pending.insert(it.value().first, it.value().second);
    return mapping;
  }

  /// 在 path（isDir=true 时含其子路径）下的所有缓冲路径（删除确认统计用）
  QStringList pathsUnder(const QString &path, bool isDir) const {
    const QString clean = QDir::cleanPath(path);
    const QString prefix = clean + QLatin1Char('/');
    QStringList out;
    for (auto it = m_pending.cbegin(); it != m_pending.cend(); ++it) {
      const QString key = QDir::cleanPath(it.key());
      if (isDir ? key.startsWith(prefix) : key == clean) out.append(it.key());
    }
    return out;
  }

  /// 移除 path（isDir=true 时含其子路径）下的所有缓冲（删除后清理）
  void removeUnder(const QString &path, bool isDir) {
    const QString clean = QDir::cleanPath(path);
    const QString prefix = clean + QLatin1Char('/');
    for (auto it = m_pending.begin(); it != m_pending.end();) {
      const QString key = QDir::cleanPath(it.key());
      if (isDir ? key.startsWith(prefix) : key == clean)
        it = m_pending.erase(it);
      else
        ++it;
    }
  }

private:
  QHash<QString, QString> m_pending;  ///< 文件路径 → 缓冲内容
};
