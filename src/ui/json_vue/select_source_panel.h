/**
 * @file select_source_panel.h
 * @brief 动态数据源配置面板（可复用）
 *
 * 封装「下拉框动态数据源」的完整配置 UI：
 *   - 请求方式 + URL + 测试按钮（发 HTTP 请求提取返回字段）
 *   - 加载方式（普通 / 查询分页）+ 分页参数
 *   - 返回数据预览表格 + Label/Value 字段选择
 *
 * 被 ComboboxConfigDialog（jsonvue 下拉框配置）与 JsonSourceDialog
 * （.jsonsource 动态数据源编辑）复用，保证两处行为一致。
 */

#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

/**
 * @class SelectSourcePanel
 * @brief 动态数据源配置面板
 */
class SelectSourcePanel : public QWidget {
  Q_OBJECT

public:
  explicit SelectSourcePanel(QWidget *parent = nullptr);

  /// 设置初始配置
  void setData(const QString &url, const QString &method, const QString &valueField,
               const QString &labelField, bool paged, const QString &pageKey,
               const QString &pageSizeKey, int pageSize, const QString &searchTitle,
               const QString &searchField);

  /// 设置 HTTP 请求参数（baseUrl、authHeader、postData）
  void setHttpConfig(const QString &baseUrl, const QString &authHeader, const QString &postData);

  /// 锁定基础配置（请求URL/请求方式/加载方式/分页参数）：
  /// 引用 .jsonsource 数据源时锁定，值完全跟随数据源不可修改；测试按钮保持可用
  /// （用于查看返回示例、选择 Label/Value 字段）
  void setBaseLocked(bool locked);

  /// 锁定 Label/Value 字段下拉框（"全部使用数据源"时显示文本/实际值也不可改）
  void setFieldsLocked(bool locked);

  // ── 读取配置 ──
  QString url() const;
  QString method() const;
  QString valueField() const;
  QString labelField() const;
  bool paged() const;
  QString pageKey() const;
  QString pageSizeKey() const;
  int pageSize() const;
  QString searchTitle() const;
  QString searchField() const;

  /// 便捷：把当前配置写入 ColumnConfig / QueryFieldConfig / DialogFieldConfig 的 select 字段
  void fillSelectFields(QString *url, QString *valueField, QString *labelField, bool *paged,
                        QString *pageKey, QString *pageSizeKey, int *pageSize,
                        QString *searchTitle, QString *searchField, QString *method) const;

signals:
  /// 配置内容变化
  void configChanged();

private slots:
  void onTest();
  void onHttpFinished(const QJsonDocument &doc);
  void onHttpError(const QString &errorMsg);

private:
  void setupUI();
  void applyType(int index);

  // ── 控件 ──
  QComboBox *m_methodCombo = nullptr;   ///< 请求方式（GET/POST）
  QLineEdit *m_urlEdit = nullptr;       ///< 请求 URL
  QPushButton *m_testBtn = nullptr;     ///< 测试按钮
  QComboBox *m_typeCombo = nullptr;     ///< 加载方式（0=普通，1=查询分页）
  QWidget *m_pagedGroup = nullptr;      ///< 查询分页配置区域
  QLineEdit *m_pageKeyEdit = nullptr;   ///< 页码参数名
  QLineEdit *m_pageSizeKeyEdit = nullptr;  ///< 页大小参数名
  QSpinBox *m_pageSizeSpin = nullptr;   ///< 默认页大小
  QLineEdit *m_searchTitleEdit = nullptr; ///< 查询标题
  QLineEdit *m_searchFieldEdit = nullptr; ///< 字段名（搜索参数 key）
  QLabel *m_statusLabel = nullptr;      ///< 状态提示标签
  QTableWidget *m_previewTable = nullptr; ///< 返回数据预览表格
  QComboBox *m_valueCombo = nullptr;    ///< Value 字段下拉框
  QComboBox *m_labelCombo = nullptr;    ///< Label 字段下拉框

  // ── HTTP 配置 ──
  QString m_baseUrl;
  QString m_authHeader;
  QString m_postData;
};
