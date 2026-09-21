/**
 * @file ac_semantic_service.h
 * @brief 进程内语义服务（阶段 5b）— 编辑器/工具的程序员助手门面
 *
 * 聚合：恢复式诊断（AcValidator + 多诊断）、标识符补全、定义跳转、引用查找。
 * 复用 AcLexer / AcParser（错误恢复模式）在单文件内工作；跨文件符号（跳转/引用）
 * 由编辑器现有 WorkspaceIndex 负责（本服务不重复实现，v2 可下沉）。
 */

#pragma once

#include <QString>
#include <QVector>

#include "../validation_result.h"

/// @brief 符号定位（定义/引用）
struct AcSymbolLoc {
  QString filePath;  ///< 所属文件（相对/绝对路径）
  int line = 0;      ///< 行号（1-based）
  int col = 0;       ///< 列号（1-based，token 首字符）
};

/// @brief 补全条目
struct AcCompletionItem {
  QString label;  ///< 候选文本
  QString kind;   ///< 分类：let/function/class/interface/enum/keyword
};

/// @brief 进程内语义服务门面
class AcSemanticService {
public:
  /// @brief 单文件诊断：恢复式 parse（错误不中断），返回全部 ValidationResult（按位置排序）
  static QVector<ValidationResult> diagnose(const QString &source, const QString &filePath);

  /// @brief 补全：返回文件中出现过的标识符（去重）+ 语句关键字；空 source 返回关键字表
  static QVector<AcCompletionItem> complete(const QString &source, const QString &filePath);

  /// @brief 定义跳转：返回 (line,col) 处标识符在文件中的声明位置；
  ///        未命中或不是标识符返回空 AcSymbolLoc（filePath 为空表示未找到）
  static AcSymbolLoc resolveDefinition(const QString &source, const QString &filePath, int line,
                                       int col);

  /// @brief 引用查找：返回文件中所有 text==name 的 kIdent 出现位置（含声明处）
  static QVector<AcSymbolLoc> findReferences(const QString &source, const QString &filePath,
                                             const QString &name);
};