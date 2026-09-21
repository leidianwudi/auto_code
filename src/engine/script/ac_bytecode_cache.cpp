/**
 * @file ac_bytecode_cache.cpp
 * @brief 预编译缓存实现 — 内容哈希 + 版本失效 + 落盘
 */

#include "ac_bytecode_cache.h"

#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

#include "ac_module_io.h"

QString AcBytecodeCache::sourceHashFor(const QString &scriptFile) {
  QFile f(scriptFile);
  if (!f.open(QIODevice::ReadOnly)) return QString();
  const QByteArray data = f.readAll();
  // 内容 + 引擎缓存版本 参与哈希：语义版本变化时同一源码也视为失效
  QByteArray seed = data;
  seed.append(char(0));
  seed.append(QByteArray::number(engineCacheVersion()));
  return QString::fromLatin1(QCryptographicHash::hash(seed, QCryptographicHash::Sha1).toHex());
}

QString AcBytecodeCache::cachePathFor(const QString &scriptFile) {
  const QFileInfo fi(scriptFile);
  if (!fi.exists()) return QString();
  const QString hash = sourceHashFor(scriptFile);
  if (hash.isEmpty()) return QString();
  return QStringLiteral("%1/.ac_cache/%2.%3.acb")
      .arg(fi.absolutePath(), fi.completeBaseName(), hash.left(8));
}

bool AcBytecodeCache::tryLoad(const QString &scriptFile, AcModule &module) {
  const QString path = cachePathFor(scriptFile);
  if (path.isEmpty()) return false;
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return false;
  const QByteArray data = f.readAll();
  if (!AcModuleIo::load(data, module)) return false;
  // 严格失效校验：模块记录的源哈希必须等于当前内容哈希
  const QString fresh = sourceHashFor(scriptFile);
  if (fresh.isEmpty() || module.sourceHash != fresh) return false;
  return true;
}

bool AcBytecodeCache::trySave(const QString &scriptFile, const AcModule &module) {
  const QFileInfo fi(scriptFile);
  if (!fi.exists()) return false;
  const QString dir = QStringLiteral("%1/.ac_cache").arg(fi.absolutePath());
  if (!QDir().mkpath(dir)) return false;
  const QString path = cachePathFor(scriptFile);
  if (path.isEmpty()) return false;
  QByteArray data;
  if (!AcModuleIo::save(module, data)) return false;
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
  return f.write(data) == data.size() && f.flush();
}