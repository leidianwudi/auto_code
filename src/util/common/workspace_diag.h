/**
 * @file workspace_diag.h
 * @brief 工作区全量诊断 — 后台线程扫描所有可验证文件的错误
 *
 * 在后台线程逐个验证工作区文件（ac/json/jsonvue/tpl），产出每个文件的诊断列表，
 * 供底部「问题」面板跨文件聚合展示（VSCode 风格：未打开的文件也有诊断）。
 *
 * 线程安全：每个文件使用独立实例化的验证器（AcValidator/TplValidator/JsonValidator/
 * SchemaValidator），无共享可变状态，可在后台线程安全调用。
 */

#pragma once

#include <QHash>
#include <QStringList>
#include <QVector>

#include "src/engine/validation_result.h"

/// 工作区单个文件的诊断结果（后台扫描产出）
struct WorkspaceFileDiag {
  QString filePath;                 ///< 文件绝对路径
  QVector<ValidationResult> issues; ///< 验证结果列表（空表示无错误）
};

/// 收集工作区内所有可验证文件（主线程调用，返回绝对路径列表）
QStringList collectWorkspaceFiles(const QString &rootDir);

/// 后台线程执行：逐个验证文件并返回诊断（每文件独立验证器实例，线程安全）。
/// @param liveContents 已打开文件的实时内存内容（文件路径 → 内容）。扫描时对这些文件
///                     使用内存内容而非磁盘快照（VSCode 行为：诊断基于当前缓冲内容，
///                     避免重命名等操作后磁盘与内存不一致导致假报错）。空表则全部读磁盘。
QVector<WorkspaceFileDiag> scanWorkspaceDiagnostics(
    const QStringList &filePaths, const QHash<QString, QString> &liveContents = {});
