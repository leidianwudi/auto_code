/** @file table_import_dialog.h
 * @brief 数据库差异预览只负责收集用户选择，不执行数据库或结构变更。
 */
#pragma once
#include "table_schema_service.h"
class QWidget;
TableSchema::ImportDiffResult runImportDiffDialog(
    QWidget *parent, const QString &tableName, const QVector<TableSchema::ImportDiffEntry> &entries,
    const QVector<TableSchema::IndexDiffEntry> &indexEntries, const QString &currentComment,
    const QString &dbComment, const QStringList &initialExcluded);
