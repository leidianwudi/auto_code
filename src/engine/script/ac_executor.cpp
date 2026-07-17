/**
 * @file ac_executor.cpp
 * @brief .ac 脚本执行器实现文件
 */

#include "ac_executor.h"

#include <QDir>
#include <QFile>

#include "../ac_language.h"
#include "undeclared_ident_validator.h"

AcExecutor::AcExecutor() = default;

/**
 * @brief 解析 .ac 源码：词法分析 → 语法分析 → 模块链接
 *
 * 先解析当前文件，然后处理 import 语句：加载目标文件，
 * 独立解析，提取 export 符号注入当前程序。
 */
bool AcExecutor::parse(const QString &source) {
  m_error.clear();
  m_declaredVars.clear();
  m_sourceLines = source.split(QLatin1Char('\n'));

  // ── 步骤 1：词法分析 ──
  m_tokens = m_lexer.tokenize(source, m_error);
  if (!m_error.isEmpty()) return false;

  // ── 步骤 2：语法分析 ──
  if (!m_parser.parse(m_tokens, m_program, m_error, m_declaredVars)) return false;

  // 步骤 3：模块链接 — 处理 import 语句
  if (!linkImports()) return false;

  // 步骤 4：构建符号表
  m_symbolTable.clear();
  m_symbolTable.setFilePath(m_scriptFile.isEmpty() ? QString() : m_scriptFile);
  for (const auto &stmt : m_program.stmts) {
    m_symbolTable.collectStmt(stmt);
  }

  qDebug() << "[AcExecutor::parse] after linkImports, declaredVars:" << m_declaredVars;
  qDebug() << "[AcExecutor::parse] program stmts:" << m_program.stmts.size();
  qDebug() << "[AcExecutor::parse] symbols collected:" << m_symbolTable.allSymbols().size();
  for (int i = 0; i < m_program.stmts.size(); ++i) {
    const auto &s = m_program.stmts[i];
    QString desc;
    if (s.kind == Block::Stmt::kAssign)
      desc = "assign:" + s.assign.name;
    else if (s.kind == Block::Stmt::kClassDef)
      desc = "class:" + s.classDef.name;
    else if (s.kind == Block::Stmt::kFuncDef)
      desc = "func:" + s.funcDef.name;
    else if (s.kind == Block::Stmt::kInterfaceDef)
      desc = "iface:" + s.interfaceDef.name;
    else if (s.kind == Block::Stmt::kImport)
      desc = "import";
    else
      desc = "kind:" + QString::number((int)s.kind);
    qDebug() << "  [" << i << "]" << desc;
  }

  return true;
}

QStringList AcExecutor::validateTypes() {
  m_typeErrors.clear();

  QHash<QString, ClassDef> classes;
  QHash<QString, MethodDef> functions;
  for (const auto &stmt : m_program.stmts) {
    if (stmt.kind == Block::Stmt::kClassDef) {
      classes.insert(stmt.classDef.name, stmt.classDef);
    } else if (stmt.kind == Block::Stmt::kFuncDef) {
      functions.insert(stmt.funcDef.name, stmt.funcDef);
    }
  }

  for (const auto &name : AcClass::kAll) {
    if (!classes.contains(name)) {
      ClassDef nativeClass;
      nativeClass.name = name;
      nativeClass.isNative = true;
      classes.insert(name, nativeClass);
    }
  }

  AcTypeChecker typeChecker;
  typeChecker.check(m_program, m_declaredVars, classes, functions, m_typeErrors);
  return m_typeErrors;
}

/// @brief 执行 AST：验证 → 类型检查 → 解释执行
QJsonValue AcExecutor::execute() {
  m_error.clear();
  m_typeErrors.clear();

  // 步骤 1：配置解释器
  m_interpreter.setScriptDir(m_scriptDir);
  m_interpreter.setRootDir(m_rootDir);
  m_interpreter.setLogCallback(m_logCallback);

  qDebug() << "[AcExecutor::execute] m_logCallback is null:" << !m_logCallback;

  // 步骤 2：验证未声明的标识符
  UndeclaredIdentValidator undeclaredValidator;
  QStringList undeclared;
  undeclaredValidator.validate(m_program, m_declaredVars, undeclared);
  if (!undeclared.isEmpty()) {
    m_error = undeclared.join(QStringLiteral("\n"));
    qDebug() << "[AcExecutor::execute] undeclared errors:" << m_error;
    return QJsonValue();
  }
  qDebug() << "[AcExecutor::execute] undeclared check passed, program stmts:"
           << m_program.stmts.size();

  // 步骤 3：静态类型检查
  QStringList typeErrors = validateTypes();
  if (!typeErrors.isEmpty()) {
    m_error = typeErrors.join(QStringLiteral("\n"));
    qDebug() << "[AcExecutor::execute] type errors:" << m_error;
    return QJsonValue();
  }
  qDebug() << "[AcExecutor::execute] type check passed";

  // 步骤 4：执行
  QJsonValue result = m_interpreter.execute(m_program, m_error);
  if (!m_error.isEmpty()) {
    qDebug() << "[AcExecutor::execute] execution error:" << m_error;
    return QJsonValue();
  }

  qDebug() << "[AcExecutor::execute] execution completed, result:" << result;
  return result;
}

// ═════════════════════════════════════════════════════════════════════════════
// 模块链接
// ═════════════════════════════════════════════════════════════════════════════

/// @brief 读取文件全部内容（UTF-8）
static QString readFileUtf8(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return {};
  return QString::fromUtf8(f.readAll());
}

/**
 * @brief 处理 import 语句，加载并链接导出符号
 *
 * 遍历 AST 中的 import 语句：
 * 1. 解析文件路径（相对当前脚本目录）
 * 2. 读取并独立解析目标文件
 * 3. 从目标 AST 中提取 export 符号
 * 4. 将导出的定义注入当前程序的 AST
 * 5. 将导入的变量名注册到 declaredVars
 *
 * 循环引用保护：已处理的文件路径记录在 visited 集合中。
 */
bool AcExecutor::linkImports() {
  QSet<QString> visited;
  // 将入口文件加入导入链，检测 a→b→a 类循环
  if (!m_scriptFile.isEmpty()) {
    visited.insert(QFileInfo(m_scriptFile).absoluteFilePath());
  }
  return linkImportsRecursive(m_program, m_scriptDir, visited);
}

bool AcExecutor::linkImportsRecursive(Block &program, const QString &baseDir,
                                      QSet<QString> &visited) {
  // 收集所有 import 语句
  QVector<int> importIndices;
  for (int i = 0; i < program.stmts.size(); ++i) {
    if (program.stmts[i].kind == Block::Stmt::kImport) {
      importIndices.append(i);
    }
  }

  // 收集所有导入的符号定义（最终插入到 program 开头）
  QVector<Block::Stmt> importedStmts;

  for (int idx : importIndices) {
    const ImportStmt &imp = program.stmts[idx].importStmt;

    // 解析文件路径
    QString absPath = QDir(baseDir).absoluteFilePath(imp.filePath);
    absPath = QFileInfo(absPath).absoluteFilePath();

    if (!QFileInfo::exists(absPath)) {
      m_error = QStringLiteral("import: file not found '%1'").arg(imp.filePath);
      return false;
    }

    // 循环导入检测：如果当前文件已在导入链中，说明存在循环
    if (visited.contains(absPath)) {
      m_error = QStringLiteral("circular import detected: '%1'").arg(imp.filePath);
      return false;
    }

    // 标记当前文件正在处理（进入导入链）
    visited.insert(absPath);

    // 读取并解析目标文件
    QString source = readFileUtf8(absPath);
    if (source.isEmpty()) {
      m_error = QStringLiteral("import: cannot read file '%1'").arg(imp.filePath);
      return false;
    }

    AcLexer lexer;
    AcParser parser;
    Block moduleAst;
    QSet<QString> moduleVars;
    QString moduleError;
    QVector<Token> tokens = lexer.tokenize(source, moduleError);
    if (!moduleError.isEmpty()) {
      m_error = QStringLiteral("import: parse error in '%1': %2").arg(imp.filePath, moduleError);
      return false;
    }
    if (!parser.parse(tokens, moduleAst, moduleError, moduleVars)) {
      m_error = QStringLiteral("import: parse error in '%1': %2").arg(imp.filePath, moduleError);
      return false;
    }

    // 递归处理目标文件中的 import
    QString moduleDir = QFileInfo(absPath).absolutePath();
    if (!linkImportsRecursive(moduleAst, moduleDir, visited)) return false;

    // 回溯：从导入链中移除当前文件，允许其他分支导入同一文件（菱形导入）
    visited.remove(absPath);

    // 从目标 AST 中提取 export 符号并注入当前程序
    qDebug() << "[linkImports] module" << imp.filePath << "has" << moduleAst.stmts.size()
             << "stmts, importing:" << imp.names;
    for (const auto &stmt : moduleAst.stmts) {
      bool isExported = false;

      if (stmt.kind == Block::Stmt::kAssign && stmt.assign.isExported) {
        isExported = true;
      } else if (stmt.kind == Block::Stmt::kClassDef && stmt.classDef.isExported) {
        isExported = true;
      } else if (stmt.kind == Block::Stmt::kFuncDef && stmt.funcDef.isExported) {
        isExported = true;
      } else if (stmt.kind == Block::Stmt::kInterfaceDef && stmt.interfaceDef.isExported) {
        isExported = true;
      } else if (stmt.kind == Block::Stmt::kEnumDef && stmt.enumDef.isExported) {
        isExported = true;
      }

      // 调试输出
      QString stmtName;
      if (stmt.kind == Block::Stmt::kAssign)
        stmtName = stmt.assign.name;
      else if (stmt.kind == Block::Stmt::kClassDef)
        stmtName = stmt.classDef.name;
      else if (stmt.kind == Block::Stmt::kFuncDef)
        stmtName = stmt.funcDef.name;
      else if (stmt.kind == Block::Stmt::kInterfaceDef)
        stmtName = stmt.interfaceDef.name;
      else if (stmt.kind == Block::Stmt::kEnumDef)
        stmtName = stmt.enumDef.name;
      qDebug() << "  stmt kind:" << (int)stmt.kind << "name:" << stmtName
               << "isExported:" << isExported;

      if (!isExported) continue;

      // 检查是否在 import 列表中
      QString exportName;
      if (stmt.kind == Block::Stmt::kAssign) {
        exportName = stmt.assign.name;
      } else if (stmt.kind == Block::Stmt::kClassDef) {
        exportName = stmt.classDef.name;
      } else if (stmt.kind == Block::Stmt::kFuncDef) {
        exportName = stmt.funcDef.name;
      } else if (stmt.kind == Block::Stmt::kInterfaceDef) {
        exportName = stmt.interfaceDef.name;
      } else if (stmt.kind == Block::Stmt::kEnumDef) {
        exportName = stmt.enumDef.name;
      }

      if (!imp.names.contains(exportName)) continue;

      // 注入到当前程序（插入到开头，确保定义在 main 块语句之前执行）
      // 如果有别名，需要将符号名重命名为别名
      Block::Stmt renamedStmt = stmt;
      QString localName = exportName;
      if (imp.aliases.contains(exportName)) {
        localName = imp.aliases[exportName];
        if (renamedStmt.kind == Block::Stmt::kAssign) {
          renamedStmt.assign.name = localName;
        } else if (renamedStmt.kind == Block::Stmt::kClassDef) {
          renamedStmt.classDef.name = localName;
        } else if (renamedStmt.kind == Block::Stmt::kFuncDef) {
          renamedStmt.funcDef.name = localName;
        } else if (renamedStmt.kind == Block::Stmt::kInterfaceDef) {
          renamedStmt.interfaceDef.name = localName;
        } else if (renamedStmt.kind == Block::Stmt::kEnumDef) {
          renamedStmt.enumDef.name = localName;
        }
      }
      importedStmts.append(renamedStmt);

      // 注册变量名（使用别名或原始名）
      m_declaredVars.insert(localName);
      qDebug() << "[linkImports] injected" << exportName << "as" << localName << "into program";
    }

    // 检查所有导入名是否都找到了
    QSet<QString> foundNames;
    for (const auto &stmt : moduleAst.stmts) {
      QString name;
      if (stmt.kind == Block::Stmt::kAssign && stmt.assign.isExported)
        name = stmt.assign.name;
      else if (stmt.kind == Block::Stmt::kClassDef && stmt.classDef.isExported)
        name = stmt.classDef.name;
      else if (stmt.kind == Block::Stmt::kFuncDef && stmt.funcDef.isExported)
        name = stmt.funcDef.name;
      else if (stmt.kind == Block::Stmt::kInterfaceDef && stmt.interfaceDef.isExported)
        name = stmt.interfaceDef.name;
      else if (stmt.kind == Block::Stmt::kEnumDef && stmt.enumDef.isExported)
        name = stmt.enumDef.name;
      if (!name.isEmpty()) foundNames.insert(name);
    }

    for (const auto &name : imp.names) {
      if (!foundNames.contains(name)) {
        m_error = QStringLiteral("import: '%1' is not exported from '%2'").arg(name, imp.filePath);
        return false;
      }
    }
  }

  // 移除 import 语句（已处理完毕），并将导入的符号定义插入到开头
  QVector<Block::Stmt> filtered;
  // 先放入导入的符号定义（函数/类/变量），确保在 main 块语句之前注册
  for (auto &stmt : importedStmts) {
    filtered.append(std::move(stmt));
  }
  // 再放入原有语句（跳过 import 语句）
  for (auto &stmt : program.stmts) {
    if (stmt.kind != Block::Stmt::kImport) {
      filtered.append(std::move(stmt));
    }
  }
  program.stmts = std::move(filtered);

  return true;
}