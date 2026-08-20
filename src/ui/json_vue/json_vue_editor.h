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

  /// 主题/颜色变化后重新应用样式表（深色主题下保证表格行线/标签/文字可读）
  void reloadStyle();

  /// 事件过滤器：拦截列配置表格的空格键，避免第一次空格被输入到编辑框
  bool eventFilter(QObject *obj, QEvent *ev) override;

  /// 从 AC 脚本文件加载 HTTP 配置（baseUrl、authHeader、postData）
  void loadHttpConfigFromAcFile(const QString &acFilePath);

  /// 从 jsonvue 文件所在目录向上查找最近的 api_auth_data.ac，返回其完整路径；找不到返回空
  static QString findNearestApiAuthDataAc(const QString &jsonvueFilePath);

signals:
  /// 配置发生变化时发射
  void configChanged();

private slots:
  /// 点击"生成"按钮，发起 HTTP 请求获取列名
  void onGenerate();
  /// HTTP 请求完成（仅本编辑器发起的请求触发）
  void onHttpFinished(const class QJsonDocument &doc);
  /// HTTP 请求失败（仅本编辑器发起的请求触发）
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

  /// 按钮配置：添加按钮
  void onAddButton();
  /// 按钮配置：编辑按钮
  void onEditButton(int row);
  /// 按钮配置：删除按钮
  void onRemoveButton();

  /// 列配置：点击⚙按钮配置下拉框数据源
  void onConfigureCombobox();
  /// 查询设置：点击⚙按钮配置下拉框数据源
  void onConfigureQuerySelect();

private:
  /// 构建界面
  void setupUI();
  /// 构建接口配置区
  QWidget *buildMetaSection();
  /// 构建列配置表格区
  QWidget *buildColumnsSection();
  /// 构建查询字段区
  QWidget *buildQueryFieldsSection();
  /// 构建操作按钮区
  QWidget *buildButtonsSection();
  /// 应用样式
  void applyStyle();

  /// 从 HTTP 返回数据中提取列名，填充到列表
  /// @return 新增列的数量；-1 表示解析/校验失败（错误提示已弹出）
  int populateColumnsFromHttp(const QJsonDocument &doc);

  /// 获取列配置表格中的数据列名列表（供查询字段下拉框使用）
  QStringList columnDataNames() const;

  /// 刷新查询字段下拉框选项
  void refreshQueryFieldDataNames();

  /// 显示配置按钮的右键菜单（复制配置 / 粘贴配置）
  void showColumnConfigMenu(int row, const QPoint &globalPos);

  // ── 接口配置区控件 ──
  QLineEdit *m_descEdit = nullptr;       ///< 脚本说明
  QComboBox *m_methodCombo = nullptr;    ///< 生成数据 URL 的 HTTP 方法
  QLineEdit *m_dataUrlEdit = nullptr;    ///< 生成数据 URL
  QPushButton *m_generateBtn = nullptr;  ///< 生成按钮
  QCheckBox *m_noEditCheck = nullptr;    ///< 不可编辑（修改接口）
  QCheckBox *m_noDeleteCheck = nullptr;  ///< 不可删除（删除接口）
  QCheckBox *m_noDetailCheck = nullptr;  ///< 不可查看详情（详情接口）

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

  // ── 操作按钮表格 ──
  QTableWidget *m_buttonTable = nullptr;     ///< 操作按钮表格
  QPushButton *m_addButtonBtn = nullptr;     ///< 添加按钮
  QPushButton *m_removeButtonBtn = nullptr;  ///< 删除按钮
  QVector<ButtonConfig> m_buttons;           ///< 按钮配置数据

  // ── 状态数据 ──
  QString m_baseUrl;           ///< AC 脚本传来的 baseUrl，用于 HTTP 请求拼接
  QString m_authHeader;        ///< Authorization 请求头值（如 "Bearer xxx"）
  QString m_postData;          ///< POST 请求的默认数据（JSON 字符串）
  QString m_acConfigFilePath;  ///< 最近一次加载 HTTP 配置的 AC 文件路径（用于点击"生成"时重读）
  bool m_loading = false;      ///< 加载配置时抑制 configChanged 信号

  /// 最近加载的 meta（用于保留界面不再编辑的接口名，避免保存时丢失）
  JsonVueMeta m_loadedMeta;

  /// 已复制的列配置（用于"粘贴配置"到其它行）
  ColumnConfig m_copiedColumnConfig;
  bool m_hasCopiedConfig = false;  ///< 是否存在已复制的列配置

  /// 连接静态输入控件的编辑信号到 configChanged
  void connectStaticControlSignals();
  /// 连接单个 cell widget 的编辑信号到 configChanged
  void connectCellWidgetSignals(QWidget *widget);
};

/// 列配置表格列索引
enum ColumnTableCols {
  ColDataName = 0,  ///< 字段名
  ColTitle,         ///< 标题（列表页列标题与编辑页标签共用）
  ColQueryVisible,  ///< 列表页显示
  ColEditVisible,   ///< 编辑页显示
  ColConfig,        ///< 样式配置按钮（⚙，含显示类型/编辑样式/通用配置）
  ColCount
};

/// 查询字段表格列索引
enum QueryTableCols {
  QColDataName = 0,  ///< 字段名（下拉框）
  QColDisplayName,   ///< 标签名
  QColInputStyle,    ///< 输入框样式
  QColRelation,      ///< 查询关系
  QColConfig,        ///< 下拉框配置按钮（⚙）
  QColCount
};

/// 操作按钮表格列索引
enum ButtonTableCols {
  BColLabel = 0,   ///< 按钮文字
  BColActionKey,   ///< 动作标识
  BColPosition,    ///< 位置
  BColActionType,  ///< 行为类型
  BColConfig,      ///< 配置按钮（⚙ + 摘要文本，双击打开配置）
  BColCount
};
