#include "engine_ac.h"

#include "script_parser.h"

#include <QFile>
#include <QFileInfo>

EngineAc &EngineAc::ins() {
  static EngineAc instance;
  return instance;
}

QString EngineAc::readFileUtf8(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return {};
  return QString::fromUtf8(f.readAll());
}

QString EngineAc::execute(const QString &mainAcPath) {
  QString source = readFileUtf8(mainAcPath);
  if (source.isEmpty())
    return QStringLiteral("failed to read main.ac: %1").arg(mainAcPath);

  QFileInfo fi(mainAcPath);
  QString dir = fi.absolutePath();

  ScriptParser parser;
  parser.setScriptDir(dir);
  if (!m_rootDir.isEmpty())
    parser.setRootDir(m_rootDir);
  if (m_logCallback)
    parser.setLogCallback(m_logCallback);

  if (!parser.parse(source))
    return QStringLiteral("parse error: %1").arg(parser.error());

  QJsonValue result = parser.execute();
  m_generatedFiles = parser.generatedFiles();
  if (!parser.error().isEmpty())
    return parser.error();

  return {};
}