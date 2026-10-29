/**
 * @file json_table_designer.h
 * @brief .jsontable 单张表设计器对话框
 *
 * 编辑一张 JsonTableTable：界面按页签分区（字段 / 索引与查询 / 枚举 / 多语言 /
 * DDL）。字段页含表信息与字段表格（列名/MySQL类型/unsigned/可空/主键/自增/
 * 默认值/注释/列表/搜索/表单角色/必填，select/upload 角色带标准页降级标记）；
 * 其余页承载表级进阶键（索引、范围/模糊列与默认排序、表内枚举主从表、
 * 全局枚举列、多语言翻译表配置）与 DDL。确定时做交叉引用校验（枚举列/
 * 全局枚举列/查询排序索引列存在性、表名重复、i18n 引用完整性）。
 * 支持从数据库刷新表结构（差异预览 + 逐列勾选 + 排除列 + 索引同步 +
 * 删除列后的前端引用清理），DDL 预览与执行建表。
 * 通过构造传入待编辑表，resultTable() 取回编辑结果（getter 模式，无信号回写）。
 */

#pragma once

#include <QDialog>
#include <QHash>

#include "json_table_model.h"

class QCheckBox;
class QComboBox;
class QCompleter;
class QEvent;
class QHeaderView;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStyledItemDelegate;
class QTableWidget;

/**
 * @class JsonTableDesigner
 * @brief .jsontable 表设计器面板（可视化编辑器内嵌，逐编辑广播）
 */
class JsonTableDesigner : public QWidget {
  Q_OBJECT

public:
  /// 传入待编辑表、项目配置（proj meta.db 供"从数据库导入/执行建表"）与文件路径
  /// （联表目标跨文件解析 + .jsonsource/.jsonupload 搜索根）
  explicit JsonTableDesigner(const JsonTableTable &table, const JsonTableConfig &config,
                             const QString &filePath = QString(), QWidget *parent = nullptr);

  /// 取回编辑结果（未编辑字段沿用构造入参，extra 未知键保真）
  JsonTableTable collectTable() const;

  /// 换表重建：重新绑定表/项目上下文并全量回填界面（代码侧变更后由宿主调用）
  void load(const JsonTableTable &table, const JsonTableConfig &config, const QString &filePath);

protected:
  /// 事件过滤：仅当悬停「编辑」列表头的问号小图标时弹出两态说明

signals:
  /// 编辑发生变化（逐编辑广播；写回防抖由 CodeVisualSyncWidget 基类处理）
  void configChanged();

private:
  /// 构建界面
  void setupUI();
  /// 用 m_table/m_config 全量回填界面（setupUI 构建后与 load 换表时共用）
  void refill();
  /// 逐编辑信号接线（表格模型变更 + 全部静态输入控件；行内下拉在创建点接线）
  void connectChangeSignals();
  /// 被动交叉校验：随编辑刷新底部状态行（无按钮，结果内联展示）
  void updateValidateStatus();
  /// 数据库刷新按钮启用态（meta.db 就绪性；构造时 config 可能为空，load 后需重算）
  void updateDbButtonStates();
  /// 多语言启用态联动（翻译表配置区整体显隐）
  void applyI18nEnabled(bool on);
  /// 面板样式（内嵌后不再经 ConfigDialogFrame，自行应用主题）
  void applyStyle();
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
  // ── 表级进阶键：联表查询声明（joinTables 新格式）──
  /// 用联表查询定义全量重建关联表格
  void fillJoins(const QVector<JsonTableJoinSpec> &joins);
  /// 追加一行空联表声明
  void appendJoinRow();
  /// 从表格收集全部联表声明（跳过全空行）
  QVector<JsonTableJoinSpec> collectJoins() const;
  // ── 字段呈现（统一表格：一行 = 一个字段，列表/查询/编辑三视图合一）──
  /// 用三视图配置全量重建统一字段表格（物理列 + 联表 ref 行）
  void fillViewTable();
  /// 追加一行联表列（ref 形如 引用名.目标列）
  void appendViewJoinRow(const QString &ref);
  /// 收集统一表格 → 三视图结构（写回 out 的 selCols/eq/like/gap/editCols）
  void collectViewTable(JsonTableTable *out) const;
  /// 字段呈现表格行上移/下移（delta=-1 上移，1 下移）
  void moveViewRow(int delta);
  /// 打开 ⚙ 字段设置面板（ViewFieldConfigDialog：取值域/列表展示/编辑表单）
  void openViewFieldConfig(int row);
  // ── 操作按钮（buttons：键名与 .jsonvue ButtonConfig 一致）──
  /// 用 m_buttons 全量重建按钮表格
  void fillButtons();
  /// 追加一行按钮摘要（label/位置/行为）
  void appendButtonRow(int idx);
  /// 添加按钮（ButtonConfigDialog 新建模式）
  void onAddButton();
  /// 编辑按钮（ButtonConfigDialog 编辑模式）
  void onEditButton(int row);
  /// 删除按钮
  void onRemoveButton();
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
  /// 主表启用 i18n 且同目录无显式翻译表 .jsontable 时，返回派生翻译表的
  /// CREATE TABLE DDL（deriveI18nTable 惯例）；否则返回空串
  QString i18nCreateDdl() const;
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
  QCheckBox *m_frontendCheck = nullptr;       ///< 生成前端页面（false = 纯 API 模块）
  QCheckBox *m_appQueryCheck = nullptr;       ///< 生成 C 端只读查询（modules/app）
  QComboBox *m_appAuthCombo = nullptr;        ///< C 端鉴权：public / user
  QCheckBox *m_genAddCheck = nullptr;          ///< 生成新增按钮（工具栏）
  QCheckBox *m_genEditCheck = nullptr;         ///< 生成行内编辑按钮
  QCheckBox *m_genDeleteCheck = nullptr;       ///< 生成删除按钮（行内删除 + 工具栏批量删除）
  QCheckBox *m_genDetailCheck = nullptr;       ///< 生成行内详情按钮
  QTableWidget *m_colsTable = nullptr;        ///< 字段表格（核心编辑区）
  QTableWidget *m_indexTable = nullptr;       ///< 索引表格
  QPlainTextEdit *m_ddlView = nullptr;        ///< DDL 预览（只读）
  QPushButton *m_importBtn = nullptr;         ///< 从数据库刷新结构按钮（DDL 页）
  QPushButton *m_refreshFieldsBtn = nullptr;  ///< 从数据库刷新字段按钮（字段页）
  QLineEdit *m_excludeEdit = nullptr;         ///< 刷新时忽略的列（逗号分隔，存 table.extra）
  // ── 操作按钮（字段呈现页签底部区块）──
  QTableWidget *m_buttonTable = nullptr;      ///< 按钮摘要表格（label/位置/行为）
  QVector<ButtonConfig> m_buttons;            ///< 按钮编辑态
  // ── 表级进阶键 ──
  QTableWidget *m_sortTable = nullptr;        ///< 默认排序表格（列名 + 方向）
  QTableWidget *m_joinTable = nullptr;        ///< 联表声明表格（joinTable）
  QTableWidget *m_viewTable = nullptr;        ///< 字段呈现统一表格（一行=一字段，三视图合一）
  QHeaderView *m_viewHeader = nullptr;        ///< 字段呈现表水平表头
  QHash<int, QString> m_headerTips;           ///< 功能列表头问号提示（列索引 → 文案，问号并入标题）
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
  QStyledItemDelegate *m_joinMainColDelegate = nullptr;  ///< 联表 on[0] 主表列下拉（候选=字段列名）
  QStyledItemDelegate *m_joinTableDelegate = nullptr;  ///< 联表被联表下拉（候选=项目表名）
  QStyledItemDelegate *m_viewRefDelegate = nullptr;   ///< 字段呈现 ref 下拉委托（字段列+联表 ref）
  QStyledItemDelegate *m_sortRefDelegate = nullptr;    ///< 排序列 ref 下拉委托（字段列+联表 ref）
  QLabel *m_validateLabel = nullptr;          ///< 交叉校验状态行（随编辑被动刷新）
  QVector<JsonTableEnum> m_enums;             ///< 表内枚举编辑态（主从表联动的数据侧）
  int m_enumCurRow = -1;                      ///< 当前编辑的枚举行（-1=无）
  bool m_enumSyncing = false;                 ///< 主从表联动中（防 currentCellChange 递归）
  bool m_loading = false;                     ///< load/refill 回填中（抑制 configChanged）
  JsonTableTable m_table;                       ///< 构造入参（extra/未编辑字段保真基底）
  JsonTableConfig m_config;                     ///< 项目配置（proj meta.db 连接信息 + 跨文件表清单）
  QString m_filePath;                           ///< 当前 .jsontable 文件路径（数据源搜索根）
};
