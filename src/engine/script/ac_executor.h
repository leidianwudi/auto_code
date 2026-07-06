/**
 * @file ac_executor.h
 * @brief .ac 脚本执行器头文件 — 编排解析执行全流程
 *
 * AcExecutor 封装了词法分析 → 语法分析 → 语义验证 → 解释执行的完整流程，
 * 作为 AcEngine 和三个子模块（AcLexer / AcParser /
 * AcInterpreter）之间的编排层。
 */

#pragma once

#include "ac_interpreter.h"
#include "ac_lexer.h"
#include "ac_parser.h"
#include "ac_type.h"


#include <QString>
#include <QStringList>
#include <functional>

/// @brief .ac 脚本执行器 — 将 .ac 脚本解析为 AST 并解释执行
///
/// 封装了词法分析 → 语法分析 → 语义验证 → 解释执行的完整流程。
/// 内部调用 AcLexer、AcParser、AcInterpreter 完成各阶段工作。
class AcExecutor {
public:
  AcExecutor();

  QString error() const { return m_error; }
  void setScriptDir(const QString &dir) { m_scriptDir = dir; }
  void setRootDir(const QString &dir) { m_rootDir = dir; }
  void setJsonFile(const QString &path) { m_jsonPath = path; }
  QStringList generatedFiles() const { return m_interpreter.generatedFiles(); }

  /// 日志回调：print() 的输出通过此回调通知 UI
  using LogCallback = std::function<void(const QString &text, bool isError)>;
  void setLogCallback(LogCallback cb) { m_logCallback = std::move(cb); }

  bool parse(const QString &source);
  QJsonValue execute();

  /// @brief 解析后调用，检查所有未声明的标识符
  /// @return 错误信息列表，每个元素格式 "undefined variable 'xxx' at line N"
  QStringList validateUndeclaredIdents() const {
    return m_interpreter.validateUndeclaredIdents(m_program, m_declaredVars);
  }

private:
  // ── 子模块 ──
  AcLexer m_lexer;
  AcParser m_parser;
  AcInterpreter m_interpreter;

  // ── 状态 ──
  QString m_scriptDir;          ///< .ac 文件所在目录
  QString m_rootDir;            ///< 项目根目录
  QString m_jsonPath;           ///< 当前处理的 JSON 文件路径（保留但未使用）
  QString m_error;              ///< 错误信息
  QVector<Token> m_tokens;      ///< 词法分析结果
  QSet<QString> m_declaredVars; ///< 已用 let 声明的变量名
  Block m_program;              ///< 解析后的 AST 根节点
  LogCallback m_logCallback;
};