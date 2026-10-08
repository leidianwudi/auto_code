/**
 * @file json_table_dialog.cpp
 * @brief .jsontable 表设计器对话框实现
 */

#include "json_table_dialog.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "src/engine/ac_language.h"
#include "src/engine/function/fun_db.h"
#include "src/engine/function/fun_mgr.h"
#include "src/ui/json_vue/config_dialog_common.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/component/aui_message_box.h"


// ════════════════════════════════════════════════════════════
//  表格列索引 / 局部工具
// ════════════════════════════════════════════════════════════

namespace {

/// 字段表格列索引
enum ColCols {
  ColColName = 0,  ///< 列名
  ColColType,      ///< MySQL 类型（可编辑下拉）
  ColColUnsigned,  ///< unsigned 复选框
  ColColNullable,  ///< 可空 复选框
  ColColPrimary,   ///< 主键 复选框
  ColColAutoInc,   ///< 自增 复选框
  ColColDefault,   ///< 默认值
  ColColComment,   ///< 注释
  ColColList,      ///< 列表 复选框
  ColColSearch,    ///< 搜索 复选框
  ColColForm,      ///< 表单角色（可编辑下拉）
  ColColRequired,  ///< 必填 复选框
  ColColCount
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

/// 复选框列条目（无可编辑文本，点击勾选框直接切换）
QTableWidgetItem *makeCheckItem(bool on) {
  auto *it = new QTableWidgetItem;
  it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
  it->setCheckState(on ? Qt::Checked : Qt::Unchecked);
  it->setTextAlignment(Qt::AlignCenter);
  return it;
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

}  // namespace

// ════════════════════════════════════════════════════════════
//  构造 / 界面构建
// ════════════════════════════════════════════════════════════

JsonTableDialog::JsonTableDialog(const JsonTableTable &table, const JsonTableConfig &config,
                             QWidget *parent)
    : QDialog(parent), m_table(table), m_config(config) {
  setupUI();
}

void JsonTableDialog::setupUI() {
  QString title = QStringLiteral("表设计器");
  if (!m_table.tableName.isEmpty()) title += QStringLiteral("：%1").arg(m_table.tableName);
  ConfigDialogFrame frame = beginConfigDialog(this, title, QMargins(12, 10, 12, 10), 6);
  auto *layout = frame.contentLayout;

  // ── 顶部：表名 / 模型名 / 表注释 / manualUpdateTime ──
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
  layout->addLayout(topRow);

  // ── 字段表格工具行 ──
  auto *colHeader = new QHBoxLayout;
  colHeader->addWidget(new QLabel(QStringLiteral("字段:"), this));
  colHeader->addStretch();
  auto *addColBtn = makeCompactButton(QStringLiteral("+ 加列"), this);
  auto *delColBtn = makeCompactButton(QStringLiteral("- 删列"), this);
  auto *upColBtn = makeCompactButton(QStringLiteral("↑ 上移"), this);
  auto *downColBtn = makeCompactButton(QStringLiteral("↓ 下移"), this);
  colHeader->addWidget(addColBtn);
  colHeader->addWidget(delColBtn);
  colHeader->addWidget(upColBtn);
  colHeader->addWidget(downColBtn);
  layout->addLayout(colHeader);

  // ── 字段表格（核心编辑区）──
  m_colsTable = makeConfigTable({{QStringLiteral("列名"), QHeaderView::Stretch, 0},
                                 {QStringLiteral("MySQL类型"), QHeaderView::Interactive, 120},
                                 {QStringLiteral("unsigned"), QHeaderView::Interactive, 62},
                                 {QStringLiteral("可空"), QHeaderView::Interactive, 50},
                                 {QStringLiteral("主键"), QHeaderView::Interactive, 50},
                                 {QStringLiteral("自增"), QHeaderView::Interactive, 50},
                                 {QStringLiteral("默认值"), QHeaderView::Interactive, 90},
                                 {QStringLiteral("注释"), QHeaderView::Stretch, 0},
                                 {QStringLiteral("列表"), QHeaderView::Interactive, 50},
                                 {QStringLiteral("搜索"), QHeaderView::Interactive, 50},
                                 {QStringLiteral("表单角色"), QHeaderView::Interactive, 100},
                                 {QStringLiteral("必填"), QHeaderView::Interactive, 50}},
                                this, 180, 0, QAbstractItemView::SelectItems);
  layout->addWidget(m_colsTable, 1);

  // MySQL 类型列 / 表单角色列：可编辑下拉委托（预设 + 自由输入）
  m_colsTable->setItemDelegateForColumn(
      ColColType,
      new EditableComboDelegate(
          {QStringLiteral("bigint"), QStringLiteral("int"), QStringLiteral("tinyint"),
           QStringLiteral("varchar(64)"), QStringLiteral("text"), QStringLiteral("decimal(10,2)"),
           QStringLiteral("datetime"), QStringLiteral("json"), QStringLiteral("boolean")},
          m_colsTable));
  m_colsTable->setItemDelegateForColumn(
      ColColForm,
      new EditableComboDelegate(
          {QStringLiteral("none"), QStringLiteral("input"), QStringLiteral("textarea"),
           QStringLiteral("select"), QStringLiteral("date"), QStringLiteral("number"),
           QStringLiteral("image"), QStringLiteral("upload"), QStringLiteral("boolean")},
          m_colsTable));

  // ── 索引表格工具行 ──
  auto *idxHeader = new QHBoxLayout;
  idxHeader->addWidget(new QLabel(QStringLiteral("索引:"), this));
  idxHeader->addStretch();
  auto *addIdxBtn = makeCompactButton(QStringLiteral("+ 加索引"), this);
  auto *delIdxBtn = makeCompactButton(QStringLiteral("- 删索引"), this);
  idxHeader->addWidget(addIdxBtn);
  idxHeader->addWidget(delIdxBtn);
  layout->addLayout(idxHeader);

  m_indexTable = makeConfigTable({{QStringLiteral("索引名"), QHeaderView::Stretch, 0},
                                  {QStringLiteral("列(逗号分隔)"), QHeaderView::Stretch, 0},
                                  {QStringLiteral("unique"), QHeaderView::Interactive, 70}},
                                 this, 64, 110, QAbstractItemView::SelectItems);
  layout->addWidget(m_indexTable);

  // ── DDL 预览 + 执行区 ──
  auto *ddlHeader = new QHBoxLayout;
  ddlHeader->addWidget(new QLabel(QStringLiteral("DDL:"), this));
  ddlHeader->addStretch();
  m_importBtn = makeCompactButton(QStringLiteral("从数据库导入"), this);
  auto *previewBtn = makeCompactButton(QStringLiteral("DDL 预览"), this);
  auto *execBtn = makeCompactButton(QStringLiteral("执行建表"), this);
  ddlHeader->addWidget(m_importBtn);
  ddlHeader->addWidget(previewBtn);
  ddlHeader->addWidget(execBtn);
  layout->addLayout(ddlHeader);

  m_ddlView = new QPlainTextEdit(this);
  m_ddlView->setReadOnly(true);
  m_ddlView->setPlaceholderText(QStringLiteral("点击「DDL 预览」生成 CREATE TABLE 语句"));
  m_ddlView->setMinimumHeight(84);
  m_ddlView->setMaximumHeight(130);
  layout->addWidget(m_ddlView);

  // meta.db 四件套齐全才允许从数据库导入
  const bool dbReady = !m_config.dbHost.isEmpty() && !m_config.dbUser.isEmpty() &&
                       !m_config.dbPassword.isEmpty() && !m_config.dbDatabase.isEmpty();
  m_importBtn->setEnabled(dbReady);
  if (!dbReady)
    m_importBtn->setToolTip(
        QStringLiteral("meta.db 未配置完整（host/user/password/database），无法从数据库导入"));

  // ── 连接 ──
  connect(addColBtn, &QPushButton::clicked, this, [this]() { addColumn(); });
  connect(delColBtn, &QPushButton::clicked, this, [this]() { deleteColumn(); });
  connect(upColBtn, &QPushButton::clicked, this, [this]() { moveColumn(-1); });
  connect(downColBtn, &QPushButton::clicked, this, [this]() { moveColumn(1); });
  connect(addIdxBtn, &QPushButton::clicked, this, [this]() { appendIndexRow(); });
  connect(delIdxBtn, &QPushButton::clicked, this, [this]() {
    const int row = m_indexTable->currentRow();
    if (row >= 0) m_indexTable->removeRow(row);
  });
  connect(m_importBtn, &QPushButton::clicked, this, &JsonTableDialog::importFromDatabase);
  connect(previewBtn, &QPushButton::clicked, this, &JsonTableDialog::previewDdl);
  connect(execBtn, &QPushButton::clicked, this, &JsonTableDialog::executeCreateTable);

  finishConfigDialog(this, frame);
  setMinimumSize(880, 720);
  resize(960, 780);

  // ── 预填 ──
  m_nameEdit->setText(m_table.tableName);
  m_modelEdit->setText(m_table.modelName);
  m_commentEdit->setText(m_table.tableComment);
  m_manualUpdateCheck->setChecked(m_table.manualUpdateTime);
  fillColumns(m_table.columns);
  fillIndexes(m_table.indexes);
}

// ════════════════════════════════════════════════════════════
//  字段表格填充 / 收集 / 行操作
// ════════════════════════════════════════════════════════════

void JsonTableDialog::fillColumns(const QVector<JsonTableColumn> &cols) {
  m_colsTable->setRowCount(0);
  for (const auto &c : cols) appendColumnRow(c);
}

void JsonTableDialog::appendColumnRow(const JsonTableColumn &c) {
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
  m_colsTable->setItem(row, ColColList, makeCheckItem(c.list));
  m_colsTable->setItem(row, ColColSearch, makeCheckItem(c.search));
  m_colsTable->setItem(row, ColColForm, new QTableWidgetItem(c.form));
  m_colsTable->setItem(row, ColColRequired, makeCheckItem(c.required));
}

QString JsonTableDialog::cellText(int row, int col) const {
  const QTableWidgetItem *it = m_colsTable->item(row, col);
  return it ? it->text().trimmed() : QString();
}

bool JsonTableDialog::cellChecked(int row, int col) const {
  const QTableWidgetItem *it = m_colsTable->item(row, col);
  return it && it->checkState() == Qt::Checked;
}

JsonTableColumn JsonTableDialog::columnFromRow(int row) const {
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
  c.list = cellChecked(row, ColColList);
  c.search = cellChecked(row, ColColSearch);
  c.form = cellText(row, ColColForm);
  if (c.form.isEmpty()) c.form = QStringLiteral("input");
  c.required = cellChecked(row, ColColRequired);
  return c;
}

QVector<JsonTableColumn> JsonTableDialog::collectColumns() const {
  QVector<JsonTableColumn> out;
  for (int r = 0; r < m_colsTable->rowCount(); ++r) {
    JsonTableColumn c = columnFromRow(r);
    if (c.name.isEmpty()) continue;  // 跳过未填列名的行
    out.append(c);
  }
  return out;
}

void JsonTableDialog::addColumn() {
  appendColumnRow(JsonTableColumn());  // 模型缺省：bigint / list=true / form=input
  const int row = m_colsTable->rowCount() - 1;
  m_colsTable->setCurrentCell(row, ColColName);
  m_colsTable->editItem(m_colsTable->item(row, ColColName));  // 直接进入列名编辑
}

void JsonTableDialog::deleteColumn() {
  const int row = m_colsTable->currentRow();
  if (row >= 0) m_colsTable->removeRow(row);
}

void JsonTableDialog::moveColumn(int delta) {
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
}

// ════════════════════════════════════════════════════════════
//  索引表格
// ════════════════════════════════════════════════════════════

void JsonTableDialog::fillIndexes(const QVector<JsonTableIndex> &indexes) {
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

void JsonTableDialog::appendIndexRow() {
  const int row = m_indexTable->rowCount();
  m_indexTable->insertRow(row);
  m_indexTable->setItem(row, IndexColName, new QTableWidgetItem(QString()));
  m_indexTable->setItem(row, IndexColCols, new QTableWidgetItem(QString()));
  m_indexTable->setItem(row, IndexColUnique, makeCheckItem(false));
  m_indexTable->setCurrentCell(row, IndexColName);
}

QVector<JsonTableIndex> JsonTableDialog::collectIndexes() const {
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
//  DDL 生成 / 预览 / 执行
// ════════════════════════════════════════════════════════════

bool JsonTableDialog::validateForDdl(QString *err) const {
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

QString JsonTableDialog::toCreateDdl() const {
  const QString tableName = m_nameEdit->text().trimmed();
  const QVector<JsonTableColumn> cols = collectColumns();
  const QVector<JsonTableIndex> indexes = collectIndexes();

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
  const QString tableComment = m_commentEdit->text().trimmed();
  if (!tableComment.isEmpty()) sql += QStringLiteral(" COMMENT=%1").arg(quote(tableComment));
  sql += QLatin1Char(';');
  return sql;
}

void JsonTableDialog::previewDdl() {
  QString err;
  if (!validateForDdl(&err)) {
    AuiMessageBox::show(this, QStringLiteral("无法生成 DDL"), err);
    return;
  }
  m_ddlView->setPlainText(toCreateDdl());
}

void JsonTableDialog::importFromDatabase() {
  const QString table = m_nameEdit->text().trimmed();
  if (table.isEmpty()) {
    AuiMessageBox::show(this, QStringLiteral("无法导入"), QStringLiteral("请先填写表名"));
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
    showFunDbError(this, QStringLiteral("导入失败"),
                   QStringLiteral("读取表结构失败（表可能不存在）"));
    return;
  }

  // 合并模式：以 DB 列为基础，前端角色（list/search/form/required）按列名回填
  // .jsontable 已有值（取当前界面状态，未改过即 .jsontable 原值）
  QHash<QString, JsonTableColumn> oldCols;
  for (const auto &c : collectColumns()) oldCols.insert(c.name, c);

  QVector<JsonTableColumn> cols;
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

    const auto it = oldCols.constFind(col.name);
    if (it != oldCols.constEnd()) {
      col.list = it->list;
      col.search = it->search;
      col.form = it->form;
      col.required = it->required;
    }
    cols.append(col);
  }

  // 表注释回填
  const accore::AcJsonValue info = FunDb::tableInfo(inst, args);
  if (info.isObject()) {
    const QString cmt = info.value(QString::fromLatin1(AcDB::kTblComment)).toString();
    if (!cmt.isEmpty()) m_commentEdit->setText(cmt);
  }

  FunDb::destructor(inst, accore::AcJsonValue::makeArray());
  fillColumns(cols);
  AuiMessageBox::show(
      this, QStringLiteral("导入完成"),
      QStringLiteral("已从数据库导入 %1 列（前端角色按列名合并保留）").arg(cols.size()));
}

void JsonTableDialog::executeCreateTable() {
  QString err;
  if (!validateForDdl(&err)) {
    AuiMessageBox::show(this, QStringLiteral("无法执行建表"), err);
    return;
  }
  const QString sql = toCreateDdl();
  m_ddlView->setPlainText(sql);

  if (!AuiMessageBox::confirm(this, QStringLiteral("执行建表"),
                              QStringLiteral("将在数据库「%1」执行 CREATE TABLE（%2 列）。\n"
                                             "执行前请确认 DDL 预览内容，操作不会自动回滚。")
                                  .arg(m_config.dbDatabase, m_ddlView->toPlainText())))
    return;

  accore::AcJsonValue inst;
  if (!connectDb(m_config, &inst, this)) return;

  accore::AcJsonValue args = accore::AcJsonValue::makeArray();
  accore::AcJsonValue params = accore::AcJsonValue::makeObject();
  params.set(QString::fromLatin1(AcDB::kSql), accore::AcJsonValue(sql));
  args.append(params);
  const accore::AcJsonValue r = FunDb::exec(inst, args);
  FunDb::destructor(inst, accore::AcJsonValue::makeArray());

  if (!r.isDouble()) {
    showFunDbError(this, QStringLiteral("建表失败"), QStringLiteral("执行 DDL 失败"));
    return;
  }
  AuiMessageBox::show(this, QStringLiteral("建表成功"),
                      QStringLiteral("表 %1 创建成功").arg(tick(m_nameEdit->text().trimmed())));
}

// ════════════════════════════════════════════════════════════
//  结果收集
// ════════════════════════════════════════════════════════════

JsonTableTable JsonTableDialog::resultTable() const {
  JsonTableTable t = m_table;  // extra 未知键保真（i18n/globalEnumCols/joinTable 等原样写回）
  t.tableName = m_nameEdit->text().trimmed();
  t.modelName = m_modelEdit->text().trimmed();
  t.tableComment = m_commentEdit->text().trimmed();
  t.manualUpdateTime = m_manualUpdateCheck->isChecked();
  t.columns = collectColumns();
  t.indexes = collectIndexes();
  return t;
}
