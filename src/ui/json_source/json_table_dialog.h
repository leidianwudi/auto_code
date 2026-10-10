/**
 * @file json_table_dialog.h
 * @brief .jsontable 单张表设计器对话框
 *
 * 编辑一张 JsonTableTable：表名/模型名/注释/manualUpdateTime + 字段表格
 * （列名/MySQL类型/unsigned/可空/主键/自增/默认值/注释/列表/搜索/表单角色/必填）
 * + 索引表格 + 表级进阶键（查询配置：范围/模糊列与默认排序 + 表内枚举主从表
 * + 全局枚举列 + 多语言翻译表配置）。支持从数据库反向导入表结构（合并保留
 * .jsontable 已有列的前端角色），DDL 预览与执行建表。
 * 通过构造传入待编辑表，resultTable() 取回编辑结果（getter 模式，无信号回写）。
 */

#pragma once

#include <QDialog>

#include "json_table_model.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
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
  QLineEdit *m_selGapEdit = nullptr;          ///< 范围查询列（逗号分隔）
  QLineEdit *m_selLikeEdit = nullptr;         ///< 模糊查询列（逗号分隔）
  QTableWidget *m_sortTable = nullptr;        ///< 默认排序表格（列名 + 方向）
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
  QVector<JsonTableEnum> m_enums;             ///< 表内枚举编辑态（主从表联动的数据侧）
  int m_enumCurRow = -1;                      ///< 当前编辑的枚举行（-1=无）
  bool m_enumSyncing = false;                 ///< 主从表联动中（防 currentCellChange 递归）
  JsonTableTable m_table;                       ///< 构造入参（extra/未编辑字段保真基底）
  JsonTableConfig m_config;                     ///< 完整配置（meta.db 连接信息）
};
