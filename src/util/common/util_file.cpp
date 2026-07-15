#include "util_file.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

void UtilFile::showInExplorer(const QString &path) {
  if (path.isEmpty()) return;

  QFileInfo fi(path);
  if (!fi.exists()) return;

  QString absPath = fi.absoluteFilePath();

  if (fi.isDir()) {
    // 文件夹：直接打开
    QProcess::startDetached(QStringLiteral("explorer.exe"),
                            QStringList{QDir::toNativeSeparators(absPath)});
  } else {
    // 文件：打开所在文件夹并选中该文件
    // Windows 语法: explorer.exe /select,"绝对路径"
    QProcess::startDetached(
        QStringLiteral("explorer.exe"),
        QStringList{QStringLiteral("/select,") + QDir::toNativeSeparators(absPath)});
  }
}