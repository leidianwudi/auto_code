/**
 * @file ac_executor.cpp
 * @brief .ac 脚本执行器实现文件
 */

#include "ac_executor.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include "../ac_language.h"
#include "undeclared_ident_validator.h"

AcExecutor::AcExecutor() = default;

/// @brief 在错误信息中提取行号，附加源文件路径（前向声明）
static QString augmentErrorLine(const QString &error,
                                const QVector<QPair<int, QString>> &lineRanges);

/**
 * @brief 解析 .ac 源码：预处理 → 词法分析 → 语法分析
 *
 * 预处理阶段处理 #include 和 #path 指令，将引用的文件内容拼入源码，
 * 并建立行号→源文件的映射用于错误提示增强。
 */
bool AcExecutor::parse(const QString &source) {
  m_error.clear();
  m_declaredVars.clear();

  // ── 步骤 0：预处理 — 解析 #include / #path 指令 ──
  QSet<QString> visited;
  QVector<QPair<int, QString>> lineRanges;
  int currentLine = 1;
  QString processed = preprocess(source, m_scriptDir, visited, lineRanges, currentLine);

  // ── 步骤 1：词法分析 ──
  m_tokens = m_lexer.tokenize(processed, m_error);
  if (!m_error.isEmpty()) {
    m_error = augmentErrorLine(m_error, lineRanges);
    return false;
  }

  // ── 步骤 2：语法分析 ──
  if (!m_parser.parse(m_tokens, m_program, m_error, m_declaredVars)) {
    m_error = augmentErrorLine(m_error, lineRanges);
    return false;
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
// 辅助函数
// ═════════════════════════════════════════════════════════════════════════════

/// @brief 读取文件全部内容（UTF-8）
static QString readFileUtf8(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return {};
  return QString::fromUtf8(f.readAll());
}

/// @brief 在错误信息中提取行号，附加源文件路径
static QString augmentErrorLine(const QString &error,
                                const QVector<QPair<int, QString>> &lineRanges) {
  QRegularExpression re(QStringLiteral("at line (\\d+)"));
  auto match = re.match(error);
  if (!match.hasMatch()) return error;

  int line = match.captured(1).toInt();

  // 从后往前查找包含该行号的段
  for (int i = lineRanges.size() - 1; i >= 0; --i) {
    int startLine = lineRanges[i].first;
    if (startLine <= 0) continue;
    if (line >= startLine) {
      // 确认此行号在本段范围内（本段起始行 ≤ line < 下一段起始行）
      if (i + 1 >= lineRanges.size() || lineRanges[i + 1].first <= 0 ||
          line < lineRanges[i + 1].first) {
        QString filePath = lineRanges[i].second;
        int origLine = line - startLine + 1;
        return error + QStringLiteral(" (file: %1, line: %2)").arg(filePath).arg(origLine);
      }
    }
  }
  return error;
}

// ═════════════════════════════════════════════════════════════════════════════
// 预处理 — #include / #path 指令
// ═════════════════════════════════════════════════════════════════════════════

/**
 * @brief 递归预处理源码
 *
 * 逐行扫描源码：
 * - `#include "file.ac"` → 读取文件，递归预处理，拼入结果
 * - `#include <file.ac>` → 在搜索路径中查找文件，递归预处理
 * - `#path "dir"`       → 将路径加入搜索列表（不输出到结果）
 * - 其他行               → 原样保留
 *
 * @param source     当前文件源码
 * @param filePath   当前文件路径（绝对或相对）
 * @param visited    已处理文件集合（循环引用保护）
 * @param lineRanges [out] 行号→源文件映射
 * @param currentLine [in/out] 当前在输出结果中的行号（1-based）
 * @return 预处理后的源码
 */
QString AcExecutor::preprocess(const QString &source, const QString &filePath,
                               QSet<QString> &visited, QVector<QPair<int, QString>> &lineRanges,
                               int &currentLine) {
  QString absPath = QFileInfo(filePath).absoluteFilePath();

  // 循环引用保护
  if (visited.contains(absPath)) return {};
  visited.insert(absPath);

  // 记录当前文件段在输出中的起始行号
  int startLine = currentLine;
  lineRanges.append(qMakePair(startLine, absPath));

  // 拆分按行处理
  QStringList lines = source.split(QStringLiteral("\n"));
  QStringList resultParts;
  QStringList extraPaths;  // 当前文件内 #path 声明的路径（仅影响当前文件）

  // 构建搜索路径：约定目录 + 用户配置
  QString baseDir = QFileInfo(absPath).absolutePath();
  QStringList searchPaths;
  if (!baseDir.isEmpty()) searchPaths.append(baseDir);
  if (!baseDir.isEmpty())
    searchPaths.append(baseDir + QStringLiteral("/") + QString::fromLatin1(AcKeyword::kIncDir));
  // 用户通过 C++ API 设置的路径
  for (const auto &p : m_includePaths) searchPaths.append(p);
  // 项目根目录
  if (!m_rootDir.isEmpty() && m_rootDir != baseDir) searchPaths.append(m_rootDir);

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];
    QString trimmed = line.trimmed();

    // ── #include 指令 ──
    if (trimmed.startsWith(QString::fromLatin1(AcKeyword::kInclude))) {
      // 解析路径：#include "path" 或 #include <path>
      int quoteStart = trimmed.indexOf(QChar('"'));
      int angleStart = trimmed.indexOf(QChar('<'));
      QString incPath;

      if (quoteStart >= 0) {
        // #include "path" — 相对当前文件目录
        int quoteEnd = trimmed.indexOf(QChar('"'), quoteStart + 1);
        if (quoteEnd > quoteStart) {
          QString relPath = trimmed.mid(quoteStart + 1, quoteEnd - quoteStart - 1);
          incPath = QFileInfo(baseDir + QStringLiteral("/") + relPath).absoluteFilePath();
        }
      } else if (angleStart >= 0) {
        // #include <path> — 在搜索路径中查找
        int angleEnd = trimmed.indexOf(QChar('>'), angleStart + 1);
        if (angleEnd > angleStart) {
          QString fileName = trimmed.mid(angleStart + 1, angleEnd - angleStart - 1);
          // 合并搜索路径（约定 + 配置 + 当前文件 #path 声明的）
          QStringList combined = searchPaths;
          for (const auto &ep : extraPaths) combined.append(ep);
          for (const auto &dir : combined) {
            QString fullPath = dir + QStringLiteral("/") + fileName;
            if (QFileInfo::exists(fullPath)) {
              incPath = QFileInfo(fullPath).absoluteFilePath();
              break;
            }
          }
        }
      }

      if (!incPath.isEmpty()) {
        QString incContent = readFileUtf8(incPath);
        if (!incContent.isEmpty()) {
          // 递归预处理引用的文件
          QString processed = preprocess(incContent, incPath, visited, lineRanges, currentLine);
          if (!processed.isEmpty()) {
            resultParts.append(processed);
            // 确保末尾有换行
            if (!processed.endsWith(QChar('\n'))) resultParts.last().append(QChar('\n'));
          }
          // 如果递归返回空（循环引用或文件为空），不输出
        } else {
          // 文件存在但为空，保留原行
          resultParts.append(line);
          ++currentLine;
        }
      } else {
        // 找不到文件，保留原行（词法分析或运行时才能报错）
        resultParts.append(line);
        ++currentLine;
      }
      continue;
    }

    // ── #path 指令 — 声明搜索路径 ──
    if (trimmed.startsWith(QString::fromLatin1(AcKeyword::kPath))) {
      QString pathStr = trimmed.mid(QString::fromLatin1(AcKeyword::kPath).length()).trimmed();
      // 去掉引号
      if (pathStr.startsWith(QChar('"')) && pathStr.endsWith(QChar('"')))
        pathStr = pathStr.mid(1, pathStr.length() - 2);
      if (!pathStr.isEmpty()) {
        // 相对当前文件所在目录
        QString fullPath = QDir(baseDir).absoluteFilePath(pathStr);
        if (!extraPaths.contains(fullPath)) extraPaths.append(fullPath);
      }
      // #path 行不输出到最终源码
      continue;
    }

    // ── 普通行：原样保留 ──
    resultParts.append(line);
    ++currentLine;
  }

  return resultParts.join(QStringLiteral("\n"));
}