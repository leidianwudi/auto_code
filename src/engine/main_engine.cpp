#include "main_engine.h"

#include "script_parser.h"

#include <QFile>
#include <QFileInfo>

MainEngine &MainEngine::ins() {
  static MainEngine instance;
  return instance;
}

QString MainEngine::readFileUtf8(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return {};
  return QString::fromUtf8(f.readAll());
}

QString MainEngine::execute(const QString &mainAcPath) {
  QString source = readFileUtf8(mainAcPath);
  if (source.isEmpty())
    return QStringLiteral("failed to read main.ac: %1").arg(mainAcPath);

  QFileInfo fi(mainAcPath);
  QString dir = fi.absolutePath();

  ScriptParser parser;
  parser.setScriptDir(dir);

  if (!parser.parse(source))
    return QStringLiteral("parse error: %1").arg(parser.error());

  QJsonValue result = parser.execute();
  if (!parser.error().isEmpty())
    return parser.error();

  return {};
}