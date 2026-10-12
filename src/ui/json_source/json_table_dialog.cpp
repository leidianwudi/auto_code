/**
 * @file json_table_dialog.cpp
 * @brief .jsontable 表设计器对话框实现
 */

#include "json_table_dialog.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCompleter>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QStringListModel>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

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
  JoinColPro = 0,    ///< 联查引用名（joinEnClassPro：别名前缀 + ref 前缀）
  JoinColClass,      ///< join 目标（joinEnClass：目标表 modelName，可编辑下拉）
  JoinColFk,         ///< 本表外键列（joinFk，可编辑下拉）
  JoinColPk,         ///< 目标主键列（joinPk，缺省 id）
  JoinColType,       ///< joinType（ManyToOne/OneToMany 下拉）
  JoinColActive,     ///< isJoinRelations 复选框（true=有效声明）
  JoinColCount
};

/// 字段呈现统一表格列索引（一行 = 一个字段：列表/查询/编辑三视图合一）
enum UCols {
  UColRef = 0,     ///< 字段引用（本表列名 或 引用名.目标列，可编辑下拉）
  UColTitle,       ///< 标题（生效值 = selCols.label > 列注释 > ref；写回 selCols.label）
  UColList,        ///< 列表勾选（在 selCols 中）
  UColQuery,       ///< 查询模式（cell widget：不查询/精确/模糊/范围/仅接口）
  UColEdit,        ///< 编辑三态勾（☑渲染/■进表单不渲染/☐不进表单）
  UColRequired,    ///< 必填勾选（editCols.required）
  UColGear,        ///< ⚙ 按钮列（列宽/控件类型/编辑标题 → 字段设置面板）
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

  // ── 页签容器（对齐 jsonvue「一行=一字段」心智）：
  //    字段(物理) / 字段呈现(三视图合一+排序) / 联表与索引 / 枚举 / 多语言 / DDL ──
  auto *tabs = new QTabWidget(this);
  auto makeTab = [tabs](const QString &title) {
    auto *w = new QWidget(tabs);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(4, 8, 4, 4);
    lay->setSpacing(6);
    tabs->addTab(w, title);
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
  colHeader->addWidget(addColBtn);
  colHeader->addWidget(delColBtn);
  colHeader->addWidget(upColBtn);
  colHeader->addWidget(downColBtn);
  fieldTab->addLayout(colHeader);

  // ── 字段表格（纯物理属性，独占字段页；页面呈现由三视图接管）──
  m_colsTable = makeConfigTable({{QStringLiteral("列名"), QHeaderView::Stretch, 0},
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
  viewHeader->addWidget(addViewJoinBtn);
  viewHeader->addWidget(delViewBtn);
  presentTab->addLayout(viewHeader);

  m_viewTable = makeConfigTable({{QStringLiteral("字段引用"), QHeaderView::Stretch, 0},
                                 {QStringLiteral("标题"), QHeaderView::Stretch, 0},
                                 {QStringLiteral("列表"), QHeaderView::Interactive, 46},
                                 {QStringLiteral("查询"), QHeaderView::Interactive, 96},
                                 {QStringLiteral("编辑"), QHeaderView::Interactive, 46},
                                 {QStringLiteral("必填"), QHeaderView::Interactive, 46},
                                 {QStringLiteral("⚙"), QHeaderView::Interactive, 36}},
                                this, 380, 0, QAbstractItemView::SelectItems);
  m_viewTable->setToolTip(
      QStringLiteral("一行 = 一个字段在所有页面的呈现（对齐 jsonvue 列配置心智）：\n"
                     "· 列表：勾选后出现在列表页表格（标题/列宽在 ⚙ 配置）\n"
                     "· 查询：不查询 / 精确 / 模糊 / 范围 / 仅接口（后端生成条件、前端不渲染）\n"
                     "· 编辑：☑渲染 / ■进表单但不渲染（参数保留）/ ☐不进表单\n"
                     "· ⚙：列宽 / 控件类型 / 编辑标题覆盖\n"
                     "联表行（引用名.目标列）只读，编辑列自动禁用；枚举列/外键列查询自动下拉"));
  presentTab->addWidget(m_viewTable, 1);
  m_viewRefDelegate = new EditableComboDelegate(QStringList(), m_viewTable);
  m_viewTable->setItemDelegateForColumn(UColRef, m_viewRefDelegate);

  // ── 默认排序表格（列表呈现的附属配置）──
  auto *sortHeader = new QHBoxLayout;
  sortHeader->addWidget(new QLabel(QStringLiteral("默认排序:"), this));
  sortHeader->addStretch();
  auto *addSortBtn = makeCompactButton(QStringLiteral("+ 加排序"), this);
  auto *delSortBtn = makeCompactButton(QStringLiteral("- 删排序"), this);
  sortHeader->addWidget(addSortBtn);
  sortHeader->addWidget(delSortBtn);
  presentTab->addLayout(sortHeader);

  m_sortTable = makeConfigTable({{QStringLiteral("排序列"), QHeaderView::Stretch, 0},
                                 {QStringLiteral("方向"), QHeaderView::Interactive, 100}},
                                this, 100, 0, QAbstractItemView::SelectItems);
  presentTab->addWidget(m_sortTable, 0);
  m_sortRefDelegate = new EditableComboDelegate(QStringList(), m_sortTable);
  m_sortTable->setItemDelegateForColumn(0, m_sortRefDelegate);

  // ── 联表声明表格（joinTable：多对一列表联查/一对多详情聚合声明）──
  auto *joinHeader = new QHBoxLayout;
  joinHeader->addWidget(new QLabel(QStringLiteral("联表(joinTable):"), this));
  joinHeader->addStretch();
  auto *addJoinBtn = makeCompactButton(QStringLiteral("+ 加联表"), this);
  auto *delJoinBtn = makeCompactButton(QStringLiteral("- 删联表"), this);
  joinHeader->addWidget(addJoinBtn);
  joinHeader->addWidget(delJoinBtn);
  joinTab->addLayout(joinHeader);

  m_joinTable = makeConfigTable({{QStringLiteral("引用名"), QHeaderView::Interactive, 90},
                                 {QStringLiteral("目标表"), QHeaderView::Interactive, 110},
                                 {QStringLiteral("外键列"), QHeaderView::Interactive, 100},
                                 {QStringLiteral("主键列"), QHeaderView::Interactive, 84},
                                 {QStringLiteral("joinType"), QHeaderView::Interactive, 96},
                                 {QStringLiteral("启用"), QHeaderView::Interactive, 48}},
                                this, 120, 0, QAbstractItemView::SelectItems);
  m_joinTable->setToolTip(
      QStringLiteral("引用名=joinEnClassPro（别名前缀+前端 ref 前缀）；目标表=joinEnClass"
                     "（本文件其它表的模型名）；外键列=joinFk（本表列）；主键列=joinPk（缺省 id）。\n"
                     "ManyToOne=列表联查（联表列以 引用名_列名 平铺返回）；OneToMany=详情聚合"
                     "（生成端暂跳过并提示）。启用=isJoinRelations，未勾选条目不生效；"
                     "外键列在编辑/查询视图会自动生成目标表下拉"));
  joinTab->addWidget(m_joinTable, 1);
  // 目标表下拉候选 = 同文件 tables[].modelName（构造期固定）；外键列候选 = 字段列名（动态刷新）
  m_joinClassDelegate = new EditableComboDelegate(QStringList(), m_joinTable);
  m_joinTable->setItemDelegateForColumn(JoinColClass, m_joinClassDelegate);
  {
    QStringList modelNames;
    for (const auto &t : m_config.tables) {
      if (!t.modelName.isEmpty()) modelNames.append(t.modelName);
    }
    static_cast<EditableComboDelegate *>(m_joinClassDelegate)->setOptions(modelNames);
  }
  m_joinFkDelegate = new EditableComboDelegate(QStringList(), m_joinTable);
  m_joinTable->setItemDelegateForColumn(JoinColFk, m_joinFkDelegate);

  // ── 索引表格工具行 ──
  auto *idxHeader = new QHBoxLayout;
  idxHeader->addWidget(new QLabel(QStringLiteral("索引:"), this));
  idxHeader->addStretch();
  auto *addIdxBtn = makeCompactButton(QStringLiteral("+ 加索引"), this);
  auto *delIdxBtn = makeCompactButton(QStringLiteral("- 删索引"), this);
  idxHeader->addWidget(addIdxBtn);
  idxHeader->addWidget(delIdxBtn);
  joinTab->addLayout(idxHeader);

  m_indexTable = makeConfigTable({{QStringLiteral("索引名"), QHeaderView::Stretch, 0},
                                  {QStringLiteral("列(逗号分隔)"), QHeaderView::Stretch, 0},
                                  {QStringLiteral("unique"), QHeaderView::Interactive, 70}},
                                 this, 150, 0, QAbstractItemView::SelectItems);
  m_indexTable->setToolTip(
      QStringLiteral("unique 索引自动生成 Insert/Update 唯一性预检（后端）"));
  joinTab->addWidget(m_indexTable, 1);

  // ── 表内枚举区（主从表：主表选枚举行，从表编辑选项）──
  auto *enumHeader = new QHBoxLayout;
  enumHeader->addWidget(new QLabel(QStringLiteral("表内枚举:"), this));
  enumHeader->addStretch();
  auto *addEnumBtn = makeCompactButton(QStringLiteral("+ 加枚举"), this);
  auto *delEnumBtn = makeCompactButton(QStringLiteral("- 删枚举"), this);
  enumHeader->addWidget(addEnumBtn);
  enumHeader->addWidget(delEnumBtn);
  enumTab->addLayout(enumHeader);

  m_enumTable = makeConfigTable({{QStringLiteral("枚举列"), QHeaderView::Interactive, 150},
                                 {QStringLiteral("说明"), QHeaderView::Stretch, 0}},
                                this, 72, 0, QAbstractItemView::SelectRows);
  m_enumTable->setToolTip(QStringLiteral("枚举列需与字段表格中的列名一致（如 status）"));
  enumTab->addWidget(m_enumTable, 1);
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
  enumTab->addLayout(optHeader);

  m_enumOptTable = makeConfigTable({{QStringLiteral("成员名(key)"), QHeaderView::Interactive, 120},
                                    {QStringLiteral("说明(label)"), QHeaderView::Stretch, 0},
                                    {QStringLiteral("值(value)"), QHeaderView::Interactive, 90},
                                    {QStringLiteral("值类型"), QHeaderView::Interactive, 96}},
                                   this, 100, 0, QAbstractItemView::SelectItems);
  enumTab->addWidget(m_enumOptTable, 2);

  // ── 全局枚举列 ──
  auto *geRow = new QHBoxLayout;
  geRow->setSpacing(6);
  geRow->addWidget(new QLabel(QStringLiteral("全局枚举列:"), this));
  m_globalEnumEdit = new QLineEdit(this);
  m_globalEnumEdit->setPlaceholderText(
      QStringLiteral("逗号分隔，如 is_enable（列名 = .jsonglobalenum 中的枚举名）"));
  m_globalEnumEdit->setToolTip(
      QStringLiteral("命中列在 Go 侧生成对应 const 块，前端标准页合成全局枚举取值域；与表内枚举二选一"));
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
  auto applyI18nEnabled = [this](bool on) {
    m_i18nTableEdit->setEnabled(on);
    m_i18nExtEdit->setEnabled(on);
    m_i18nLangKeyEdit->setEnabled(on);
    m_i18nDefaultLangEdit->setEnabled(on);
    m_i18nLangFromCombo->setEnabled(on);
    m_i18nFieldsEdit->setEnabled(on);
  };
  applyI18nEnabled(false);

  // ── DDL 预览 + 执行区（DDL 页）──
  auto *ddlHeader = new QHBoxLayout;
  ddlHeader->addWidget(new QLabel(QStringLiteral("DDL:"), this));
  ddlHeader->addStretch();
  m_importBtn = makeCompactButton(QStringLiteral("从数据库导入"), this);
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
  connect(addViewJoinBtn, &QPushButton::clicked, this, [this]() {
    // 加联表列：候选 = 已启用 ManyToOne 联表的「引用名.目标列」
    QStringList cands;
    for (const auto &j : collectJoins()) {
      if (!j.isJoinRelations || j.joinType != QStringLiteral("ManyToOne") ||
          j.joinEnClassPro.isEmpty()) {
        continue;
      }
      for (const auto &t : m_config.tables) {
        if (t.modelName != j.joinEnClass) continue;
        for (const auto &c : t.columns) {
          if (!c.name.isEmpty()) cands.append(j.joinEnClassPro + QLatin1Char('.') + c.name);
        }
        break;
      }
    }
    if (cands.isEmpty()) {
      AuiMessageBox::show(this, QStringLiteral("无法添加"),
                          QStringLiteral("请先在「联表与索引」页声明启用的 ManyToOne 联表"));
      return;
    }
    bool ok = false;
    const QString ref = QInputDialog::getItem(this, QStringLiteral("添加联表列"),
                                              QStringLiteral("选择联表列："), cands, 0, false, &ok);
    if (ok && !ref.isEmpty()) appendViewJoinRow(ref);
  });
  connect(delViewBtn, &QPushButton::clicked, this, [this]() {
    const int row = m_viewTable->currentRow();
    if (row < 0) return;
    // 仅联表行可删（物理列行跟随字段表格）
    const QString ref = cellOf(m_viewTable, row, UColRef);
    if (!ref.contains(QLatin1Char('.'))) {
      AuiMessageBox::show(this, QStringLiteral("无法删除"),
                          QStringLiteral("物理列行跟随「字段」页的字段表格，不可在此删除；"
                                         "联表行（引用名.目标列）才可删除"));
      return;
    }
    m_viewTable->removeRow(row);
  });
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
  connect(m_i18nCheck, &QCheckBox::toggled, this, [this, applyI18nEnabled](bool on) {
    applyI18nEnabled(on);
  });
  // 字段表格编辑 → 列名候选刷新（枚举列下拉/联表外键列/编辑列等）
  connect(m_colsTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *it) {
    if (!it) return;
    refreshColumnCandidates();
  });
  // 字段呈现表 ⚙ 列点击 → 字段设置面板
  connect(m_viewTable, &QTableWidget::cellClicked, this, [this](int row, int col) {
    if (col == UColGear) openViewFieldConfig(row);
  });
  // 翻译表名输入 → 多语言字段候选切换
  connect(m_i18nTableEdit, &QLineEdit::textChanged, this,
          [this]() { refreshI18nFieldCandidates(); });

  layout->addWidget(tabs, 1);
  finishConfigDialog(this, frame);
  setMinimumSize(860, 600);
  resize(980, 740);

  // ── 预填 ──
  m_nameEdit->setText(m_table.tableName);
  m_modelEdit->setText(m_table.modelName);
  m_commentEdit->setText(m_table.tableComment);
  m_manualUpdateCheck->setChecked(m_table.manualUpdateTime);
  fillColumns(m_table.columns);
  fillIndexes(m_table.indexes);
  fillJoins(m_table.joinTable);
  fillViewTable();
  fillSortTable(m_table.selColsSort);
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
  refreshColumnCandidates();
  refreshI18nFieldCandidates();
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
  refreshColumnCandidates();  // 行删除不触发 itemChanged，显式刷新候选
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
  refreshColumnCandidates();  // 行搬运不触发 itemChanged，显式刷新候选
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
//  联表声明（joinTable）/ 列表读模型（selCols）
// ════════════════════════════════════════════════════════════

void JsonTableDialog::fillJoins(const QVector<JsonTableJoin> &joins) {
  m_joinTable->setRowCount(0);
  for (const auto &j : joins) appendJoinRow();
  // 逐行回填（appendJoinRow 建的是空行）
  for (int r = 0; r < joins.size() && r < m_joinTable->rowCount(); ++r) {
    const JsonTableJoin &j = joins[r];
    m_joinTable->setItem(r, JoinColPro, new QTableWidgetItem(j.joinEnClassPro));
    m_joinTable->setItem(r, JoinColClass, new QTableWidgetItem(j.joinEnClass));
    m_joinTable->setItem(r, JoinColFk, new QTableWidgetItem(j.joinFk));
    // joinPk 缺省 id 显示为空（空 = id 语义），非缺省值才回显
    if (!j.joinPk.isEmpty() && j.joinPk != QStringLiteral("id")) {
      m_joinTable->setItem(r, JoinColPk, new QTableWidgetItem(j.joinPk));
    }
    auto *typeCombo = qobject_cast<QComboBox *>(m_joinTable->cellWidget(r, JoinColType));
    if (typeCombo) comboSelectData(typeCombo, j.joinType);
    m_joinTable->setItem(r, JoinColActive, makeCheckItem(j.isJoinRelations));
  }
}

void JsonTableDialog::appendJoinRow() {
  const int row = m_joinTable->rowCount();
  m_joinTable->insertRow(row);
  m_joinTable->setItem(row, JoinColPro, new QTableWidgetItem(QString()));
  m_joinTable->setItem(row, JoinColClass, new QTableWidgetItem(QString()));
  m_joinTable->setItem(row, JoinColFk, new QTableWidgetItem(QString()));
  m_joinTable->setItem(row, JoinColPk, new QTableWidgetItem(QString()));
  auto *typeCombo = AuiComboBox::create(this);
  typeCombo->addItem(QStringLiteral("ManyToOne"), QStringLiteral("ManyToOne"));
  typeCombo->addItem(QStringLiteral("OneToMany"), QStringLiteral("OneToMany"));
  m_joinTable->setCellWidget(row, JoinColType, typeCombo);
  m_joinTable->setItem(row, JoinColActive, makeCheckItem(true));
  m_joinTable->setCurrentCell(row, JoinColPro);
}

QVector<JsonTableJoin> JsonTableDialog::collectJoins() const {
  QVector<JsonTableJoin> out;
  for (int r = 0; r < m_joinTable->rowCount(); ++r) {
    JsonTableJoin j;
    j.joinEnClassPro = cellOf(m_joinTable, r, JoinColPro);
    j.joinEnClass = cellOf(m_joinTable, r, JoinColClass);
    j.joinFk = cellOf(m_joinTable, r, JoinColFk);
    j.joinPk = cellOf(m_joinTable, r, JoinColPk);
    if (j.joinPk.isEmpty()) j.joinPk = QStringLiteral("id");
    auto *typeCombo = qobject_cast<QComboBox *>(m_joinTable->cellWidget(r, JoinColType));
    j.joinType = typeCombo ? typeCombo->currentData().toString() : QStringLiteral("ManyToOne");
    if (j.joinType.isEmpty()) j.joinType = QStringLiteral("ManyToOne");
    const QTableWidgetItem *activeIt = m_joinTable->item(r, JoinColActive);
    j.isJoinRelations = activeIt && activeIt->checkState() == Qt::Checked;
    // 引用名与目标均未填的行视为空行，跳过
    if (j.joinEnClassPro.isEmpty() && j.joinEnClass.isEmpty() && j.joinFk.isEmpty()) continue;
    out.append(j);
  }
  return out;
}

// ════════════════════════════════════════════════════════════
//  字段呈现（统一表格：三视图双向映射 + ⚙ 字段设置面板）
// ════════════════════════════════════════════════════════════

void JsonTableDialog::fillViewTable() {
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

  // 模式判定需区分键归属，循环三键填 mode2（ref → {mode, visible}）
  QHash<QString, QPair<QString, bool>> mode2;
  for (const auto &c : m_table.selColsEq) mode2.insert(c.ref, {QStringLiteral("eq"), c.visible});
  for (const auto &c : m_table.selColsLike) mode2.insert(c.ref, {QStringLiteral("like"), c.visible});
  for (const auto &c : m_table.selColsGap) mode2.insert(c.ref, {QStringLiteral("gap"), c.visible});

  auto appendRow = [&](const QString &ref, bool isJoinRow) {
    const int row = m_viewTable->rowCount();
    m_viewTable->insertRow(row);
    // 标题（生效值）：selCols.label > 列注释(本表/联表目标列) > ref
    QString title;
    const auto selIt = selOf.constFind(ref);
    if (selIt != selOf.constEnd() && !selIt.value().label.isEmpty()) {
      title = selIt.value().label;
    } else if (isJoinRow) {
      const int dot = ref.indexOf(QLatin1Char('.'));
      const QString pro = ref.left(dot);
      const QString cn = ref.mid(dot + 1);
      const auto jc = joins.constFind(pro);
      if (dot > 0 && jc != joins.constEnd() && jc.value().comments.contains(cn)) {
        title = jc.value().comments.value(cn);
      }
      if (title.isEmpty()) title = ref;
    } else if (colComments.contains(ref) && !colComments.value(ref).isEmpty()) {
      title = colComments.value(ref);
    } else {
      title = ref;
    }
    auto *refItem = new QTableWidgetItem(ref);
    const auto selMeta = selOf.value(ref);
    refItem->setData(Qt::UserRole + 1, selMeta.width > 0 ? selMeta.width : 0);   // 列宽
    const auto editMeta = editOf.value(ref);
    refItem->setData(Qt::UserRole + 2, editMeta.widget);                          // 控件类型
    refItem->setData(Qt::UserRole + 3, editMeta.label);                           // 编辑标题
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
    m_viewTable->setCellWidget(row, UColQuery, modeCombo);
    // 编辑三态勾（联表行只读禁用；三态用 ItemIsUserTristate flag）
    auto *editItem = new QTableWidgetItem;
    editItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                       (isJoinRow ? Qt::NoItemFlags
                                  : Qt::ItemFlags(Qt::ItemIsUserCheckable |
                                                  Qt::ItemIsUserTristate)));
    if (const auto eIt = editOf.constFind(ref); eIt != editOf.constEnd()) {
      editItem->setCheckState(eIt.value().visible ? Qt::Checked : Qt::PartiallyChecked);
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
    // ⚙ 按钮（cellClicked 列判断触发，见构造函数连接）
    m_viewTable->setItem(row, UColGear, new QTableWidgetItem(QStringLiteral("⚙")));
    m_viewTable->item(row, UColGear)->setTextAlignment(Qt::AlignCenter);
  };

  // 行 1：物理列（跟随字段表格）
  for (const auto &c : m_table.columns) appendRow(c.name, false);
  // 行 2：联表 ref（selCols 联表路径 + 查询键联表路径，去重）
  QSet<QString> joinRefs;
  for (auto it = selOf.constBegin(); it != selOf.constEnd(); ++it) {
    if (it.key().contains(QLatin1Char('.'))) joinRefs.insert(it.key());
  }
  for (auto it = queryOf.constBegin(); it != queryOf.constEnd(); ++it) {
    if (it.key().contains(QLatin1Char('.'))) joinRefs.insert(it.key());
  }
  QStringList joinRefList = QStringList(joinRefs.begin(), joinRefs.end());
  joinRefList.sort();
  for (const QString &ref : joinRefList) appendRow(ref, true);
}

void JsonTableDialog::appendViewJoinRow(const QString &ref) {
  // 联表行：列表/查询默认开，编辑只读禁用（联表列只读）
  const int row = m_viewTable->rowCount();
  m_viewTable->insertRow(row);
  auto *refItem = new QTableWidgetItem(ref);
  refItem->setData(Qt::UserRole + 1, 0);
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
  m_viewTable->setCellWidget(row, UColQuery, modeCombo);
  auto *editItem = new QTableWidgetItem;
  editItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);  // 联表行只读
  editItem->setTextAlignment(Qt::AlignCenter);
  m_viewTable->setItem(row, UColEdit, editItem);
  auto *reqItem = new QTableWidgetItem;
  reqItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
  reqItem->setTextAlignment(Qt::AlignCenter);
  m_viewTable->setItem(row, UColRequired, reqItem);
  m_viewTable->setItem(row, UColGear, new QTableWidgetItem(QStringLiteral("⚙")));
  m_viewTable->item(row, UColGear)->setTextAlignment(Qt::AlignCenter);
  m_viewTable->setCurrentCell(row, UColRef);
}

void JsonTableDialog::collectViewTable(JsonTableTable *out) const {
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
    QString editLabel;
    if (const QTableWidgetItem *refIt = m_viewTable->item(r, UColRef)) {
      widthStr = refIt->data(Qt::UserRole + 1).toString();
      widget = refIt->data(Qt::UserRole + 2).toString();
      editLabel = refIt->data(Qt::UserRole + 3).toString();
    }
    // 列表视图
    const QTableWidgetItem *listIt = m_viewTable->item(r, UColList);
    if (listIt && listIt->checkState() == Qt::Checked) {
      JsonTableSelCol c;
      c.ref = ref;
      if (!title.isEmpty() && title != ref) c.label = title;
      c.width = widthStr.toInt();
      out->selCols.append(c);
    }
    // 查询视图（模式下拉）
    auto *modeCombo = qobject_cast<QComboBox *>(m_viewTable->cellWidget(r, UColQuery));
    const QString mode = modeCombo ? modeCombo->currentData().toString() : QStringLiteral("none");
    if (mode != QStringLiteral("none")) {
      JsonTableQueryCol qc;
      qc.ref = ref;
      qc.visible = (mode != QStringLiteral("api"));  // 仅接口 = 后端生成、前端不渲染
      if (mode == QStringLiteral("eq")) out->selColsEq.append(qc);
      else if (mode == QStringLiteral("like")) out->selColsLike.append(qc);
      else if (mode == QStringLiteral("gap")) out->selColsGap.append(qc);
    }
    // 编辑视图（三态；联表行禁用天然 Unchecked）
    const QTableWidgetItem *editIt = m_viewTable->item(r, UColEdit);
    if (editIt && editIt->checkState() != Qt::Unchecked && !ref.contains(QLatin1Char('.'))) {
      JsonTableEditCol ec;
      ec.ref = ref;
      ec.visible = (editIt->checkState() == Qt::Checked);
      // 编辑标题：⚙ 覆盖 > 主表格标题（空 = 生成端回退列注释）
      if (!editLabel.isEmpty()) {
        ec.label = editLabel;
      } else if (!title.isEmpty() && title != ref) {
        ec.label = title;
      }
      ec.widget = widget;
      const QTableWidgetItem *reqIt = m_viewTable->item(r, UColRequired);
      ec.required = reqIt && reqIt->checkState() == Qt::Checked;
      out->editCols.append(ec);
    }
  }
}

void JsonTableDialog::openViewFieldConfig(int row) {
  if (row < 0 || row >= m_viewTable->rowCount()) return;
  const QString ref = cellOf(m_viewTable, row, UColRef);
  if (ref.isEmpty()) return;
  const bool isJoinRow = ref.contains(QLatin1Char('.'));
  QTableWidgetItem *refIt = m_viewTable->item(row, UColRef);

  QDialog dlg(this);
  ConfigDialogFrame frame = beginConfigDialog(&dlg, QStringLiteral("字段设置：%1").arg(ref),
                                              QMargins(12, 10, 12, 10), 6);
  auto *form = new QFormLayout;
  form->setSpacing(6);
  auto *widthEdit = new QLineEdit(&dlg);
  widthEdit->setPlaceholderText(QStringLiteral("空 = 自适应"));
  widthEdit->setText(refIt->data(Qt::UserRole + 1).toString());
  form->addRow(QStringLiteral("列表列宽(px):"), widthEdit);
  auto *widgetCombo = AuiComboBox::create(&dlg);
  widgetCombo->addItem(QStringLiteral("自动（按 mysqlType 推导）"), QStringLiteral(""));
  widgetCombo->addItem(QStringLiteral("input"), QStringLiteral("input"));
  widgetCombo->addItem(QStringLiteral("textarea"), QStringLiteral("textarea"));
  widgetCombo->addItem(QStringLiteral("select"), QStringLiteral("select"));
  widgetCombo->addItem(QStringLiteral("date"), QStringLiteral("date"));
  widgetCombo->addItem(QStringLiteral("number"), QStringLiteral("number"));
  widgetCombo->addItem(QStringLiteral("image"), QStringLiteral("image"));
  widgetCombo->addItem(QStringLiteral("upload"), QStringLiteral("upload"));
  widgetCombo->addItem(QStringLiteral("boolean"), QStringLiteral("boolean"));
  comboSelectData(widgetCombo, refIt->data(Qt::UserRole + 2).toString());
  widgetCombo->setEnabled(!isJoinRow);
  form->addRow(QStringLiteral("编辑控件:"), widgetCombo);
  auto *editLabelEdit = new QLineEdit(&dlg);
  editLabelEdit->setPlaceholderText(QStringLiteral("空 = 用主表格标题"));
  editLabelEdit->setText(refIt->data(Qt::UserRole + 3).toString());
  editLabelEdit->setEnabled(!isJoinRow);
  form->addRow(QStringLiteral("编辑标题覆盖:"), editLabelEdit);
  frame.contentLayout->addLayout(form);
  if (isJoinRow) {
    auto *tip = new QLabel(QStringLiteral("联表列只读：无编辑控件/标题/列宽配置"), &dlg);
    frame.contentLayout->addWidget(tip);
  }
  finishConfigDialog(&dlg, frame);

  if (dlg.exec() != QDialog::Accepted) return;
  // 确定后写回行 UserRole（collectViewTable 读取落三视图）
  refIt->setData(Qt::UserRole + 1, widthEdit->text().trimmed());
  refIt->setData(Qt::UserRole + 2, widgetCombo->currentData().toString());
  refIt->setData(Qt::UserRole + 3, editLabelEdit->text().trimmed());
}
// ════════════════════════════════════════════════════════════
//  表级进阶键：查询配置 / 排序 / 表内枚举 / i18n
// ════════════════════════════════════════════════════════════

QStringList JsonTableDialog::splitCsv(const QString &text) {
  QStringList out;
  for (const QString &part : text.split(QLatin1Char(','))) {
    const QString item = part.trimmed();
    if (!item.isEmpty()) out.append(item);
  }
  return out;
}

QString JsonTableDialog::joinCsv(const QStringList &list) {
  return list.join(QStringLiteral(","));
}

void JsonTableDialog::fillSortTable(const QVector<QPair<QString, QString>> &sort) {
  m_sortTable->setRowCount(0);
  for (const auto &p : sort) appendSortRow(p.first, p.second);
}

void JsonTableDialog::appendSortRow(const QString &col, const QString &dir) {
  const int row = m_sortTable->rowCount();
  m_sortTable->insertRow(row);
  m_sortTable->setItem(row, 0, new QTableWidgetItem(col));
  auto *dirCombo = AuiComboBox::create(this);
  dirCombo->addItem(QStringLiteral("ASC"), QStringLiteral("ASC"));
  dirCombo->addItem(QStringLiteral("DESC"), QStringLiteral("DESC"));
  comboSelectData(dirCombo, dir.isEmpty() ? QStringLiteral("DESC") : dir);
  m_sortTable->setCellWidget(row, 1, dirCombo);
}

QVector<QPair<QString, QString>> JsonTableDialog::collectSort() const {
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

void JsonTableDialog::syncEnumDrafts() {
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

void JsonTableDialog::rebuildEnumMaster() {
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

void JsonTableDialog::fillEnumOptions(int row) {
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
      comboSelectData(typeCombo, o.valueType.isEmpty() ? QStringLiteral("string")
                                                       : o.valueType);
      m_enumOptTable->setCellWidget(r, 3, typeCombo);
    }
  }
  m_enumSyncing = false;
}

void JsonTableDialog::appendEnumOptRow() {
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

QVector<JsonTableEnumOption> JsonTableDialog::collectEnumOptions() const {
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

QVector<JsonTableEnum> JsonTableDialog::collectEnums() const {
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

// ════════════════════════════════════════════════════════════
//  交叉校验 / 列名候选刷新
// ════════════════════════════════════════════════════════════

void JsonTableDialog::accept() {
  QString err;
  if (!validateCrossRefs(&err)) {
    AuiMessageBox::show(this, QStringLiteral("无法保存"), err);
    return;
  }
  QDialog::accept();
}

bool JsonTableDialog::validateCrossRefs(QString *err) const {
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
    if (e.column.isEmpty())
      return fail(QStringLiteral("第 %1 个表内枚举未填枚举列").arg(i + 1));
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

  // ── 联表声明（joinTable）：引用名去重、joinType 合法、目标/外键/主键可解析 ──
  const QVector<JsonTableJoin> joins = collectJoins();
  QSet<QString> joinPros;                          // 有效 ManyToOne 条目的引用名
  QHash<QString, const JsonTableTable *> joinsByPro;  // 引用名 → 目标表（ref 解析用）
  for (int i = 0; i < joins.size(); ++i) {
    const JsonTableJoin &j = joins[i];
    if (!j.isJoinRelations) continue;  // 未启用条目不生效，不参与校验
    if (j.joinEnClassPro.isEmpty())
      return fail(QStringLiteral("第 %1 条联表未填引用名(joinEnClassPro)").arg(i + 1));
    if (joinPros.contains(j.joinEnClassPro))
      return fail(QStringLiteral("联表引用名 '%1' 重复").arg(j.joinEnClassPro));
    if (j.joinType != QStringLiteral("ManyToOne") && j.joinType != QStringLiteral("OneToMany"))
      return fail(QStringLiteral("联表 '%1' 的 joinType 仅支持 ManyToOne/OneToMany")
                      .arg(j.joinEnClassPro));
    // 目标表按 modelName 定位（joinEnClass 填目标表模型名）
    const JsonTableTable *target = nullptr;
    for (const auto &t : m_config.tables) {
      if (t.modelName == j.joinEnClass) {
        target = &t;
        break;
      }
    }
    if (!target)
      return fail(QStringLiteral("联表 '%1' 的目标 '%2' 不在本文件 tables[] 中"
                                 "（joinEnClass 填目标表模型名）")
                      .arg(j.joinEnClassPro, j.joinEnClass));
    if (j.joinType == QStringLiteral("ManyToOne")) {
      if (j.joinFk.isEmpty() || !colNames.contains(j.joinFk))
        return fail(QStringLiteral("联表 '%1' 的外键列 '%2' 不在字段列表中")
                        .arg(j.joinEnClassPro, j.joinFk.isEmpty() ? QStringLiteral("(空)")
                                                                  : j.joinFk));
      joinsByPro.insert(j.joinEnClassPro, target);  // 仅 ManyToOne 供读模型 ref 解析
    }
    joinPros.insert(j.joinEnClassPro);
  }

  // ── 读模型 ref 可解析（本表列名 或 引用名.目标列；别名 pro_col 不得撞主表列名）──
  auto refOk = [&](const QString &ref, QString *aliasOut) -> bool {
    const int dot = ref.indexOf(QLatin1Char('.'));
    if (dot <= 0) return colNames.contains(ref);  // 本表列
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

  // ── 编辑视图（editCols）：ref 仅物理列、去重、required 与 invisible 互斥 ──
  const QVector<JsonTableEditCol> editRows = view.editCols;
  QSet<QString> editRefs;
  for (int i = 0; i < editRows.size(); ++i) {
    const JsonTableEditCol &ec = editRows[i];
    if (!colNames.contains(ec.ref))
      return fail(QStringLiteral("编辑列 '%1' 不在字段列表中（联表列只读，不可编辑）").arg(ec.ref));
    if (editRefs.contains(ec.ref))
      return fail(QStringLiteral("编辑列 '%1' 重复配置").arg(ec.ref));
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
    const JsonTableTable *trans = nullptr;
    for (const auto &t : m_config.tables) {
      if (t.tableName == i18nTable) {
        trans = &t;
        break;
      }
    }
    if (!trans)
      return fail(QStringLiteral("翻译表 '%1' 不在本 .jsontable 文件的 tables[] 中").arg(i18nTable));
    QSet<QString> transCols;
    for (const auto &c : trans->columns) transCols.insert(c.name);
    const QString extKey = m_i18nExtEdit->text().trimmed();
    const QString langKey = m_i18nLangKeyEdit->text().trimmed();
    if (extKey.isEmpty() || !transCols.contains(extKey))
      return fail(
          QStringLiteral("i18n 外键列 '%1' 不在翻译表 '%2' 的列中")
              .arg(extKey.isEmpty() ? QStringLiteral("(空)") : extKey, i18nTable));
    if (langKey.isEmpty() || !transCols.contains(langKey))
      return fail(
          QStringLiteral("i18n 语言列 '%1' 不在翻译表 '%2' 的列中")
              .arg(langKey.isEmpty() ? QStringLiteral("(空)") : langKey, i18nTable));
    for (const QString &f : splitCsv(m_i18nFieldsEdit->text())) {
      if (!transCols.contains(f))
        return fail(QStringLiteral("多语言字段 '%1' 不在翻译表 '%2' 的列中").arg(f, i18nTable));
      if (colNames.contains(f))
        return fail(QStringLiteral("多语言字段 '%1' 与主表列重名").arg(f));
    }
  }
  return true;
}

void JsonTableDialog::refreshColumnCandidates() {
  // 本表列名（枚举列/全局枚举列/联表外键列/编辑列候选）
  QStringList names;
  for (int r = 0; r < m_colsTable->rowCount(); ++r) {
    const QString n = cellText(r, ColColName);
    if (!n.isEmpty()) names.append(n);
  }
  // 读模型 ref 候选 = 本表列名 + 有效 ManyToOne 联表的「引用名.目标列」
  QStringList refCands = names;
  for (const auto &j : collectJoins()) {
    if (!j.isJoinRelations || j.joinType != QStringLiteral("ManyToOne") ||
        j.joinEnClassPro.isEmpty())
      continue;
    for (const auto &t : m_config.tables) {
      if (t.modelName != j.joinEnClass) continue;
      for (const auto &c : t.columns) {
        if (!c.name.isEmpty()) refCands.append(j.joinEnClassPro + QLatin1Char('.') + c.name);
      }
      break;
    }
  }
  if (m_enumColDelegate)
    static_cast<EditableComboDelegate *>(m_enumColDelegate)->setOptions(names);
  if (m_joinFkDelegate)
    static_cast<EditableComboDelegate *>(m_joinFkDelegate)->setOptions(names);
  if (m_viewRefDelegate)
    static_cast<EditableComboDelegate *>(m_viewRefDelegate)->setOptions(refCands);
  if (m_sortRefDelegate)
    static_cast<EditableComboDelegate *>(m_sortRefDelegate)->setOptions(refCands);
  if (m_globalEnumCompleter)
    static_cast<CsvCompleter *>(m_globalEnumCompleter)->setCandidates(names);
}

void JsonTableDialog::refreshI18nFieldCandidates() {
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

  // 导入为纯物理结构：DB 列为基础重建（列级呈现角色已退役，列表/查询/编辑
  // 三个视图的配置不随导入变化，仅物理列定义被替换）
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

    // 导入为纯物理结构（列级呈现角色已退役，编辑/查询/列表视图不受影响）
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
  JsonTableTable t = m_table;  // extra 未知键保真（joinTable 等未结构化节点原样写回）
  t.tableName = m_nameEdit->text().trimmed();
  t.modelName = m_modelEdit->text().trimmed();
  t.tableComment = m_commentEdit->text().trimmed();
  t.manualUpdateTime = m_manualUpdateCheck->isChecked();
  t.columns = collectColumns();
  t.indexes = collectIndexes();
  // ── 表级进阶键 ──
  t.joinTable = collectJoins();
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
  return t;
}
