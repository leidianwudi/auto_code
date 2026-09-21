/**
 * @file ac_diagnostic.h
 * @brief 结构化诊断 — 错误码 + 位置 + 严重级别的统一诊断模型
 *
 * 所有编译阶段（词法/语法/链接/未声明检查/类型检查）通过 AcDiagCollector
 * 收集 AcDiagnostic。与旧错误通道并行（双轨）：
 * - 旧通道（单错误字符串 / QStringList）保持不变，首错仍写入，兼容现有调用方
 * - 新通道（收集器）产出全量结构化诊断，供编辑器校验、错误恢复、语义服务消费
 */

#pragma once

#include <QString>
#include <QVector>

#include "../validation_result.h"
#include "ac_type.h"  // AcLoc

/// @brief 诊断严重级别
enum class AcDiagSeverity { kError, kWarning, kInfo };

/// @brief 单条结构化诊断（编译各阶段产出）
struct AcDiagnostic {
  QString code;                                         ///< 诊断码（AcDiagCode::xxx，如 "AC1001"）
  AcDiagSeverity severity = AcDiagSeverity::kError;     ///< 严重级别
  QString filePath;                                     ///< 所属源文件（import 文件的真实路径；空表示入口文件）
  AcLoc loc;                                            ///< 起始位置（行/列 1-based，UTF-16 码元）
  int length = 0;                                       ///< 覆盖长度（0 表示仅行级精度，编辑器按行高亮）
  QString message;                                      ///< 干净消息（不含行号/文件名后缀）

  /// @brief 是否错误级
  bool isError() const { return severity == AcDiagSeverity::kError; }
  /// @brief 是否警告级
  bool isWarning() const { return severity == AcDiagSeverity::kWarning; }
};

/// @brief 诊断收集器 — 收集一次编译流程的全部诊断
class AcDiagCollector {
public:
  /// @brief 追加一条诊断
  void add(const AcDiagnostic &d) { m_items.append(d); }

  /// @brief 追加一条错误
  void error(const QString &code, const QString &msg, const QString &filePath, const AcLoc &loc,
             int length = 0);

  /// @brief 追加一条警告
  void warning(const QString &code, const QString &msg, const QString &filePath, const AcLoc &loc,
               int length = 0);

  /// @brief 是否存在错误级诊断
  bool hasErrors() const;

  /// @brief 全部诊断（按产生顺序）
  const QVector<AcDiagnostic> &all() const { return m_items; }

  /// @brief 仅错误级诊断
  QVector<AcDiagnostic> errors() const;

  /// @brief 首个错误消息（legacy 兼容：无错误时返回空串）
  QString firstErrorMessage() const;

  /// @brief 清空
  void clear() { m_items.clear(); }

  /// @brief 诊断数量
  int size() const { return m_items.size(); }

private:
  QVector<AcDiagnostic> m_items;  ///< 已收集的诊断
};

/// @brief 诊断码常量 — 编码规则：AC0xxx 词法 / AC1xxx 语法 / AC2xxx 链接 /
///        AC3xxx 未声明 / AC4xxx 类型 / AC5xxx 运行时（预留）
namespace AcDiagCode {
// ── 词法（AC0xxx） ──
inline constexpr char kLexUnterminatedString[] = "AC0001";    ///< 字符串字面量未闭合
inline constexpr char kLexUnterminatedTemplate[] = "AC0002";  ///< 模板字符串未闭合
inline constexpr char kLexUnterminatedComment[] = "AC0003";   ///< 块注释未闭合
inline constexpr char kLexUnexpectedChar[] = "AC0004";        ///< 非法字符

// ── 语法（AC1xxx） ──
inline constexpr char kSyntaxExpected[] = "AC1001";     ///< 期望某 token（expect 失败）
inline constexpr char kSyntaxMissingSemi[] = "AC1002";  ///< 缺少分号
inline constexpr char kSyntaxDupDecl[] = "AC1003";      ///< 同一作用域重复声明
inline constexpr char kSyntaxBadType[] = "AC1004";      ///< 类型名错误（大小写/裸 Array）
inline constexpr char kSyntaxUnexpected[] = "AC1005";   ///< 意外 token
inline constexpr char kSyntaxClassMember[] = "AC1006";  ///< 类成员声明错误
inline constexpr char kSyntaxOther[] = "AC1007";        ///< 其他语法错误

// ── 链接（AC2xxx） ──
inline constexpr char kLinkFileNotFound[] = "AC2001";    ///< import 文件不存在
inline constexpr char kLinkCircularImport[] = "AC2002";  ///< 循环导入
inline constexpr char kLinkSymbolMissing[] = "AC2003";   ///< import 的符号在目标文件中不存在

// ── 未声明（AC3xxx） ──
inline constexpr char kUndeclaredIdent[] = "AC3001";  ///< 使用未声明标识符

// ── 类型（AC4xxx） ──
inline constexpr char kTypeError[] = "AC4001";  ///< 静态类型错误
}  // namespace AcDiagCode

/// @brief AcDiagnostic → ValidationResult 映射（编辑器校验结果统一消费）
ValidationResult toValidationResult(const AcDiagnostic &d);
