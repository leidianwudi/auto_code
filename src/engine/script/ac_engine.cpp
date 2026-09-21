/**
 * @file ac_engine.cpp
 * @brief .ac 脚本引擎实现文件
 */

#include "ac_engine.h"

#include <QFile>
#include <QFileInfo>

#include "ac_executor.h"

QString AcEngine::readFileUtf8(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return {};
  return QString::fromUtf8(f.readAll());
}

QString AcEngine::execute(const QString &acFilePath) {
  resetCancel();
  QString source = readFileUtf8(acFilePath);
  if (source.isEmpty()) return QStringLiteral("failed to read file: %1").arg(acFilePath);

  QFileInfo fi(acFilePath);
  QString dir = fi.absolutePath();

  AcExecutor executor;
  executor.setScriptDir(dir);
  executor.setScriptFile(fi.absoluteFilePath());
  executor.setCancelFlag(&m_cancelRequested);
  executor.setDebugger(m_debugger);
  executor.setExecMode(m_execMode);
  if (!m_rootDir.isEmpty()) executor.setRootDir(m_rootDir);
  if (m_logCallback) executor.setLogCallback(m_logCallback);
#ifdef AC_DEBUG
  qDebug() << "[AcEngine::execute] m_logCallback is null:" << !m_logCallback;
#endif

  if (!executor.parse(source)) return QStringLiteral("parse error: %1").arg(executor.error());
#ifdef AC_DEBUG
  qDebug() << "[AcEngine::execute] parse succeeded";
#endif

  // 执行（返回值当前无消费方；引擎语义以 error() / generatedFiles() 出口为准）
  executor.execute();
  m_generatedFiles = executor.generatedFiles();
  if (!executor.error().isEmpty()) return executor.error();

  return {};
}