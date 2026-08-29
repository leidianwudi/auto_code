/**
 * @file workspace_index.h
 * @brief 工作区语义索引 — 统一的跨文件符号解析（引用 / 重命名 / 跳转定义共用）
 *
 * 设计目标（替代原先散落在各处、各写一套的 import/别名/作用域解析）：
 * - 一次构建模块表：解析工作区内所有 .ac 文件，收集每个文件的顶层导出符号、
 *   类成员、import 绑定（含别名），形成统一的语义模型。
 * - 所有语义查询（findReferences / resolveDefinition）都基于这份模块表，
 *   保证"重命名 / 查找引用 / 跳转定义"三处行为一致、修一处处处生效。
 * - 纯计算，可在后台线程调用（QtConcurrent::run），不访问任何 UI。
 */

#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

#include "src/engine/rename/symbol_rename.h"  // RenameRef（保持现有调用方兼容）

/// 语义符号（声明信息）
struct SemanticSymbol {
  QString key;             ///< 唯一键：g:<file>#<name> / m:<file>#<Class>#<name> / l:<file>#<line>
  QString name;            ///< 符号名
  QString kind;            ///< function / class / method / property / variable / enum / interface
  QString filePath;        ///< 声明所在文件
  int line = 0;            ///< 声明行（1-based）
  int endLine = 0;         ///< 结束行
  QString parentClass;     ///< 方法/属性所属类（顶层符号为空）
  bool isStatic = false;   ///< 是否静态
  bool isExported = false; ///< 是否导出
  QString signature;       ///< 完整签名（悬停提示用）
  QString returnType;      ///< 返回类型
  QStringList params;      ///< 参数名列表
};

/// import 绑定：import { A as B } from "path" → { sourceFile, originalName=A, localName=B }
struct SemanticImport {
  QString sourceFile;    ///< 来源文件绝对路径
  QString originalName;  ///< 源文件中的导出名
  QString localName;     ///< 本地绑定名（无别名时 = originalName）
};

/// 工作区语义索引（单例）
class WorkspaceIndex {
public:
  static WorkspaceIndex &ins();

  /// 全量重建模块表（可在后台线程调用；root 变化或文件增删改后应重新调用）
  void rebuild(const QString &rootDir);

  /// 索引是否已就绪（root 非空）
  bool isReady() const { return !m_rootDir.isEmpty(); }
  /// 当前索引根目录
  const QString &rootDir() const { return m_rootDir; }

  /// 收集 (filePath, line, column) 处 name 符号的所有语义引用（含声明、import 子句）。
  /// 与重命名/查找引用共用，行为一致。
  /// @param liveContents 可选：文件路径 → 实时内存内容（已打开编辑器 + 未打开但有缓冲修改
  ///                     的文件）。提供时对这些文件优先读缓冲而非磁盘，保证重命名未保存期间
  ///                     对新名的引用收集一致。调用方在主线程构建后按值传入（后台线程安全）。
  QVector<RenameRef> findReferences(const QString &rootDir, const QString &triggerFilePath,
                                    int triggerLine, int triggerColumn, const QString &name,
                                    const QHash<QString, QString> &liveContents = {});

  /// 解析 (filePath, line, column) 处 name 符号的定义（跨文件 import / 别名 / 成员感知）。
  /// 返回声明信息；无法解析时返回的 key 为空。
  /// @param contentProvider 可选：文件路径 → 实时内存内容（已打开编辑器 / 缓冲文件）。
  ///                        提供时对触发文件与定义文件优先读缓冲而非磁盘快照，
  ///                        保证重命名未保存期间跳转定义一致。主线程调用。
  SemanticSymbol resolveDefinition(const QString &filePath, int line, int column,
                                   const QString &name,
                                   const std::function<QString(const QString &)> &contentProvider = {}) const;

  // ── 模块表查询（供其它模块 / 测试）──

  /// 顶层符号查询：文件 → 名字 → 符号
  const SemanticSymbol *findTopSymbol(const QString &filePath, const QString &name) const;
  /// 类成员查询：文件 → 类名 → 成员名 → 符号
  const SemanticSymbol *findMemberSymbol(const QString &filePath, const QString &className,
                                         const QString &memberName) const;
  /// import 绑定解析：localName → 来源文件（找不到返回空）
  QString importSourceOf(const QString &filePath, const QString &localName) const;

private:
  WorkspaceIndex() = default;
  WorkspaceIndex(const WorkspaceIndex &) = delete;
  WorkspaceIndex &operator=(const WorkspaceIndex &) = delete;

  /// 当前已构建的模块数据（每个文件一份）
  struct ModuleInfo;

  /// 解析源码文本并填充模块信息（顶层符号 / 类成员 / import），rebuild 与实时解析共用
  static void collectModuleSymbols(const QString &text, const QString &path, const QString &root,
                                   ModuleInfo &mi);

  QHash<QString, ModuleInfo *> m_modules;  ///< 文件绝对路径 → 模块信息
  QString m_rootDir;
};
