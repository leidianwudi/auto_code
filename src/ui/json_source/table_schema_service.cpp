#include "table_schema_service.h"

#include <QJsonArray>
#include <QSet>
namespace TableSchema {
/// 反引号包裹标识符（内嵌反引号双写转义）
QString tick(QString ident) {
  ident.replace(QLatin1Char('`'), QStringLiteral("``"));
  return QStringLiteral("`%1`").arg(ident);
}

/// 单引号包裹 SQL 字面量（内嵌单引号双写转义）
QString quote(QString literal) {
  literal.replace(QLatin1Char('\''), QStringLiteral("''"));
  return QStringLiteral("'%1'").arg(literal);
}

/// 是否"数据库侧默认值"（建表 DDL 中不加引号）：含成对括号或命中时间关键字。
/// 与 file/crud_gin/template/core/param_go.ac 的 isDbDefault 保持一致，两处需同步维护。
bool isDbDefaultExpr(const QString &value) {
  const QString v = value.trimmed();
  if (v.isEmpty() || v.contains(QLatin1Char(';'))) return false;
  if (v.contains(QLatin1Char('(')) && v.contains(QLatin1Char(')'))) return true;
  const QString lv = v.toLower();
  return lv == QStringLiteral("current_timestamp") || lv == QStringLiteral("current_date") ||
         lv == QStringLiteral("current_time") || lv == QStringLiteral("localtimestamp") ||
         lv == QStringLiteral("now");
}

QString importColumnType(const JsonTableColumn &c) {
  return c.isUnsigned ? c.mysqlType + QStringLiteral(" unsigned") : c.mysqlType;
}

QString importValueText(const QString &v) { return v.isEmpty() ? QStringLiteral("（空）") : v; }

QString importColumnSummary(const JsonTableColumn &c) {
  QStringList parts;
  parts << importColumnType(c);
  parts << (c.nullable ? QStringLiteral("可空") : QStringLiteral("非空"));
  if (c.isPrimary) parts << QStringLiteral("主键");
  if (c.isAutoInc) parts << QStringLiteral("自增");
  if (!c.defaultValue.isEmpty()) parts << QStringLiteral("默认 %1").arg(c.defaultValue);
  if (!c.comment.isEmpty()) parts << c.comment;
  return parts.join(QStringLiteral(" · "));
}

QStringList importColumnChanges(const JsonTableColumn &a, const JsonTableColumn &b) {
  QStringList out;
  if (importColumnType(a).compare(importColumnType(b), Qt::CaseInsensitive) != 0) {
    out << QStringLiteral("类型 %1 → %2").arg(importColumnType(a), importColumnType(b));
  }
  if (a.nullable != b.nullable) {
    out << QStringLiteral("可空 %1 → %2")
               .arg(a.nullable ? QStringLiteral("是") : QStringLiteral("否"),
                    b.nullable ? QStringLiteral("是") : QStringLiteral("否"));
  }
  if (a.isPrimary != b.isPrimary) {
    out << QStringLiteral("主键 %1 → %2")
               .arg(a.isPrimary ? QStringLiteral("是") : QStringLiteral("否"),
                    b.isPrimary ? QStringLiteral("是") : QStringLiteral("否"));
  }
  if (a.isAutoInc != b.isAutoInc) {
    out << QStringLiteral("自增 %1 → %2")
               .arg(a.isAutoInc ? QStringLiteral("是") : QStringLiteral("否"),
                    b.isAutoInc ? QStringLiteral("是") : QStringLiteral("否"));
  }
  if (a.defaultValue != b.defaultValue) {
    out << QStringLiteral("默认值 %1 → %2")
               .arg(importValueText(a.defaultValue), importValueText(b.defaultValue));
  }
  if (a.comment != b.comment) {
    out << QStringLiteral("注释 %1 → %2")
               .arg(importValueText(a.comment), importValueText(b.comment));
  }
  return out;
}

QString importKindText(ImportDiffKind kind) {
  switch (kind) {
    case ImportDiffKind::Added:
      return QStringLiteral("新增");
    case ImportDiffKind::Modified:
      return QStringLiteral("修改");
    case ImportDiffKind::Removed:
      return QStringLiteral("删除");
    default:
      return QStringLiteral("未变");
  }
}

QString importEntryTooltip(const ImportDiffEntry &e) {
  if (e.excluded) return QStringLiteral("在「排除列」中，刷新时忽略该列");
  switch (e.kind) {
    case ImportDiffKind::Added:
      return QStringLiteral("数据库中新增的列，勾选后加入配置");
    case ImportDiffKind::Modified:
      return importColumnChanges(e.current, e.db).join(QStringLiteral("\n"));
    case ImportDiffKind::Removed:
      return QStringLiteral("数据库已无该列，勾选后从配置删除");
    default:
      return QString();
  }
}

/// 计算当前配置列与数据库列的差异（跳过无变化的列；排除列即使无变化也保留展示）
QVector<ImportDiffEntry> buildImportDiffEntries(const QVector<JsonTableColumn> &current,
                                                const QVector<JsonTableColumn> &db,
                                                const QStringList &excluded) {
  QHash<QString, int> currentIndexByKey;
  for (int i = 0; i < current.size(); ++i) {
    currentIndexByKey.insert(current[i].name.toLower(), i);
  }
  QSet<QString> excludedKeys;
  for (const auto &name : excluded) {
    const QString key = name.trimmed().toLower();
    if (!key.isEmpty()) excludedKeys.insert(key);
  }

  QVector<ImportDiffEntry> entries;
  QSet<QString> dbKeys;
  for (const auto &d : db) {
    const QString key = d.name.toLower();
    dbKeys.insert(key);
    ImportDiffEntry e;
    e.name = d.name;
    e.db = d;
    e.hasDb = true;
    const auto it = currentIndexByKey.constFind(key);
    if (it != currentIndexByKey.constEnd()) {
      e.current = current[*it];
      e.hasCurrent = true;
      e.kind = importColumnChanges(e.current, d).isEmpty() ? ImportDiffKind::Unchanged
                                                           : ImportDiffKind::Modified;
    } else {
      e.kind = ImportDiffKind::Added;
    }
    e.excluded = excludedKeys.contains(key);
    if (e.kind == ImportDiffKind::Unchanged && !e.excluded) continue;
    entries.append(e);
  }
  // 配置中存在、数据库已删除的列（按配置顺序追加）
  for (const auto &c : current) {
    if (dbKeys.contains(c.name.toLower())) continue;
    ImportDiffEntry e;
    e.name = c.name;
    e.current = c;
    e.hasCurrent = true;
    e.kind = ImportDiffKind::Removed;
    entries.append(e);
  }
  return entries;
}

/// 排除列写入 table.extra（空列表时移除键；生成期不消费该键）
void setExcludedColumns(JsonTableTable *table, const QStringList &columns) {
  if (!table) return;
  const QString key = QString::fromLatin1(JsonTableKey::kExcludeColumns);
  QStringList unique;
  for (const auto &name : columns) {
    const QString trimmed = name.trimmed();
    if (!trimmed.isEmpty() && !unique.contains(trimmed, Qt::CaseInsensitive)) {
      unique << trimmed;
    }
  }
  if (unique.isEmpty()) {
    table->extra.remove(key);
    return;
  }
  QJsonArray array;
  for (const auto &name : unique) array.append(name);
  table->extra.insert(key, array);
}

/// 清理被删除列（及其联表别名）在视图/查询/排序/编辑/枚举/多语言/按钮中的引用。
/// 只修改传入的表模型，不触碰界面；调用方决定确认后再 load 回填。
RefCleanupStats cleanRemovedColumnRefs(JsonTableTable *table, const QStringList &removedColumns) {
  RefCleanupStats stats;
  if (!table || removedColumns.isEmpty()) return stats;

  QSet<QString> removedKeys;
  for (const auto &name : removedColumns) {
    const QString key = name.trimmed().toLower();
    if (!key.isEmpty()) removedKeys.insert(key);
  }
  if (removedKeys.isEmpty()) return stats;

  // 联表外键列被删除 → 移除整条联表声明，其视图引用一并清理
  QVector<JsonTableJoin> joins;
  QSet<QString> droppedAliases;
  for (const auto &join : table->joinTable) {
    if (removedKeys.contains(join.joinFk.toLower())) {
      ++stats.joins;
      stats.details
          << QStringLiteral("联表「%1」（外键 %2）").arg(join.joinEnClassPro, join.joinFk);
      if (!join.joinEnClassPro.isEmpty()) droppedAliases.insert(join.joinEnClassPro.toLower());
      continue;
    }
    joins << join;
  }
  table->joinTable = joins;

  auto isDroppedRef = [&](const QString &ref) {
    const QString r = ref.trimmed();
    if (removedKeys.contains(r.toLower())) return true;
    const int dot = r.indexOf(QLatin1Char('.'));
    return dot > 0 && droppedAliases.contains(r.left(dot).toLower());
  };

  auto cleanRefVector = [&](auto &vector, int &counter, const QString &label) {
    const auto copy = vector;
    vector.clear();
    for (const auto &col : copy) {
      if (isDroppedRef(col.ref)) {
        ++counter;
        stats.details << QStringLiteral("%1 %2").arg(label, col.ref);
      } else {
        vector << col;
      }
    }
  };
  cleanRefVector(table->selCols, stats.selCols, QStringLiteral("列表列"));
  cleanRefVector(table->selColsEq, stats.queryCols, QStringLiteral("查询列"));
  cleanRefVector(table->selColsGap, stats.queryCols, QStringLiteral("范围查询列"));
  cleanRefVector(table->selColsLike, stats.queryCols, QStringLiteral("模糊查询列"));
  cleanRefVector(table->editCols, stats.editCols, QStringLiteral("编辑列"));

  QVector<QPair<QString, QString>> sorts;
  for (const auto &sort : table->selColsSort) {
    if (isDroppedRef(sort.first)) {
      ++stats.sortCols;
      stats.details << QStringLiteral("排序 %1").arg(sort.first);
    } else {
      sorts << sort;
    }
  }
  table->selColsSort = sorts;

  QVector<JsonTableEnum> enums;
  for (const auto &item : table->enums) {
    if (removedKeys.contains(item.column.toLower())) {
      ++stats.enums;
      stats.details << QStringLiteral("枚举列 %1").arg(item.column);
    } else {
      enums << item;
    }
  }
  table->enums = enums;

  QStringList globalEnums;
  for (const auto &name : table->globalEnumCols) {
    if (removedKeys.contains(name.toLower())) {
      ++stats.globalEnumCols;
      stats.details << QStringLiteral("全局枚举列 %1").arg(name);
    } else {
      globalEnums << name;
    }
  }
  table->globalEnumCols = globalEnums;

  QStringList i18nFields;
  for (const auto &name : table->i18n.fields) {
    if (removedKeys.contains(name.toLower())) {
      ++stats.i18nFields;
      stats.details << QStringLiteral("多语言字段 %1").arg(name);
    } else {
      i18nFields << name;
    }
  }
  table->i18n.fields = i18nFields;

  // 按钮对话框字段（fieldName 通常与表单项/字段同名）
  for (auto &button : table->buttons) {
    QVector<DialogFieldConfig> fields;
    for (const auto &field : button.dialogFields) {
      if (removedKeys.contains(field.fieldName.toLower())) {
        ++stats.buttonFields;
        stats.details << QStringLiteral("按钮「%1」字段 %2").arg(button.label, field.fieldName);
      } else {
        fields << field;
      }
    }
    button.dialogFields = fields;
  }

  return stats;
}

// ════════════════════════════════════════════════════════════
//  索引差异
// ════════════════════════════════════════════════════════════

bool sameIndexShape(const JsonTableIndex &a, const JsonTableIndex &b) {
  return a.unique == b.unique && a.cols == b.cols;
}

QString indexSummary(const JsonTableIndex &index) {
  return QStringLiteral("%1 · %2").arg(
      index.unique ? QStringLiteral("unique") : QStringLiteral("普通"),
      index.cols.join(QStringLiteral(", ")));
}

QStringList indexChanges(const JsonTableIndex &a, const JsonTableIndex &b) {
  QStringList out;
  if (a.cols != b.cols) {
    out << QStringLiteral("列 %1 → %2")
               .arg(a.cols.join(QStringLiteral(", ")), b.cols.join(QStringLiteral(", ")));
  }
  if (a.unique != b.unique) {
    out << QStringLiteral("unique %1 → %2")
               .arg(a.unique ? QStringLiteral("是") : QStringLiteral("否"),
                    b.unique ? QStringLiteral("是") : QStringLiteral("否"));
  }
  return out;
}

QVector<IndexDiffEntry> buildIndexDiffEntries(const QVector<JsonTableIndex> &current,
                                              const QVector<JsonTableIndex> &db) {
  QHash<QString, int> currentIndexByKey;
  for (int i = 0; i < current.size(); ++i) {
    currentIndexByKey.insert(current[i].name.toLower(), i);
  }
  QVector<IndexDiffEntry> entries;
  QSet<QString> dbKeys;
  for (const auto &d : db) {
    const QString key = d.name.toLower();
    dbKeys.insert(key);
    IndexDiffEntry e;
    e.name = d.name;
    e.db = d;
    e.hasDb = true;
    const auto it = currentIndexByKey.constFind(key);
    if (it != currentIndexByKey.constEnd()) {
      e.current = current[*it];
      e.hasCurrent = true;
      e.kind = sameIndexShape(e.current, d) ? ImportDiffKind::Unchanged : ImportDiffKind::Modified;
    } else {
      e.kind = ImportDiffKind::Added;
    }
    if (e.kind == ImportDiffKind::Unchanged) continue;
    entries.append(e);
  }
  for (const auto &c : current) {
    if (dbKeys.contains(c.name.toLower())) continue;
    IndexDiffEntry e;
    e.name = c.name;
    e.current = c;
    e.hasCurrent = true;
    e.kind = ImportDiffKind::Removed;
    entries.append(e);
  }
  return entries;
}

QString createDdlText(const QString &tableName, const QString &tableComment,
                      const QVector<JsonTableColumn> &cols,
                      const QVector<JsonTableIndex> &indexes) {
  QStringList defs;
  QStringList pkCols;
  for (const auto &c : cols) {
    if (c.isPrimary) pkCols.append(tick(c.name));
    QStringList parts;
    parts << tick(c.name) << c.mysqlType.trimmed();
    if (c.isUnsigned) parts << QStringLiteral("unsigned");
    parts << (c.nullable ? QStringLiteral("NULL") : QStringLiteral("NOT NULL"));
    if (!c.defaultValue.isEmpty()) {
      // 表达式默认值（now()/CURRENT_TIMESTAMP）不加引号，否则 datetime 列 DDL 非法
      parts << QStringLiteral("DEFAULT %1")
                   .arg(isDbDefaultExpr(c.defaultValue) ? c.defaultValue.trimmed()
                                                        : quote(c.defaultValue));
    }
    if (c.isAutoInc) parts << QStringLiteral("AUTO_INCREMENT");
    if (!c.comment.isEmpty()) parts << QStringLiteral("COMMENT %1").arg(quote(c.comment));
    defs << QStringLiteral("  ") + parts.join(QLatin1Char(' '));
  }
  if (!pkCols.isEmpty())
    defs << QStringLiteral("  PRIMARY KEY (%1)").arg(pkCols.join(QStringLiteral(", ")));
  for (const auto &ix : indexes) {
    if (ix.cols.isEmpty()) continue;  // 无列的索引不落 DDL
    QStringList colTicks;
    for (const QString &c : ix.cols) colTicks.append(tick(c));
    defs << QStringLiteral("  %1 %2 (%3)")
                .arg(ix.unique ? QStringLiteral("UNIQUE KEY") : QStringLiteral("KEY"),
                     tick(ix.name), colTicks.join(QStringLiteral(", ")));
  }

  QString sql = QStringLiteral("CREATE TABLE %1 (\n%2\n) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4")
                    .arg(tick(tableName), defs.join(QStringLiteral(",\n")));
  if (!tableComment.isEmpty()) sql += QStringLiteral(" COMMENT=%1").arg(quote(tableComment));
  sql += QLatin1Char(';');
  return sql;
}

bool validateCrossRefs(const JsonTableTable &table, const JsonTableConfig &config,
                       const QVector<JsonTableTable> &projectTables,
                       const QString &originalTableName, QString *err) {
  if (err) err->clear();
  auto fail = [err](const QString &msg) {
    if (err) *err = msg;
    return false;
  };

  // ── 基础项（与 validateForDdl 同源）──
  const QString tableName = table.tableName.trimmed();
  if (tableName.isEmpty()) return fail(QStringLiteral("表名不能为空"));
  const QVector<JsonTableColumn> cols = table.columns;
  if (cols.isEmpty()) return fail(QStringLiteral("请先添加字段列"));

  // 列名集合（重复列名会让 DDL/生成产生歧义）
  QSet<QString> colNames;
  for (const auto &c : cols) {
    if (colNames.contains(c.name)) return fail(QStringLiteral("列名 '%1' 重复").arg(c.name));
    colNames.insert(c.name);
  }

  // 表名与同文件其它表重复（生成期同名表会被跳过并报错）
  for (const auto &t : config.tables) {
    if (t.tableName == tableName && t.tableName != originalTableName)
      return fail(QStringLiteral("表名 '%1' 与文件中其它表重复").arg(tableName));
  }

  // ── 表内枚举：枚举列须在字段列表中且互不重复 ──
  const QVector<JsonTableEnum> enums = table.enums;
  QSet<QString> enumCols;
  for (int i = 0; i < enums.size(); ++i) {
    const JsonTableEnum &e = enums[i];
    if (e.column.isEmpty()) return fail(QStringLiteral("第 %1 个表内枚举未填枚举列").arg(i + 1));
    if (!colNames.contains(e.column))
      return fail(QStringLiteral("表内枚举列 '%1' 不在字段列表中").arg(e.column));
    if (enumCols.contains(e.column))
      return fail(QStringLiteral("列 '%1' 重复配置表内枚举").arg(e.column));
    enumCols.insert(e.column);
  }

  // ── 全局枚举列：列名=枚举名，须在字段列表中；与表内枚举二选一 ──
  // （.jsonglobalenum 文件中枚举是否存在由生成期校验，设计器无文件上下文）
  for (const QString &ge : table.globalEnumCols) {
    if (!colNames.contains(ge))
      return fail(QStringLiteral("全局枚举列 '%1' 不在字段列表中（列名须与 .jsonglobalenum "
                                 "枚举名一致）")
                      .arg(ge));
    if (enumCols.contains(ge))
      return fail(QStringLiteral("列 '%1' 同时配置了表内枚举与全局枚举（二选一）").arg(ge));
  }

  // ── 联表查询声明（joinTables 新格式）：表/类型/ON 两列/输出列可解析 ──
  const QVector<JsonTableJoinSpec> joinSpecs = table.joinTables;
  QSet<QString> joinAliases;  // 输出别名（表名_列名）去重 + 主表列冲突校验
  const QVector<JsonTableTable> &joinProjectTables = projectTables;
  auto targetColumnsOf = [&](const QString &tbl) -> QStringList {
    for (const auto &t : joinProjectTables) {
      if (t.tableName == tbl) {
        QStringList cols;
        for (const auto &c : t.columns) cols.append(c.name);
        return cols;
      }
    }
    return {};
  };
  for (int i = 0; i < joinSpecs.size(); ++i) {
    const JsonTableJoinSpec &j = joinSpecs[i];
    if (j.table.isEmpty()) return fail(QStringLiteral("第 %1 条联表未填被联表名").arg(i + 1));
    const QStringList tcols = targetColumnsOf(j.table);
    if (tcols.isEmpty())
      return fail(QStringLiteral("联表 '%1' 的目标表不在项目 .jsontable 中").arg(j.table));
    if (j.type != QStringLiteral("LEFT") && j.type != QStringLiteral("INNER"))
      return fail(QStringLiteral("联表 '%1' 的 type 仅支持 LEFT/INNER").arg(j.table));
    if (j.on.size() != 2 || j.on.at(0).isEmpty() || j.on.at(1).isEmpty())
      return fail(QStringLiteral("联表 '%1' 的 on 须为 [主表列, 联表列]").arg(j.table));
    if (!colNames.contains(j.on.at(0)))
      return fail(
          QStringLiteral("联表 '%1' 的主表列 '%2' 不在字段列表中").arg(j.table, j.on.at(0)));
    if (!tcols.contains(j.on.at(1)))
      return fail(
          QStringLiteral("联表 '%1' 的联表列 '%2' 不在目标表列中").arg(j.table, j.on.at(1)));
    for (const auto &c : j.cols) {
      if (!tcols.contains(c.col))
        return fail(QStringLiteral("联表 '%1' 的输出列 '%2' 不在目标表列中").arg(j.table, c.col));
      if (!c.filter.isEmpty() && c.filter != QStringLiteral("like") &&
          c.filter != QStringLiteral("eq"))
        return fail(
            QStringLiteral("联表 '%1' 输出列 '%2' 的 filter 仅支持 like/eq").arg(j.table, c.col));
      const QString alias = j.table + QLatin1Char('_') + c.col;
      if (joinAliases.contains(alias))
        return fail(QStringLiteral("联表输出别名 '%1' 重复").arg(alias));
      if (colNames.contains(alias))
        return fail(QStringLiteral("联表输出别名 '%1' 与主表列名冲突").arg(alias));
      joinAliases.insert(alias);
    }
  }

  // ── 读模型 ref 可解析（本表列名 / 新格式输出别名 / 旧格式 引用名.目标列）──
  QHash<QString, const JsonTableTable *> joinsByPro;  // 旧格式：引用名 → 目标表
  for (const auto &j : table.joinTable) {
    if (!j.isJoinRelations || j.joinType != QStringLiteral("ManyToOne")) continue;
    for (const auto &t : config.tables) {
      if (t.modelName == j.joinEnClass) {
        joinsByPro.insert(j.joinEnClassPro, &t);
        break;
      }
    }
  }
  auto refOk = [&](const QString &ref, QString *aliasOut) -> bool {
    const int dot = ref.indexOf(QLatin1Char('.'));
    if (dot <= 0)
      return colNames.contains(ref) || joinAliases.contains(ref);  // 本表列/联表输出别名
    const QString pro = ref.left(dot).trimmed();
    const QString col = ref.mid(dot + 1).trimmed();
    if (col.isEmpty() || !joinsByPro.contains(pro)) return false;
    bool has = false;
    for (const auto &c : joinsByPro.value(pro)->columns) {
      if (c.name == col) {
        has = true;
        break;
      }
    }
    if (has && aliasOut) *aliasOut = pro + QLatin1Char('_') + col;
    return has;
  };
  auto checkRef = [&](const QString &kind, const QString &ref) -> bool {
    QString alias;
    if (!refOk(ref, &alias))
      return fail(QStringLiteral("%1 '%2' 无法解析（须为本表列名或已启用 ManyToOne 联表的"
                                 "「引用名.目标列」）")
                      .arg(kind, ref));
    if (!alias.isEmpty() && colNames.contains(alias))
      return fail(QStringLiteral("联表别名 '%1' 与主表列名冲突（ref %2）").arg(alias, ref));
    return true;
  };
  // ── 字段呈现统一表格 → 三视图（交叉校验基于收集产物）──
  const JsonTableTable &view = table;
  for (const auto &c : view.selCols)
    if (!checkRef(QStringLiteral("列表列"), c.ref)) return false;
  for (const auto &c : view.selColsEq)
    if (!checkRef(QStringLiteral("精确查询列"), c.ref)) return false;
  for (const auto &c : view.selColsGap)
    if (!checkRef(QStringLiteral("范围查询列"), c.ref)) return false;
  for (const auto &c : view.selColsLike)
    if (!checkRef(QStringLiteral("模糊查询列"), c.ref)) return false;

  // ── 编辑视图（editCols）：ref 为物理列或联表输出别名（只读展示）、去重、
  //    required 与 invisible 互斥 ──
  const QVector<JsonTableEditCol> editRows = view.editCols;
  QSet<QString> editRefs;
  for (int i = 0; i < editRows.size(); ++i) {
    const JsonTableEditCol &ec = editRows[i];
    const bool isAliasRef = joinAliases.contains(ec.ref);
    if (!colNames.contains(ec.ref) && !isAliasRef)
      return fail(QStringLiteral("编辑列 '%1' 不在字段列表中").arg(ec.ref));
    if (!isAliasRef && ec.editEditable == false && ec.required)
      return fail(QStringLiteral("编辑列 '%1' 只读展示与必填互斥").arg(ec.ref));
    if (editRefs.contains(ec.ref)) return fail(QStringLiteral("编辑列 '%1' 重复配置").arg(ec.ref));
    editRefs.insert(ec.ref);
    if (ec.required && !ec.visible)
      return fail(QStringLiteral("编辑列 '%1' 必填但前端显示关闭（矛盾配置）").arg(ec.ref));
  }
  for (const auto &p : table.selColsSort)
    if (!checkRef(QStringLiteral("默认排序列"), p.first)) return false;

  // ── 索引列存在性 ──
  for (const auto &ix : table.indexes)
    for (const QString &c : ix.cols)
      if (!colNames.contains(c))
        return fail(QStringLiteral("索引 '%1' 的列 '%2' 不在字段列表中").arg(ix.name, c));

  // ── i18n 引用完整性（对齐生成期 main_api 校验）──
  if (table.i18n.isEnabled()) {
    const QString i18nTable = table.i18n.table.trimmed();
    if (i18nTable.isEmpty()) return fail(QStringLiteral("启用多语言后翻译表不能为空"));
    // 翻译表列集解析顺序：显式配置（本文件 tables[] / 同目录其它 .jsontable）
    // 优先；都没有时按惯例派生（deriveI18nTable 保证 extKey/langKey/fields
    // 列齐备）。单表新格式下本文件没有 tables[]，旧"必须在 tables[] 中"
    // 的检查已不适用
    JsonTableTable trans;
    bool transFound = false;
    for (const auto &t : config.tables) {
      if (t.tableName == i18nTable) {
        trans = t;
        transFound = true;
        break;
      }
    }
    if (!transFound) {
      for (const auto &t : projectTables) {
        if (t.tableName == i18nTable) {
          trans = t;
          transFound = true;
          break;
        }
      }
    }
    if (!transFound) trans = deriveI18nTable(table);
    QSet<QString> transCols;
    for (const auto &c : trans.columns) transCols.insert(c.name);
    const QString extKey = table.i18n.extKey.trimmed();
    const QString langKey = table.i18n.langKey.trimmed();
    if (extKey.isEmpty() || !transCols.contains(extKey))
      return fail(QStringLiteral("i18n 外键列 '%1' 不在翻译表 '%2' 的列中")
                      .arg(extKey.isEmpty() ? QStringLiteral("(空)") : extKey, i18nTable));
    if (langKey.isEmpty() || !transCols.contains(langKey))
      return fail(QStringLiteral("i18n 语言列 '%1' 不在翻译表 '%2' 的列中")
                      .arg(langKey.isEmpty() ? QStringLiteral("(空)") : langKey, i18nTable));
    for (const QString &f : table.i18n.fields) {
      if (!transCols.contains(f))
        return fail(QStringLiteral("多语言字段 '%1' 不在翻译表 '%2' 的列中").arg(f, i18nTable));
      if (colNames.contains(f)) return fail(QStringLiteral("多语言字段 '%1' 与主表列重名").arg(f));
    }
  }
  return true;
}

}  // namespace TableSchema
