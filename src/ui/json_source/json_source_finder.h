/**
 * @file json_source_finder.h
 * @brief .jsonsource / .jsonupload 配置文件查找辅助（header-only）
 *
 * 作用域规则（项目根判定见 PathResolver::findProjectRootUpward）：
 *   1. baseDir（含其祖先目录）所在项目根（含 project.acproj 标记文件）存在时，
 *      只递归收集该项目根下的对应后缀文件（jsonvue 与 jsonsource/jsonupload
 *      通常不在同一目录，必须从项目根向下收集）
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

/// 递归收集 dir 下所有指定后缀文件的绝对路径（suffix 形如 ".jsonsource"）
inline void collectBySuffixRecursive(const QString &dir, const QString &suffix,
                                     QStringList &out) {
  QDir d(dir);
  if (!d.exists()) return;
  const QFileInfoList files =
      d.entryInfoList(QStringList() << ("*" + suffix), QDir::Files);
  for (const QFileInfo &fi : files) out.append(QDir::cleanPath(fi.absoluteFilePath()));
  const QFileInfoList dirs = d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QFileInfo &di : dirs) collectBySuffixRecursive(di.absoluteFilePath(), suffix, out);
}

/// 按后缀查找工作区中的配置文件（去重、排序），jsonsource / jsonupload 共用实现
/// @param baseDir 当前编辑文件所在目录（可为空）
/// @param suffix 文件后缀（形如 ".jsonsource"）
inline QStringList findConfigFilesBySuffix(const QString &baseDir, const QString &suffix) {
  // 项目作用域：baseDir 所在项目根（含 project.acproj）存在时，只返回该项目根下的文件
  if (!baseDir.isEmpty()) {
    const QString projectRoot = PathResolver::findProjectRootUpward(baseDir);
    if (!projectRoot.isEmpty()) {
      QStringList all;
      collectBySuffixRecursive(projectRoot, suffix, all);
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
    collectBySuffixRecursive(root, suffix, tmp);
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

/// 查找工作区中的所有 .jsonsource 数据源文件（去重、排序）
inline QStringList findJsonsourceFiles(const QString &baseDir = QString()) {
  return findConfigFilesBySuffix(baseDir, QStringLiteral(".jsonsource"));
}

/// 查找工作区中的所有 .jsonupload 上传预设文件（去重、排序）
inline QStringList findJsonuploadFiles(const QString &baseDir = QString()) {
  return findConfigFilesBySuffix(baseDir, QStringLiteral(".jsonupload"));
}
