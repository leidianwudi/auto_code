/**
 * @file ac_executor.h
 * @brief .ac 脚本执行器头文件 — 编排解析执行全流程
 *
 * AcExecutor 封装了词法分析 → 语法分析 → 语义验证 → 解释执行的完整流程，
 * 作为 AcEngine 和三个子模块（AcLexer / AcParser /
 * AcInterpreter）之间的编排层。
 */

#pragma once

#include <QFileInfo>
#include <QMap>
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
/// 封装了词法分析 → 语法分析 → 语义验证 → 解释执行的完整流程。
/// 内部调用 AcLexer、AcParser、AcInterpreter 完成各阶段工作。
class AcExecutor {
public:
  AcExecutor();

  QString error() const { return m_error; }
  void setScriptDir(const QString &dir) { m_scriptDir = dir; }
  void setRootDir(const QString &dir) { m_rootDir = dir; }
  void setJsonFile(const QString &path) { m_jsonPath = path; }
  void setIncludePaths(const QStringList &paths) { m_includePaths = paths; }
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
  // ── 预处理（#include / #path） ──
  /// @brief 递归预处理源码，解析 #include 和 #path 指令
  /// @param source    当前文件源码
  /// @param filePath  当前文件绝对路径（用于解析相对路径、检测循环引用）
  /// @param visited   已处理的文件路径集合（循环引用保护）
  /// @param lineRanges [out] 行号→文件映射（每段起始行号+文件路径）
  /// @return 预处理后的完整源码
  QString preprocess(const QString &source, const QString &filePath, QSet<QString> &visited,
                     QVector<QPair<int, QString>> &lineRanges, int &currentLine);

  /// @brief 将 lineRanges 转换为错误增强信息（line → filePath + originalLine）
  /// @param combinedLine 预处理后源码中的行号（1-based）
  /// @return (源文件路径, 源文件中的行号)
  QPair<QString, int> resolveFileLine(int combinedLine,
                                      const QVector<QPair<int, QString>> &lineRanges) const;

  // ── 子模块 ──
  AcLexer m_lexer;
  AcParser m_parser;
  AcInterpreter m_interpreter;

  // ── 状态 ──
  QString m_scriptDir;           ///< .ac 文件所在目录
  QString m_rootDir;             ///< 项目根目录
  QString m_jsonPath;            ///< 当前处理的 JSON 文件路径（保留但未使用）
  QString m_error;               ///< 错误信息
  QStringList m_includePaths;    ///< 头文件搜索路径（#include <xxx>）
  QVector<Token> m_tokens;       ///< 词法分析结果
  QSet<QString> m_declaredVars;  ///< 已用 let 声明的变量名
  Block m_program;               ///< 解析后的 AST 根节点
  QStringList m_typeErrors;      ///< 类型检查错误列表
  LogCallback m_logCallback;
};