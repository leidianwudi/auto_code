/**
 * @file json_global_enum_editor.h
 * @brief .jsonglobalenum 可视化编辑器面板
 *
 * 以表格形式管理 .jsonglobalenum 文件中的多条全局枚举：
 *   - 表格列：名称 / 说明 / 选项摘要
 *   - 支持添加 / 删除 / 上移 / 下移枚举
 *   - 表格只读，双击数据行或点击「配置」按钮打开 JsonGlobalEnumDialog
 *     编辑单条枚举（名称 / 选项列表）
 */

#pragma once

#include <QWidget>

#include "json_global_enum_model.h"

class QPushButton;
class QTableWidget;

/**
 * @class JsonGlobalEnumEditor
 * @brief .jsonglobalenum 文件可视化编辑面板
 */
class JsonGlobalEnumEditor : public QWidget {
  Q_OBJECT

public:
  explicit JsonGlobalEnumEditor(QWidget *parent = nullptr);
  ~JsonGlobalEnumEditor() override = default;

  /// 加载配置到界面
  void loadConfig(const JsonGlobalEnumConfig &config);

  /// 从界面收集配置
  JsonGlobalEnumConfig collectConfig() const;

  /// 缓存磁盘原始 .jsonglobalenum 内容（写回时做保真合并，避免未知字段丢失）
  void setPreservedSource(const QString &src);

  /// 以界面当前配置为主、磁盘原文为底做保真合并，返回可写回磁盘的完整 JSON 对象
  QJsonObject collectMergedObject() const;

  /// 主题/颜色变化后重新应用样式表
  void reloadStyle();

signals:
  /// 配置发生变化时发射
  void configChanged();

private slots:
  /// 添加枚举
  void onAddEnum();
  /// 删除枚举
  void onRemoveEnum();
  /// 上移选中枚举
  void onMoveUp();
  /// 下移选中枚举
  void onMoveDown();
  /// 编辑选中枚举（配置按钮）
  void onEditEnum();

private:
  /// 构建界面
  void setupUI();
  /// 应用样式
  void applyStyle();
  /// 创建「配置」按钮并连接到 onEditEnum（用于表格行）
  QPushButton *makeConfigButton();
  /// 刷新指定行的选项摘要文本
  void refreshSummary(int row);

  QTableWidget *m_table = nullptr;  ///< 枚举列表表格
  QPushButton *m_addBtn = nullptr;
  QPushButton *m_removeBtn = nullptr;
  QPushButton *m_moveUpBtn = nullptr;
  QPushButton *m_moveDownBtn = nullptr;

  QVector<JsonGlobalEnum> m_enums;  ///< 枚举数据（表格编辑态）

  // ── 状态 ──
  QJsonObject m_preserved;  ///< 磁盘原文（保真合并底）
  bool m_loading = false;   ///< 加载时抑制 configChanged
};

/// 枚举表格列索引
enum JsonGlobalEnumTableCols {
  GEColName = 0,  ///< 名称
  GEColRemark,    ///< 说明
  GEColConfig,    ///< 选项摘要（配置按钮）
  GEColCount
};
