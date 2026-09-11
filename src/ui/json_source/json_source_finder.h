/**
 * @file json_source_finder.h
 * @brief .jsonsource 文件查找辅助（header-only）
 *
 * 在以下范围内递归查找 *.jsonsource 文件，供 jsonvue 下拉框数据源选择复用：
 *   1. 当前编辑文件所在目录（baseDir，可为空）
 *   2. 项目资源目录 file/（PROJECT_SOURCE_DIR/file）
 * 结果去重后按路径排序返回。
 */

#pragma once

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStringList>

#include "src/engine/ac_language.h"
#include "src/util/common/code_constants.h"

/// 递归收集 dir 下所有 *.jsonsource 文件的绝对路径
inline void collectJsonsourceRecursive(const QString &dir, QStringList &out) {
  QDir d(dir);
  if (!d.exists()) return;
  const QFileInfoList files =
      d.entryInfoList(QStringList() << QStringLiteral("*.jsonsource"), QDir::Files);
  for (const QFileInfo &fi : files) out.append(QDir::cleanPath(fi.absoluteFilePath()));
  const QFileInfoList dirs = d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QFileInfo &di : dirs) collectJsonsourceRecursive(di.absoluteFilePath(), out);
}

/// 查找工作区中的所有 .jsonsource 文件（去重、排序）
/// @param baseDir 当前编辑文件所在目录（可为空）
inline QStringList findJsonsourceFiles(const QString &baseDir = QString()) {
  QStringList roots;
  if (!baseDir.isEmpty()) roots.append(baseDir);
  roots.append(QStringLiteral(PROJECT_SOURCE_DIR) + CodeConstants::Paths::fileDir());

  QStringList all;
  QSet<QString> seen;
  for (const QString &root : roots) {
    QStringList tmp;
    collectJsonsourceRecursive(root, tmp);
    for (const QString &p : tmp) {
      const QString clean = QDir::cleanPath(p);
      if (!seen.contains(clean)) {
        seen.insert(clean);
        all.append(clean);
      }
    }
  }
  all.sort(Qt::CaseInsensitive);
  return all;
}
