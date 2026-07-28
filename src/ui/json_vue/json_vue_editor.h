/**
 * @file json_vue_editor.h
 * @brief .jsonvue 可视化编辑器面板
 *
 * 替换编辑器面板内容，提供图形化配置界面。
 * 包含：
 *   - 接口配置区（生成数据URL、查询/删除/修改接口）
 *   - 列配置表格（HTTP 返回列名 + 查询/编辑界面设置）
 *   - 查询字段配置区（动态添加查询列）
 *
 * 编辑结果通过 configChanged 信号通知外部，由外部负责写回文件。
 */

#pragma once

#include <QWidget>

#include "json_vue_model.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QSpinBox;

/**
 * @class JsonVueEditor
 * @brief .jsonvue 文件可视化编辑面板
 */
class JsonVueEditor : public QWidget {
  Q_OBJECT

public:
  explicit JsonVueEditor(QWidget *parent = nullptr);
  ~JsonVueEditor() override = default;

  /// 加载配置到界面
  void loadConfig(const JsonVueConfig &config);

  /// 从界面收集配置
  JsonVueConfig collectConfig() const;

  /// 设置 baseUrl（用于 HTTP 请求拼接）
  void setBaseUrl(const QString &baseUrl) { m_baseUrl = baseUrl; }

  /// 从 AC 脚本文件加载 HTTP 配置（baseUrl、authHeader、postData）
  void loadHttpConfigFromAcFile(const QString &acFilePath);

signals:
  /// 配置发生变化时发射
  void configChanged();

private slots:
  /// 点击"生成"按钮，发起 HTTP 请求获取列名
  void onGenerate();
  /// HTTP 请求完成
  void onHttpFinished(const QString &url, const class QJsonDocument &doc);
  /// HTTP 请求失败
  void onHttpError(const QString &url, const QString &errorMsg);

  /// 列表：上移选中行
  void onMoveUp();
  /// 列表：下移选中行
  void onMoveDown();
  /// 列表：添加列
  void onAddColumn();
  /// 列表：删除列
  void onRemoveColumn();

  /// 查询设置：添加查询字段
  void onAddQueryField();
  /// 查询设置：删除查询字段
  void onRemoveQueryField();

private:
  /// 构建界面
  void setupUI();
  /// 构建接口配置区
  QWidget *buildMetaSection();
  /// 构建列配置表格区
  QWidget *buildColumnsSection();
  /// 构建查询字段区
  QWidget *buildQueryFieldsSection();
  /// 应用样式
  void applyStyle();

  /// 从 HTTP 返回数据中提取列名，填充到列表
  void populateColumnsFromHttp(const QJsonDocument &doc);

  /// 获取列配置表格中的数据列名列表（供查询字段下拉框使用）
  QStringList columnDataNames() const;

  /// 刷新查询字段下拉框选项
  void refreshQueryFieldDataNames();

  // ── 接口配置区控件 ──
  QComboBox *m_methodCombo = nullptr;    ///< 生成数据 URL 的 HTTP 方法
  QLineEdit *m_dataUrlEdit = nullptr;    ///< 生成数据 URL
  QPushButton *m_generateBtn = nullptr;  ///< 生成按钮
  QLineEdit *m_queryApiEdit = nullptr;   ///< 查询接口
  QLineEdit *m_deleteApiEdit = nullptr;  ///< 删除接口
  QCheckBox *m_noDeleteCheck = nullptr;  ///< 不可删除
  QLineEdit *m_updateApiEdit = nullptr;  ///< 修改接口
  QCheckBox *m_noEditCheck = nullptr;    ///< 不可编辑

  // ── 列配置表格 ──
  QTableWidget *m_columnTable = nullptr;  ///< 列配置表格
  QPushButton *m_moveUpBtn = nullptr;     ///< 上移
  QPushButton *m_moveDownBtn = nullptr;   ///< 下移
  QPushButton *m_addColBtn = nullptr;     ///< 添加列
  QPushButton *m_removeColBtn = nullptr;  ///< 删除列

  // ── 查询字段表格 ──
  QTableWidget *m_queryTable = nullptr;     ///< 查询字段表格
  QPushButton *m_addQueryBtn = nullptr;     ///< 添加查询字段
  QPushButton *m_removeQueryBtn = nullptr;  ///< 删除查询字段

  // ── 状态数据 ──
  QString m_baseUrl;       ///< AC 脚本传来的 baseUrl，用于 HTTP 请求拼接
  QString m_authHeader;    ///< Authorization 请求头值（如 "Bearer xxx"）
  QString m_postData;      ///< POST 请求的默认数据（JSON 字符串）
  bool m_loading = false;  ///< 加载配置时抑制 configChanged 信号

  /// 连接静态输入控件的编辑信号到 configChanged
  void connectStaticControlSignals();
  /// 连接单个 cell widget 的编辑信号到 configChanged
  void connectCellWidgetSignals(QWidget *widget);
  /// 设置查询样式与开关可编辑的联动（查询样式为 text 时禁用开关可编辑）
  void setupQueryStyleLinkage(int row);
};

/// 列配置表格列索引
enum ColumnTableCols {
  ColDataName = 0,    ///< 数据列名
  ColQueryVisible,    ///< 查询界面是否显示
  ColQueryName,       ///< 查询界面列名
  ColQueryStyle,      ///< 查询界面样式
  ColSwitchEditable,  ///< 开关样式时是否可编辑
  ColEditVisible,     ///< 编辑界面是否显示
  ColEditName,        ///< 编辑界面列名
  ColEditStyle,       ///< 编辑界面样式
  ColEditEditable,    ///< 编辑界面是否可编辑
  ColCount
};

/// 查询字段表格列索引
enum QueryTableCols {
  QColDisplayName = 0,  ///< 显示的列名
  QColDataName,         ///< 数据列名（下拉框）
  QColInputStyle,       ///< 输入框样式
  QColRelation,         ///< 查询关系
  QColCount
};
