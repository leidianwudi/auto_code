#include "util_file.h"

#include <QDir>
#include <QFile>
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

QString UtilFile::readUtf8(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return {};
  return QString::fromUtf8(f.readAll());
}