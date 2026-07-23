/**
 * @file ac_builtin_loader.cpp
 * @brief builtin.d.ac 声明文件加载器实现
 */

#include "ac_builtin_loader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include "../../util/common/path_resolver.h"
#include "../../util/common/util_file.h"
#include "../ac_language.h"
#include "ac_lexer.h"
#include "ac_parser.h"

QString AcBuiltinLoader::findBuiltinFile(const QString &scriptPath) {
  return PathResolver::findFile(scriptPath, QStringLiteral("builtin.d.ac"));
}

bool AcBuiltinLoader::loadDeclarations(const QString &scriptPath, QHash<QString, ClassDef> &classes,
                                       QHash<QString, MethodDef> &functions) {
  QString builtinPath = findBuiltinFile(scriptPath);
  if (builtinPath.isEmpty()) return false;

  QString source = UtilFile::readUtf8(builtinPath);
  if (source.trimmed().isEmpty()) return false;

  AcLexer lexer;
  AcParser parser;
  Block builtinAst;
  QSet<QString> builtinVars;
  QString lexError;
  QVector<Token> tokens = lexer.tokenize(source, lexError);
  if (!lexError.isEmpty()) return false;
  if (!parser.parse(tokens, builtinAst, builtinVars)) return false;

  for (const auto &stmt : builtinAst.stmts) {
    if (stmt.kind == Block::Stmt::kClassDef) {
      if (!classes.contains(stmt.classDef.name)) {
        classes.insert(stmt.classDef.name, stmt.classDef);
      }
    } else if (stmt.kind == Block::Stmt::kFuncDef) {
      if (!functions.contains(stmt.funcDef.name)) {
        functions.insert(stmt.funcDef.name, stmt.funcDef);
      }
    }
  }
  return true;
}

void AcBuiltinLoader::registerNativeClasses(QHash<QString, ClassDef> &classes) {
  for (const auto &name : AcClass::kAll) {
    if (!classes.contains(name)) {
      ClassDef nativeClass;
      nativeClass.name = name;
      nativeClass.isNative = true;
      classes.insert(name, nativeClass);
    }
  }
}

void AcBuiltinLoader::loadAll(const QString &scriptPath, QHash<QString, ClassDef> &classes,
                              QHash<QString, MethodDef> &functions) {
  loadDeclarations(scriptPath, classes, functions);
  registerNativeClasses(classes);
}
