#include "table_import_dialog.h"

#include <QAbstractItemView>
#include <QAction>
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
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidget>

#include "src/util/ui/component/aui_config_dialog.h"
#include "src/util/ui/component/aui_style.h"
using namespace TableSchema;
namespace {
QString cellOf(const QTableWidget *table, int row, int col) {
  const auto *item = table ? table->item(row, col) : nullptr;
  return item ? item->text().trimmed() : QString();
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
}  // namespace
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
    QTableWidgetItem *apply = makeConfigCheckItem(defaultApply);
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
      exclude = makeConfigCheckItem(e.excluded);
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
      indexTable->setItem(row, IdxApply, makeConfigCheckItem(true));
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
