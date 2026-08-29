/**
 * @file symbol_rename.cpp
 * @brief 语义级符号引用收集兼容层
 *
 * 真正的实现已迁至工作区语义索引（src/engine/semantic/workspace_index.*）。
 * 这里保留 collectSymbolReferences 接口，转发给 WorkspaceIndex，保持
 * 「查找所有引用」与「重命名」的既有调用方不变，且两者共享同一套语义逻辑。
 */

#include "symbol_rename.h"

#include "src/engine/semantic/workspace_index.h"

QVector<RenameRef> collectSymbolReferences(const QString &rootDir,
                                           const QString &triggerFilePath, int triggerLine,
                                           int triggerColumn, const QString &name) {
  return WorkspaceIndex::ins().findReferences(rootDir, triggerFilePath, triggerLine,
                                              triggerColumn, name);
}

QVector<RenameRef> collectSymbolReferencesLive(
    const QString &rootDir, const QString &triggerFilePath, int triggerLine, int triggerColumn,
    const QString &name, const QHash<QString, QString> &liveContents) {
  return WorkspaceIndex::ins().findReferences(rootDir, triggerFilePath, triggerLine,
                                              triggerColumn, name, liveContents);
}
