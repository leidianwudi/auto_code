/**
 * @file json_table_editor.h
 * @brief .jsontable 可视化编辑器面板
 *
 * 以表格形式管理 .jsontable 文件中的多张表定义：
 *   - 表格列：表名 / 模型名 / 列数 / 表注释 / 设计（按钮列）
 *   - 支持添加 / 删除表（表顺序不影响代码生成，不提供上移/下移）
 *   - 表格只读，双击数据行或点击「设计」按钮打开 JsonTableDialog
 *     编辑单张表（列定义与索引）
 */

#pragma once

#include <QWidget>

#include "json_table_model.h"

class QPushButton;
class QTableWidget;

/**
 * @class JsonTableEditor
 * @brief .jsontable 文件可视化编辑面板
 */
class JsonTableEditor : public QWidget {
  Q_OBJECT

public:
  explicit JsonTableEditor(QWidget *parent = nullptr);
  ~JsonTableEditor() override = default;

  /// 加载配置到界面
  void loadConfig(const JsonTableConfig &config);

  /// 从界面收集配置
  JsonTableConfig collectConfig() const;

  /// 缓存磁盘原始 .jsontable 内容（写回时做保真合并，避免未知字段丢失）
  void setPreservedSource(const QString &src);

  /// 以界面当前配置为主、磁盘原文为底做保真合并，返回可写回磁盘的完整 JSON 对象
  QJsonObject collectMergedObject() const;

  /// 主题/颜色变化后重新应用样式表
  void reloadStyle();

signals:
  /// 配置发生变化时发射
  void configChanged();

private slots:
  /// 添加表
  void onAddTable();
  /// 删除表
  void onRemoveTable();
  /// 编辑选中表（设计按钮）
  void onEditTable();

private:
  /// 构建界面
  void setupUI();
  /// 应用样式
  void applyStyle();
  /// 创建「设计」按钮并连接到 onEditTable（用于表格行）
  QPushButton *makeDesignButton();
  /// 将一张表追加为表格新行
  void appendTableRow(const JsonTableTable &t);
  /// 刷新指定行的显示文本（表名/模型名/列数/表注释）
  void refreshRow(int row);

  QTableWidget *m_table = nullptr;   ///< 表定义列表表格
  QPushButton *m_addBtn = nullptr;
  QPushButton *m_removeBtn = nullptr;

  QVector<JsonTableTable> m_tables;  ///< 表数据（表格编辑态）

  // ── 状态 ──
  QJsonObject m_preserved;  ///< 磁盘原文（保真合并底）
  bool m_loading = false;   ///< 加载时抑制 configChanged
};

/// 表定义表格列索引
enum JsonTableTableCols {
  JATableName = 0,  ///< 表名
  JAModelName,      ///< 模型名
  JAColNum,         ///< 列数
  JATableComment,   ///< 表注释
  JADesign,         ///< 设计（按钮列）
  JAColCount        ///< 列总数
};
