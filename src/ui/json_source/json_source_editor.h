/**
 * @file json_source_editor.h
 * @brief .jsonsource 可视化编辑器面板
 *
 * 以表格形式管理 .jsonsource 文件中的多条数据源：
 *   - 表格列：说明 / 类型 / URL / 配置（摘要）
 *   - 支持添加 / 删除 / 上移 / 下移数据源
 *   - 表格只读，双击数据行或点击「配置」按钮打开 JsonSourceDialog
 *     编辑单条数据源（静态/动态）
 */

#pragma once

#include <QWidget>

#include "json_source_model.h"

class QPushButton;
class QTableWidget;

/**
 * @class JsonSourceEditor
 * @brief .jsonsource 文件可视化编辑面板
 */
class JsonSourceEditor : public QWidget {
  Q_OBJECT

public:
  explicit JsonSourceEditor(QWidget *parent = nullptr);
  ~JsonSourceEditor() override = default;

  /// 加载配置到界面
  void loadConfig(const JsonSourceConfig &config);

  /// 从界面收集配置
  JsonSourceConfig collectConfig() const;

  /// 缓存磁盘原始 .jsonsource 内容（写回时做保真合并，避免未知字段丢失）
  void setPreservedSource(const QString &src);

  /// 以界面当前配置为主、磁盘原文为底做保真合并，返回可写回磁盘的完整 JSON 对象
  QJsonObject collectMergedObject() const;

  /// 设置 HTTP 请求参数（动态数据源"测试"按钮使用）
  void setHttpConfig(const QString &baseUrl, const QString &authHeader, const QString &postData);

  /// 主题/颜色变化后重新应用样式表
  void reloadStyle();

signals:
  /// 配置发生变化时发射
  void configChanged();

private slots:
  /// 添加数据源
  void onAddSource();
  /// 删除数据源
  void onRemoveSource();
  /// 上移选中数据源
  void onMoveUp();
  /// 下移选中数据源
  void onMoveDown();
  /// 编辑选中数据源（配置按钮）
  void onEditSource();

private:
  /// 构建界面
  void setupUI();
  /// 应用样式
  void applyStyle();
  /// 创建「配置」按钮并连接到 onEditSource（用于表格行）
  QPushButton *makeConfigButton();
  /// 刷新指定行的配置摘要文本
  void refreshSummary(int row);

  QTableWidget *m_table = nullptr;   ///< 数据源列表表格
  QPushButton *m_addBtn = nullptr;
  QPushButton *m_removeBtn = nullptr;
  QPushButton *m_moveUpBtn = nullptr;
  QPushButton *m_moveDownBtn = nullptr;

  QVector<JsonSource> m_sources;  ///< 数据源数据（表格编辑态）

  // ── 状态 ──
  QString m_baseUrl;
  QString m_authHeader;
  QString m_postData;
  QJsonObject m_preserved;  ///< 磁盘原文（保真合并底）
  bool m_loading = false;   ///< 加载时抑制 configChanged
};

/// 数据源表格列索引
enum JsonSourceTableCols {
  JDColRemark = 0,  ///< 说明
  JDColType,        ///< 类型
  JDColUrl,         ///< URL
  JDColConfig,      ///< 配置摘要
  JDColCount
};
