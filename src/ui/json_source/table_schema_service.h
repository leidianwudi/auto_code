/** @file table_schema_service.h
 * @brief 表结构比较、引用校验和 DDL 生成；只依赖 QtCore 与配置模型。
 */
#pragma once
#include <QHash>

#include "json_table_model.h"
namespace TableSchema {
enum class ImportDiffKind { Added, Modified, Removed, Unchanged };

struct ImportDiffEntry {
  QString name;  ///< 列名（保持数据库/配置中的原始大小写）
  ImportDiffKind kind = ImportDiffKind::Unchanged;
  JsonTableColumn current;  ///< 当前配置列（hasCurrent=false 时无效）
  JsonTableColumn db;       ///< 数据库列（hasDb=false 时无效）
  bool hasCurrent = false;
  bool hasDb = false;
  bool excluded = false;  ///< 是否在排除列列表中（刷新时忽略）
};

struct ImportDiffResult {
  bool accepted = false;
  QHash<QString, bool> applyByKey;       ///< 列名小写 → 是否应用（已排除的列恒为 false）
  QHash<QString, bool> indexApplyByKey;  ///< 索引名小写 → 是否应用
  QStringList excludedColumns;           ///< 应用后的排除列列表
  bool applyTableComment = false;        ///< 是否同步表注释
};

struct RefCleanupStats {
  int selCols = 0;
  int queryCols = 0;
  int sortCols = 0;
  int editCols = 0;
  int joins = 0;
  int enums = 0;
  int globalEnumCols = 0;
  int i18nFields = 0;
  int buttonFields = 0;
  QStringList details;

  int total() const {
    return selCols + queryCols + sortCols + editCols + joins + enums + globalEnumCols + i18nFields +
           buttonFields;
  }

  QString summary() const {
    QStringList parts;
    if (selCols > 0) parts << QStringLiteral("列表 %1").arg(selCols);
    if (queryCols > 0) parts << QStringLiteral("查询 %1").arg(queryCols);
    if (sortCols > 0) parts << QStringLiteral("排序 %1").arg(sortCols);
    if (editCols > 0) parts << QStringLiteral("编辑 %1").arg(editCols);
    if (joins > 0) parts << QStringLiteral("联表 %1").arg(joins);
    if (enums > 0) parts << QStringLiteral("枚举 %1").arg(enums);
    if (globalEnumCols > 0) parts << QStringLiteral("全局枚举 %1").arg(globalEnumCols);
    if (i18nFields > 0) parts << QStringLiteral("多语言字段 %1").arg(i18nFields);
    if (buttonFields > 0) parts << QStringLiteral("按钮字段 %1").arg(buttonFields);
    return parts.join(QStringLiteral("、"));
  }
};

struct IndexDiffEntry {
  QString name;
  ImportDiffKind kind = ImportDiffKind::Unchanged;
  JsonTableIndex current;
  JsonTableIndex db;
  bool hasCurrent = false;
  bool hasDb = false;
};

/// 生成数据库结构差异，保留配置顺序并按排除列标记。
QVector<ImportDiffEntry> buildImportDiffEntries(const QVector<JsonTableColumn> &current,
                                                const QVector<JsonTableColumn> &db,
                                                const QStringList &excluded);
QVector<IndexDiffEntry> buildIndexDiffEntries(const QVector<JsonTableIndex> &current,
                                              const QVector<JsonTableIndex> &db);
/// 删除物理列后清理查询、表单、联表、枚举和按钮的失效引用。
RefCleanupStats cleanRemovedColumnRefs(JsonTableTable *table, const QStringList &removedColumns);
/// 保存设计器排除列配置，空列表移除配置键。
void setExcludedColumns(JsonTableTable *table, const QStringList &columns);
/// 校验收集后的表模型；项目表快照由宿主提供，校验过程不访问控件或磁盘。
bool validateCrossRefs(const JsonTableTable &table, const JsonTableConfig &config,
                       const QVector<JsonTableTable> &projectTables,
                       const QString &originalTableName, QString *err);
/// 生成带标识符/字面量转义的建表语句。
QString createDdlText(const QString &tableName, const QString &tableComment,
                      const QVector<JsonTableColumn> &cols, const QVector<JsonTableIndex> &indexes);
/// 数据库结构差异的展示文案，供预览页复用。
QString importColumnSummary(const JsonTableColumn &column);
QString importEntryTooltip(const ImportDiffEntry &entry);
QString importKindText(ImportDiffKind kind);
QString indexSummary(const JsonTableIndex &index);
QStringList indexChanges(const JsonTableIndex &current, const JsonTableIndex &db);
/// 转义 SQL 字面量，供结构查询使用。
QString quote(QString literal);

}  // namespace TableSchema
