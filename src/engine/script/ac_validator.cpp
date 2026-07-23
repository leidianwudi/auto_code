/**
 * @file ac_validator.cpp
 * @brief AC 脚本验证器实现
 */

#include "ac_validator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

#include "../ac_language.h"

QVector<ValidationResult> AcValidator::validate(const QString &source) {
  QVector<ValidationResult> results;

  if (source.trimmed().isEmpty()) return results;

  m_sourceLines = source.split(QLatin1Char('\n'));

  // 清空类和函数表（在 import 解析之前清空，以便导入文件的类能被收集）
  m_classes.clear();
  m_functions.clear();

  // ── 步骤 1：词法分析 ──
  QString lexError;
  QVector<Token> tokens = m_lexer.tokenize(source, lexError);
  if (!lexError.isEmpty()) {
    int line = 1;
    // 尝试从错误信息中提取行号
    QRegularExpression re(QStringLiteral("at line (\\d+)"));
    auto match = re.match(lexError);
    if (match.hasMatch()) line = match.captured(1).toInt();
    results.append(ValidationResult::atLine(line, lexError));
    return results;
  }

  // ── 步骤 2：语法分析 ──
  m_declaredVars.clear();
  m_program = Block();
  if (!m_parser.parse(tokens, m_program, m_declaredVars)) {
    QString parseErrMsg = m_parser.error();
    int line = 1;
    QRegularExpression re(QStringLiteral("at line (\\d+)"));
    auto match = re.match(parseErrMsg);
    if (match.hasMatch()) line = match.captured(1).toInt();
    results.append(ValidationResult::atLine(line, parseErrMsg));
    return results;
  }

  // ── 步骤 2.5：构建符号表（先加载依赖，再收集当前文件，确保类型推断可用）──
  m_symbolTable.clear();
  m_symbolTable.setFilePath(m_filePath);

  // 步骤 2.5a：加载内置函数声明文件 (builtin.d.ac)
  // 必须在当前文件符号收集之前加载，以便类型推断引擎能查到内置函数返回类型
  m_visitedFiles.clear();
  if (!m_filePath.isEmpty()) {
    m_visitedFiles.insert(QFileInfo(m_filePath).canonicalFilePath());
  }
  {
    QStringList searchPaths;
    if (!m_filePath.isEmpty()) {
      searchPaths << QFileInfo(m_filePath).dir().absolutePath();
      searchPaths << QFileInfo(m_filePath).dir().absolutePath() + QStringLiteral("/..");
    }
    searchPaths << QCoreApplication::applicationDirPath() + QStringLiteral("/file");
    searchPaths << QStringLiteral(PROJECT_SOURCE_DIR) + QStringLiteral("/file");

    for (const auto &dir : searchPaths) {
      QString builtinPath = QDir(dir).filePath(QStringLiteral("builtin.d.ac"));
      if (QFile::exists(builtinPath)) {
        m_visitedFiles.remove(QFileInfo(builtinPath).canonicalFilePath());
        QStringList allNames;
        collectSymbolsFromFile(builtinPath, allNames);
        break;
      }
    }
  }

  // 步骤 2.5b：解析 import 语句，收集跨文件符号
  if (!m_filePath.isEmpty()) {
    resolveImportedSymbols(m_program);
  }

  // 步骤 2.5c：收集当前文件符号（此时内置函数和 import 符号已在符号表中，类型推断可正常工作）
  for (const auto &stmt : m_program.stmts) {
    m_symbolTable.collectStmt(stmt);
  }

  // ── 步骤 3：未声明标识符检查 ──
  QStringList undeclaredErrors;
  m_undeclaredValidator.validate(m_program, m_declaredVars, undeclaredErrors);
  for (const QString &err : undeclaredErrors) {
    if (!err.isEmpty()) results.append(parseError(err));
  }

  // ── 步骤 4：静态类型检查 ──
  // 注意：m_classes/m_functions 已在 validate() 开头清空，并在 import 解析时收集了导入文件的类
  collectClassesAndFunctions(m_program);

  // 注册 C++ 原生类
  for (const auto &name : AcClass::kAll) {
    if (!m_classes.contains(name)) {
      ClassDef nativeClass;
      nativeClass.name = name;
      nativeClass.isNative = true;
      m_classes.insert(name, nativeClass);
    }
  }

  QStringList typeErrors;
  m_typeChecker.setFilePath(m_filePath);
  m_typeChecker.check(m_program, m_declaredVars, m_classes, m_functions, typeErrors);
  for (const QString &err : typeErrors) {
    if (!err.isEmpty()) results.append(parseError(err));
  }

  // 按行号排序
  std::sort(results.begin(), results.end(),
            [](const ValidationResult &a, const ValidationResult &b) {
              if (a.line != b.line) return a.line < b.line;
              return a.column < b.column;
            });

  return results;
}

// ═════════════════════════════════════════════════════════════════════════════
//  辅助方法
// ═════════════════════════════════════════════════════════════════════════════

void AcValidator::collectClassesAndFunctions(const Block &program) {
  for (const auto &stmt : program.stmts) {
    if (stmt.kind == Block::Stmt::kClassDef) {
      m_classes.insert(stmt.classDef.name, stmt.classDef);
    } else if (stmt.kind == Block::Stmt::kFuncDef) {
      m_functions.insert(stmt.funcDef.name, stmt.funcDef);
    } else if (stmt.kind == Block::Stmt::kImport) {
      // import 的符号名在编辑器校验时视为已知类/函数，避免误报 unknown class
      for (const auto &name : stmt.importStmt.names) {
        if (!m_classes.contains(name)) {
          ClassDef importedClass;
          importedClass.name = name;
          importedClass.isNative = true;
          m_classes.insert(name, importedClass);
        }
      }
    }
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  跨文件 import 符号解析
// ═════════════════════════════════════════════════════════════════════════════

void AcValidator::resolveImportedSymbols(const Block &program) {
  for (const auto &stmt : program.stmts) {
    if (stmt.kind == Block::Stmt::kImport) {
      const auto &imp = stmt.importStmt;
      if (imp.filePath.isEmpty()) continue;

      // 将 import 路径解析为绝对路径
      QString absPath;
      QFileInfo fi(imp.filePath);
      // 相对路径：基于当前文件目录解析
      if (fi.isRelative()) {
        QDir dir = QFileInfo(m_filePath).dir();
        absPath = dir.filePath(imp.filePath);
      } else {
        absPath = imp.filePath;
      }
      absPath = QDir::cleanPath(absPath);

      // 读取目标文件并收集符号
      collectSymbolsFromFile(absPath, imp.names);
    }
  }
}

void AcValidator::collectSymbolsFromFile(const QString &filePath, const QStringList &importNames) {
  // 防止循环 import
  QString canonical = QFileInfo(filePath).canonicalFilePath();
  if (canonical.isEmpty()) canonical = filePath;
  if (m_visitedFiles.contains(canonical)) return;
  m_visitedFiles.insert(canonical);

  // 读取文件内容
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

  QTextStream stream(&file);
  QString source = stream.readAll();
  file.close();

  if (source.trimmed().isEmpty()) return;

  // 词法分析
  QString lexError;
  QVector<Token> tokens = m_lexer.tokenize(source, lexError);
  if (!lexError.isEmpty()) return;

  // 语法分析
  QSet<QString> declaredVars;
  Block program;
  if (!m_parser.parse(tokens, program, declaredVars)) return;

  // 构建目标文件的符号表
  AcSymbolTable importedTable;
  importedTable.setFilePath(filePath);
  for (const auto &stmt : program.stmts) {
    importedTable.collectStmt(stmt);
  }

  // 将 import 列表中指定的符号合并到主符号表
  m_symbolTable.mergeFrom(importedTable, importNames);

  // 收集导入文件的类定义到 m_classes（供类型检查器使用）
  for (const auto &stmt : program.stmts) {
    if (stmt.kind == Block::Stmt::kClassDef) {
      if (!m_classes.contains(stmt.classDef.name)) {
        m_classes.insert(stmt.classDef.name, stmt.classDef);
      }
    } else if (stmt.kind == Block::Stmt::kFuncDef) {
      if (!m_functions.contains(stmt.funcDef.name)) {
        m_functions.insert(stmt.funcDef.name, stmt.funcDef);
      }
    }
  }

  // 递归解析目标文件的 import（支持间接 import）
  QString savedFilePath = m_filePath;
  m_filePath = filePath;
  resolveImportedSymbols(program);
  m_filePath = savedFilePath;
}

int AcValidator::extractLine(const QString &msg) const {
  QRegularExpression re(QStringLiteral("at line (\\d+)"));
  auto match = re.match(msg);
  if (match.hasMatch()) return match.captured(1).toInt();
  return 0;
}

ValidationResult AcValidator::parseError(const QString &msg) const {
  int line = extractLine(msg);

  // 尝试提取错误类型前缀
  QString cleanMsg = msg;
  // 去掉 "type error: " 或 "undefined variable " 等前缀中可能附加的行号信息
  // 如果行号已提取，从消息中去除行号后缀
  if (line > 0) {
    QRegularExpression re(QStringLiteral(" at line \\d+$"));
    cleanMsg = cleanMsg.remove(re);
  }

  return ValidationResult::atLine(line, cleanMsg);
}