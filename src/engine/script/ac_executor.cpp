/**
 * @file ac_executor.cpp
 * @brief .ac 脚本执行器实现文件
 */

#include "ac_executor.h"

AcExecutor::AcExecutor() = default;

/// @brief 解析 .ac 源码：词法分析 → 语法分析
bool AcExecutor::parse(const QString &source) {
  m_error.clear();
  m_declaredVars.clear();

  // 步骤 1：词法分析
  m_tokens = m_lexer.tokenize(source, m_error);
  if (!m_error.isEmpty())
    return false;

  // 步骤 2：语法分析
  if (!m_parser.parse(m_tokens, m_program, m_error, m_declaredVars))
    return false;

  return true;
}

/// @brief 执行 AST：验证 → 解释执行
QJsonValue AcExecutor::execute() {
  m_error.clear();

  // 步骤 1：配置解释器
  m_interpreter.setScriptDir(m_scriptDir);
  m_interpreter.setRootDir(m_rootDir);
  m_interpreter.setLogCallback(m_logCallback);

  // 步骤 2：验证未声明的标识符
  QStringList undeclared =
      m_interpreter.validateUndeclaredIdents(m_program, m_declaredVars);
  if (!undeclared.isEmpty()) {
    m_error = undeclared.join(QStringLiteral("\n"));
    return QJsonValue();
  }

  // 步骤 3：执行
  QJsonValue result = m_interpreter.execute(m_program, m_error);
  if (!m_error.isEmpty())
    return QJsonValue();

  return result;
}