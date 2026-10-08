/**
 * @file json_table_dialog.h
 * @brief .jsontable 单张表设计器对话框
 *
 * 编辑一张 JsonTableTable：表名/模型名/注释/manualUpdateTime + 字段表格
 * （列名/MySQL类型/unsigned/可空/主键/自增/默认值/注释/列表/搜索/表单角色/必填）
 * + 索引表格。支持从数据库反向导入表结构（合并保留 .jsontable 已有列的
 * 前端角色），DDL 预览与执行建表（FunDb::constructor/exec/destructor 直连）。
 * 通过构造传入待编辑表，resultTable() 取回编辑结果（getter 模式，无信号回写）。
 */

#pragma once

#include <QDialog>

#include "json_table_model.h"

class QCheckBox;
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
  JsonTableTable m_table;                       ///< 构造入参（extra/未编辑字段保真基底）
  JsonTableConfig m_config;                     ///< 完整配置（meta.db 连接信息）
};
