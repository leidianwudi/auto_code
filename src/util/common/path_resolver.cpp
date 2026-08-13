/**
 * @file path_resolver.cpp
 * @brief 文件路径解析工具实现
 */

#include "path_resolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSet>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

#include "src/util/common/code_constants.h"

QStringList PathResolver::fileSearchPaths(const QString &scriptPath) {
  QStringList paths;
  QSet<QString> seen;

  auto addPath = [&](const QString &p) {
    if (!p.isEmpty() && !seen.contains(p)) {
      seen.insert(p);
      paths << p;
    }
  };

  if (!scriptPath.isEmpty()) {
    QString dir = QFileInfo(scriptPath).dir().absolutePath();
    addPath(dir);
    addPath(dir + QStringLiteral("/.."));
  }

  // 优先使用 PROJECT_SOURCE_DIR/file（开发期源码目录）
  addPath(QStringLiteral(PROJECT_SOURCE_DIR) +
          QString::fromUtf8(CodeConstants::Paths::kFileDirName));
  addPath(QCoreApplication::applicationDirPath() +
          QString::fromUtf8(CodeConstants::Paths::kFileDirName));
  addPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../../file"));
  addPath(QDir::currentPath() + QString::fromUtf8(CodeConstants::Paths::kFileDirName));

  return paths;
}

QString PathResolver::findFile(const QString &scriptPath, const QString &fileName) {
  for (const auto &dir : fileSearchPaths(scriptPath)) {
    QString candidate = QDir(dir).filePath(fileName);
    if (QFile::exists(candidate)) {
      return candidate;
    }
  }
  return QString();
}

QString PathResolver::resolveImportPath(const QString &importPath, const QString &scriptPath) {
  QString absPath;
  QFileInfo fi(importPath);
  if (fi.isRelative()) {
    QDir dir = QFileInfo(scriptPath).dir();
    absPath = dir.filePath(importPath);
  } else {
    absPath = importPath;
  }
  return QDir::cleanPath(absPath);
}
