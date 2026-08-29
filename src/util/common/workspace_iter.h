/**
 * @file workspace_iter.h
 * @brief 工作区文件遍历公共函数（查找 / 引用 / 后台诊断共用）
 *
 * 三个调用方此前各自用 QDirIterator 遍历工作区文件并过滤：
 * - SearchPanel：遍历 + shouldScanFile 过滤 + 逐行搜索
 * - ReferencePanel：遍历 + shouldScanFile 过滤 + 整文件引用扫描
 * - collectWorkspaceFiles：遍历 + isVerifiableFile 过滤
 * 收敛为统一的 forEachWorkspaceFile（遍历骨架 + 过滤谓词 + 访问回调）。
 */

#pragma once

#include <QDir>
#include <QDirIterator>
#include <QString>

#include <functional>

/// 遍历工作区（root 下所有文件），对每个文件调用 filter 判断是否参与，
/// 通过 filter 的文件调用 visitor。
/// @param followSymlinks 是否跟随符号链接（查找/引用面板用 true，诊断扫描用 false；
///                       默认 QDirIterator 不跟随符号链接）
/// @return 通过 filter 参与处理的文件数
inline int forEachWorkspaceFile(const QString &root, bool followSymlinks,
                                const std::function<bool(const QString &)> &filter,
                                const std::function<void(const QString &)> &visitor) {
  if (root.isEmpty() || !filter || !visitor) return 0;
  QDirIterator::IteratorFlags flags = QDirIterator::Subdirectories;
  if (followSymlinks) flags |= QDirIterator::FollowSymlinks;
  int count = 0;
  QDirIterator it(root, QDir::Files, flags);
  while (it.hasNext()) {
    const QString p = it.next();
    if (!filter(p)) continue;
    ++count;
    visitor(p);
  }
  return count;
}
