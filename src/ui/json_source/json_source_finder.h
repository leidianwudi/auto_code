/**
 * @file json_source_finder.h
 * @brief .jsonsource 文件查找辅助（header-only）
 *
 * 作用域规则（项目根判定见 PathResolver::findProjectRootUpward）：
 *   1. baseDir（含其祖先目录）所在项目根（含 project.acproj 标记文件）存在时，
 *      只递归收集该项目根下的 *.jsonsource（jsonvue 与 jsonsource 通常不在同一
 *      目录，必须从项目根向下收集）
 *   2. 无项目根（如 template/ 模板目录、未设项目的文件夹）→ 回退旧行为：
 *      baseDir + 项目资源目录 file/ 全局递归
 * 结果按路径排序返回。
 */

#pragma once

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStringList>

#include "src/engine/ac_language.h"
#include "src/util/common/code_constants.h"
#include "src/util/common/path_resolver.h"

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
  // 项目作用域：baseDir 所在项目根（含 project.acproj）存在时，只返回该项目根下的数据源
  if (!baseDir.isEmpty()) {
    const QString projectRoot = PathResolver::findProjectRootUpward(baseDir);
    if (!projectRoot.isEmpty()) {
      QStringList all;
      collectJsonsourceRecursive(projectRoot, all);
      all.sort(Qt::CaseInsensitive);
      return all;
    }
  }

  // 无项目根（如 template/ 模板目录）→ 回退旧行为：baseDir + 全局 file/
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
