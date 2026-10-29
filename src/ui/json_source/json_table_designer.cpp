/**
 * @file json_table_designer.cpp
 * @brief .jsontable 表设计器面板实现（可视化编辑器内嵌，逐编辑广播）
 */

#include "json_table_designer.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QHelpEvent>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QSplitter>
#include <QStringListModel>
#include <QStyleOptionHeader>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidget>

#include "src/engine/ac_language.h"
#include "src/engine/function/fun_db.h"
#include "src/engine/function/fun_mgr.h"
#include "src/ui/json_vue/button_config_dialog.h"
#include "src/ui/json_vue/config_dialog_common.h"
#include "src/ui/json_vue/json_vue_model.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/component/aui_icon.h"
#include "src/util/ui/component/aui_input_dialog.h"
#include "src/util/ui/component/aui_message_box.h"
#include "src/util/ui/component/aui_splitter.h"
#include "src/util/ui/component/aui_style.h"
#include "view_field_config_dialog.h"

// ════════════════════════════════════════════════════════════
//  表格列索引 / 局部工具
// ════════════════════════════════════════════════════════════

namespace {

/// 字段表格列索引（页面呈现全部由三视图接管，字段表格只留纯物理属性；
/// 历史 列表/搜索/表单角色/必填 列均已退役）
enum ColCols {
  ColColName = 0,  ///< 列名
  ColColType,      ///< MySQL 类型（可编辑下拉）
  ColColUnsigned,  ///< unsigned 复选框
  ColColNullable,  ///< 可空 复选框
  ColColPrimary,   ///< 主键 复选框
  ColColAutoInc,   ///< 自增 复选框
  ColColDefault,   ///< 默认值
  ColColComment,   ///< 注释
  ColColCount
};

/// 联表声明表格列索引（joinTable）
enum JoinCols {
  JoinColTable = 0,  ///< 被联表名（joinTables[].table，可编辑下拉=项目表名）
  JoinColType,       ///< JOIN 类型（LEFT/INNER 下拉）
  JoinColOnMain,     ///< on[0] 主表列（可编辑下拉=本表列名）
  JoinColOnJoin,     ///< on[1] 被联表列（目标表列名）
  JoinColOutCols,    ///< 输出列（逗号分隔列名；col:filter 旧写法仍兼容解析）
  JoinColCount
};

/// 字段呈现统一表格列索引（一行 = 一个字段：列表/查询/编辑三视图合一）
enum UCols {
  UColRef = 0,   ///< 字段引用（本表列名 或 引用名.目标列，可编辑下拉）
  UColTitle,     ///< 标题（生效值 = selCols.label > 列注释 > ref；写回 selCols.label）
  UColList,      ///< 列表勾选（在 selCols 中）
  UColQuery,     ///< 查询模式（cell widget：不查询/精确/模糊/范围/仅接口）
  UColEdit,      ///< 编辑勾（☑进编辑表单/☐不进表单；编辑/新增/详情复用同一 write.vue）
  UColRequired,  ///< 必填勾选（editCols.required）
  UColCfg,       ///< 配置（⚙ + 一行式摘要合成一列：点击打开字段设置面板）
  UColCount
};

/// 索引表格列索引
enum IndexCols {
  IndexColName = 0,  ///< 索引名
  IndexColCols,      ///< 列（逗号分隔文本）
  IndexColUnique,    ///< unique 复选框
  IndexColCount
};

/// 可编辑下拉框委托：预设项下拉 + 自由输入，提交编辑框文本
class EditableComboDelegate : public QStyledItemDelegate {
public:
  EditableComboDelegate(QStringList options, QObject *parent = nullptr)
      : QStyledItemDelegate(parent), m_options(std::move(options)) {}

  /// 动态更新下拉预设（如枚举列候选跟随字段表格列名变化）
  void setOptions(const QStringList &options) { m_options = options; }

  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                        const QModelIndex &) const override {
    QComboBox *combo = AuiComboBox::create(parent);
    combo->setEditable(true);
    for (const QString &opt : m_options) combo->addItem(opt);
    // 选中预设项后立即提交并关闭编辑器（自由输入走失焦提交）
    QObject::connect(combo, &QComboBox::activated, combo, [this, combo]() {
      emit const_cast<EditableComboDelegate *>(this)->commitData(combo);
      emit const_cast<EditableComboDelegate *>(this)->closeEditor(combo);
    });
    return combo;
  }

  void setEditorData(QWidget *editor, const QModelIndex &index) const override {
    if (auto *combo = qobject_cast<QComboBox *>(editor))
      combo->setEditText(index.data(Qt::EditRole).toString());
  }

  void setModelData(QWidget *editor, QAbstractItemModel *model,
                    const QModelIndex &index) const override {
    if (auto *combo = qobject_cast<QComboBox *>(editor))
      model->setData(index, combo->currentText().trimmed(), Qt::EditRole);
  }

private:
  QStringList m_options;  ///< 下拉预设项（仍可自由输入）
};

/// 逗号分隔多值补全器：按最后一个逗号后的片段匹配，选中后拼回前缀
/// （全局枚举列 / 多语言字段等 "a,b,c" 形态输入框共用）
class CsvCompleter : public QCompleter {
public:
  explicit CsvCompleter(QLineEdit *edit) : QCompleter(edit), m_model(new QStringListModel(this)) {
    setModel(m_model);
  }

  /// 更新候选列表
  void setCandidates(const QStringList &items) { m_model->setStringList(items); }

  /// 匹配片段 = 最后一个逗号之后的文本（去空格）
  QStringList splitPath(const QString &path) const override {
    return QStringList{path.mid(path.lastIndexOf(QLatin1Char(',')) + 1).trimmed()};
  }

  /// 选中候选后拼回逗号前缀（"a,b," + 候选）
  QString pathFromIndex(const QModelIndex &index) const override {
    const QString tail = index.data(Qt::EditRole).toString();
    QString prefix;
    if (auto *edit = qobject_cast<QLineEdit *>(widget())) {
      const int pos = edit->text().lastIndexOf(QLatin1Char(','));
      if (pos >= 0) prefix = edit->text().left(pos + 1) + QStringLiteral(" ");
    }
    return prefix + tail;
  }

private:
  QStringListModel *m_model;  ///< 候选模型（父对象为本补全器，随构造销毁）
};

/// 复选框列条目（无可编辑文本，点击勾选框直接切换）
QTableWidgetItem *makeCheckItem(bool on) {
  auto *it = new QTableWidgetItem;
  it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
  it->setCheckState(on ? Qt::Checked : Qt::Unchecked);
  it->setTextAlignment(Qt::AlignCenter);
  return it;
}

/// 通用表格单元格文本（空表/缺条目返回空串；截断空白）
QString cellOf(const QTableWidget *table, int row, int col) {
  const QTableWidgetItem *it = table ? table->item(row, col) : nullptr;
  return it ? it->text().trimmed() : QString();
}

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

/// 弹窗显示 FunMgr 错误通道里的最近错误（空则用 fallback 文案）
void showFunDbError(QWidget *parent, const QString &title, const QString &fallback) {
  QString err = FunMgr::takeError();
  if (err.isEmpty()) err = fallback;
  AuiMessageBox::show(parent, title, err);
}

/// 用 meta.db 四件套 + port 建立连接（FunDb::constructor）；失败弹窗并返回 false
bool connectDb(const JsonTableConfig &config, accore::AcJsonValue *instance, QWidget *parent) {
  accore::AcJsonValue cfg = accore::AcJsonValue::makeObject();
  cfg.set(QString::fromLatin1(AcDB::kHost), accore::AcJsonValue(config.dbHost));
  cfg.set(QString::fromLatin1(AcDB::kPort), accore::AcJsonValue(config.dbPort));
  cfg.set(QString::fromLatin1(AcDB::kUser), accore::AcJsonValue(config.dbUser));
  cfg.set(QString::fromLatin1(AcDB::kPassword), accore::AcJsonValue(config.dbPassword));
  cfg.set(QString::fromLatin1(AcDB::kDatabase), accore::AcJsonValue(config.dbDatabase));
  accore::AcJsonValue args = accore::AcJsonValue::makeArray();
  args.append(cfg);
  *instance = FunDb::constructor(args);
  if (!instance->isObject() ||
      !instance->value(QString::fromLatin1(AcDB::kConnected)).toBool(false)) {
    showFunDbError(parent, QStringLiteral("连接失败"), QStringLiteral("数据库连接失败"));
    return false;
  }
  return true;
}

// ════════════════════════════════════════════════════════════
//  从数据库刷新结构：差异计算 + 差异预览对话框
// ════════════════════════════════════════════════════════════

/// 单列差异类别
enum class ImportDiffKind { Added, Modified, Removed, Unchanged };

/// 差异预览的一行
struct ImportDiffEntry {
  QString name;  ///< 列名（保持数据库/配置中的原始大小写）
  ImportDiffKind kind = ImportDiffKind::Unchanged;
  JsonTableColumn current;  ///< 当前配置列（hasCurrent=false 时无效）
  JsonTableColumn db;       ///< 数据库列（hasDb=false 时无效）
  bool hasCurrent = false;
  bool hasDb = false;
  bool excluded = false;  ///< 是否在排除列列表中（刷新时忽略）
};

/// 差异预览结果（applyByKey 的 key 为列名小写）
struct ImportDiffResult {
  bool accepted = false;
  QHash<QString, bool> applyByKey;       ///< 列名小写 → 是否应用（已排除的列恒为 false）
  QHash<QString, bool> indexApplyByKey;  ///< 索引名小写 → 是否应用
  QStringList excludedColumns;           ///< 应用后的排除列列表
  bool applyTableComment = false;        ///< 是否同步表注释
};

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

QColor importKindColor(ImportDiffKind kind) {
  switch (kind) {
    case ImportDiffKind::Added:
      return AuiStyle::successTextColor();
    case ImportDiffKind::Modified:
      return AuiStyle::modifiedTextColor();
    case ImportDiffKind::Removed:
      return AuiStyle::errorTextColor();
    default:
      return AuiStyle::mutedTextColor();
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

/// 删除列后前端引用清理统计
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

struct IndexDiffEntry {
  QString name;
  ImportDiffKind kind = ImportDiffKind::Unchanged;
  JsonTableIndex current;
  JsonTableIndex db;
  bool hasCurrent = false;
  bool hasDb = false;
};

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

/// 差异预览对话框：逐列勾选应用/排除，返回用户选择（取消时 accepted=false）
ImportDiffResult runImportDiffDialog(QWidget *parent, const QString &tableName,
                                     const QVector<ImportDiffEntry> &entries,
                                     const QVector<IndexDiffEntry> &indexEntries,
                                     const QString &currentComment, const QString &dbComment,
                                     const QStringList &initialExcluded) {
  ImportDiffResult result;
  QDialog dlg(parent);
  dlg.setMinimumWidth(760);
  ConfigDialogFrame frame = beginConfigDialog(
      &dlg, QStringLiteral("从数据库刷新结构：%1").arg(tableName), QMargins(12, 10, 12, 10), 6);

  int added = 0;
  int modified = 0;
  int removed = 0;
  int excluded = 0;
  for (const auto &e : entries) {
    if (e.excluded) ++excluded;
    switch (e.kind) {
      case ImportDiffKind::Added:
        ++added;
        break;
      case ImportDiffKind::Modified:
        ++modified;
        break;
      case ImportDiffKind::Removed:
        ++removed;
        break;
      default:
        break;
    }
  }
  int indexAdded = 0;
  int indexModified = 0;
  int indexRemoved = 0;
  for (const auto &e : indexEntries) {
    switch (e.kind) {
      case ImportDiffKind::Added:
        ++indexAdded;
        break;
      case ImportDiffKind::Modified:
        ++indexModified;
        break;
      case ImportDiffKind::Removed:
        ++indexRemoved;
        break;
      default:
        break;
    }
  }
  QString summaryText =
      QStringLiteral(
          "共 %1 项列差异：新增 %2 / 修改 %3 / 删除 %4 / 已排除 %5。\n"
          "勾选要应用的变更；删除列默认不勾选，排除列不会被新增或修改。\n"
          "勾选删除列时，会同时清理列表/查询/排序/编辑/联表/枚举/按钮中的相关引用。")
          .arg(entries.size())
          .arg(added)
          .arg(modified)
          .arg(removed)
          .arg(excluded);
  if (!indexEntries.isEmpty()) {
    summaryText +=
        QStringLiteral("\n索引差异：新增 %1 / 修改 %2 / 删除 %3（默认全部应用，不涉及数据行）。")
            .arg(indexAdded)
            .arg(indexModified)
            .arg(indexRemoved);
  }
  auto *summary = new QLabel(summaryText, &dlg);
  summary->setWordWrap(true);
  summary->setStyleSheet(QStringLiteral("color: %1;").arg(AuiStyle::mutedTextColor().name()));
  frame.contentLayout->addWidget(summary);

  QCheckBox *commentCheck = nullptr;
  if (!dbComment.isEmpty() && dbComment != currentComment) {
    commentCheck = new QCheckBox(
        QStringLiteral("同步表注释：%1 → %2")
            .arg(currentComment.isEmpty() ? QStringLiteral("（空）") : currentComment, dbComment),
        &dlg);
    commentCheck->setChecked(true);
    frame.contentLayout->addWidget(commentCheck);
  }

  auto *btnRow = new QHBoxLayout;
  auto *selectAllBtn = makeCompactButton(QStringLiteral("全选变更"), &dlg);
  auto *selectNoneBtn = makeCompactButton(QStringLiteral("全不选"), &dlg);
  btnRow->addWidget(selectAllBtn);
  btnRow->addWidget(selectNoneBtn);
  btnRow->addStretch();
  frame.contentLayout->addLayout(btnRow);

  enum DiffCol { DiffApply = 0, DiffName, DiffStatus, DiffCurrent, DiffDb, DiffExclude };
  auto *table = makeConfigTable({{QStringLiteral("应用"), QHeaderView::Fixed, 60},
                                 {QStringLiteral("列名"), QHeaderView::Interactive, 140},
                                 {QStringLiteral("变更"), QHeaderView::Fixed, 80},
                                 {QStringLiteral("当前配置"), QHeaderView::Stretch, 0},
                                 {QStringLiteral("数据库"), QHeaderView::Stretch, 0},
                                 {QStringLiteral("排除"), QHeaderView::Fixed, 60}},
                                &dlg, 260, 0, QAbstractItemView::SelectRows);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->verticalHeader()->setDefaultSectionSize(28);
  frame.contentLayout->addWidget(table, 1);

  for (const auto &e : entries) {
    const int row = table->rowCount();
    table->insertRow(row);

    const bool defaultApply =
        !e.excluded && (e.kind == ImportDiffKind::Added || e.kind == ImportDiffKind::Modified);
    QTableWidgetItem *apply = makeCheckItem(defaultApply);
    if (e.excluded) apply->setFlags(apply->flags() & ~Qt::ItemIsUserCheckable);
    table->setItem(row, DiffApply, apply);

    auto *name = new QTableWidgetItem(e.name);
    name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    table->setItem(row, DiffName, name);

    auto *status =
        new QTableWidgetItem(e.excluded ? QStringLiteral("已排除") : importKindText(e.kind));
    status->setForeground(
        QBrush(e.excluded ? AuiStyle::mutedTextColor() : importKindColor(e.kind)));
    status->setToolTip(importEntryTooltip(e));
    table->setItem(row, DiffStatus, status);

    auto *cur =
        new QTableWidgetItem(e.hasCurrent ? importColumnSummary(e.current) : QStringLiteral("-"));
    cur->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    cur->setToolTip(cur->text());
    table->setItem(row, DiffCurrent, cur);

    auto *db = new QTableWidgetItem(e.hasDb ? importColumnSummary(e.db) : QStringLiteral("-"));
    db->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    db->setToolTip(db->text());
    table->setItem(row, DiffDb, db);

    QTableWidgetItem *exclude = nullptr;
    if (e.hasDb) {
      exclude = makeCheckItem(e.excluded);
      exclude->setToolTip(QStringLiteral("勾选后，后续从数据库刷新将忽略该列"));
    } else {
      exclude = new QTableWidgetItem;
      exclude->setFlags(Qt::ItemIsEnabled);
      exclude->setTextAlignment(Qt::AlignCenter);
    }
    table->setItem(row, DiffExclude, exclude);
  }

  // ── 索引差异（若存在）──
  enum IndexDiffCol { IdxApply = 0, IdxName, IdxStatus, IdxCurrent, IdxDb };
  QTableWidget *indexTable = nullptr;
  if (!indexEntries.isEmpty()) {
    auto *idxLabel = new QLabel(QStringLiteral("索引差异:"), &dlg);
    idxLabel->setStyleSheet(QStringLiteral("color: %1;").arg(AuiStyle::mutedTextColor().name()));
    frame.contentLayout->addWidget(idxLabel);
    indexTable = makeConfigTable({{QStringLiteral("应用"), QHeaderView::Fixed, 60},
                                  {QStringLiteral("索引名"), QHeaderView::Interactive, 140},
                                  {QStringLiteral("变更"), QHeaderView::Fixed, 80},
                                  {QStringLiteral("当前配置"), QHeaderView::Stretch, 0},
                                  {QStringLiteral("数据库"), QHeaderView::Stretch, 0}},
                                 &dlg, 140, 0, QAbstractItemView::SelectRows);
    indexTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    indexTable->verticalHeader()->setDefaultSectionSize(28);
    indexTable->setMaximumHeight(200);
    frame.contentLayout->addWidget(indexTable);
    for (const auto &e : indexEntries) {
      const int row = indexTable->rowCount();
      indexTable->insertRow(row);
      indexTable->setItem(row, IdxApply, makeCheckItem(true));
      auto *name = new QTableWidgetItem(e.name);
      name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      indexTable->setItem(row, IdxName, name);
      auto *status = new QTableWidgetItem(importKindText(e.kind));
      status->setForeground(QBrush(importKindColor(e.kind)));
      if (e.kind == ImportDiffKind::Modified) {
        status->setToolTip(indexChanges(e.current, e.db).join(QStringLiteral("\n")));
      }
      indexTable->setItem(row, IdxStatus, status);
      auto *cur =
          new QTableWidgetItem(e.hasCurrent ? indexSummary(e.current) : QStringLiteral("-"));
      cur->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      cur->setToolTip(cur->text());
      indexTable->setItem(row, IdxCurrent, cur);
      auto *db = new QTableWidgetItem(e.hasDb ? indexSummary(e.db) : QStringLiteral("-"));
      db->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      db->setToolTip(db->text());
      indexTable->setItem(row, IdxDb, db);
    }
  }

  // 排除勾选联动：勾上 → 取消应用且禁用；取消 → 恢复默认应用勾选
  bool syncing = false;
  QObject::connect(table, &QTableWidget::itemChanged, &dlg, [&](QTableWidgetItem *item) {
    if (syncing || item->column() != DiffExclude) return;
    if (!(item->flags() & Qt::ItemIsUserCheckable)) return;
    syncing = true;
    const int row = item->row();
    const auto &e = entries[row];
    const bool isExcluded = item->checkState() == Qt::Checked;
    QTableWidgetItem *apply = table->item(row, DiffApply);
    if (isExcluded) {
      apply->setCheckState(Qt::Unchecked);
      apply->setFlags(apply->flags() & ~Qt::ItemIsUserCheckable);
    } else {
      apply->setFlags(apply->flags() | Qt::ItemIsUserCheckable);
      apply->setCheckState((e.kind == ImportDiffKind::Added || e.kind == ImportDiffKind::Modified)
                               ? Qt::Checked
                               : Qt::Unchecked);
    }
    QTableWidgetItem *status = table->item(row, DiffStatus);
    status->setText(isExcluded ? QStringLiteral("已排除") : importKindText(e.kind));
    status->setForeground(
        QBrush(isExcluded ? AuiStyle::mutedTextColor() : importKindColor(e.kind)));
    syncing = false;
  });

  QObject::connect(selectAllBtn, &QPushButton::clicked, &dlg, [&]() {
    for (int r = 0; r < table->rowCount(); ++r) {
      QTableWidgetItem *apply = table->item(r, DiffApply);
      if (apply && (apply->flags() & Qt::ItemIsUserCheckable)) {
        apply->setCheckState(Qt::Checked);
      }
    }
    if (indexTable) {
      for (int r = 0; r < indexTable->rowCount(); ++r) {
        indexTable->item(r, IdxApply)->setCheckState(Qt::Checked);
      }
    }
  });
  QObject::connect(selectNoneBtn, &QPushButton::clicked, &dlg, [&]() {
    for (int r = 0; r < table->rowCount(); ++r) {
      QTableWidgetItem *apply = table->item(r, DiffApply);
      if (apply && (apply->flags() & Qt::ItemIsUserCheckable)) {
        apply->setCheckState(Qt::Unchecked);
      }
    }
    if (indexTable) {
      for (int r = 0; r < indexTable->rowCount(); ++r) {
        indexTable->item(r, IdxApply)->setCheckState(Qt::Unchecked);
      }
    }
  });

  finishConfigDialog(&dlg, frame);
  if (dlg.exec() != QDialog::Accepted) return result;

  result.accepted = true;
  QSet<QString> dbKeysInDiff;
  for (int r = 0; r < entries.size(); ++r) {
    const auto &e = entries[r];
    const bool excludeChecked = e.hasDb && table->item(r, DiffExclude)->checkState() == Qt::Checked;
    const bool applyChecked =
        table->item(r, DiffApply)->checkState() == Qt::Checked && !excludeChecked;
    result.applyByKey.insert(e.name.toLower(), applyChecked);
    // 只要该列出现在差异表（数据库中存在），就由本次勾选决定其排除状态；
    // 未勾选排除时不再回填到排除列表（否则取消排除会被旧值覆盖）。
    if (e.hasDb) dbKeysInDiff.insert(e.name.toLower());
    if (excludeChecked) {
      result.excludedColumns << e.name;
    }
  }
  // 保留初始排除列表中不在本次差异里的条目（拼写/未来列），避免被误删
  for (const auto &name : initialExcluded) {
    if (dbKeysInDiff.contains(name.toLower())) continue;
    if (result.excludedColumns.contains(name, Qt::CaseInsensitive)) continue;
    result.excludedColumns << name;
  }
  if (indexTable) {
    for (int r = 0; r < indexEntries.size(); ++r) {
      result.indexApplyByKey.insert(indexEntries[r].name.toLower(),
                                    indexTable->item(r, IdxApply)->checkState() == Qt::Checked);
    }
  }
  result.applyTableComment = commentCheck && commentCheck->isChecked();
  return result;
}

}  // namespace

// ════════════════════════════════════════════════════════════
//  构造 / 界面构建
// ════════════════════════════════════════════════════════════

JsonTableDesigner::JsonTableDesigner(const JsonTableTable &table, const JsonTableConfig &config,
                                     const QString &filePath, QWidget *parent)
    : QWidget(parent), m_table(table), m_config(config), m_filePath(filePath) {
  m_buttons = table.buttons;
  setupUI();
  refill();
  connectChangeSignals();
}

void JsonTableDesigner::setupUI() {
  // 外围间距对齐 jsonvue 可视化编辑器（margins 2 / spacing 2）
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(2, 2, 2, 2);
  rootLayout->setSpacing(2);
  auto *layout = rootLayout;

  // ── 页签容器（对齐 jsonvue「一行=一字段」心智）：
  //    字段(物理) / 字段呈现(生成设置+三视图合一+排序) / 联表与索引 / 枚举 / 多语言 / DDL ──
  auto *tabs = new QTabWidget(this);
  auto makeTab = [tabs](const QString &title) {
    auto *w = new QWidget(tabs);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(2, 2, 2, 2);
    lay->setSpacing(2);
    tabs->addTab(w, title);
    return lay;
  };
  // 可拖动分隔页内的「分区」：标题行 + 表格装进同一个 QWidget，
  // 作为 QSplitter 的独立段，拖动分隔条即可调整各段高度
  auto makeSection = [](QSplitter *owner) {
    auto *w = new QWidget(owner);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);
    return lay;
  };
  auto *fieldTab = makeTab(QStringLiteral("字段"));
  auto *presentTab = makeTab(QStringLiteral("字段呈现"));
  auto *joinTab = makeTab(QStringLiteral("联表与索引"));
  auto *enumTab = makeTab(QStringLiteral("枚举"));
  auto *i18nTab = makeTab(QStringLiteral("多语言"));
  auto *ddlTab = makeTab(QStringLiteral("DDL"));

  // ── 顶部：表名 / 模型名 / 表注释 / manualUpdateTime（字段页）──
  auto *topRow = new QHBoxLayout;
  topRow->setSpacing(6);
  auto addField = [&topRow](const QString &label, QLineEdit **edit, const QString &placeholder) {
    topRow->addWidget(new QLabel(label, topRow->parentWidget()));
    auto *e = new QLineEdit(topRow->parentWidget());
    e->setPlaceholderText(placeholder);
    topRow->addWidget(e, 1);
    *edit = e;
  };
  addField(QStringLiteral("表名:"), &m_nameEdit, QStringLiteral("如 shop"));
  addField(QStringLiteral("模型名:"), &m_modelEdit, QStringLiteral("生成代码用的标识"));
  addField(QStringLiteral("表注释:"), &m_commentEdit, QStringLiteral("如 商品表"));
  m_manualUpdateCheck = new QCheckBox(QStringLiteral("手动维护更新时间"), this);
  m_manualUpdateCheck->setToolTip(
      QStringLiteral("勾选后生成端不由数据库 ON UPDATE 维护更新时间字段"));
  topRow->addWidget(m_manualUpdateCheck);
  fieldTab->addLayout(topRow);

  // ── 字段表格工具行 ──
  auto *colHeader = new QHBoxLayout;
  colHeader->addWidget(new QLabel(QStringLiteral("字段:"), this));
  colHeader->addStretch();
  auto *addColBtn = makeCompactButton(QStringLiteral("+ 加列"), this);
  auto *delColBtn = makeCompactButton(QStringLiteral("- 删列"), this);
  auto *upColBtn = makeCompactButton(QStringLiteral("↑ 上移"), this);
  auto *downColBtn = makeCompactButton(QStringLiteral("↓ 下移"), this);
  m_refreshFieldsBtn = makeCompactButton(QStringLiteral("从数据库刷新字段"), this);
  colHeader->addWidget(addColBtn);
  colHeader->addWidget(delColBtn);
  colHeader->addWidget(upColBtn);
  colHeader->addWidget(downColBtn);
  colHeader->addSpacing(8);
  colHeader->addWidget(m_refreshFieldsBtn);
  fieldTab->addLayout(colHeader);

  // ── 字段表格（纯物理属性，独占字段页；页面呈现由三视图接管）──
  m_colsTable = makeConfigTable({{QStringLiteral("列名"), QHeaderView::Interactive, 110},
                                 {QStringLiteral("MySQL类型"), QHeaderView::Interactive, 120},
                                 {QStringLiteral("unsigned"), QHeaderView::Interactive, 62},
                                 {QStringLiteral("可空"), QHeaderView::Interactive, 50},
                                 {QStringLiteral("主键"), QHeaderView::Interactive, 50},
                                 {QStringLiteral("自增"), QHeaderView::Interactive, 50},
                                 {QStringLiteral("默认值"), QHeaderView::Interactive, 90},
                                 {QStringLiteral("注释"), QHeaderView::Stretch, 0}},
                                this, 300, 0, QAbstractItemView::SelectItems);
  fieldTab->addWidget(m_colsTable, 1);

  // MySQL 类型列：可编辑下拉委托（预设 + 自由输入）
  m_colsTable->setItemDelegateForColumn(
      ColColType,
      new EditableComboDelegate(
          {QStringLiteral("bigint"), QStringLiteral("int"), QStringLiteral("tinyint"),
           QStringLiteral("varchar(64)"), QStringLiteral("text"), QStringLiteral("decimal(10,2)"),
           QStringLiteral("datetime"), QStringLiteral("json"), QStringLiteral("boolean")},
          m_colsTable));

  // ── 字段呈现统一表格（一行 = 一个字段：列表/查询/编辑三视图合一）──
  auto *viewHeader = new QHBoxLayout;
  viewHeader->addWidget(new QLabel(QStringLiteral("字段呈现:"), this));
  viewHeader->addStretch();
  auto *addViewJoinBtn = makeCompactButton(QStringLiteral("+ 联表列"), this);
  auto *delViewBtn = makeCompactButton(QStringLiteral("- 删联表行"), this);
  auto *upViewBtn = makeCompactButton(QStringLiteral("↑ 上移"), this);
  auto *downViewBtn = makeCompactButton(QStringLiteral("↓ 下移"), this);
  viewHeader->addWidget(addViewJoinBtn);
  viewHeader->addWidget(delViewBtn);
  viewHeader->addWidget(upViewBtn);
  viewHeader->addWidget(downViewBtn);
  // 三段式垂直分隔：字段呈现 / 默认排序 / 操作按钮 各占一段，
  // 拖动任意分隔条即可调整高度；短窗口下排序/按钮区不会被压到只剩一两行。
  auto *presentSplit = new AuiSplitter(Qt::Vertical, this);
  presentSplit->setObjectName(QStringLiteral("presentSplit"));
  presentSplit->setChildrenCollapsible(false);

  // 上段：字段呈现
  auto *viewLay = makeSection(presentSplit);
  viewLay->addLayout(viewHeader);
  presentSplit->addWidget(viewLay->parentWidget());

  // 列模式：文本列与勾选列全部 Interactive（可拖拽调宽），末列 ⚙ Stretch 吸收
  // 剩余宽度——前两列原为 Stretch 会被 Qt 禁用拖拽柄，导致列宽拖不动
  m_viewTable = makeConfigTable({{QStringLiteral("字段引用"), QHeaderView::Interactive, 150},
                                 {QStringLiteral("标题"), QHeaderView::Interactive, 130},
                                 {QStringLiteral("列表"), QHeaderView::Interactive, 56},
                                 {QStringLiteral("查询"), QHeaderView::Interactive, 88},
                                 {QStringLiteral("编辑"), QHeaderView::Interactive, 56},
                                 {QStringLiteral("必填"), QHeaderView::Interactive, 56},
                                 {QStringLiteral("配置"), QHeaderView::Stretch, 0}},
                                this, 240, 0, QAbstractItemView::SelectItems);
  // 功能列表头：问号并入标题（「列表 ?」），悬停整格显示对应说明
  m_headerTips = {
      {UColList, QStringLiteral("勾选后该字段出现在列表页表格中\n（列标题 / 列宽在 ⚙ 列配置）")},
      {UColQuery, QStringLiteral("查询方式：\n"
                                 "· 不查询：列表页无该字段的查询控件\n"
                                 "· 精确 / 模糊 / 范围：前端渲染输入控件，后端生成对应查询条件\n"
                                 "· 仅接口：后端生成查询条件，前端不渲染输入控件\n"
                                 "枚举列 / 外键列的查询控件自动为下拉框")},
      {UColEdit, QStringLiteral("编辑列勾选：\n"
                                "☑ 进编辑表单（渲染控件、参与校验与提交）\n"
                                "☐ 不进编辑表单\n"
                                "编辑/新增/详情复用同一 write.vue，详情为整表单只读，\n"
                                "无需列级只读态；联表行勾选 = 只读展示")},
      {UColRequired, QStringLiteral("勾选后编辑表单提交时对该字段做必填校验")},
      {UColCfg, QStringLiteral("配置：⚙ 显示=列表展示类型、编辑=编辑控件、"
                               "取值域=字段取值域、列宽=列表列宽；点击单元格打开设置")}};
  m_viewHeader = m_viewTable->horizontalHeader();
  for (auto tipIt = m_headerTips.constBegin(); tipIt != m_headerTips.constEnd(); ++tipIt) {
    if (auto *h = m_viewTable->horizontalHeaderItem(tipIt.key())) {
      h->setText(h->text() + QStringLiteral(" ?"));
      h->setToolTip(tipIt.value());
    }
  }
  viewLay->addWidget(m_viewTable, 1);
  m_viewRefDelegate = new EditableComboDelegate(QStringList(), m_viewTable);
  m_viewTable->setItemDelegateForColumn(UColRef, m_viewRefDelegate);

  // 中段：默认排序（列表呈现的附属配置）
  auto *sortLay = makeSection(presentSplit);
  presentSplit->addWidget(sortLay->parentWidget());
  auto *sortHeader = new QHBoxLayout;
  sortHeader->addWidget(new QLabel(QStringLiteral("默认排序:"), this));
  sortHeader->addStretch();
  auto *addSortBtn = makeCompactButton(QStringLiteral("+ 加排序"), this);
  auto *delSortBtn = makeCompactButton(QStringLiteral("- 删排序"), this);
  sortHeader->addWidget(addSortBtn);
  sortHeader->addWidget(delSortBtn);
  sortLay->addLayout(sortHeader);

  m_sortTable = makeConfigTable({{QStringLiteral("排序列"), QHeaderView::Interactive, 140},
                                 {QStringLiteral("方向"), QHeaderView::Stretch, 0}},
                                this, 60, 0, QAbstractItemView::SelectItems);
  sortLay->addWidget(m_sortTable, 1);
  m_sortRefDelegate = new EditableComboDelegate(QStringList(), m_sortTable);
  m_sortTable->setItemDelegateForColumn(0, m_sortRefDelegate);

  // 下段：操作按钮（buttons：键名与 .jsonvue ButtonConfig 完全一致）
  auto *btnLay = makeSection(presentSplit);
  presentSplit->addWidget(btnLay->parentWidget());
  auto *btnHeader = new QHBoxLayout;
  btnHeader->addWidget(new QLabel(QStringLiteral("操作按钮:"), this));
  btnHeader->addStretch();
  auto *addBtnBtn = makeCompactButton(QStringLiteral("+ 加按钮"), this);
  auto *delBtnBtn = makeCompactButton(QStringLiteral("- 删按钮"), this);
  btnHeader->addWidget(addBtnBtn);
  btnHeader->addWidget(delBtnBtn);
  btnLay->addLayout(btnHeader);

  m_buttonTable = makeConfigTable({{QStringLiteral("按钮文字"), QHeaderView::Interactive, 140},
                                   {QStringLiteral("位置"), QHeaderView::Interactive, 90},
                                   {QStringLiteral("行为"), QHeaderView::Interactive, 90},
                                   {QStringLiteral("动作"), QHeaderView::Stretch, 0}},
                                  this, 0, 0, QAbstractItemView::SelectRows);
  m_buttonTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_buttonTable->setToolTip(
      QStringLiteral("列表页自定义按钮（工具栏/行操作列），行为支持 直接调 API / 二次确认 / "
                     "对话框表单 / 跳转；\n双击数据行或「+ 加按钮」打开按钮配置对话框"));
  btnLay->addWidget(m_buttonTable, 1);
  connect(addBtnBtn, &QPushButton::clicked, this, &JsonTableDesigner::onAddButton);
  connect(delBtnBtn, &QPushButton::clicked, this, &JsonTableDesigner::onRemoveButton);
  connect(m_buttonTable, &QTableWidget::cellDoubleClicked, this,
          [this](int row, int) { onEditButton(row); });
  fillButtons();

  // 操作按钮段容器：显式给定最小尺寸，允许默认收缩到只剩标题行（≈26px），
  // 让出的高度全部给「字段呈现」正表。注意必须 setMinimumSize(0, 26)（两维
  // 均有效才覆盖 minimumSizeHint）；只 setMinimumHeight(0) 时 minimumSize
  // 仍是无效值（宽=-1），QSplitter 会回退用 minimumSizeHint（QTableWidget
  // 自带 ≈60px），导致 setSizes 的小值被钳制到 90 左右"无效"。
  btnLay->parentWidget()->setMinimumSize(0, 26);

  // 三段高度比例：字段呈现 3 份，排序 / 按钮各 1 份（可拖动调整）
  presentSplit->setStretchFactor(0, 3);
  presentSplit->setStretchFactor(1, 1);
  presentSplit->setStretchFactor(2, 1);
  // setSizes 是期望值，总和高出/不足部分按比例缩放：{320,90,10} 实际把多余
  // 空间分给前两段，第三段被上方 26px 最小尺寸钳到标题行
  presentSplit->setSizes({320, 90, 60});
  // ── 顶部：生成设置（表级生成开关，从字段页迁入；门禁下方三视图/按钮产物）──
  //    一行三段，段间竖线分隔：
  //      生成前端页面 │ 按钮: 新增 编辑 删除 详情 │ 生成C端查询 [鉴权]
  //    生成前端页面 = 三视图总开关（false = 纯 API 模块，下方配置不生效）；
  //    按钮组同属页面呈现范畴；删除勾选同时控制工具栏批量删除与行内删除图标
  //    （两者同生同灭，避免"能批量删但不能单删"的怪状态）
  auto *genSep = new QLabel(QStringLiteral("── 生成设置 ──"), this);
  genSep->setAlignment(Qt::AlignCenter);
  presentTab->addWidget(genSep);
  auto *genRow = new QHBoxLayout;
  genRow->setSpacing(6);
  // 段间竖线（VLine + Sunken 阴影，跟随主题边框色）
  auto addVSep = [this, genRow]() {
    auto *v = new QFrame(this);
    v->setFrameShape(QFrame::VLine);
    v->setFrameShadow(QFrame::Sunken);
    genRow->addWidget(v);
  };
  m_frontendCheck = new QCheckBox(QStringLiteral("生成前端页面"), this);
  m_frontendCheck->setChecked(m_table.frontend);
  m_frontendCheck->setToolTip(
      QStringLiteral("取消勾选 = 纯 API 模块：后端链照常生成 "
                     "API，前端链跳过此表（下方三视图/排序/按钮配置不生效）"));
  genRow->addWidget(m_frontendCheck);
  addVSep();
  genRow->addWidget(new QLabel(QStringLiteral("按钮:"), this));
  m_genAddCheck = new QCheckBox(QStringLiteral("新增"), this);
  m_genAddCheck->setToolTip(QStringLiteral("工具栏「新增」按钮（打开编辑弹窗新增）"));
  genRow->addWidget(m_genAddCheck);
  m_genEditCheck = new QCheckBox(QStringLiteral("编辑"), this);
  m_genEditCheck->setToolTip(QStringLiteral("行操作列「编辑」图标（打开编辑弹窗修改）"));
  genRow->addWidget(m_genEditCheck);
  m_genDeleteCheck = new QCheckBox(QStringLiteral("删除"), this);
  m_genDeleteCheck->setToolTip(
      QStringLiteral("行操作列「删除」图标 + 工具栏「删除」批量按钮（两者同一开关）"));
  genRow->addWidget(m_genDeleteCheck);
  m_genDetailCheck = new QCheckBox(QStringLiteral("详情"), this);
  m_genDetailCheck->setToolTip(QStringLiteral("行操作列「详情」图标（只读弹窗查看）"));
  genRow->addWidget(m_genDetailCheck);
  addVSep();
  // C 端只读查询（modules/app）：默认不生成，鉴权默认需登录
  m_appQueryCheck = new QCheckBox(QStringLiteral("生成C端查询"), this);
  m_appQueryCheck->setToolTip(
      QStringLiteral("勾选后生成 modules/app 的 selectByIn/selectById 只读接口，永不生成写接口"));
  genRow->addWidget(m_appQueryCheck);
  m_appAuthCombo = AuiComboBox::create(this);
  m_appAuthCombo->addItem(QStringLiteral("公开浏览"), QStringLiteral("public"));
  m_appAuthCombo->addItem(QStringLiteral("需登录"), QStringLiteral("user"));
  m_appAuthCombo->setToolTip(QStringLiteral(
      "public=不挂 UserAuth（如商品列表）；user=需登录（默认，宁可漏开放不可误开放）"));
  genRow->addWidget(m_appAuthCombo);
  genRow->addStretch();
  connect(m_appQueryCheck, &QCheckBox::toggled, this,
          [this](bool on) { m_appAuthCombo->setEnabled(on); });
  presentTab->addLayout(genRow);

  presentTab->addWidget(presentSplit, 1);

  // ── 联表查询声明表格（joinTables 新格式：声明式 JOIN → 生成 QuerySpec 流水线）──
  auto *joinHeader = new QHBoxLayout;
  joinHeader->addWidget(new QLabel(QStringLiteral("联表(joinTables):"), this));
  joinHeader->addStretch();
  auto *addJoinBtn = makeCompactButton(QStringLiteral("+ 加联表"), this);
  auto *delJoinBtn = makeCompactButton(QStringLiteral("- 删联表"), this);
  joinHeader->addWidget(addJoinBtn);
  joinHeader->addWidget(delJoinBtn);

  // 上下两段可拖动：联表声明 / 索引
  auto *joinSplit = new AuiSplitter(Qt::Vertical, this);
  joinSplit->setObjectName(QStringLiteral("joinSplit"));
  joinSplit->setChildrenCollapsible(false);
  auto *joinLay = makeSection(joinSplit);
  joinLay->addLayout(joinHeader);
  joinSplit->addWidget(joinLay->parentWidget());

  m_joinTable = makeConfigTable({{QStringLiteral("联表表"), QHeaderView::Interactive, 110},
                                 {QStringLiteral("类型"), QHeaderView::Interactive, 76},
                                 {QStringLiteral("主表列"), QHeaderView::Interactive, 100},
                                 {QStringLiteral("联表列"), QHeaderView::Interactive, 100},
                                 {QStringLiteral("输出列(逗号分隔)"), QHeaderView::Stretch, 0}},
                                this, 120, 0, QAbstractItemView::SelectItems);
  m_joinTable->setToolTip(
      QStringLiteral("联表表=被联表名（项目内 .jsontable）；类型=LEFT/INNER（缺省 LEFT）；\n"
                     "主表列/联表列=ON 两列（on:[主表列,联表列]，表名由规格自带）；\n"
                     "输出列=联表输出列名，逗号分隔（只管列表输出哪些列）；\n"
                     "输出别名固定为 表名_列名（前端统一字段名），查询能力在「字段呈现」页的\n"
                     "查询视图（精确/模糊）里按输出别名配置，与主表列同源。"
                     "生成端产出 EnXxxView 视图 + querySpec 联表流水线"));
  joinLay->addWidget(m_joinTable, 1);
  // 被联表下拉候选 = 项目内全部表名（跨文件，fillJoins 时按当前文件目录刷新）；
  // 主表列候选 = 本表字段列名（动态刷新）
  m_joinTableDelegate = new EditableComboDelegate(QStringList(), m_joinTable);
  m_joinTable->setItemDelegateForColumn(JoinColTable, m_joinTableDelegate);
  m_joinMainColDelegate = new EditableComboDelegate(QStringList(), m_joinTable);
  m_joinTable->setItemDelegateForColumn(JoinColOnMain, m_joinMainColDelegate);

  // ── 索引表格工具行 ──
  auto *idxHeader = new QHBoxLayout;
  idxHeader->addWidget(new QLabel(QStringLiteral("索引:"), this));
  idxHeader->addStretch();
  auto *addIdxBtn = makeCompactButton(QStringLiteral("+ 加索引"), this);
  auto *delIdxBtn = makeCompactButton(QStringLiteral("- 删索引"), this);
  idxHeader->addWidget(addIdxBtn);
  idxHeader->addWidget(delIdxBtn);
  auto *idxLay = makeSection(joinSplit);
  idxLay->addLayout(idxHeader);
  joinSplit->addWidget(idxLay->parentWidget());

  m_indexTable = makeConfigTable({{QStringLiteral("索引名"), QHeaderView::Stretch, 0},
                                  {QStringLiteral("列(逗号分隔)"), QHeaderView::Stretch, 0},
                                  {QStringLiteral("unique"), QHeaderView::Interactive, 70}},
                                 this, 150, 0, QAbstractItemView::SelectItems);
  m_indexTable->setToolTip(QStringLiteral("unique 索引自动生成 Insert/Update 唯一性预检（后端）"));
  idxLay->addWidget(m_indexTable, 1);
  joinSplit->setStretchFactor(0, 1);
  joinSplit->setStretchFactor(1, 1);
  joinSplit->setSizes({240, 240});
  joinTab->addWidget(joinSplit, 1);

  // ── 表内枚举区（主从表：主表选枚举行，从表编辑选项）──
  auto *enumHeader = new QHBoxLayout;
  enumHeader->addWidget(new QLabel(QStringLiteral("表内枚举:"), this));
  enumHeader->addStretch();
  auto *addEnumBtn = makeCompactButton(QStringLiteral("+ 加枚举"), this);
  auto *delEnumBtn = makeCompactButton(QStringLiteral("- 删枚举"), this);
  enumHeader->addWidget(addEnumBtn);
  enumHeader->addWidget(delEnumBtn);

  // 上下两段可拖动：枚举主表 / 枚举选项
  auto *enumSplit = new AuiSplitter(Qt::Vertical, this);
  enumSplit->setObjectName(QStringLiteral("enumSplit"));
  enumSplit->setChildrenCollapsible(false);
  auto *enumLay = makeSection(enumSplit);
  enumLay->addLayout(enumHeader);
  enumSplit->addWidget(enumLay->parentWidget());

  m_enumTable = makeConfigTable({{QStringLiteral("枚举列"), QHeaderView::Interactive, 150},
                                 {QStringLiteral("说明"), QHeaderView::Stretch, 0}},
                                this, 72, 0, QAbstractItemView::SelectRows);
  m_enumTable->setToolTip(QStringLiteral("枚举列需与字段表格中的列名一致（如 status）"));
  enumLay->addWidget(m_enumTable, 1);
  // 枚举列下拉候选：预设 = 字段表格列名（refreshColumnCandidates 动态更新）
  m_enumColDelegate = new EditableComboDelegate(QStringList(), m_enumTable);
  m_enumTable->setItemDelegateForColumn(0, m_enumColDelegate);

  auto *optHeader = new QHBoxLayout;
  optHeader->addWidget(new QLabel(QStringLiteral("枚举选项（选中上方枚举后编辑）:"), this));
  optHeader->addStretch();
  auto *addOptBtn = makeCompactButton(QStringLiteral("+ 加选项"), this);
  auto *delOptBtn = makeCompactButton(QStringLiteral("- 删选项"), this);
  optHeader->addWidget(addOptBtn);
  optHeader->addWidget(delOptBtn);
  auto *optLay = makeSection(enumSplit);
  optLay->addLayout(optHeader);
  enumSplit->addWidget(optLay->parentWidget());

  m_enumOptTable = makeConfigTable({{QStringLiteral("成员名(key)"), QHeaderView::Interactive, 120},
                                    {QStringLiteral("说明(label)"), QHeaderView::Stretch, 0},
                                    {QStringLiteral("值(value)"), QHeaderView::Interactive, 90},
                                    {QStringLiteral("值类型"), QHeaderView::Interactive, 96}},
                                   this, 100, 0, QAbstractItemView::SelectItems);
  optLay->addWidget(m_enumOptTable, 1);
  enumSplit->setStretchFactor(0, 1);
  enumSplit->setStretchFactor(1, 2);
  enumSplit->setSizes({160, 320});
  enumTab->addWidget(enumSplit, 1);

  // 各配置表统一行高，避免默认行高偏挤
  for (auto *tw : {m_colsTable, m_viewTable, m_sortTable, m_buttonTable, m_joinTable, m_indexTable,
                   m_enumTable, m_enumOptTable}) {
    tw->verticalHeader()->setDefaultSectionSize(30);
  }

  // ── 全局枚举列 ──
  auto *geRow = new QHBoxLayout;
  geRow->setSpacing(6);
  geRow->addWidget(new QLabel(QStringLiteral("全局枚举列:"), this));
  m_globalEnumEdit = new QLineEdit(this);
  m_globalEnumEdit->setPlaceholderText(
      QStringLiteral("逗号分隔，如 is_enable（列名 = .jsonglobalenum 中的枚举名）"));
  m_globalEnumEdit->setToolTip(QStringLiteral(
      "命中列在 Go 侧生成对应 const 块，前端标准页合成全局枚举取值域；与表内枚举二选一"));
  geRow->addWidget(m_globalEnumEdit, 1);
  enumTab->addLayout(geRow);
  // 全局枚举列补全：候选 = 字段列名（约定列名 = .jsonglobalenum 枚举名）
  m_globalEnumCompleter = new CsvCompleter(m_globalEnumEdit);
  m_globalEnumCompleter->setCaseSensitivity(Qt::CaseInsensitive);
  m_globalEnumCompleter->setFilterMode(Qt::MatchContains);
  m_globalEnumEdit->setCompleter(m_globalEnumCompleter);

  // ── 多语言翻译表配置（多语言页）──
  m_i18nCheck =
      new QCheckBox(QStringLiteral("启用多语言（主表 + 翻译表联合查询/事务写入/级联删除）"), this);
  i18nTab->addWidget(m_i18nCheck);
  auto *i18nRow1 = new QHBoxLayout;
  i18nRow1->setSpacing(6);
  i18nRow1->addWidget(new QLabel(QStringLiteral("翻译表:"), this));
  m_i18nTableEdit = new QLineEdit(this);
  m_i18nTableEdit->setPlaceholderText(QStringLiteral("如 shop0"));
  i18nRow1->addWidget(m_i18nTableEdit, 1);
  i18nRow1->addWidget(new QLabel(QStringLiteral("外键列:"), this));
  m_i18nExtEdit = new QLineEdit(this);
  m_i18nExtEdit->setPlaceholderText(QStringLiteral("如 ext_id"));
  i18nRow1->addWidget(m_i18nExtEdit, 1);
  i18nRow1->addWidget(new QLabel(QStringLiteral("语言列:"), this));
  m_i18nLangKeyEdit = new QLineEdit(this);
  m_i18nLangKeyEdit->setPlaceholderText(QStringLiteral("如 lang"));
  i18nRow1->addWidget(m_i18nLangKeyEdit, 1);
  i18nTab->addLayout(i18nRow1);
  auto *i18nRow2 = new QHBoxLayout;
  i18nRow2->setSpacing(6);
  i18nRow2->addWidget(new QLabel(QStringLiteral("默认语言:"), this));
  m_i18nDefaultLangEdit = new QLineEdit(this);
  m_i18nDefaultLangEdit->setMaximumWidth(80);
  m_i18nDefaultLangEdit->setPlaceholderText(QStringLiteral("zh"));
  i18nRow2->addWidget(m_i18nDefaultLangEdit);
  i18nRow2->addWidget(new QLabel(QStringLiteral("语言来源:"), this));
  m_i18nLangFromCombo = AuiComboBox::create(this);
  m_i18nLangFromCombo->addItem(QStringLiteral("body（请求体 lang 字段）"), QStringLiteral("body"));
  m_i18nLangFromCombo->addItem(QStringLiteral("header（请求头，预留）"), QStringLiteral("header"));
  i18nRow2->addWidget(m_i18nLangFromCombo);
  i18nRow2->addWidget(new QLabel(QStringLiteral("多语言字段:"), this));
  m_i18nFieldsEdit = new QLineEdit(this);
  m_i18nFieldsEdit->setPlaceholderText(
      QStringLiteral("逗号分隔，如 name,intro（翻译表的文本列，不能与主表列重名）"));
  i18nRow2->addWidget(m_i18nFieldsEdit, 1);
  i18nTab->addLayout(i18nRow2);
  // 多语言字段补全：候选 = 翻译表列名（随翻译表名输入实时切换）
  m_i18nFieldsCompleter = new CsvCompleter(m_i18nFieldsEdit);
  m_i18nFieldsCompleter->setCaseSensitivity(Qt::CaseInsensitive);
  m_i18nFieldsCompleter->setFilterMode(Qt::MatchContains);
  m_i18nFieldsEdit->setCompleter(m_i18nFieldsCompleter);
  i18nTab->addStretch(1);
  applyI18nEnabled(false);

  // ── 刷新时忽略的列（排除列）──
  // 从数据库刷新结构时，这些列不会新增也不会被修改；用于 token/locked 等
  // 不希望进入生成配置的认证列。持久化在 table.extra.excludeColumns。
  auto *excludeRow = new QHBoxLayout;
  excludeRow->setSpacing(6);
  excludeRow->addWidget(new QLabel(QStringLiteral("排除列:"), this));
  m_excludeEdit = new QLineEdit(this);
  m_excludeEdit->setPlaceholderText(
      QStringLiteral("逗号分隔，如 token,locked（从数据库刷新时忽略这些列）"));
  m_excludeEdit->setToolTip(
      QStringLiteral("从数据库刷新结构时忽略的列：既不新增、也不修改；\n"
                     "适合 token/locked 等不想进入生成配置的认证列"));
  excludeRow->addWidget(m_excludeEdit, 1);
  ddlTab->addLayout(excludeRow);

  // ── DDL 预览 + 执行区（DDL 页）──
  auto *ddlHeader = new QHBoxLayout;
  ddlHeader->addWidget(new QLabel(QStringLiteral("DDL:"), this));
  ddlHeader->addStretch();
  m_importBtn = makeCompactButton(QStringLiteral("从数据库刷新结构"), this);
  auto *previewBtn = makeCompactButton(QStringLiteral("DDL 预览"), this);
  auto *execBtn = makeCompactButton(QStringLiteral("执行建表"), this);
  ddlHeader->addWidget(m_importBtn);
  ddlHeader->addWidget(previewBtn);
  ddlHeader->addWidget(execBtn);
  ddlTab->addLayout(ddlHeader);

  m_ddlView = new QPlainTextEdit(this);
  m_ddlView->setReadOnly(true);
  m_ddlView->setPlaceholderText(QStringLiteral("点击「DDL 预览」生成 CREATE TABLE 语句"));
  m_ddlView->setMinimumHeight(160);
  ddlTab->addWidget(m_ddlView, 1);

  // meta.db 就绪性由 updateDbButtonStates 统一处理（构造时 config 可能为空，
  // 真正的连接信息在 load() 后才到位，refill 末尾会重新评估）
  updateDbButtonStates();

  // ── 连接 ──
  connect(m_refreshFieldsBtn, &QPushButton::clicked, this, &JsonTableDesigner::importFromDatabase);
  connect(addColBtn, &QPushButton::clicked, this, [this]() { addColumn(); });
  connect(delColBtn, &QPushButton::clicked, this, [this]() { deleteColumn(); });
  connect(upColBtn, &QPushButton::clicked, this, [this]() { moveColumn(-1); });
  connect(downColBtn, &QPushButton::clicked, this, [this]() { moveColumn(1); });
  connect(addIdxBtn, &QPushButton::clicked, this, [this]() { appendIndexRow(); });
  connect(delIdxBtn, &QPushButton::clicked, this, [this]() {
    const int row = m_indexTable->currentRow();
    if (row >= 0) m_indexTable->removeRow(row);
  });
  connect(m_importBtn, &QPushButton::clicked, this, &JsonTableDesigner::importFromDatabase);
  connect(previewBtn, &QPushButton::clicked, this, &JsonTableDesigner::previewDdl);
  connect(execBtn, &QPushButton::clicked, this, &JsonTableDesigner::executeCreateTable);
  connect(addSortBtn, &QPushButton::clicked, this,
          [this]() { appendSortRow(QString(), QStringLiteral("DESC")); });
  connect(delSortBtn, &QPushButton::clicked, this, [this]() {
    const int row = m_sortTable->currentRow();
    if (row >= 0) m_sortTable->removeRow(row);
  });
  connect(addJoinBtn, &QPushButton::clicked, this, [this]() { appendJoinRow(); });
  connect(delJoinBtn, &QPushButton::clicked, this, [this]() {
    const int row = m_joinTable->currentRow();
    if (row >= 0) m_joinTable->removeRow(row);
  });
  // 加联表列：候选 = 联表输出别名（joinTables 新格式：表名_列名）
  connect(addViewJoinBtn, &QPushButton::clicked, this, [this]() {
    QStringList cands;
    for (const auto &j : collectJoins()) {
      for (const auto &c : j.cols) {
        if (!c.col.isEmpty()) cands.append(j.table + QLatin1Char('_') + c.col);
      }
    }
    if (cands.isEmpty()) {
      AuiMessageBox::show(this, QStringLiteral("无法添加"),
                          QStringLiteral("请先在「联表与索引」页声明联表及输出列"));
      return;
    }
    bool ok = false;
    const QString ref = AuiInputDialog::getItem(this, QStringLiteral("添加联表列"),
                                                QStringLiteral("选择联表列："), cands, 0, &ok);
    if (ok && !ref.isEmpty()) appendViewJoinRow(ref);
  });
  connect(delViewBtn, &QPushButton::clicked, this, [this]() {
    const int row = m_viewTable->currentRow();
    if (row < 0) return;
    // 仅联表行可删（物理列行跟随字段表格）
    const QString ref = cellOf(m_viewTable, row, UColRef);
    bool isAliasRow = false;
    for (const auto &js : m_table.joinTables) {
      for (const auto &c : js.cols) {
        if (ref == js.table + QLatin1Char('_') + c.col) {
          isAliasRow = true;
          break;
        }
      }
      if (isAliasRow) break;
    }
    if (!ref.contains(QLatin1Char('.')) && !isAliasRow) {
      AuiMessageBox::show(this, QStringLiteral("无法删除"),
                          QStringLiteral("物理列行跟随「字段」页的字段表格，不可在此删除；"
                                         "联表行才可删除"));
      return;
    }
    m_viewTable->removeRow(row);
  });
  connect(upViewBtn, &QPushButton::clicked, this, [this]() { moveViewRow(-1); });
  connect(downViewBtn, &QPushButton::clicked, this, [this]() { moveViewRow(1); });
  connect(addEnumBtn, &QPushButton::clicked, this, [this]() {
    syncEnumDrafts();
    JsonTableEnum e;
    m_enums.append(e);
    rebuildEnumMaster();
    const int newRow = m_enumTable->rowCount() - 1;
    m_enumTable->selectRow(newRow);
    m_enumTable->setCurrentCell(newRow, 0);
    m_enumTable->editItem(m_enumTable->item(newRow, 0));  // 直接进入枚举列编辑
  });
  connect(delEnumBtn, &QPushButton::clicked, this, [this]() {
    const int row = m_enumTable->currentRow();
    if (row < 0 || row >= m_enums.size()) return;
    syncEnumDrafts();
    m_enums.removeAt(row);
    rebuildEnumMaster();
    const int next = qMin(row, m_enumTable->rowCount() - 1);
    if (next >= 0) m_enumTable->selectRow(next);
  });
  connect(addOptBtn, &QPushButton::clicked, this, [this]() {
    if (m_enumCurRow < 0) return;
    appendEnumOptRow();
  });
  connect(delOptBtn, &QPushButton::clicked, this, [this]() {
    const int row = m_enumOptTable->currentRow();
    if (row >= 0) m_enumOptTable->removeRow(row);
  });
  // 枚举主从表联动：行切换前把主表/选项表的编辑内容存回 m_enums
  connect(m_enumTable, &QTableWidget::currentCellChanged, this,
          [this](int cur, int, int prev, int) {
            if (m_enumSyncing) return;
            syncEnumDrafts();
            fillEnumOptions(cur);
          });
  connect(m_i18nCheck, &QCheckBox::toggled, this, [this](bool on) { applyI18nEnabled(on); });
  // 字段表格编辑 → 列名候选刷新（枚举列下拉/联表外键列/编辑列等）
  connect(m_colsTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *it) {
    if (!it) return;
    refreshColumnCandidates();
  });
  // 字段呈现表「配置」列点击（⚙ 与摘要同一单元格）→ 字段设置面板
  connect(m_viewTable, &QTableWidget::cellClicked, this, [this](int row, int col) {
    if (col == UColCfg) openViewFieldConfig(row);
  });
  // 翻译表名输入 → 多语言字段候选切换
  connect(m_i18nTableEdit, &QLineEdit::textChanged, this,
          [this]() { refreshI18nFieldCandidates(); });

  layout->addWidget(tabs, 1);

  // ── 交叉校验状态行（被动：随编辑自动刷新，替代原弹窗确定拦截与校验按钮）──
  m_validateLabel = new QLabel(this);
  m_validateLabel->setContentsMargins(4, 0, 4, 0);
  layout->addWidget(m_validateLabel);
  applyStyle();
}

// ════════════════════════════════════════════════════════════
//  refill（loadConfig 后全量回填各表格）
// ════════════════════════════════════════════════════════════

void JsonTableDesigner::refill() {
  m_loading = true;
  m_nameEdit->setText(m_table.tableName);
  m_modelEdit->setText(m_table.modelName);
  m_commentEdit->setText(m_table.tableComment);
  m_manualUpdateCheck->setChecked(m_table.manualUpdateTime);
  m_frontendCheck->setChecked(m_table.frontend);
  m_genAddCheck->setChecked(m_table.genAdd);
  m_genEditCheck->setChecked(m_table.genEdit);
  m_genDeleteCheck->setChecked(m_table.genDelete);
  m_genDetailCheck->setChecked(m_table.genDetail);
  m_appQueryCheck->setChecked(m_table.appQuery);
  {
    const int authIdx = m_appAuthCombo->findData(m_table.appAuth);
    m_appAuthCombo->setCurrentIndex(authIdx >= 0 ? authIdx : 1);
    m_appAuthCombo->setEnabled(m_table.appQuery);
  }
  fillColumns(m_table.columns);
  fillIndexes(m_table.indexes);
  fillJoins(m_table.joinTables);
  fillViewTable();
  fillSortTable(m_table.selColsSort);
  m_buttons = m_table.buttons;
  fillButtons();
  m_enums = m_table.enums;
  rebuildEnumMaster();
  if (m_enumTable->rowCount() > 0) m_enumTable->selectRow(0);
  m_globalEnumEdit->setText(joinCsv(m_table.globalEnumCols));
  m_i18nCheck->setChecked(m_table.i18n.isEnabled());
  m_i18nTableEdit->setText(m_table.i18n.table);
  m_i18nExtEdit->setText(m_table.i18n.extKey);
  m_i18nLangKeyEdit->setText(m_table.i18n.langKey);
  m_i18nDefaultLangEdit->setText(m_table.i18n.defaultLang);
  comboSelectData(m_i18nLangFromCombo, m_table.i18n.langFrom);
  m_i18nFieldsEdit->setText(joinCsv(m_table.i18n.fields));
  applyI18nEnabled(m_i18nCheck->isChecked());
  // 排除列（设计器专属键，存 table.extra，生成期不消费）
  QStringList excludedColumns;
  const QJsonArray excludedArray =
      m_table.extra.value(QString::fromLatin1(JsonTableKey::kExcludeColumns)).toArray();
  for (const auto &value : excludedArray) {
    const QString name = value.toString().trimmed();
    if (!name.isEmpty()) excludedColumns << name;
  }
  m_excludeEdit->setText(joinCsv(excludedColumns));
  refreshColumnCandidates();
  refreshI18nFieldCandidates();
  m_loading = false;
  updateDbButtonStates();
  updateValidateStatus();
}

void JsonTableDesigner::updateDbButtonStates() {
  // 密码允许为空（本机 MySQL 免密场景），只要求连接定位三要素齐全
  const bool ready =
      !m_config.dbHost.isEmpty() && !m_config.dbUser.isEmpty() && !m_config.dbDatabase.isEmpty();
  const QString readyTip = QStringLiteral("按当前表名拉取数据库结构，先展示差异再按勾选应用");
  const QString missingTip =
      QStringLiteral("meta.db 未配置完整（host/user/database），无法从数据库刷新结构");
  for (QPushButton *button : {m_importBtn, m_refreshFieldsBtn}) {
    if (!button) continue;
    button->setEnabled(ready);
    button->setToolTip(ready ? readyTip : missingTip);
  }
}

void JsonTableDesigner::load(const JsonTableTable &table, const JsonTableConfig &config,
                             const QString &filePath) {
  m_table = table;
  m_config = config;
  m_filePath = filePath;
  m_buttons = table.buttons;
  refill();
}

void JsonTableDesigner::updateValidateStatus() {
  if (!m_validateLabel) return;
  QString err;
  if (validateCrossRefs(&err)) {
    m_validateLabel->setText(QStringLiteral("✓ 交叉引用校验通过"));
    m_validateLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px;").arg(AuiStyle::mutedTextColor().name()));
  } else {
    m_validateLabel->setText(QStringLiteral("⚠ %1").arg(err));
    m_validateLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px;").arg(AuiStyle::warningColor().name()));
  }
}

void JsonTableDesigner::applyI18nEnabled(bool on) {
  m_i18nTableEdit->setEnabled(on);
  m_i18nExtEdit->setEnabled(on);
  m_i18nLangKeyEdit->setEnabled(on);
  m_i18nDefaultLangEdit->setEnabled(on);
  m_i18nLangFromCombo->setEnabled(on);
  m_i18nFieldsEdit->setEnabled(on);
}

void JsonTableDesigner::connectChangeSignals() {
  // 表格：模型级变更（条目编辑/行增删/搬运）全部覆盖；行内下拉在创建点单独接线
  auto onChange = [this]() {
    if (m_loading) return;
    emit configChanged();
    updateValidateStatus();
  };
  for (QTableWidget *tw : findChildren<QTableWidget *>()) {
    connect(tw->model(), &QAbstractItemModel::dataChanged, this, onChange);
    connect(tw->model(), &QAbstractItemModel::rowsInserted, this, onChange);
    connect(tw->model(), &QAbstractItemModel::rowsRemoved, this, onChange);
  }
  // 静态输入控件（setupUI 构建的顶层控件；setupUI 结束时已全部存在）
  for (QLineEdit *le : findChildren<QLineEdit *>()) {
    connect(le, &QLineEdit::textChanged, this, onChange);
  }
  for (QCheckBox *cb : findChildren<QCheckBox *>()) {
    connect(cb, &QCheckBox::toggled, this, onChange);
  }
  for (QComboBox *combo : findChildren<QComboBox *>()) {
    connect(combo, &QComboBox::currentIndexChanged, this, onChange);
  }
}

void JsonTableDesigner::applyStyle() {
  const auto fontPx = QString::number(AuiStyle::dialogFontSize());
  const auto text = AuiStyle::textColor().name();
  const auto border = AuiStyle::borderColor().name();
  const auto bg = AuiStyle::panelBackground().name();
  const auto headerBg = AuiStyle::background().name();
  const auto alt = AuiStyle::listAlternateBackground().name();

  QString qss = QStringLiteral(
                    "QLabel { font-size: %1px; color: %2; background: transparent; }"
                    "QPushButton { padding: 2px 8px; font-size: %1px; color: %2; }"
                    "QCheckBox { font-size: %1px; color: %2; }"
                    "QTabWidget::pane { border: 1px solid %3; border-radius: 4px;"
                    "  background: %4; top: -1px; }"
                    "QTabBar::tab { font-size: %1px; color: %2; padding: 4px 10px;"
                    "  border: 1px solid %3; border-bottom: none;"
                    "  background: transparent; border-top-left-radius: 4px;"
                    "  border-top-right-radius: 4px; }"
                    "QTabBar::tab:selected { background: %4; color: %2; }"
                    "QTableWidget { font-size: %1px; background: %4; color: %2;"
                    "  gridline-color: %3; alternate-background-color: %5; }"
                    "QTableWidget::item { color: %2; }"
                    "QHeaderView::section { padding: 2px; font-size: %1px;"
                    "  background: %6; color: %2; border: none;"
                    "  border-right: 1px solid %3; border-bottom: 1px solid %3; }"
                    "QComboBox { font-size: %1px; color: %2; background: %4;"
                    "  border: 1px solid %3; border-radius: 3px; padding: 1px 4px; }"
                    "QLineEdit { font-size: %1px; color: %2; background: %4;"
                    "  border: 1px solid %3; border-radius: 3px; padding: 1px 4px; }"
                    "QPlainTextEdit { font-size: %1px; color: %2; background: %4;"
                    "  border: 1px solid %3; }"
                    // 分隔条由 AuiSplitterHandle 自绘（1px 细线 + 三点抓手），
                    // 不走 QSS，避免样式表与自绘重叠
                    "QScrollBar:vertical, QScrollBar:horizontal { background: %4; }")
                    .arg(fontPx, text, border, bg, alt, headerBg);
  setStyleSheet(qss);
}

// ════════════════════════════════════════════════════════════
//  字段表格填充 / 收集 / 行操作
// ════════════════════════════════════════════════════════════

void JsonTableDesigner::fillColumns(const QVector<JsonTableColumn> &cols) {
  m_colsTable->setRowCount(0);
  for (const auto &c : cols) appendColumnRow(c);
}

void JsonTableDesigner::appendColumnRow(const JsonTableColumn &c) {
  const int row = m_colsTable->rowCount();
  m_colsTable->insertRow(row);
  m_colsTable->setItem(row, ColColName, new QTableWidgetItem(c.name));
  m_colsTable->setItem(row, ColColType, new QTableWidgetItem(c.mysqlType));
  m_colsTable->setItem(row, ColColUnsigned, makeCheckItem(c.isUnsigned));
  m_colsTable->setItem(row, ColColNullable, makeCheckItem(c.nullable));
  m_colsTable->setItem(row, ColColPrimary, makeCheckItem(c.isPrimary));
  m_colsTable->setItem(row, ColColAutoInc, makeCheckItem(c.isAutoInc));
  m_colsTable->setItem(row, ColColDefault, new QTableWidgetItem(c.defaultValue));
  m_colsTable->setItem(row, ColColComment, new QTableWidgetItem(c.comment));
}

QString JsonTableDesigner::cellText(int row, int col) const {
  const QTableWidgetItem *it = m_colsTable->item(row, col);
  return it ? it->text().trimmed() : QString();
}

bool JsonTableDesigner::cellChecked(int row, int col) const {
  const QTableWidgetItem *it = m_colsTable->item(row, col);
  return it && it->checkState() == Qt::Checked;
}

JsonTableColumn JsonTableDesigner::columnFromRow(int row) const {
  JsonTableColumn c;
  c.name = cellText(row, ColColName);
  c.mysqlType = cellText(row, ColColType);
  if (c.mysqlType.isEmpty()) c.mysqlType = QStringLiteral("bigint");
  c.isUnsigned = cellChecked(row, ColColUnsigned);
  c.nullable = cellChecked(row, ColColNullable);
  c.isPrimary = cellChecked(row, ColColPrimary);
  c.isAutoInc = cellChecked(row, ColColAutoInc);
  c.defaultValue = cellText(row, ColColDefault);
  c.comment = cellText(row, ColColComment);
  return c;
}

QVector<JsonTableColumn> JsonTableDesigner::collectColumns() const {
  QVector<JsonTableColumn> out;
  for (int r = 0; r < m_colsTable->rowCount(); ++r) {
    JsonTableColumn c = columnFromRow(r);
    if (c.name.isEmpty()) continue;  // 跳过未填列名的行
    out.append(c);
  }
  return out;
}

void JsonTableDesigner::addColumn() {
  appendColumnRow(JsonTableColumn());  // 模型缺省：bigint / list=true / form=input
  const int row = m_colsTable->rowCount() - 1;
  m_colsTable->setCurrentCell(row, ColColName);
  m_colsTable->editItem(m_colsTable->item(row, ColColName));  // 直接进入列名编辑
}

void JsonTableDesigner::deleteColumn() {
  const int row = m_colsTable->currentRow();
  if (row < 0) return;
  const QString name = cellText(row, ColColName);
  if (name.isEmpty()) {
    m_colsTable->removeRow(row);
    refreshColumnCandidates();
    return;
  }

  // 在模型副本上删除并清理引用；用户取消时界面不受影响
  JsonTableTable table = collectTable();
  for (int i = 0; i < table.columns.size(); ++i) {
    if (table.columns[i].name.compare(name, Qt::CaseInsensitive) == 0) {
      table.columns.removeAt(i);
      break;
    }
  }
  const RefCleanupStats cleanup = cleanRemovedColumnRefs(&table, QStringList() << name);
  if (!cleanup.details.isEmpty()) {
    const QString message =
        QStringLiteral("删除列「%1」将同时清理以下前端引用：\n\n%2\n\n是否继续？")
            .arg(name, cleanup.details.join(QStringLiteral("\n")));
    if (!AuiMessageBox::confirm(this, QStringLiteral("删除列并清理引用"), message)) return;
  }

  load(table, m_config, m_filePath);
  emit configChanged();
  updateValidateStatus();
}

void JsonTableDesigner::moveColumn(int delta) {
  const int row = m_colsTable->currentRow();
  const int target = row + delta;
  if (row < 0 || target < 0 || target >= m_colsTable->rowCount()) return;

  // 整行搬运：先摘下条目，删行后按新位置插回（复选框状态随条目保留）
  QVector<QTableWidgetItem *> items(ColColCount);
  for (int c = 0; c < ColColCount; ++c) items[c] = m_colsTable->takeItem(row, c);
  m_colsTable->removeRow(row);
  m_colsTable->insertRow(target);
  for (int c = 0; c < ColColCount; ++c) m_colsTable->setItem(target, c, items[c]);
  m_colsTable->setCurrentCell(target, ColColName);
  refreshColumnCandidates();  // 行搬运不触发 itemChanged，显式刷新候选
}

/// @brief 字段呈现表格行上移/下移（delta=-1 上移，1 下移）
/// 查询模式下拉是 cellWidget 不随条目搬运，单独交换选中值；
/// 其余列整行 takeItem 搬运（复选框/⚙/行内元数据随条目保留）
void JsonTableDesigner::moveViewRow(int delta) {
  const int row = m_viewTable->currentRow();
  const int target = row + delta;
  if (row < 0 || target < 0 || target >= m_viewTable->rowCount()) return;
  for (int c = 0; c < UColCount; ++c) {
    if (c == UColQuery) {
      auto *a = qobject_cast<QComboBox *>(m_viewTable->cellWidget(row, c));
      auto *b = qobject_cast<QComboBox *>(m_viewTable->cellWidget(target, c));
      if (a && b && a->currentIndex() != b->currentIndex()) {
        const int ia = a->currentIndex();
        const int ib = b->currentIndex();
        a->blockSignals(true);
        b->blockSignals(true);
        a->setCurrentIndex(ib);
        b->setCurrentIndex(ia);
        a->blockSignals(false);
        b->blockSignals(false);
      }
      continue;
    }
    QTableWidgetItem *it1 = m_viewTable->takeItem(row, c);
    QTableWidgetItem *it2 = m_viewTable->takeItem(target, c);
    m_viewTable->setItem(row, c, it2);
    m_viewTable->setItem(target, c, it1);
  }
  m_viewTable->setCurrentCell(target, UColRef);
  if (!m_loading) emit configChanged();
}

// ════════════════════════════════════════════════════════════
//  索引表格
// ════════════════════════════════════════════════════════════

void JsonTableDesigner::fillIndexes(const QVector<JsonTableIndex> &indexes) {
  m_indexTable->setRowCount(0);
  for (const auto &ix : indexes) {
    const int row = m_indexTable->rowCount();
    m_indexTable->insertRow(row);
    m_indexTable->setItem(row, IndexColName, new QTableWidgetItem(ix.name));
    m_indexTable->setItem(row, IndexColCols,
                          new QTableWidgetItem(ix.cols.join(QStringLiteral(","))));
    m_indexTable->setItem(row, IndexColUnique, makeCheckItem(ix.unique));
  }
}

void JsonTableDesigner::appendIndexRow() {
  const int row = m_indexTable->rowCount();
  m_indexTable->insertRow(row);
  m_indexTable->setItem(row, IndexColName, new QTableWidgetItem(QString()));
  m_indexTable->setItem(row, IndexColCols, new QTableWidgetItem(QString()));
  m_indexTable->setItem(row, IndexColUnique, makeCheckItem(false));
  m_indexTable->setCurrentCell(row, IndexColName);
}

QVector<JsonTableIndex> JsonTableDesigner::collectIndexes() const {
  QVector<JsonTableIndex> out;
  for (int r = 0; r < m_indexTable->rowCount(); ++r) {
    JsonTableIndex ix;
    const QTableWidgetItem *nameIt = m_indexTable->item(r, IndexColName);
    ix.name = nameIt ? nameIt->text().trimmed() : QString();
    if (ix.name.isEmpty()) continue;  // 跳过未填索引名的行
    const QTableWidgetItem *colsIt = m_indexTable->item(r, IndexColCols);
    const QString colsStr = colsIt ? colsIt->text() : QString();
    for (const QString &c : colsStr.split(QLatin1Char(','))) {
      const QString col = c.trimmed();
      if (!col.isEmpty()) ix.cols.append(col);
    }
    const QTableWidgetItem *uniqIt = m_indexTable->item(r, IndexColUnique);
    ix.unique = uniqIt && uniqIt->checkState() == Qt::Checked;
    out.append(ix);
  }
  return out;
}

// ════════════════════════════════════════════════════════════
//  联表查询声明（joinTables 新格式）/ 列表读模型（selCols）
// ════════════════════════════════════════════════════════════

void JsonTableDesigner::fillJoins(const QVector<JsonTableJoinSpec> &joins) {
  // 被联表下拉候选 = 项目内全部表名（跨文件；m_filePath 所在目录为搜索根）
  if (m_joinTableDelegate) {
    const QString searchDir = m_filePath.isEmpty() ? QStringLiteral(PROJECT_SOURCE_DIR)
                                                   : QFileInfo(m_filePath).absolutePath();
    QStringList tableNames;
    for (const auto &t : listProjectTables(searchDir)) {
      if (!t.tableName.isEmpty()) tableNames.append(t.tableName);
    }
    static_cast<EditableComboDelegate *>(m_joinTableDelegate)->setOptions(tableNames);
  }
  m_joinTable->setRowCount(0);
  for (const auto &j : joins) appendJoinRow();
  // 逐行回填（appendJoinRow 建的是空行）
  for (int r = 0; r < joins.size() && r < m_joinTable->rowCount(); ++r) {
    const JsonTableJoinSpec &j = joins[r];
    m_joinTable->setItem(r, JoinColTable, new QTableWidgetItem(j.table));
    auto *typeCombo = qobject_cast<QComboBox *>(m_joinTable->cellWidget(r, JoinColType));
    if (typeCombo) comboSelectData(typeCombo, j.type);
    if (j.on.size() == 2) {
      m_joinTable->setItem(r, JoinColOnMain, new QTableWidgetItem(j.on.at(0)));
      m_joinTable->setItem(r, JoinColOnJoin, new QTableWidgetItem(j.on.at(1)));
    }
    QStringList colParts;
    for (const auto &c : j.cols) {
      colParts.append(c.filter.isEmpty() ? c.col : c.col + QLatin1Char(':') + c.filter);
    }
    m_joinTable->setItem(r, JoinColOutCols,
                         new QTableWidgetItem(colParts.join(QStringLiteral(","))));
  }
}

void JsonTableDesigner::appendJoinRow() {
  const int row = m_joinTable->rowCount();
  m_joinTable->insertRow(row);
  m_joinTable->setItem(row, JoinColTable, new QTableWidgetItem(QString()));
  auto *typeCombo = AuiComboBox::create(this);
  typeCombo->addItem(QStringLiteral("LEFT"), QStringLiteral("LEFT"));
  typeCombo->addItem(QStringLiteral("INNER"), QStringLiteral("INNER"));
  m_joinTable->setCellWidget(row, JoinColType, typeCombo);
  m_joinTable->setItem(row, JoinColOnMain, new QTableWidgetItem(QString()));
  m_joinTable->setItem(row, JoinColOnJoin, new QTableWidgetItem(QString()));
  m_joinTable->setItem(row, JoinColOutCols, new QTableWidgetItem(QString()));
  m_joinTable->setCurrentCell(row, JoinColTable);
}

QVector<JsonTableJoinSpec> JsonTableDesigner::collectJoins() const {
  QVector<JsonTableJoinSpec> out;
  for (int r = 0; r < m_joinTable->rowCount(); ++r) {
    JsonTableJoinSpec j;
    j.table = cellOf(m_joinTable, r, JoinColTable).trimmed();
    auto *typeCombo = qobject_cast<QComboBox *>(m_joinTable->cellWidget(r, JoinColType));
    j.type = typeCombo ? typeCombo->currentData().toString() : QStringLiteral("LEFT");
    if (j.type.isEmpty()) j.type = QStringLiteral("LEFT");
    j.on.append(cellOf(m_joinTable, r, JoinColOnMain).trimmed());
    j.on.append(cellOf(m_joinTable, r, JoinColOnJoin).trimmed());
    const QString colsText = cellOf(m_joinTable, r, JoinColOutCols).trimmed();
    const QStringList parts = colsText.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
      const QString p = part.trimmed();
      if (p.isEmpty()) continue;
      JsonTableJoinCol c;
      const int colon = p.indexOf(QLatin1Char(':'));
      if (colon > 0) {
        c.col = p.left(colon).trimmed();
        c.filter = p.mid(colon + 1).trimmed();
      } else {
        c.col = p;
      }
      if (!c.col.isEmpty()) j.cols.append(c);
    }
    // 全空行（表/ON/输出列均未填）跳过
    if (j.table.isEmpty() && j.on.value(0).isEmpty() && j.on.value(1).isEmpty() &&
        j.cols.isEmpty()) {
      continue;
    }
    out.append(j);
  }
  return out;
}

// ════════════════════════════════════════════════════════════
//  字段呈现（统一表格：三视图双向映射 + ⚙ 字段设置面板）
// ════════════════════════════════════════════════════════════

/// @brief 配置摘要（对齐 jsonvue 配置列：一行式只读展示关键设置）
/// 形如 "取值域数据源 dropurl, 显示:image, 编辑:select, 列宽:120"；
/// 空值按缺省语义显示（displayType 空=text、widget 空=text、未配置不显示该段）；
/// domainType 来自列级取值域（物理列）——⚙ 面板取值域区
static QString viewCfgSummary(const JsonTableSelCol &sc, const JsonTableEditCol &ec,
                              const QString &domainType) {
  QStringList parts;
  if (!domainType.isEmpty()) {
    parts << (QStringLiteral("取值域:") + domainType);
  }
  parts << (QStringLiteral("显示:") +
            (sc.displayType.isEmpty() ? QStringLiteral("text") : sc.displayType));
  parts << (QStringLiteral("编辑:") + (ec.widget.isEmpty() ? QStringLiteral("text") : ec.widget));
  if (sc.width > 0) {
    parts << (QStringLiteral("列宽:") + QString::number(sc.width));
  }
  return parts.join(QStringLiteral(", "));
}

void JsonTableDesigner::fillViewTable() {
  m_viewTable->setRowCount(0);
  // 联表上下文：引用名 → {目标表名, 主键, 列类型, 列注释}（行填充/⚙面板共用）
  struct JoinCtx {
    QString tableName;
    QString pk;
    QHash<QString, QString> types;
    QHash<QString, QString> comments;
  };
  QHash<QString, JoinCtx> joins;
  for (const auto &j : m_table.joinTable) {
    if (!j.isJoinRelations || j.joinType != QStringLiteral("ManyToOne") ||
        j.joinEnClassPro.isEmpty()) {
      continue;
    }
    for (const auto &t : m_config.tables) {
      if (t.modelName != j.joinEnClass) continue;
      JoinCtx ctx;
      ctx.tableName = t.tableName;
      ctx.pk = j.joinPk.isEmpty() ? QStringLiteral("id") : j.joinPk;
      for (const auto &c : t.columns) {
        ctx.types.insert(c.name, c.mysqlType);
        ctx.comments.insert(c.name, c.comment);
      }
      joins.insert(j.joinEnClassPro, ctx);
      break;
    }
  }
  // 新格式联表输出别名：ref（表名_列名）→ 目标列注释（标题回退）
  QSet<QString> aliasRefs;
  QHash<QString, QString> aliasComments;
  if (!m_table.joinTables.isEmpty()) {
    const QString joinDir = m_filePath.isEmpty() ? QStringLiteral(PROJECT_SOURCE_DIR)
                                                 : QFileInfo(m_filePath).absolutePath();
    QHash<QString, QHash<QString, QString>> targetComments;  // 表名 → 列注释
    for (const auto &t : listProjectTables(joinDir)) {
      QHash<QString, QString> cm;
      for (const auto &c : t.columns) cm.insert(c.name, c.comment);
      targetComments.insert(t.tableName, cm);
    }
    for (const auto &js : m_table.joinTables) {
      const auto tc = targetComments.constFind(js.table);
      for (const auto &c : js.cols) {
        const QString alias = js.table + QLatin1Char('_') + c.col;
        aliasRefs.insert(alias);
        if (tc != targetComments.constEnd() && tc.value().contains(c.col)) {
          aliasComments.insert(alias, tc.value().value(c.col));
        }
      }
    }
  }
  // 三视图配置索引（ref → 项）
  QHash<QString, JsonTableSelCol> selOf;
  for (const auto &c : m_table.selCols) selOf.insert(c.ref, c);
  QHash<QString, JsonTableQueryCol> queryOf;  // 命中模式（后写覆盖，同 ref 多模式罕见）
  for (const auto &c : m_table.selColsEq) queryOf.insert(c.ref, c);
  for (const auto &c : m_table.selColsLike) queryOf.insert(c.ref, c);
  for (const auto &c : m_table.selColsGap) queryOf.insert(c.ref, c);
  QHash<QString, JsonTableEditCol> editOf;
  for (const auto &e : m_table.editCols) editOf.insert(e.ref, e);
  // 物理列注释（标题回退用）
  QHash<QString, QString> colComments;
  for (const auto &c : m_table.columns) colComments.insert(c.name, c.comment);
  QHash<QString, QString> colTitles;
  for (const auto &c : m_table.columns) colTitles.insert(c.name, c.title);

  // 模式判定需区分键归属，循环三键填 mode2（ref → {mode, visible}）
  QHash<QString, QPair<QString, bool>> mode2;
  for (const auto &c : m_table.selColsEq) mode2.insert(c.ref, {QStringLiteral("eq"), c.visible});
  for (const auto &c : m_table.selColsLike)
    mode2.insert(c.ref, {QStringLiteral("like"), c.visible});
  for (const auto &c : m_table.selColsGap) mode2.insert(c.ref, {QStringLiteral("gap"), c.visible});

  auto appendRow = [&](const QString &ref, bool isJoinRow) {
    const int row = m_viewTable->rowCount();
    m_viewTable->insertRow(row);
    // 标题（生效值）：selCols.label > 列定义 title > 列注释(本表/联表目标列) > ref
    QString title;
    const auto selIt = selOf.constFind(ref);
    if (selIt != selOf.constEnd() && !selIt.value().label.isEmpty()) {
      title = selIt.value().label;
    } else if (colTitles.contains(ref) && !colTitles.value(ref).isEmpty()) {
      title = colTitles.value(ref);
    } else if (isJoinRow) {
      const int dot = ref.indexOf(QLatin1Char('.'));
      if (dot > 0) {
        const QString pro = ref.left(dot);
        const QString cn = ref.mid(dot + 1);
        const auto jc = joins.constFind(pro);
        if (jc != joins.constEnd() && jc.value().comments.contains(cn)) {
          title = jc.value().comments.value(cn);
        }
      }
      // 新格式联表输出别名：标题回退 = 目标列注释
      if (title.isEmpty() && aliasComments.contains(ref)) {
        title = aliasComments.value(ref);
      }
      if (title.isEmpty()) title = ref;
    } else if (colComments.contains(ref) && !colComments.value(ref).isEmpty()) {
      title = colComments.value(ref);
    } else {
      title = ref;
    }
    auto *refItem = new QTableWidgetItem(ref);
    const auto selMeta = selOf.value(ref);
    const auto editMeta = editOf.value(ref);
    refItem->setData(Qt::UserRole + 1, selMeta.width > 0 ? selMeta.width : 0);  // 列宽
    refItem->setData(Qt::UserRole + 2, editMeta.widget);                        // 控件类型
    // 列表展示/上传绑定/缩略图（⚙ 面板读写；collectViewTable 落三视图）
    refItem->setData(Qt::UserRole + 4, selMeta.displayType);  // 展示类型
    refItem->setData(Qt::UserRole + 5,
                     selMeta.listThumbWidth > 0 ? selMeta.listThumbWidth : 0);  // 列表缩略图宽
    refItem->setData(Qt::UserRole + 6,
                     selMeta.listThumbHeight > 0 ? selMeta.listThumbHeight : 0);  // 列表缩略图高
    refItem->setData(Qt::UserRole + 7, editMeta.uploadSourceFile);                // 上传预设文件
    refItem->setData(Qt::UserRole + 8, editMeta.uploadSourceId);                  // 上传预设 id
    refItem->setData(Qt::UserRole + 9,
                     editMeta.editThumbWidth > 0 ? editMeta.editThumbWidth : 0);  // 编辑缩略图宽
    refItem->setData(Qt::UserRole + 10,
                     editMeta.editThumbHeight > 0 ? editMeta.editThumbHeight : 0);  // 编辑缩略图高
    // 列级取值域（物理列；+11..+14 暂存，collectTable 回写 columns）
    QString dType, dFile, dId, dUrl;
    for (const auto &c : m_table.columns) {
      if (c.name == ref) {
        dType = c.domainType;
        dFile = c.domainSourceFile;
        dId = c.domainSourceId;
        dUrl = c.domainUrl;
        break;
      }
    }
    refItem->setData(Qt::UserRole + 11, dType);
    refItem->setData(Qt::UserRole + 12, dFile);
    refItem->setData(Qt::UserRole + 13, dId);
    refItem->setData(Qt::UserRole + 14, dUrl);
    // 编辑表单精细参数（+15..+23；⚙ 面板读写，collectViewTable 落 editCols）
    refItem->setData(Qt::UserRole + 15, editMeta.placeholder);
    refItem->setData(Qt::UserRole + 16, editMeta.maxlength > 0 ? editMeta.maxlength : 0);
    refItem->setData(Qt::UserRole + 17, editMeta.textareaRows > 0 ? editMeta.textareaRows : 0);
    refItem->setData(Qt::UserRole + 18, editMeta.minValue != 0 ? editMeta.minValue : 0);
    refItem->setData(Qt::UserRole + 19, editMeta.maxValue != 0 ? editMeta.maxValue : 0);
    refItem->setData(Qt::UserRole + 20, editMeta.precision > 0 ? editMeta.precision : 0);
    refItem->setData(Qt::UserRole + 21, editMeta.dateFormat);
    refItem->setData(Qt::UserRole + 22, editMeta.formSpan > 0 ? editMeta.formSpan : 0);
    refItem->setData(Qt::UserRole + 23, editMeta.formDefaultValue);
    m_viewTable->setItem(row, UColRef, refItem);
    m_viewTable->setItem(row, UColTitle, new QTableWidgetItem(title));
    // 列表勾选
    auto *listItem = new QTableWidgetItem;
    listItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    listItem->setCheckState(selOf.contains(ref) ? Qt::Checked : Qt::Unchecked);
    listItem->setTextAlignment(Qt::AlignCenter);
    m_viewTable->setItem(row, UColList, listItem);
    // 查询模式下拉（不查询/精确/模糊/范围/仅接口）
    auto *modeCombo = AuiComboBox::create(this);
    modeCombo->addItem(QStringLiteral("不查询"), QStringLiteral("none"));
    modeCombo->addItem(QStringLiteral("精确"), QStringLiteral("eq"));
    modeCombo->addItem(QStringLiteral("模糊"), QStringLiteral("like"));
    modeCombo->addItem(QStringLiteral("范围"), QStringLiteral("gap"));
    modeCombo->addItem(QStringLiteral("仅接口"), QStringLiteral("api"));
    QString mode = QStringLiteral("none");
    bool qVisible = true;
    if (const auto mIt = mode2.constFind(ref); mIt != mode2.constEnd()) {
      mode = mIt.value().first;
      qVisible = mIt.value().second;
    }
    if (!qVisible) mode = QStringLiteral("api");
    comboSelectData(modeCombo, mode);
    connect(modeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
      if (!m_loading) emit configChanged();
    });
    m_viewTable->setCellWidget(row, UColQuery, modeCombo);
    // 编辑勾选（两态；联表行勾选 = 编辑表单内只读展示，editEditable=false）
    // 旧配置 visible:false 视作未勾选（生成端本就过滤该字段，语义等价，重存后此项移除）
    auto *editItem = new QTableWidgetItem;
    editItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    if (const auto eIt = editOf.constFind(ref); eIt != editOf.constEnd() && eIt.value().visible) {
      editItem->setCheckState(Qt::Checked);
    } else {
      editItem->setCheckState(Qt::Unchecked);
    }
    editItem->setTextAlignment(Qt::AlignCenter);
    m_viewTable->setItem(row, UColEdit, editItem);
    // 必填勾选
    auto *reqItem = new QTableWidgetItem;
    reqItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                      (isJoinRow ? Qt::NoItemFlags : Qt::ItemIsUserCheckable));
    reqItem->setCheckState(editMeta.required ? Qt::Checked : Qt::Unchecked);
    reqItem->setTextAlignment(Qt::AlignCenter);
    m_viewTable->setItem(row, UColRequired, reqItem);
    // 配置列（⚙ 与摘要合成一列，对齐 jsonvue；点击单元格打开设置面板）
    auto *cfgItem =
        new QTableWidgetItem(QStringLiteral("⚙ ") + viewCfgSummary(selMeta, editMeta, dType));
    cfgItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_viewTable->setItem(row, UColCfg, cfgItem);
    // 编辑可编辑槽位（+24；联表行固定只读，物理列取 editCols 配置，缺省 true）
    refItem->setData(
        Qt::UserRole + 24,
        isJoinRow ? 0 : (editOf.contains(ref) ? (editOf.value(ref).editEditable ? 1 : 0) : 1));
  };

  // 行序 = selCols 顺序优先（字段呈现里的排序即列表列顺序，保存重开不丢）；
  // 未列出的 ref（未勾选列表的物理列/仅查询 ref/旧格式联表路径/输出别名）追加在后
  QStringList orderedRefs;
  auto pushRef = [&](const QString &r) {
    if (r.isEmpty() || orderedRefs.contains(r)) return;
    orderedRefs.append(r);
  };
  for (const auto &sc : m_table.selCols) pushRef(sc.ref);
  for (const auto &c : m_table.columns) pushRef(c.name);
  for (auto it = selOf.constBegin(); it != selOf.constEnd(); ++it) pushRef(it.key());
  for (auto it = queryOf.constBegin(); it != queryOf.constEnd(); ++it) pushRef(it.key());
  for (const QString &ref : orderedRefs) {
    const bool isJoin = ref.contains(QLatin1Char('.')) || aliasRefs.contains(ref);
    appendRow(ref, isJoin);
  }
}

void JsonTableDesigner::appendViewJoinRow(const QString &ref) {
  // 联表行：列表/查询默认开；编辑列可勾选（= 编辑表单内只读展示，editEditable=false）
  const int row = m_viewTable->rowCount();
  m_viewTable->insertRow(row);
  auto *refItem = new QTableWidgetItem(ref);
  refItem->setData(Qt::UserRole + 1, 0);          // 列宽
  refItem->setData(Qt::UserRole + 2, QString());  // 控件类型
  refItem->setData(Qt::UserRole + 4, QString());  // 展示类型
  refItem->setData(Qt::UserRole + 5, 0);
  refItem->setData(Qt::UserRole + 6, 0);
  refItem->setData(Qt::UserRole + 7, QString());
  refItem->setData(Qt::UserRole + 8, QString());
  refItem->setData(Qt::UserRole + 9, 0);
  refItem->setData(Qt::UserRole + 10, 0);
  refItem->setData(Qt::UserRole + 11, QString());
  refItem->setData(Qt::UserRole + 12, QString());
  refItem->setData(Qt::UserRole + 13, QString());
  refItem->setData(Qt::UserRole + 14, QString());
  refItem->setData(Qt::UserRole + 15, QString());
  refItem->setData(Qt::UserRole + 16, 0);
  refItem->setData(Qt::UserRole + 17, 0);
  refItem->setData(Qt::UserRole + 18, 0);
  refItem->setData(Qt::UserRole + 19, 0);
  refItem->setData(Qt::UserRole + 20, 0);
  refItem->setData(Qt::UserRole + 21, QString());
  refItem->setData(Qt::UserRole + 22, 0);
  refItem->setData(Qt::UserRole + 23, QString());
  refItem->setData(Qt::UserRole + 24, 0);  // 联表列只读
  m_viewTable->setItem(row, UColRef, refItem);
  m_viewTable->setItem(row, UColTitle, new QTableWidgetItem(ref));
  auto *listItem = new QTableWidgetItem;
  listItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
  listItem->setCheckState(Qt::Checked);
  listItem->setTextAlignment(Qt::AlignCenter);
  m_viewTable->setItem(row, UColList, listItem);
  auto *modeCombo = AuiComboBox::create(this);
  modeCombo->addItem(QStringLiteral("不查询"), QStringLiteral("none"));
  modeCombo->addItem(QStringLiteral("精确"), QStringLiteral("eq"));
  modeCombo->addItem(QStringLiteral("模糊"), QStringLiteral("like"));
  modeCombo->addItem(QStringLiteral("范围"), QStringLiteral("gap"));
  modeCombo->addItem(QStringLiteral("仅接口"), QStringLiteral("api"));
  modeCombo->setCurrentIndex(1);  // 缺省精确
  connect(modeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
    if (!m_loading) emit configChanged();
  });
  m_viewTable->setCellWidget(row, UColQuery, modeCombo);
  auto *editItem = new QTableWidgetItem;
  editItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
  editItem->setTextAlignment(Qt::AlignCenter);
  m_viewTable->setItem(row, UColEdit, editItem);
  auto *reqItem = new QTableWidgetItem;
  reqItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
  reqItem->setTextAlignment(Qt::AlignCenter);
  m_viewTable->setItem(row, UColRequired, reqItem);
  // 配置列（⚙ 与摘要合成一列；联表行未配置 → 显示缺省 text 语义）
  auto *cfgItem = new QTableWidgetItem(QStringLiteral("⚙ 显示:text, 编辑:text"));
  cfgItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
  m_viewTable->setItem(row, UColCfg, cfgItem);
  m_viewTable->setCurrentCell(row, UColRef);
}

void JsonTableDesigner::collectViewTable(JsonTableTable *out) const {
  out->selCols.clear();
  out->selColsEq.clear();
  out->selColsLike.clear();
  out->selColsGap.clear();
  out->editCols.clear();
  for (int r = 0; r < m_viewTable->rowCount(); ++r) {
    const QString ref = cellOf(m_viewTable, r, UColRef);
    if (ref.isEmpty()) continue;
    const QString title = cellOf(m_viewTable, r, UColTitle);
    QString widthStr;
    QString widget;
    QString displayType;
    int listThumbW = 0;
    int listThumbH = 0;
    QString uploadFile;
    QString uploadId;
    int editThumbW = 0;
    int editThumbH = 0;
    QString domainType;
    QString domainFile;
    QString domainId;
    QString domainUrl;
    QString placeholder;
    int maxlength = 0;
    int textareaRows = 0;
    double minValue = 0;
    double maxValue = 0;
    int precision = 0;
    QString dateFormat;
    int formSpan = 0;
    QString formDefaultValue;
    if (const QTableWidgetItem *refIt = m_viewTable->item(r, UColRef)) {
      widthStr = refIt->data(Qt::UserRole + 1).toString();
      widget = refIt->data(Qt::UserRole + 2).toString();
      displayType = refIt->data(Qt::UserRole + 4).toString();
      listThumbW = refIt->data(Qt::UserRole + 5).toInt();
      listThumbH = refIt->data(Qt::UserRole + 6).toInt();
      uploadFile = refIt->data(Qt::UserRole + 7).toString();
      uploadId = refIt->data(Qt::UserRole + 8).toString();
      editThumbW = refIt->data(Qt::UserRole + 9).toInt();
      editThumbH = refIt->data(Qt::UserRole + 10).toInt();
      domainType = refIt->data(Qt::UserRole + 11).toString();
      domainFile = refIt->data(Qt::UserRole + 12).toString();
      domainId = refIt->data(Qt::UserRole + 13).toString();
      domainUrl = refIt->data(Qt::UserRole + 14).toString();
      placeholder = refIt->data(Qt::UserRole + 15).toString();
      maxlength = refIt->data(Qt::UserRole + 16).toInt();
      textareaRows = refIt->data(Qt::UserRole + 17).toInt();
      minValue = refIt->data(Qt::UserRole + 18).toDouble();
      maxValue = refIt->data(Qt::UserRole + 19).toDouble();
      precision = refIt->data(Qt::UserRole + 20).toInt();
      dateFormat = refIt->data(Qt::UserRole + 21).toString();
      formSpan = refIt->data(Qt::UserRole + 22).toInt();
      formDefaultValue = refIt->data(Qt::UserRole + 23).toString();
    }
    // 列表视图
    const QTableWidgetItem *listIt = m_viewTable->item(r, UColList);
    if (listIt && listIt->checkState() == Qt::Checked) {
      JsonTableSelCol c;
      c.ref = ref;
      if (!title.isEmpty() && title != ref) c.label = title;
      c.width = widthStr.toInt();
      c.displayType = displayType;
      c.listThumbWidth = listThumbW;
      c.listThumbHeight = listThumbH;
      out->selCols.append(c);
    }
    // 查询视图（模式下拉）
    auto *modeCombo = qobject_cast<QComboBox *>(m_viewTable->cellWidget(r, UColQuery));
    const QString mode = modeCombo ? modeCombo->currentData().toString() : QStringLiteral("none");
    if (mode != QStringLiteral("none")) {
      JsonTableQueryCol qc;
      qc.ref = ref;
      qc.visible = (mode != QStringLiteral("api"));  // 仅接口 = 后端生成、前端不渲染
      if (mode == QStringLiteral("eq"))
        out->selColsEq.append(qc);
      else if (mode == QStringLiteral("like"))
        out->selColsLike.append(qc);
      else if (mode == QStringLiteral("gap"))
        out->selColsGap.append(qc);
    }
    // 编辑视图（两态；联表行勾选 = 编辑表单内只读展示，editEditable=false）
    const QTableWidgetItem *editIt = m_viewTable->item(r, UColEdit);
    if (editIt && editIt->checkState() != Qt::Unchecked) {
      JsonTableEditCol ec;
      ec.ref = ref;
      ec.visible = (editIt->checkState() == Qt::Checked);
      ec.editEditable = false;  // 默认值，下面从行槽位覆盖
      // 编辑标题：唯一来源 = 「字段呈现」的标题列（空 = 生成端回退列注释）
      // 旧 editCols.label（⚙ 覆盖）已废弃，此处不再读取
      if (!title.isEmpty() && title != ref) {
        ec.label = title;
      }
      ec.widget = widget;
      ec.uploadSourceFile = uploadFile;
      ec.uploadSourceId = uploadId;
      ec.editThumbWidth = editThumbW;
      ec.editThumbHeight = editThumbH;
      ec.placeholder = placeholder;
      ec.maxlength = maxlength;
      ec.textareaRows = textareaRows;
      ec.minValue = minValue;
      ec.maxValue = maxValue;
      ec.precision = precision;
      ec.dateFormat = dateFormat;
      ec.formSpan = formSpan;
      ec.formDefaultValue = formDefaultValue;
      const QTableWidgetItem *reqIt = m_viewTable->item(r, UColRequired);
      ec.required = reqIt && reqIt->checkState() == Qt::Checked;
      // 编辑可编辑槽位（+24）：联表行固定 false（只读展示），物理列取 ⚙ 配置
      if (const QTableWidgetItem *eeIt = m_viewTable->item(r, UColRef)) {
        ec.editEditable = eeIt->data(Qt::UserRole + 24).toInt() != 0;
      }
      out->editCols.append(ec);
    }
    // 列级取值域暂存 → 回写物理列定义（联表行无域；域随列存储）
    if (!ref.contains(QLatin1Char('.')) && !domainType.isEmpty()) {
      for (auto &c : out->columns) {
        if (c.name == ref) {
          c.domainType = domainType;
          c.domainSourceFile = domainFile;
          c.domainSourceId = domainId;
          c.domainUrl = domainUrl;
          break;
        }
      }
    }
    // 列级标题 → 回写物理列定义（三视图共用一个标题；与注释/引用相同的值
    // 视为未覆写不落键，避免批量固化注释导致后续注释修改无法传导）
    if (!ref.contains(QLatin1Char('.'))) {
      for (auto &c : out->columns) {
        if (c.name == ref) {
          c.title = (title.isEmpty() || title == ref || title == c.comment) ? QString() : title;
          break;
        }
      }
    }
  }
}

void JsonTableDesigner::openViewFieldConfig(int row) {
  if (row < 0 || row >= m_viewTable->rowCount()) return;
  const QString ref = cellOf(m_viewTable, row, UColRef);
  if (ref.isEmpty()) return;
  // 联表行 = 旧格式 pro.col 或 新格式输出别名（编辑表单区整区折叠：别名只读展示）
  bool isJoinRow = ref.contains(QLatin1Char('.'));
  if (!isJoinRow) {
    for (const auto &js : m_table.joinTables) {
      for (const auto &c : js.cols) {
        if (ref == js.table + QLatin1Char('_') + c.col) {
          isJoinRow = true;
          break;
        }
      }
      if (isJoinRow) break;
    }
  }
  QTableWidgetItem *refIt = m_viewTable->item(row, UColRef);
  if (!refIt) return;

  // 行槽位 → 双视图结构（列表侧 sc / 编辑侧 ec）
  JsonTableSelCol sc;
  sc.ref = ref;
  sc.width = refIt->data(Qt::UserRole + 1).toInt();
  sc.displayType = refIt->data(Qt::UserRole + 4).toString();
  sc.listThumbWidth = refIt->data(Qt::UserRole + 5).toInt();
  sc.listThumbHeight = refIt->data(Qt::UserRole + 6).toInt();
  JsonTableEditCol ec;
  ec.widget = refIt->data(Qt::UserRole + 2).toString();
  ec.editEditable = refIt->data(Qt::UserRole + 24).toInt() != 0;
  ec.uploadSourceFile = refIt->data(Qt::UserRole + 7).toString();
  ec.uploadSourceId = refIt->data(Qt::UserRole + 8).toString();
  ec.editThumbWidth = refIt->data(Qt::UserRole + 9).toInt();
  ec.editThumbHeight = refIt->data(Qt::UserRole + 10).toInt();
  ec.placeholder = refIt->data(Qt::UserRole + 15).toString();
  ec.maxlength = refIt->data(Qt::UserRole + 16).toInt();
  ec.textareaRows = refIt->data(Qt::UserRole + 17).toInt();
  ec.minValue = refIt->data(Qt::UserRole + 18).toDouble();
  ec.maxValue = refIt->data(Qt::UserRole + 19).toDouble();
  ec.precision = refIt->data(Qt::UserRole + 20).toInt();
  ec.dateFormat = refIt->data(Qt::UserRole + 21).toString();
  ec.formSpan = refIt->data(Qt::UserRole + 22).toInt();
  ec.formDefaultValue = refIt->data(Qt::UserRole + 23).toString();

  ViewFieldConfigDialog dlg(ref, isJoinRow, this);
  dlg.setSearchRoot(m_filePath.isEmpty() ? QString() : QFileInfo(m_filePath).absolutePath());
  dlg.setCurrent(sc, ec);
  if (!isJoinRow) {
    // 列级取值域初值（+11..+14 暂存槽位）
    JsonTableColumn dc;
    dc.domainType = refIt->data(Qt::UserRole + 11).toString();
    dc.domainSourceFile = refIt->data(Qt::UserRole + 12).toString();
    dc.domainSourceId = refIt->data(Qt::UserRole + 13).toString();
    dc.domainUrl = refIt->data(Qt::UserRole + 14).toString();
    dlg.setDomain(dc);
  }
  if (dlg.exec() != QDialog::Accepted) return;
  // 确定后写回行槽位（collectViewTable 读取落三视图/列定义），并刷新配置摘要
  dlg.fillListResult(&sc);
  dlg.fillEditResult(&ec);
  refIt->setData(Qt::UserRole + 1, sc.width);
  refIt->setData(Qt::UserRole + 2, ec.widget);
  refIt->setData(Qt::UserRole + 4, sc.displayType);
  refIt->setData(Qt::UserRole + 5, sc.listThumbWidth);
  refIt->setData(Qt::UserRole + 6, sc.listThumbHeight);
  refIt->setData(Qt::UserRole + 7, ec.uploadSourceFile);
  refIt->setData(Qt::UserRole + 8, ec.uploadSourceId);
  refIt->setData(Qt::UserRole + 9, ec.editThumbWidth);
  refIt->setData(Qt::UserRole + 10, ec.editThumbHeight);
  refIt->setData(Qt::UserRole + 15, ec.placeholder);
  refIt->setData(Qt::UserRole + 16, ec.maxlength);
  refIt->setData(Qt::UserRole + 17, ec.textareaRows);
  refIt->setData(Qt::UserRole + 18, ec.minValue);
  refIt->setData(Qt::UserRole + 19, ec.maxValue);
  refIt->setData(Qt::UserRole + 20, ec.precision);
  refIt->setData(Qt::UserRole + 21, ec.dateFormat);
  refIt->setData(Qt::UserRole + 22, ec.formSpan);
  refIt->setData(Qt::UserRole + 23, ec.formDefaultValue);
  refIt->setData(Qt::UserRole + 24, ec.editEditable ? 1 : 0);
  refIt->setData(Qt::UserRole + 11, dlg.domainType());
  refIt->setData(Qt::UserRole + 12, dlg.domainSourceFile());
  refIt->setData(Qt::UserRole + 13, dlg.domainSourceId());
  refIt->setData(Qt::UserRole + 14, dlg.domainUrl());
  // 刷新配置摘要（domainType 取写回后的列级取值域）
  if (auto *cfgIt = m_viewTable->item(row, UColCfg)) {
    cfgIt->setText(QStringLiteral("⚙ ") + viewCfgSummary(sc, ec, dlg.domainType()));
  }
}
// ════════════════════════════════════════════════════════════
//  表级进阶键：查询配置 / 排序 / 表内枚举 / i18n
// ════════════════════════════════════════════════════════════

QStringList JsonTableDesigner::splitCsv(const QString &text) {
  QStringList out;
  for (const QString &part : text.split(QLatin1Char(','))) {
    const QString item = part.trimmed();
    if (!item.isEmpty()) out.append(item);
  }
  return out;
}

QString JsonTableDesigner::joinCsv(const QStringList &list) {
  return list.join(QStringLiteral(","));
}

void JsonTableDesigner::fillSortTable(const QVector<QPair<QString, QString>> &sort) {
  m_sortTable->setRowCount(0);
  for (const auto &p : sort) appendSortRow(p.first, p.second);
}

void JsonTableDesigner::appendSortRow(const QString &col, const QString &dir) {
  const int row = m_sortTable->rowCount();
  m_sortTable->insertRow(row);
  m_sortTable->setItem(row, 0, new QTableWidgetItem(col));
  auto *dirCombo = AuiComboBox::create(this);
  dirCombo->addItem(QStringLiteral("ASC"), QStringLiteral("ASC"));
  dirCombo->addItem(QStringLiteral("DESC"), QStringLiteral("DESC"));
  comboSelectData(dirCombo, dir.isEmpty() ? QStringLiteral("DESC") : dir);
  connect(dirCombo, &QComboBox::currentIndexChanged, this, [this](int) {
    if (!m_loading) emit configChanged();
  });
  m_sortTable->setCellWidget(row, 1, dirCombo);
}

QVector<QPair<QString, QString>> JsonTableDesigner::collectSort() const {
  QVector<QPair<QString, QString>> out;
  for (int r = 0; r < m_sortTable->rowCount(); ++r) {
    const QTableWidgetItem *colIt = m_sortTable->item(r, 0);
    const QString col = colIt ? colIt->text().trimmed() : QString();
    if (col.isEmpty()) continue;  // 跳过排序列为空的行
    auto *dirCombo = qobject_cast<QComboBox *>(m_sortTable->cellWidget(r, 1));
    QString dir = dirCombo ? dirCombo->currentData().toString() : QString();
    if (dir.isEmpty()) dir = QStringLiteral("DESC");
    out.append({col, dir});
  }
  return out;
}

// ════════════════════════════════════════════════════════════
//  操作按钮（buttons：键名与 .jsonvue ButtonConfig 一致）
// ════════════════════════════════════════════════════════════

void JsonTableDesigner::fillButtons() {
  m_buttonTable->setRowCount(0);
  for (int i = 0; i < m_buttons.size(); ++i) {
    appendButtonRow(i);
  }
}

void JsonTableDesigner::appendButtonRow(int idx) {
  if (idx < 0 || idx >= m_buttons.size()) return;
  const ButtonConfig &b = m_buttons[idx];
  const int row = m_buttonTable->rowCount();
  m_buttonTable->insertRow(row);
  m_buttonTable->setItem(row, 0, new QTableWidgetItem(b.label));
  m_buttonTable->setItem(
      row, 1,
      new QTableWidgetItem(b.position == ButtonPosition::Toolbar ? QStringLiteral("工具栏")
                                                                 : QStringLiteral("行操作")));
  QString action;
  switch (b.actionType) {
    case ButtonActionType::Ajax:
      action = QStringLiteral("调 API");
      break;
    case ButtonActionType::Confirm:
      action = QStringLiteral("二次确认");
      break;
    case ButtonActionType::Dialog:
      action = QStringLiteral("对话框");
      break;
    case ButtonActionType::Link:
      action = QStringLiteral("跳转");
      break;
  }
  m_buttonTable->setItem(row, 2, new QTableWidgetItem(action));
  m_buttonTable->setItem(row, 3, new QTableWidgetItem(b.actionKey));
}

void JsonTableDesigner::onAddButton() {
  ButtonConfigDialog dlg(this);
  dlg.setSearchRoot(m_filePath.isEmpty() ? QString() : QFileInfo(m_filePath).absolutePath());
  if (dlg.exec() != QDialog::Accepted) return;
  m_buttons.append(dlg.getData());
  appendButtonRow(m_buttons.size() - 1);
}

void JsonTableDesigner::onEditButton(int row) {
  if (row < 0 || row >= m_buttons.size()) return;
  ButtonConfigDialog dlg(m_buttons[row], this);
  dlg.setSearchRoot(m_filePath.isEmpty() ? QString() : QFileInfo(m_filePath).absolutePath());
  if (dlg.exec() != QDialog::Accepted) return;
  m_buttons[row] = dlg.getData();
  // 摘要刷新：重建该行文本（行数不变）
  const ButtonConfig &b = m_buttons[row];
  m_buttonTable->item(row, 0)->setText(b.label);
  m_buttonTable->item(row, 1)->setText(
      b.position == ButtonPosition::Toolbar ? QStringLiteral("工具栏") : QStringLiteral("行操作"));
  QString action;
  switch (b.actionType) {
    case ButtonActionType::Ajax:
      action = QStringLiteral("调 API");
      break;
    case ButtonActionType::Confirm:
      action = QStringLiteral("二次确认");
      break;
    case ButtonActionType::Dialog:
      action = QStringLiteral("对话框");
      break;
    case ButtonActionType::Link:
      action = QStringLiteral("跳转");
      break;
  }
  m_buttonTable->item(row, 2)->setText(action);
  m_buttonTable->item(row, 3)->setText(b.actionKey);
}

void JsonTableDesigner::onRemoveButton() {
  int row = m_buttonTable->currentRow();
  if (row < 0 || row >= m_buttons.size()) return;
  m_buttonTable->removeRow(row);
  m_buttons.removeAt(row);
}

void JsonTableDesigner::syncEnumDrafts() {
  // 主表编辑内容 → m_enums（列名/说明直接读表格条目）
  for (int r = 0; r < m_enumTable->rowCount() && r < m_enums.size(); ++r) {
    const QTableWidgetItem *colIt = m_enumTable->item(r, 0);
    const QTableWidgetItem *cmtIt = m_enumTable->item(r, 1);
    if (colIt) m_enums[r].column = colIt->text().trimmed();
    if (cmtIt) m_enums[r].comment = cmtIt->text().trimmed();
  }
  // 选项表格 → 当前行
  if (m_enumCurRow >= 0 && m_enumCurRow < m_enums.size()) {
    m_enums[m_enumCurRow].options = collectEnumOptions();
  }
}

void JsonTableDesigner::rebuildEnumMaster() {
  m_enumSyncing = true;
  m_enumTable->setRowCount(0);
  for (const auto &e : m_enums) {
    const int row = m_enumTable->rowCount();
    m_enumTable->insertRow(row);
    m_enumTable->setItem(row, 0, new QTableWidgetItem(e.column));
    m_enumTable->setItem(row, 1, new QTableWidgetItem(e.comment));
  }
  m_enumSyncing = false;
}

void JsonTableDesigner::fillEnumOptions(int row) {
  m_enumSyncing = true;
  m_enumCurRow = (row >= 0 && row < m_enums.size()) ? row : -1;
  m_enumOptTable->setRowCount(0);
  if (m_enumCurRow >= 0) {
    for (const auto &o : m_enums[m_enumCurRow].options) {
      const int r = m_enumOptTable->rowCount();
      m_enumOptTable->insertRow(r);
      m_enumOptTable->setItem(r, 0, new QTableWidgetItem(o.key));
      m_enumOptTable->setItem(r, 1, new QTableWidgetItem(o.label));
      m_enumOptTable->setItem(r, 2, new QTableWidgetItem(o.value));
      auto *typeCombo = AuiComboBox::create(this);
      typeCombo->addItem(QStringLiteral("string"), QStringLiteral("string"));
      typeCombo->addItem(QStringLiteral("number"), QStringLiteral("number"));
      comboSelectData(typeCombo, o.valueType.isEmpty() ? QStringLiteral("string") : o.valueType);
      m_enumOptTable->setCellWidget(r, 3, typeCombo);
    }
  }
  m_enumSyncing = false;
}

void JsonTableDesigner::appendEnumOptRow() {
  const int row = m_enumOptTable->rowCount();
  m_enumOptTable->insertRow(row);
  m_enumOptTable->setItem(row, 0, new QTableWidgetItem(QString()));
  m_enumOptTable->setItem(row, 1, new QTableWidgetItem(QString()));
  m_enumOptTable->setItem(row, 2, new QTableWidgetItem(QString()));
  auto *typeCombo = AuiComboBox::create(this);
  typeCombo->addItem(QStringLiteral("string"), QStringLiteral("string"));
  typeCombo->addItem(QStringLiteral("number"), QStringLiteral("number"));
  m_enumOptTable->setCellWidget(row, 3, typeCombo);
  m_enumOptTable->setCurrentCell(row, 0);
}

QVector<JsonTableEnumOption> JsonTableDesigner::collectEnumOptions() const {
  QVector<JsonTableEnumOption> out;
  for (int r = 0; r < m_enumOptTable->rowCount(); ++r) {
    JsonTableEnumOption o;
    const QTableWidgetItem *keyIt = m_enumOptTable->item(r, 0);
    const QTableWidgetItem *labelIt = m_enumOptTable->item(r, 1);
    const QTableWidgetItem *valIt = m_enumOptTable->item(r, 2);
    o.key = keyIt ? keyIt->text().trimmed() : QString();
    o.label = labelIt ? labelIt->text().trimmed() : QString();
    o.value = valIt ? valIt->text().trimmed() : QString();
    auto *typeCombo = qobject_cast<QComboBox *>(m_enumOptTable->cellWidget(r, 3));
    o.valueType = typeCombo ? typeCombo->currentData().toString() : QString();
    if (o.value.isEmpty()) continue;  // 跳过未填 value 的行（value 是必填键）
    out.append(o);
  }
  return out;
}

QVector<JsonTableEnum> JsonTableDesigner::collectEnums() const {
  QVector<JsonTableEnum> out = m_enums;
  // 主表编辑内容 → out；当前行选项表格内容 → out（const 下按值修正副本）
  for (int r = 0; r < m_enumTable->rowCount() && r < out.size(); ++r) {
    const QTableWidgetItem *colIt = m_enumTable->item(r, 0);
    const QTableWidgetItem *cmtIt = m_enumTable->item(r, 1);
    if (colIt) out[r].column = colIt->text().trimmed();
    if (cmtIt) out[r].comment = cmtIt->text().trimmed();
  }
  if (m_enumCurRow >= 0 && m_enumCurRow < out.size()) {
    out[m_enumCurRow].options = collectEnumOptions();
  }
  return out;
}

// ════════════════════════════════════════════════════════════
//  DDL 生成 / 预览 / 执行
// ════════════════════════════════════════════════════════════

bool JsonTableDesigner::validateForDdl(QString *err) const {
  if (m_nameEdit->text().trimmed().isEmpty()) {
    if (err) *err = QStringLiteral("表名不能为空");
    return false;
  }
  if (m_colsTable->rowCount() == 0) {
    if (err) *err = QStringLiteral("请先添加字段列");
    return false;
  }
  for (int r = 0; r < m_colsTable->rowCount(); ++r) {
    if (cellText(r, ColColName).isEmpty()) {
      if (err) *err = QStringLiteral("第 %1 行列名不能为空").arg(r + 1);
      return false;
    }
  }
  return true;
}

// ════════════════════════════════════════════════════════════
//  交叉校验 / 列名候选刷新
// ════════════════════════════════════════════════════════════

bool JsonTableDesigner::validateCrossRefs(QString *err) const {
  auto fail = [err](const QString &msg) {
    if (err) *err = msg;
    return false;
  };

  // ── 基础项（与 validateForDdl 同源）──
  const QString tableName = m_nameEdit->text().trimmed();
  if (tableName.isEmpty()) return fail(QStringLiteral("表名不能为空"));
  const QVector<JsonTableColumn> cols = collectColumns();
  if (cols.isEmpty()) return fail(QStringLiteral("请先添加字段列"));

  // 列名集合（重复列名会让 DDL/生成产生歧义）
  QSet<QString> colNames;
  for (const auto &c : cols) {
    if (colNames.contains(c.name)) return fail(QStringLiteral("列名 '%1' 重复").arg(c.name));
    colNames.insert(c.name);
  }

  // 表名与同文件其它表重复（生成期同名表会被跳过并报错）
  for (const auto &t : m_config.tables) {
    if (t.tableName == tableName && t.tableName != m_table.tableName)
      return fail(QStringLiteral("表名 '%1' 与文件中其它表重复").arg(tableName));
  }

  // ── 表内枚举：枚举列须在字段列表中且互不重复 ──
  const QVector<JsonTableEnum> enums = collectEnums();
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
  for (const QString &ge : splitCsv(m_globalEnumEdit->text())) {
    if (!colNames.contains(ge))
      return fail(QStringLiteral("全局枚举列 '%1' 不在字段列表中（列名须与 .jsonglobalenum "
                                 "枚举名一致）")
                      .arg(ge));
    if (enumCols.contains(ge))
      return fail(QStringLiteral("列 '%1' 同时配置了表内枚举与全局枚举（二选一）").arg(ge));
  }

  // ── 联表查询声明（joinTables 新格式）：表/类型/ON 两列/输出列可解析 ──
  const QVector<JsonTableJoinSpec> joinSpecs = collectJoins();
  QSet<QString> joinAliases;  // 输出别名（表名_列名）去重 + 主表列冲突校验
  const QString joinSearchDir = m_filePath.isEmpty() ? QStringLiteral(PROJECT_SOURCE_DIR)
                                                     : QFileInfo(m_filePath).absolutePath();
  const QVector<JsonTableTable> joinProjectTables = listProjectTables(joinSearchDir);
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
  for (const auto &j : m_table.joinTable) {
    if (!j.isJoinRelations || j.joinType != QStringLiteral("ManyToOne")) continue;
    for (const auto &t : m_config.tables) {
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
  JsonTableTable view;
  collectViewTable(&view);
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
  for (const auto &p : collectSort())
    if (!checkRef(QStringLiteral("默认排序列"), p.first)) return false;

  // ── 索引列存在性 ──
  for (const auto &ix : collectIndexes())
    for (const QString &c : ix.cols)
      if (!colNames.contains(c))
        return fail(QStringLiteral("索引 '%1' 的列 '%2' 不在字段列表中").arg(ix.name, c));

  // ── i18n 引用完整性（对齐生成期 main_api 校验）──
  if (m_i18nCheck->isChecked()) {
    const QString i18nTable = m_i18nTableEdit->text().trimmed();
    if (i18nTable.isEmpty()) return fail(QStringLiteral("启用多语言后翻译表不能为空"));
    // 翻译表列集解析顺序：显式配置（本文件 tables[] / 同目录其它 .jsontable）
    // 优先；都没有时按惯例派生（deriveI18nTable 保证 extKey/langKey/fields
    // 列齐备）。单表新格式下本文件没有 tables[]，旧"必须在 tables[] 中"
    // 的检查已不适用
    JsonTableTable trans;
    bool transFound = false;
    for (const auto &t : m_config.tables) {
      if (t.tableName == i18nTable) {
        trans = t;
        transFound = true;
        break;
      }
    }
    if (!transFound) {
      const QString searchDir = m_filePath.isEmpty() ? QStringLiteral(PROJECT_SOURCE_DIR)
                                                     : QFileInfo(m_filePath).absolutePath();
      for (const auto &t : listProjectTables(searchDir)) {
        if (t.tableName == i18nTable) {
          trans = t;
          transFound = true;
          break;
        }
      }
    }
    if (!transFound) trans = deriveI18nTable(collectTable());
    QSet<QString> transCols;
    for (const auto &c : trans.columns) transCols.insert(c.name);
    const QString extKey = m_i18nExtEdit->text().trimmed();
    const QString langKey = m_i18nLangKeyEdit->text().trimmed();
    if (extKey.isEmpty() || !transCols.contains(extKey))
      return fail(QStringLiteral("i18n 外键列 '%1' 不在翻译表 '%2' 的列中")
                      .arg(extKey.isEmpty() ? QStringLiteral("(空)") : extKey, i18nTable));
    if (langKey.isEmpty() || !transCols.contains(langKey))
      return fail(QStringLiteral("i18n 语言列 '%1' 不在翻译表 '%2' 的列中")
                      .arg(langKey.isEmpty() ? QStringLiteral("(空)") : langKey, i18nTable));
    for (const QString &f : splitCsv(m_i18nFieldsEdit->text())) {
      if (!transCols.contains(f))
        return fail(QStringLiteral("多语言字段 '%1' 不在翻译表 '%2' 的列中").arg(f, i18nTable));
      if (colNames.contains(f)) return fail(QStringLiteral("多语言字段 '%1' 与主表列重名").arg(f));
    }
  }
  return true;
}

void JsonTableDesigner::refreshColumnCandidates() {
  // 本表列名（枚举列/全局枚举列/联表外键列/编辑列候选）
  QStringList names;
  for (int r = 0; r < m_colsTable->rowCount(); ++r) {
    const QString n = cellText(r, ColColName);
    if (!n.isEmpty()) names.append(n);
  }
  // 读模型 ref 候选 = 本表列名 + 联表输出别名（joinTables 新格式）
  //                  + 旧格式 ManyToOne 联表的「引用名.目标列」
  QStringList refCands = names;
  for (const auto &j : collectJoins()) {
    for (const auto &c : j.cols) {
      if (!c.col.isEmpty()) refCands.append(j.table + QLatin1Char('_') + c.col);
    }
  }
  for (const auto &j : m_table.joinTable) {
    if (!j.isJoinRelations || j.joinType != QStringLiteral("ManyToOne") ||
        j.joinEnClassPro.isEmpty()) {
      continue;
    }
    for (const auto &t : m_config.tables) {
      if (t.modelName != j.joinEnClass) continue;
      for (const auto &c : t.columns) {
        if (!c.name.isEmpty()) refCands.append(j.joinEnClassPro + QLatin1Char('.') + c.name);
      }
      break;
    }
  }
  if (m_enumColDelegate) static_cast<EditableComboDelegate *>(m_enumColDelegate)->setOptions(names);
  if (m_joinMainColDelegate)
    static_cast<EditableComboDelegate *>(m_joinMainColDelegate)->setOptions(names);
  if (m_viewRefDelegate)
    static_cast<EditableComboDelegate *>(m_viewRefDelegate)->setOptions(refCands);
  if (m_sortRefDelegate)
    static_cast<EditableComboDelegate *>(m_sortRefDelegate)->setOptions(refCands);
  if (m_globalEnumCompleter)
    static_cast<CsvCompleter *>(m_globalEnumCompleter)->setCandidates(names);
}

void JsonTableDesigner::refreshI18nFieldCandidates() {
  QStringList names;
  const QString trans = m_i18nTableEdit->text().trimmed();
  if (!trans.isEmpty()) {
    for (const auto &t : m_config.tables) {
      if (t.tableName != trans) continue;
      for (const auto &c : t.columns) names.append(c.name);
      break;
    }
  }
  if (m_i18nFieldsCompleter)
    static_cast<CsvCompleter *>(m_i18nFieldsCompleter)->setCandidates(names);
}

/// CREATE TABLE DDL 通用构造（列 + 主键 + 索引，InnoDB/utf8mb4）；
/// toCreateDdl（主表）与 i18nCreateDdl（派生翻译表）共用
static QString createDdlText(const QString &tableName, const QString &tableComment,
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
    if (!c.defaultValue.isEmpty()) parts << QStringLiteral("DEFAULT %1").arg(quote(c.defaultValue));
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

QString JsonTableDesigner::toCreateDdl() const {
  return createDdlText(m_nameEdit->text().trimmed(), m_commentEdit->text().trimmed(),
                       collectColumns(), collectIndexes());
}

QString JsonTableDesigner::i18nCreateDdl() const {
  const JsonTableTable main = collectTable();
  if (!main.i18n.isEnabled()) return {};
  // 同目录（含子目录）已有显式翻译表 .jsontable 时不派生（显式配置优先）
  const QString searchDir = m_filePath.isEmpty() ? QStringLiteral(PROJECT_SOURCE_DIR)
                                                 : QFileInfo(m_filePath).absolutePath();
  for (const auto &t : listProjectTables(searchDir)) {
    if (t.tableName == main.i18n.table) return {};
  }
  const JsonTableTable derived = deriveI18nTable(main);
  return createDdlText(derived.tableName, derived.tableComment, derived.columns, derived.indexes);
}

void JsonTableDesigner::previewDdl() {
  QString err;
  if (!validateForDdl(&err)) {
    AuiMessageBox::show(this, QStringLiteral("无法生成 DDL"), err);
    return;
  }
  QString sql = toCreateDdl();
  const QString i18nSql = i18nCreateDdl();
  if (!i18nSql.isEmpty()) sql += QStringLiteral("\n\n") + i18nSql;
  m_ddlView->setPlainText(sql);
}

void JsonTableDesigner::importFromDatabase() {
  const QString table = m_nameEdit->text().trimmed();
  if (table.isEmpty()) {
    AuiMessageBox::show(this, QStringLiteral("无法刷新"), QStringLiteral("请先填写表名"));
    return;
  }

  accore::AcJsonValue inst;
  if (!connectDb(m_config, &inst, this)) return;

  // tableSchema / tableInfo 共用 {table} 参数
  accore::AcJsonValue args = accore::AcJsonValue::makeArray();
  accore::AcJsonValue params = accore::AcJsonValue::makeObject();
  params.set(QString::fromLatin1(AcDB::kTable), accore::AcJsonValue(table));
  args.append(params);

  const accore::AcJsonValue schema = FunDb::tableSchema(inst, args);
  if (!schema.isArray()) {
    FunDb::destructor(inst, accore::AcJsonValue::makeArray());
    showFunDbError(this, QStringLiteral("刷新失败"),
                   QStringLiteral("读取表结构失败（表可能不存在）"));
    return;
  }

  // 数据库列 → JsonTableColumn（纯物理属性）
  QVector<JsonTableColumn> dbColumns;
  for (const auto &v : schema.items()) {
    JsonTableColumn col;
    col.name = v.value(QString::fromLatin1(AcDB::kColName)).toString();
    // COLUMN_TYPE 形如 "bigint unsigned"：拆出 unsigned 标记后存纯类型
    QString type = v.value(QString::fromLatin1(AcDB::kColType)).toString().trimmed();
    if (type.endsWith(QStringLiteral("unsigned"), Qt::CaseInsensitive)) {
      col.isUnsigned = true;
      type.chop(QStringLiteral("unsigned").size());
      type = type.trimmed();
    }
    col.mysqlType = type;
    col.nullable = v.value(QString::fromLatin1(AcDB::kColNullable)).toBool(false);
    col.isPrimary = v.value(QString::fromLatin1(AcDB::kColKey)).toString() == QStringLiteral("PRI");
    col.isAutoInc = v.value(QString::fromLatin1(AcDB::kColExtra))
                        .toString()
                        .contains(QStringLiteral("auto_increment"));
    // default 兼容数字字面量（Qt6 的 toString 不转换数值，需显式处理）
    const accore::AcJsonValue dv = v.value(QString::fromLatin1(AcDB::kColDefault));
    if (dv.isString()) {
      col.defaultValue = dv.toString();
    } else if (dv.isDouble()) {
      col.defaultValue = QString::number(dv.toDouble());
    }
    col.comment = v.value(QString::fromLatin1(AcDB::kColComment)).toString();
    dbColumns.append(col);
  }

  // 表注释
  QString dbComment;
  const accore::AcJsonValue info = FunDb::tableInfo(inst, args);
  if (info.isObject()) {
    dbComment = info.value(QString::fromLatin1(AcDB::kTblComment)).toString();
  }

  // 索引（INFORMATION_SCHEMA.STATISTICS；PRIMARY 由主键列维护，跳过）
  QVector<JsonTableIndex> dbIndexes;
  bool indexQueryOk = false;
  {
    accore::AcJsonValue idxArgs = accore::AcJsonValue::makeArray();
    accore::AcJsonValue idxParams = accore::AcJsonValue::makeObject();
    idxParams.set(QString::fromLatin1(AcDB::kSql),
                  accore::AcJsonValue(
                      QStringLiteral("SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME "
                                     "FROM INFORMATION_SCHEMA.STATISTICS "
                                     "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = %1 "
                                     "ORDER BY INDEX_NAME, SEQ_IN_INDEX")
                          .arg(quote(table))));
    idxArgs.append(idxParams);
    const accore::AcJsonValue idxRows = FunDb::query(inst, idxArgs);
    indexQueryOk = idxRows.isArray();
    if (indexQueryOk) {
      QHash<QString, int> indexPositions;
      for (const auto &row : idxRows.items()) {
        const QString name = row.value(QStringLiteral("INDEX_NAME")).toString();
        if (name.isEmpty() || name.compare(QStringLiteral("PRIMARY"), Qt::CaseInsensitive) == 0) {
          continue;
        }
        const QString column = row.value(QStringLiteral("COLUMN_NAME")).toString();
        if (column.isEmpty()) continue;  // 函数索引等无列名条目
        const QString key = name.toLower();
        int position = indexPositions.value(key, -1);
        if (position < 0) {
          position = dbIndexes.size();
          indexPositions.insert(key, position);
          JsonTableIndex index;
          index.name = name;
          index.unique = row.value(QStringLiteral("NON_UNIQUE")).toString() == QStringLiteral("0");
          dbIndexes.append(index);
        }
        dbIndexes[position].cols << column;
      }
    }
  }
  FunDb::destructor(inst, accore::AcJsonValue::makeArray());

  // 差异计算：当前配置 vs 数据库（排除列跳过，但会在预览中标记）
  const QVector<JsonTableColumn> currentColumns = collectColumns();
  const QVector<JsonTableIndex> currentIndexes = collectIndexes();
  const QString currentComment = m_commentEdit->text().trimmed();
  const QStringList initialExcluded = splitCsv(m_excludeEdit->text());
  const QVector<ImportDiffEntry> entries =
      buildImportDiffEntries(currentColumns, dbColumns, initialExcluded);
  const QVector<IndexDiffEntry> indexEntries =
      indexQueryOk ? buildIndexDiffEntries(currentIndexes, dbIndexes) : QVector<IndexDiffEntry>();
  const bool commentDiffers = !dbComment.isEmpty() && dbComment != currentComment;
  if (entries.isEmpty() && indexEntries.isEmpty() && !commentDiffers) {
    AuiMessageBox::show(this, QStringLiteral("刷新结构"),
                        QStringLiteral("结构与数据库一致，无需刷新。"));
    return;
  }

  const ImportDiffResult result = runImportDiffDialog(this, table, entries, indexEntries,
                                                      currentComment, dbComment, initialExcluded);
  if (!result.accepted) return;

  // 应用选择：现有列保序；新增列按数据库顺序追加；删除列仅在勾选时移除
  QHash<QString, JsonTableColumn> dbByKey;
  for (const auto &col : dbColumns) {
    dbByKey.insert(col.name.toLower(), col);
  }
  QVector<JsonTableColumn> nextColumns;
  for (const auto &cur : currentColumns) {
    const QString key = cur.name.toLower();
    if (dbByKey.contains(key)) {
      if (result.applyByKey.value(key, false)) {
        // 物理属性取数据库值，列级取值域等设计器配置按原列保留
        JsonTableColumn updated = dbByKey.value(key);
        updated.domainType = cur.domainType;
        updated.domainSourceFile = cur.domainSourceFile;
        updated.domainSourceId = cur.domainSourceId;
        updated.domainUrl = cur.domainUrl;
        nextColumns.append(updated);
      } else {
        nextColumns.append(cur);
      }
    } else if (!result.applyByKey.value(key, false)) {
      nextColumns.append(cur);  // 删除列默认保留
    }
  }
  QSet<QString> keptKeys;
  for (const auto &col : nextColumns) {
    keptKeys.insert(col.name.toLower());
  }
  for (const auto &db : dbColumns) {
    const QString key = db.name.toLower();
    if (keptKeys.contains(key)) continue;
    if (result.applyByKey.value(key, false)) nextColumns.append(db);
  }

  // 索引应用（索引查询失败时保持原索引，不做任何删除）
  QVector<JsonTableIndex> nextIndexes = currentIndexes;
  if (indexQueryOk) {
    QHash<QString, JsonTableIndex> dbIndexByKey;
    for (const auto &index : dbIndexes) {
      dbIndexByKey.insert(index.name.toLower(), index);
    }
    nextIndexes.clear();
    for (const auto &cur : currentIndexes) {
      const QString key = cur.name.toLower();
      if (dbIndexByKey.contains(key)) {
        nextIndexes.append(result.indexApplyByKey.value(key, false) ? dbIndexByKey.value(key)
                                                                    : cur);
      } else if (!result.indexApplyByKey.value(key, false)) {
        nextIndexes.append(cur);  // 删除索引默认保留
      }
    }
    QSet<QString> keptIndexKeys;
    for (const auto &index : nextIndexes) {
      keptIndexKeys.insert(index.name.toLower());
    }
    for (const auto &index : dbIndexes) {
      const QString key = index.name.toLower();
      if (keptIndexKeys.contains(key)) continue;
      if (result.indexApplyByKey.value(key, false)) nextIndexes.append(index);
    }
  }

  int appliedAdded = 0;
  int appliedModified = 0;
  int appliedRemoved = 0;
  QStringList removedColumns;
  for (const auto &e : entries) {
    if (!result.applyByKey.value(e.name.toLower(), false)) continue;
    switch (e.kind) {
      case ImportDiffKind::Added:
        ++appliedAdded;
        break;
      case ImportDiffKind::Modified:
        ++appliedModified;
        break;
      case ImportDiffKind::Removed:
        ++appliedRemoved;
        removedColumns << e.name;
        break;
      default:
        break;
    }
  }
  int appliedIndexAdded = 0;
  int appliedIndexModified = 0;
  int appliedIndexRemoved = 0;
  for (const auto &e : indexEntries) {
    if (!result.indexApplyByKey.value(e.name.toLower(), false)) continue;
    switch (e.kind) {
      case ImportDiffKind::Added:
        ++appliedIndexAdded;
        break;
      case ImportDiffKind::Modified:
        ++appliedIndexModified;
        break;
      case ImportDiffKind::Removed:
        ++appliedIndexRemoved;
        break;
      default:
        break;
    }
  }

  // 模型回填：列/索引/注释/排除列 + 删除列的前端引用清理，最后 load 统一刷新界面
  JsonTableTable updated = collectTable();
  updated.columns = nextColumns;
  if (indexQueryOk) updated.indexes = nextIndexes;
  if (result.applyTableComment && !dbComment.isEmpty()) updated.tableComment = dbComment;
  setExcludedColumns(&updated, result.excludedColumns);
  const RefCleanupStats cleanup = cleanRemovedColumnRefs(&updated, removedColumns);

  load(updated, m_config, m_filePath);
  emit configChanged();
  updateValidateStatus();

  QString msg = QStringLiteral("已应用 %1 项列变更（新增 %2 / 修改 %3 / 删除 %4）。")
                    .arg(appliedAdded + appliedModified + appliedRemoved)
                    .arg(appliedAdded)
                    .arg(appliedModified)
                    .arg(appliedRemoved);
  if (indexQueryOk) {
    msg += QStringLiteral("\n索引：新增 %1 / 修改 %2 / 删除 %3。")
               .arg(appliedIndexAdded)
               .arg(appliedIndexModified)
               .arg(appliedIndexRemoved);
  } else {
    msg += QStringLiteral("\n索引结构读取失败，本次未同步索引。");
  }
  if (cleanup.total() > 0) {
    msg += QStringLiteral("\n已清理前端引用 %1 处（%2）。")
               .arg(cleanup.total())
               .arg(cleanup.summary());
  }
  if (!result.excludedColumns.isEmpty()) {
    msg += QStringLiteral("\n排除列：%1").arg(result.excludedColumns.join(QStringLiteral(", ")));
  }
  AuiMessageBox::show(this, QStringLiteral("刷新完成"), msg);
}

void JsonTableDesigner::executeCreateTable() {
  QString err;
  if (!validateForDdl(&err)) {
    AuiMessageBox::show(this, QStringLiteral("无法执行建表"), err);
    return;
  }
  // 主表 + 派生翻译表（启用 i18n 且同目录无显式翻译表配置时附带建表）
  QStringList sqls;
  sqls << toCreateDdl();
  const QString i18nSql = i18nCreateDdl();
  if (!i18nSql.isEmpty()) sqls << i18nSql;
  m_ddlView->setPlainText(sqls.join(QStringLiteral("\n\n")));

  if (!AuiMessageBox::confirm(this, QStringLiteral("执行建表"),
                              QStringLiteral("将在数据库「%1」执行 CREATE TABLE（%2 张表）。\n"
                                             "执行前请确认 DDL 预览内容，操作不会自动回滚。")
                                  .arg(m_config.dbDatabase)
                                  .arg(sqls.size())))
    return;

  accore::AcJsonValue inst;
  if (!connectDb(m_config, &inst, this)) return;

  QString doneTables;
  for (const QString &sql : sqls) {
    accore::AcJsonValue args = accore::AcJsonValue::makeArray();
    accore::AcJsonValue params = accore::AcJsonValue::makeObject();
    params.set(QString::fromLatin1(AcDB::kSql), accore::AcJsonValue(sql));
    args.append(params);
    const accore::AcJsonValue r = FunDb::exec(inst, args);
    if (!r.isDouble()) {
      FunDb::destructor(inst, accore::AcJsonValue::makeArray());
      showFunDbError(this, QStringLiteral("建表失败"),
                     QStringLiteral("执行 DDL 失败：\n%1").arg(sql));
      return;
    }
    // 从 DDL 提取表名拼成功消息（CREATE TABLE `xxx`）
    const int bt = sql.indexOf(QLatin1Char('`'));
    const int et = sql.indexOf(QLatin1Char('`'), bt + 1);
    if (bt >= 0 && et > bt)
      doneTables +=
          (doneTables.isEmpty() ? QString() : QStringLiteral("、")) + sql.mid(bt + 1, et - bt - 1);
  }
  FunDb::destructor(inst, accore::AcJsonValue::makeArray());
  AuiMessageBox::show(this, QStringLiteral("建表成功"),
                      QStringLiteral("表 %1 创建成功").arg(doneTables));
}

// ════════════════════════════════════════════════════════════
//  结果收集
// ════════════════════════════════════════════════════════════

JsonTableTable JsonTableDesigner::collectTable() const {
  JsonTableTable t = m_table;  // extra 未知键保真（joinTable 等未结构化节点原样写回）
  t.tableName = m_nameEdit->text().trimmed();
  t.modelName = m_modelEdit->text().trimmed();
  t.tableComment = m_commentEdit->text().trimmed();
  t.manualUpdateTime = m_manualUpdateCheck->isChecked();
  t.frontend = m_frontendCheck->isChecked();
  t.genAdd = m_genAddCheck->isChecked();
  t.genEdit = m_genEditCheck->isChecked();
  t.genDelete = m_genDeleteCheck->isChecked();
  t.genDetail = m_genDetailCheck->isChecked();
  t.appQuery = m_appQueryCheck->isChecked();
  t.appAuth = m_appAuthCombo->currentData().toString();
  if (t.appAuth.isEmpty()) t.appAuth = QStringLiteral("user");
  t.buttons = m_buttons;
  t.columns = collectColumns();
  t.indexes = collectIndexes();
  // ── 表级进阶键 ──
  t.joinTable = m_table.joinTable;  // 旧格式联表不再编辑，原样保真（避免旧文件保存丢失）
  t.joinTables = collectJoins();
  collectViewTable(&t);  // 统一字段表格 → 三视图（selCols/eq/like/gap/editCols）
  t.selColsSort = collectSort();
  t.enums = collectEnums();
  t.globalEnumCols = splitCsv(m_globalEnumEdit->text());
  if (m_i18nCheck->isChecked()) {
    t.i18n.table = m_i18nTableEdit->text().trimmed();
    t.i18n.extKey = m_i18nExtEdit->text().trimmed();
    t.i18n.langKey = m_i18nLangKeyEdit->text().trimmed();
    t.i18n.fields = splitCsv(m_i18nFieldsEdit->text());
    t.i18n.defaultLang = m_i18nDefaultLangEdit->text().trimmed();
    if (t.i18n.defaultLang.isEmpty()) t.i18n.defaultLang = QStringLiteral("zh");
    t.i18n.langFrom = m_i18nLangFromCombo->currentData().toString();
    if (t.i18n.langFrom.isEmpty()) t.i18n.langFrom = QStringLiteral("body");
  } else {
    t.i18n = JsonTableI18n();  // 未启用：清空（table 空 → toJson 不落键）
  }
  // 排除列：设计器专属键，写入 extra 保真（生成期不消费）
  setExcludedColumns(&t, splitCsv(m_excludeEdit->text()));
  return t;
}
