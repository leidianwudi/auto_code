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

/// 后台线程执行：逐个验证文件并返回诊断（每文件独立验证器实例，线程安全）
QVector<WorkspaceFileDiag> scanWorkspaceDiagnostics(const QStringList &filePaths);
