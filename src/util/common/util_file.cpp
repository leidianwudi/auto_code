#include "util_file.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>

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

bool UtilFile::writeUtf8(const QString &path, const QString &content, QString *error,
                         bool createParents) {
  if (error) error->clear();
  auto fail = [error](const QString &message) {
    if (error) *error = message;
    return false;
  };
  if (path.isEmpty()) return fail(QStringLiteral("文件路径不能为空"));
  if (createParents && !QDir().mkpath(QFileInfo(path).absolutePath()))
    return fail(QStringLiteral("无法创建父目录: %1").arg(path));

  // 临时文件完整写入后再提交，禁止退回直接覆盖，避免失败时截断已有内容。
  QSaveFile file(path);
  file.setDirectWriteFallback(false);
  if (!file.open(QIODevice::WriteOnly)) return fail(file.errorString());
  const QByteArray bytes = content.toUtf8();
  if (file.write(bytes) != bytes.size()) {
    const QString message = file.errorString();
    file.cancelWriting();
    return fail(message);
  }
  if (!file.commit()) return fail(file.errorString());
  return true;
}
