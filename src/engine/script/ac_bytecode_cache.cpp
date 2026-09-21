/**
 * @file ac_bytecode_cache.cpp
 * @brief 预编译缓存实现 — 失效键 = 入口 + import + builtin + 版本
 */

#include "ac_bytecode_cache.h"

#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

#include "ac_builtin_loader.h"
#include "ac_module_io.h"

QString AcBytecodeCache::sourceHashFor(const QString &scriptFile) {
  QFile f(scriptFile);
  if (!f.open(QIODevice::ReadOnly)) return QString();
  const QByteArray data = f.readAll();
  QByteArray seed = data;
  seed.append(char(0));
  seed.append(QByteArray::number(engineCacheVersion()));
  return QString::fromLatin1(QCryptographicHash::hash(seed, QCryptographicHash::Sha1).toHex());
}

QString AcBytecodeCache::invalidationHashFor(const QString &scriptFile,
                                             const QStringList &importFiles) {
  QFile f(scriptFile);
  if (!f.open(QIODevice::ReadOnly)) return QString();
  QByteArray seed = f.readAll();
  seed.append(char(0));
  seed.append(QByteArray::number(engineCacheVersion()));

  // import 文件：去重 + 稳定排序，路径名 + 内容（缺失显式封存，文件出现时也会失效）
  QStringList paths = importFiles;
  paths.removeDuplicates();
  paths.sort();
  for (const auto &p : paths) {
    seed.append('#');  // 分隔符
    seed.append(QFileInfo(p).fileName().toUtf8());
    seed.append(':');
    QFile q(p);
    if (q.open(QIODevice::ReadOnly)) {
      seed.append(q.readAll());
    } else {
      seed.append("<missing>");  // 上一轮存在本轮缺失 → 内容变化同样失效
    }
  }

  // builtin.d.ac：内置声明文件变化同样影响编译结果
  const QString builtin = AcBuiltinLoader::findBuiltinFile(scriptFile);
  if (!builtin.isEmpty()) {
    QFile b(builtin);
    if (b.open(QIODevice::ReadOnly)) {
      seed.append('!');
      seed.append(QFileInfo(builtin).fileName().toUtf8());
      seed.append(':');
      seed.append(b.readAll());
    }
  }
  return QString::fromLatin1(QCryptographicHash::hash(seed, QCryptographicHash::Sha1).toHex());
}

QString AcBytecodeCache::cachePathFor(const QString &scriptFile,
                                      const QStringList &importFiles) {
  const QFileInfo fi(scriptFile);
  if (!fi.exists()) return QString();
  const QString hash = invalidationHashFor(scriptFile, importFiles);
  if (hash.isEmpty()) return QString();
  return QStringLiteral("%1/.ac_cache/%2.%3.acb")
      .arg(fi.absolutePath(), fi.completeBaseName(), hash.left(8));
}

bool AcBytecodeCache::tryLoad(const QString &scriptFile, const QStringList &importFiles,
                              AcModule &module) {
  const QString path = cachePathFor(scriptFile, importFiles);
  if (path.isEmpty()) return false;
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return false;
  const QByteArray data = f.readAll();
  if (!AcModuleIo::load(data, module)) return false;
  // 严格失效校验：模块记录的失效键必须等于当前失效键
  const QString fresh = invalidationHashFor(scriptFile, importFiles);
  if (fresh.isEmpty() || module.sourceHash != fresh) return false;
  return true;
}

bool AcBytecodeCache::trySave(const QString &scriptFile, const QStringList &importFiles,
                              const AcModule &module) {
  const QFileInfo fi(scriptFile);
  if (!fi.exists()) return false;
  const QString dir = QStringLiteral("%1/.ac_cache").arg(fi.absolutePath());
  if (!QDir().mkpath(dir)) return false;
  const QString path = cachePathFor(scriptFile, importFiles);
  if (path.isEmpty()) return false;
  QByteArray data;
  if (!AcModuleIo::save(module, data)) return false;
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
  if (f.write(data) != data.size() || !f.flush()) return false;

  // 清理同入口名的旧失效键缓存文件（<basename>.<旧哈希8>.acb），只留当前命中文件
  const QString base = fi.completeBaseName() + QLatin1Char('.');
  const QString curName = QFileInfo(path).fileName();
  const QDir cacheDir(dir);
  const QStringList olds = cacheDir.entryList({base + QStringLiteral("*.acb")}, QDir::Files);
  for (const auto &name : olds) {
    if (name != curName) QFile::remove(cacheDir.absoluteFilePath(name));
  }
  return true;
}