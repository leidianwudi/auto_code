/**
 * @file ac_symbol_table.h
 * @brief AC 脚本符号表 — 收集所有符号定义位置，支持悬停提示、跳转、查找引用
 *
 * 符号表遍历 AST，收集函数、类、方法、变量的定义位置（文件+行号），
 * 为编辑器提供以下功能：
 * - 悬停提示：显示函数签名、参数、返回值
 * - 转到定义：跳转到符号定义位置
 * - 查找引用：扫描文件查找所有引用位置
 */

#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include "src/engine/script/ac_type.h"

/// @brief 符号类型
enum class AcSymbolKind {
  kFunction,   ///< 全局函数
  kMethod,     ///< 类方法
  kClass,      ///< 类定义
  kVariable,   ///< 变量
  kProperty,   ///< 类属性
  kParameter,  ///< 函数参数
};

/// @brief 符号表条目
struct AcSymbolEntry {
  QString name;                               ///< 符号名
  AcSymbolKind kind;                          ///< 符号类型
  QString filePath;                           ///< 定义文件路径
  int line = 0;                               ///< 定义行号（1-based）
  int endLine = 0;                            ///< 结束行号（用于代码折叠）
  QString signature;                          ///< 完整签名（用于悬停提示）
  QString returnType;                         ///< 返回类型
  QVector<ParamDef> params;                   ///< 参数列表
  QString comment;                            ///< 注释/文档
  QString parentClass;                        ///< 所属类名（方法/属性使用）
  bool isStatic = false;                      ///< 是否静态
  bool isExported = false;                    ///< 是否导出
  AccessLevel access = AccessLevel::kPublic;  ///< 访问级别
};

/// @brief 引用位置
struct AcReference {
  QString filePath;
  int line = 0;
  int column = 0;
  int length = 0;
  QString contextLine;  ///< 该行源码（用于预览）
};

/**
 * @class AcSymbolTable
 * @brief AC 脚本符号表
 *
 * 用法：
 * @code
 * AcSymbolTable table;
 * table.buildFromAst(filePath, ast);
 * auto *entry = table.findSymbol("myFunction");
 * auto refs = table.findReferences("myFunction");
 * @endcode
 */
class AcSymbolTable {
public:
  AcSymbolTable() = default;

  /// 从 AST 构建符号表
  void buildFromAst(const QString &filePath, const Block::Stmt &stmt);

  /// 从 AST 语句块构建符号表
  void buildFromBlock(const QString &filePath, const Block &block);

  /// 从 AST 语句收集符号（供外部直接调用）
  void collectStmt(const Block::Stmt &stmt);

  /// 查找符号定义（优先精确匹配，其次模糊匹配）
  AcSymbolEntry *findSymbol(const QString &name);

  /// 查找符号定义（const 版本）
  const AcSymbolEntry *findSymbol(const QString &name) const;

  /// 查找所有符号（支持前缀匹配，用于自动补全）
  QVector<AcSymbolEntry> findSymbolsByPrefix(const QString &prefix) const;

  /// 获取所有符号
  const QHash<QString, AcSymbolEntry> &allSymbols() const { return m_symbols; }

  /// 清除符号表
  void clear();

  /// 合并另一个符号表中的指定符号（用于跨文件 import）
  /// @param other 源符号表
  /// @param names 需要合并的符号名列表（精确匹配 + ClassName.* 前缀匹配）
  void mergeFrom(const AcSymbolTable &other, const QStringList &names);

  /// 获取当前文件路径
  const QString &filePath() const { return m_filePath; }

  /// 设置当前文件路径
  void setFilePath(const QString &path) { m_filePath = path; }

private:
  /// 从语句收集符号（内部递归调用）
  void collectFromStmt(const Block::Stmt &stmt);

  /// 从 if 分支收集符号（处理 else if 链）
  void collectFromIfBranch(const IfStmt *ifStmt);

  /// AcType 转字符串
  QString acTypeToString(const AcType &type) const;

  /// 生成函数签名
  QString makeFunctionSignature(const QString &name, const QVector<ParamDef> &params,
                                const AcType &returnType) const;

  /// 生成方法签名
  QString makeMethodSignature(const QString &className, const QString &name,
                              const QVector<ParamDef> &params, const AcType &returnType,
                              bool isStatic) const;

  QString m_filePath;  ///< 当前文件路径
  QHash<QString, AcSymbolEntry> m_symbols;
};