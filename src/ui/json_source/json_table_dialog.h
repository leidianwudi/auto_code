/**
 * @file json_table_dialog.h
 * @brief .jsontable 单张表设计器对话框
 *
 * 编辑一张 JsonTableTable：界面按页签分区（字段 / 索引与查询 / 枚举 / 多语言 /
 * DDL）。字段页含表信息与字段表格（列名/MySQL类型/unsigned/可空/主键/自增/
 * 默认值/注释/列表/搜索/表单角色/必填，select/upload 角色带标准页降级标记）；
 * 其余页承载表级进阶键（索引、范围/模糊列与默认排序、表内枚举主从表、
 * 全局枚举列、多语言翻译表配置）与 DDL。确定时做交叉引用校验（枚举列/
 * 全局枚举列/查询排序索引列存在性、表名重复、i18n 引用完整性）。
 * 支持从数据库反向导入表结构（合并保留
 * .jsontable 已有列的前端角色），DDL 预览与执行建表。
 * 通过构造传入待编辑表，resultTable() 取回编辑结果（getter 模式，无信号回写）。
 */

#pragma once

#include <QDialog>

#include "json_table_model.h"

class QCheckBox;
class QComboBox;
class QCompleter;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStyledItemDelegate;
class QTableWidget;

/**
 * @class JsonTableDialog
 * @brief .jsontable 表设计器对话框
 */
class JsonTableDialog : public QDialog {
  Q_OBJECT

public:
  /// 传入待编辑表与完整配置（meta.db 连接信息供"从数据库导入/执行建表"）
  explicit JsonTableDialog(const JsonTableTable &table, const JsonTableConfig &config,
                         QWidget *parent = nullptr);

  /// 取回编辑结果（未编辑字段沿用构造入参，extra 未知键保真）
  JsonTableTable resultTable() const;

  /// 确定：先做交叉引用校验（不过则弹窗拦截），通过才接受关闭
  void accept() override;

private:
  /// 构建界面
  void setupUI();
  // ── 字段表格 ──
  /// 用列定义全量重建字段表格
  void fillColumns(const QVector<JsonTableColumn> &cols);
  /// 追加一行列定义
  void appendColumnRow(const JsonTableColumn &c);
  /// 从表格收集全部列定义（跳过列名为空的行）
  QVector<JsonTableColumn> collectColumns() const;
  /// 读取指定行为列定义
  JsonTableColumn columnFromRow(int row) const;
  QString cellText(int row, int col) const;
  bool cellChecked(int row, int col) const;
  /// 加列（追加默认列并直接进入列名编辑）
  void addColumn();
  /// 删列（删除当前行）
  void deleteColumn();
  /// 上移/下移当前行（delta = -1 / +1）
  void moveColumn(int delta);
  // ── 索引表格 ──
  /// 用索引定义全量重建索引表格
  void fillIndexes(const QVector<JsonTableIndex> &indexes);
  /// 追加一行空索引
  void appendIndexRow();
  /// 从表格收集全部索引定义（跳过索引名为空的行）
  QVector<JsonTableIndex> collectIndexes() const;
  // ── 表级进阶键：查询配置 / 排序 ──
  /// 用排序定义全量重建排序表格
  void fillSortTable(const QVector<QPair<QString, QString>> &sort);
  /// 追加一行排序（列名 + 方向，缺省 DESC）
  void appendSortRow(const QString &col, const QString &dir);
  /// 从表格收集全部排序定义（跳过排序列为空的行）
  QVector<QPair<QString, QString>> collectSort() const;
  // ── 表级进阶键：联表声明（joinTable）──
  /// 用联表定义全量重建关联表格
  void fillJoins(const QVector<JsonTableJoin> &joins);
  /// 追加一行空联表声明
  void appendJoinRow();
  /// 从表格收集全部联表声明（跳过引用名与目标均未填的行）
  QVector<JsonTableJoin> collectJoins() const;
  // ── 表级进阶键：列表读模型（selCols）──
  /// 用列表列定义全量重建列表列表格
  void fillSelCols(const QVector<JsonTableSelCol> &cols);
  /// 追加一行空列表列
  void appendSelColRow();
  /// 从表格收集全部列表列（跳过 ref 为空的行）
  QVector<JsonTableSelCol> collectSelCols() const;
  // ── 表级进阶键：查询视图（三键合一表格，mode 拆分）──
  /// 用查询定义全量重建查询条件表格（eq/like/gap 合并填充）
  void fillQueryTable(const QVector<JsonTableQueryCol> &eq, const QVector<JsonTableQueryCol> &like,
                      const QVector<JsonTableQueryCol> &gap);
  /// 追加一行空查询条件（模式缺省精确）
  void appendQueryRow(const QString &ref, const QString &mode, bool visible);
  /// 收集指定模式的查询列（visible 随行勾选）
  QVector<JsonTableQueryCol> collectQueryRows(const QString &mode) const;
  // ── 表级进阶键：编辑视图（editCols）──
  /// 用编辑视图定义全量重建编辑表格
  void fillEditCols(const QVector<JsonTableEditCol> &cols);
  /// 追加一行空编辑列
  void appendEditColRow(const QString &ref);
  /// 从表格收集全部编辑列（跳过 ref 为空的行）
  QVector<JsonTableEditCol> collectEditCols() const;
  // ── 表级进阶键：表内枚举（主从表联动）──
  /// 把主表/选项表格的当前编辑内容同步回 m_enums（结构变化前调用）
  void syncEnumDrafts();
  /// 用枚举定义全量重建枚举主表
  void rebuildEnumMaster();
  /// 选中枚举行变化 → 选项表格切换为该枚举的选项
  void fillEnumOptions(int row);
  /// 追加一行空枚举选项
  void appendEnumOptRow();
  /// 从选项表格收集当前枚举的选项（跳过 value 为空的行）
  QVector<JsonTableEnumOption> collectEnumOptions() const;
  /// 收集全部枚举（当前行选项表格内容先并入）
  QVector<JsonTableEnum> collectEnums() const;
  // ── 工具 ──
  /// 逗号分隔文本 → 列表（去空格去空项）
  static QStringList splitCsv(const QString &text);
  /// 列表 → 逗号分隔文本
  static QString joinCsv(const QStringList &list);
  // ── DDL / 数据库 ──
  /// DDL 生成前校验（表名/列名非空），不通过时 err 返回原因
  bool validateForDdl(QString *err) const;
  /// 确定前交叉校验（对齐生成期规则）：枚举列/全局枚举列/查询排序索引列存在性、
  /// 联表声明（joinFk/joinEnClass/joinPk/ref 可解析、引用名去重、别名不撞列名）、
  /// 表名重复、i18n 翻译表引用完整性；不通过时 err 返回原因
  bool validateCrossRefs(QString *err) const;
  /// 刷新列引用候选：枚举列下拉与全局枚举列补全（候选=字段列名）、
  /// 列表列/查询条件/排序的 ref 候选（字段列名 + 联表 别名.列）、
  /// 联表外键列下拉（候选=字段列名）
  void refreshColumnCandidates();
  /// 刷新多语言字段补全候选（候选=翻译表列名，按翻译表名实时切换）
  void refreshI18nFieldCandidates();
  /// 生成 CREATE TABLE DDL（列 + 主键 + 索引，InnoDB/utf8mb4）
  QString toCreateDdl() const;
  /// 从数据库反向导入表结构（合并模式：保留 .jsontable 已有列的前端角色）
  void importFromDatabase();
  /// 生成 DDL 显示在预览区（校验失败弹警告）
  void previewDdl();
  /// 确认后连接数据库执行 CREATE TABLE
  void executeCreateTable();

  QLineEdit *m_nameEdit = nullptr;            ///< 物理表名
  QLineEdit *m_modelEdit = nullptr;           ///< 模型名
  QLineEdit *m_commentEdit = nullptr;         ///< 表注释
  QCheckBox *m_manualUpdateCheck = nullptr;   ///< 手动维护更新时间
  QTableWidget *m_colsTable = nullptr;        ///< 字段表格（核心编辑区）
  QTableWidget *m_indexTable = nullptr;       ///< 索引表格
  QPlainTextEdit *m_ddlView = nullptr;        ///< DDL 预览（只读）
  QPushButton *m_importBtn = nullptr;         ///< 从数据库导入按钮
  // ── 表级进阶键 ──
  QTableWidget *m_sortTable = nullptr;        ///< 默认排序表格（列名 + 方向）
  QTableWidget *m_joinTable = nullptr;        ///< 联表声明表格（joinTable）
  QTableWidget *m_selColsTable = nullptr;     ///< 列表视图表格（selCols：ref + label + width）
  QTableWidget *m_queryTable = nullptr;       ///< 查询视图表格（ref + 模式 + 前端显示）
  QTableWidget *m_editColsTable = nullptr;    ///< 编辑视图表格（ref/标题/控件/必填/前端显示）
  QTableWidget *m_enumTable = nullptr;        ///< 表内枚举主表（枚举列 + 说明）
  QTableWidget *m_enumOptTable = nullptr;     ///< 选中枚举的选项表格（key/label/value/valueType）
  QLineEdit *m_globalEnumEdit = nullptr;      ///< 全局枚举列（逗号分隔）
  QCheckBox *m_i18nCheck = nullptr;           ///< 启用多语言
  QLineEdit *m_i18nTableEdit = nullptr;       ///< 翻译表名
  QLineEdit *m_i18nExtEdit = nullptr;         ///< 翻译表外键列
  QLineEdit *m_i18nLangKeyEdit = nullptr;     ///< 语言码列
  QLineEdit *m_i18nDefaultLangEdit = nullptr; ///< 默认语言
  QComboBox *m_i18nLangFromCombo = nullptr;   ///< 语言码来源（body/header）
  QLineEdit *m_i18nFieldsEdit = nullptr;      ///< 多语言字段（逗号分隔）
  // ── 列名候选联动（补全器/委托实例在 cpp 内具体类型，成员存基类指针）──
  QCompleter *m_globalEnumCompleter = nullptr;    ///< 全局枚举列补全（候选=字段列名）
  QCompleter *m_i18nFieldsCompleter = nullptr;    ///< 多语言字段补全（候选=翻译表列名）
  QStyledItemDelegate *m_enumColDelegate = nullptr;   ///< 枚举列下拉委托（预设=字段列名）
  QStyledItemDelegate *m_joinFkDelegate = nullptr;    ///< 联表外键列下拉委托（预设=字段列名）
  QStyledItemDelegate *m_joinClassDelegate = nullptr; ///< 联表目标下拉委托（预设=tables[].modelName）
  QStyledItemDelegate *m_selColsRefDelegate = nullptr; ///< 列表列 ref 下拉委托（字段列+联表 ref）
  QStyledItemDelegate *m_queryRefDelegate = nullptr;   ///< 查询条件 ref 下拉委托（字段列+联表 ref）
  QStyledItemDelegate *m_editRefDelegate = nullptr;    ///< 编辑列 ref 下拉委托（预设=字段列名）
  QStyledItemDelegate *m_sortRefDelegate = nullptr;    ///< 排序列 ref 下拉委托（字段列+联表 ref）
  QVector<JsonTableEnum> m_enums;             ///< 表内枚举编辑态（主从表联动的数据侧）
  int m_enumCurRow = -1;                      ///< 当前编辑的枚举行（-1=无）
  bool m_enumSyncing = false;                 ///< 主从表联动中（防 currentCellChange 递归）
  JsonTableTable m_table;                       ///< 构造入参（extra/未编辑字段保真基底）
  JsonTableConfig m_config;                     ///< 完整配置（meta.db 连接信息）
};
