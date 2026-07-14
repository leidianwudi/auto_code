/**
 * @file ac_executor.h
 * @brief .ac 脚本执行器头文件 — 编排解析执行全流程
 *
 * AcExecutor 封装了词法分析 → 语法分析 → 模块链接 → 语义验证 → 解释执行的完整流程，
 * 作为 AcEngine 和子模块之间的编排层。
 */

#pragma once

#include <QSet>
#include <QString>
#include <QStringList>
#include <functional>

#include "ac_interpreter.h"
#include "ac_lexer.h"
#include "ac_parser.h"
#include "ac_type.h"
#include "ac_type_checker.h"

/// @brief .ac 脚本执行器 — 将 .ac 脚本解析为 AST 并解释执行
///
/// 封装了词法分析 → 语法分析 → 模块链接 → 语义验证 → 解释执行的完整流程。
/// 内部调用 AcLexer、AcParser、AcInterpreter 完成各阶段工作。
class AcExecutor {
public:
  AcExecutor();

  QString error() const { return m_error; }
  void setScriptDir(const QString &dir) { m_scriptDir = dir; }
  void setScriptFile(const QString &path) { m_scriptFile = path; }
  void setRootDir(const QString &dir) { m_rootDir = dir; }
  void setJsonFile(const QString &path) { m_jsonPath = path; }
  QStringList generatedFiles() const { return m_interpreter.generatedFiles(); }

  /// 日志回调：print() 的输出通过此回调通知 UI
  using LogCallback = std::function<void(const QString &text, bool isError)>;
  void setLogCallback(LogCallback cb) { m_logCallback = std::move(cb); }

  bool parse(const QString &source);
  QJsonValue execute();

  /// @brief 解析后调用，运行静态类型检查
  /// @return 类型错误信息列表
  QStringList validateTypes();

private:
  // ── 模块链接 ──
  /// @brief 处理 import 语句，加载并链接导出符号
  bool linkImports();
  /// @brief 递归处理模块链接
  /// @param program 当前 AST
  /// @param baseDir 基准目录（解析相对路径）
  /// @param visited 已处理文件集合（循环引用保护）
  bool linkImportsRecursive(Block &program, const QString &baseDir, QSet<QString> &visited);

  // ── 子模块 ──
  AcLexer m_lexer;
  AcParser m_parser;
  AcInterpreter m_interpreter;

  // ── 状态 ──
  QString m_scriptDir;           ///< .ac 文件所在目录
  QString m_scriptFile;          ///< .ac 入口文件绝对路径（循环导入检测用）
  QString m_rootDir;             ///< 项目根目录
  QString m_jsonPath;            ///< 当前处理的 JSON 文件路径（保留但未使用）
  QString m_error;               ///< 错误信息
  QVector<Token> m_tokens;       ///< 词法分析结果
  QSet<QString> m_declaredVars;  ///< 已用 let 声明的变量名
  Block m_program;               ///< 解析后的 AST 根节点
  QStringList m_typeErrors;      ///< 类型检查错误列表
  LogCallback m_logCallback;
};