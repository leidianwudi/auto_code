/**
 * @file ac_validator.cpp
 * @brief AC 脚本验证器实现
 */

#include "ac_validator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

#include "../../util/common/path_resolver.h"
#include "../../util/common/util_file.h"
#include "../ac_language.h"
#include "ac_builtin_loader.h"
#include "ac_lexer.h"
#include "ac_parser.h"

QVector<ValidationResult> AcValidator::validate(const QString &source) {
  QVector<ValidationResult> results;

  if (source.trimmed().isEmpty()) return results;

  m_sourceLines = source.split(QLatin1Char('\n'));

  // 清空类和函数表（在 import 解析之前清空，以便导入文件的类能被收集）
  m_classes.clear();
  m_functions.clear();
  m_importErrors.clear();

  // ── 步骤 1+2：词法+语法分析 + AST 构建 ──
  m_declaredVars.clear();
  m_program = Block();
  QString parseErrMsg;
  if (!parseSource(source, m_program, m_declaredVars, parseErrMsg)) {
    int line = extractLine(parseErrMsg);
    if (line == 0) line = 1;
    ValidationResult vr = ValidationResult::atLine(line, parseErrMsg);
    results.append(vr);
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
    QString builtinPath = AcBuiltinLoader::findBuiltinFile(m_filePath);
    if (!builtinPath.isEmpty()) {
      m_visitedFiles.remove(QFileInfo(builtinPath).canonicalFilePath());
      QStringList allNames;
      collectSymbolsFromFile(builtinPath, allNames);
    }
  }

  // 步骤 2.5b：解析 import 语句，收集跨文件符号
  if (!m_filePath.isEmpty()) {
    resolveImportedSymbols(m_program);
    // import 的符号在目标文件中不存在 → 报错（如 import { add } 但目标文件无 add）
    for (const QString &err : m_importErrors) {
      if (!err.isEmpty()) results.append(parseError(err));
    }
  }

  // 步骤 2.5c：收集当前文件符号（此时内置函数和 import 符号已在符号表中，类型推断可正常工作）
  for (const auto &stmt : m_program.stmts) {
    m_symbolTable.collectStmt(stmt);
  }

  // ── 步骤 3：未声明标识符检查 ──
  QStringList undeclaredErrors;
  m_undeclaredValidator.setFilePath(m_filePath);
  m_undeclaredValidator.validate(m_program, m_declaredVars, undeclaredErrors);
  for (const QString &err : undeclaredErrors) {
    if (!err.isEmpty()) results.append(parseError(err));
  }

  // ── 步骤 4：静态类型检查 ──
  // 注意：m_classes/m_functions 已在 validate() 开头清空，并在 import 解析时收集了导入文件的类
  collectClassesAndFunctions(m_program);

  // 注册 C++ 原生类
  AcBuiltinLoader::registerNativeClasses(m_classes);

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
      QString absPath = PathResolver::resolveImportPath(imp.filePath, m_filePath);

      // 读取目标文件并收集符号
      collectSymbolsFromFile(absPath, imp.names, imp.line);

      // 注册别名（import { A as B }）：使 B 的悬停/跳转指向 A 的定义。
      // 此时 collectSymbolsFromFile 已把原导出名合并进符号表（尚未被当前文件同名声明覆盖）
      for (auto ait = imp.aliases.begin(); ait != imp.aliases.end(); ++ait) {
        m_symbolTable.registerAlias(ait.value(), ait.key());
      }
    }
  }
}

void AcValidator::collectSymbolsFromFile(const QString &filePath, const QStringList &importNames,
                                         int importLine) {
  // 防止循环 import
  QString canonical = QFileInfo(filePath).canonicalFilePath();
  if (canonical.isEmpty()) canonical = filePath;
  if (m_visitedFiles.contains(canonical)) return;
  m_visitedFiles.insert(canonical);

  // 读取文件内容：优先使用提供器返回的实时缓冲（已打开文件），否则读磁盘
  QString source;
  if (m_contentProvider) source = m_contentProvider(filePath);
  if (source.isEmpty()) source = UtilFile::readUtf8(filePath);
  if (source.trimmed().isEmpty()) return;

  // ANTLR 词法+语法分析 + AST 构建
  QSet<QString> declaredVars;
  Block program;
  QString parseError;
  if (!parseSource(source, program, declaredVars, parseError)) return;

  // 构建目标文件的符号表
  AcSymbolTable importedTable;
  importedTable.setFilePath(filePath);
  for (const auto &stmt : program.stmts) {
    importedTable.collectStmt(stmt);
  }

  // 校验 import 的符号在目标文件中是否存在（精确匹配或 Class.member 前缀匹配）
  if (!importNames.isEmpty()) {
    // 目标文件可直接导出的顶层名字（函数/类/接口/枚举/顶层变量）；
    // 注意：allSymbols() 不收集枚举，需直接从 AST 顶层声明补齐
    QSet<QString> avail;
    for (const auto &s : program.stmts) {
      switch (s.kind) {
        case Block::Stmt::kFuncDef: avail.insert(s.funcDef.name); break;
        case Block::Stmt::kClassDef: avail.insert(s.classDef.name); break;
        case Block::Stmt::kInterfaceDef: avail.insert(s.interfaceDef.name); break;
        case Block::Stmt::kEnumDef: avail.insert(s.enumDef.name); break;
        case Block::Stmt::kAssign:
          if (s.assign.isDeclaration && !s.assign.name.isEmpty()) avail.insert(s.assign.name);
          break;
        default: break;
      }
    }
    const auto &syms = importedTable.allSymbols();
    for (const QString &name : importNames) {
      if (avail.contains(name) || syms.contains(name)) continue;
      const QString prefix = name + QLatin1Char('.');
      bool found = false;
      for (auto sit = syms.begin(); sit != syms.end(); ++sit) {
        if (sit.key().startsWith(prefix)) {
          found = true;
          break;
        }
      }
      if (!found) {
        m_importErrors << QStringLiteral("import 的符号「%1」在 %2 中不存在 at line %3")
                              .arg(name, QFileInfo(filePath).fileName())
                              .arg(importLine);
      }
    }
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

bool AcValidator::parseSource(const QString &source, Block &program, QSet<QString> &declaredVars,
                              QString &error) {
  // ── 旧递归下降词法分析 ──
  QVector<Token> tokens = AcLexer::tokenize(source, error);
  if (tokens.isEmpty()) {
    if (error.isEmpty()) error = QStringLiteral("lexer returned empty token list");
    return false;
  }

  // ── 旧递归下降语法分析 ──
  AcParser parser;
  if (!parser.parse(tokens, program, declaredVars)) {
    error = parser.error();
    return false;
  }
  return true;
}

int AcValidator::extractLine(const QString &msg) const {
  // 匹配 "at line N" 或 "at line N in file"
  QRegularExpression re(QStringLiteral("at line (\\d+)"));
  auto match = re.match(msg);
  if (match.hasMatch()) return match.captured(1).toInt();
  return 0;
}

ValidationResult AcValidator::parseError(const QString &msg) const {
  int line = extractLine(msg);

  // 清理消息：去掉行号和文件名后缀，只保留错误描述
  QString cleanMsg = msg;
  if (line > 0) {
    // 去掉 " at line N in file.ac" 或 " at line N" 后缀
    QRegularExpression re(QStringLiteral(" at line \\d+( in \\S+)?$"));
    cleanMsg = cleanMsg.remove(re);
  } else {
    // 去掉 " (line unknown) in file.ac" 或 " (line unknown)" 后缀
    QRegularExpression re(QStringLiteral(" \\(line unknown\\)( in \\S+)?$"));
    cleanMsg = cleanMsg.remove(re);
  }

  return ValidationResult::atLine(line, cleanMsg);
}