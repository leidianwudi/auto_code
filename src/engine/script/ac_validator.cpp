/**
 * @file ac_validator.cpp
 * @brief AC 脚本验证器实现
 */

#include "ac_validator.h"

#include <QRegularExpression>

#include "../ac_language.h"

QVector<ValidationResult> AcValidator::validate(const QString &source) {
  QVector<ValidationResult> results;

  if (source.trimmed().isEmpty()) return results;

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
  QString parseErrMsg;
  if (!m_parser.parse(tokens, m_program, parseErrMsg, m_declaredVars)) {
    int line = 1;
    QRegularExpression re(QStringLiteral("at line (\\d+)"));
    auto match = re.match(parseErrMsg);
    if (match.hasMatch()) line = match.captured(1).toInt();
    results.append(ValidationResult::atLine(line, parseErrMsg));
    return results;
  }

  // ── 步骤 3：未声明标识符检查 ──
  QStringList undeclaredErrors;
  m_undeclaredValidator.validate(m_program, m_declaredVars, undeclaredErrors);
  for (const QString &err : undeclaredErrors) {
    if (!err.isEmpty()) results.append(parseError(err));
  }

  // ── 步骤 4：静态类型检查 ──
  m_classes.clear();
  m_functions.clear();
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
    }
  }
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