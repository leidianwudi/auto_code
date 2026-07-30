/**
 * @file style_config_dialog.h
 * @brief 字段样式配置对话框
 *
 * 统一配置表格列显示和编辑表单的所有样式。
 * 显示类型（含开关）和编辑样式在此对话框内选择，子控件根据选择动态展示。
 */

#pragma once

#include <QDialog>

#include "json_vue_model.h"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLineEdit;
class QLabel;
class QPushButton;
class QTableWidget;
class QVBoxLayout;
class QWidget;

/**
 * @class StyleConfigDialog
 * @brief 字段样式配置对话框
 *
 * 支持两种模式：
 *   - 列配置模式：配置表格列显示 + 编辑表单 + 通用配置
 *   - 查询字段模式：配置查询输入样式
 */
class StyleConfigDialog : public QDialog {
  Q_OBJECT

public:
  /// 构造为列配置模式
  explicit StyleConfigDialog(EditStyle style, QWidget *parent = nullptr);
  /// 构造为查询字段模式
  explicit StyleConfigDialog(QueryInputStyle style, QWidget *parent = nullptr);

  ~StyleConfigDialog() override;

protected:
  /// 确认时验证数据（检查 tagItems 重复/空值等）
  void accept() override;

public:
  // ── 编辑样式（从列配置表格移入）──
  void setEditStyle(EditStyle style);
  EditStyle editStyle() const;

  // ── 编辑可编辑 ──
  void setEditEditable(bool v);
  bool editEditable() const;

  // ── 开关可编辑（displayType 为 boolean/tag 且 switchEditable 时生效）──
  void setSwitchEditable(bool v);
  bool switchEditable() const;

  // ── 下拉框数据源（editStyle == Select 时使用，由对话框内部按钮配置）──
  void setSelectUrl(const QString &v);
  QString selectUrl() const;
  void setSelectValueField(const QString &v);
  QString selectValueField() const;
  void setSelectLabelField(const QString &v);
  QString selectLabelField() const;
  /// 设置 HTTP 配置（供下拉框数据源测试按钮使用）
  void setHttpConfig(const QString &baseUrl, const QString &authHeader, const QString &postData);

  // ── 编辑样式子配置 ──
  void setPlaceholder(const QString &v);
  QString placeholder() const;

  void setMaxlength(int v);
  int maxlength() const;

  void setMinValue(double v);
  double minValue() const;

  void setMaxValue(double v);
  double maxValue() const;

  void setPrecision(int v);
  int precision() const;

  void setDateFormat(const QString &v);
  QString dateFormat() const;

  void setTextareaRows(int v);
  int textareaRows() const;

  // ── 通用配置（仅列配置模式）──
  void setRequired(bool v);
  bool required() const;

  void setColumnWidth(int v);
  int columnWidth() const;

  void setColumnFixed(const QString &v);
  QString columnFixed() const;

  void setFormatter(const QString &v);
  QString formatter() const;

  void setFormSpan(int v);
  int formSpan() const;

  // ── 表格列显示样式 ──
  void setDisplayType(const QString &v);
  QString displayType() const;

  /// 设置标签映射数组（displayType == "tag" 时使用）
  void setTagItems(const QList<TagItem> &items);
  /// 获取标签映射数组
  QList<TagItem> tagItems() const;

  void setBoolTrueText(const QString &v);
  QString boolTrueText() const;

  void setBoolFalseText(const QString &v);
  QString boolFalseText() const;

  // ── 通用配置 ──
  void setDefaultValue(const QString &v);
  QString defaultValue() const;

  void setDefaultSort(const QString &v);
  QString defaultSort() const;

private:
  /// 构建界面
  void setupUI();
  /// 添加一行标签+控件
  void addRow(const QString &labelText, QWidget *widget);
  /// 重建显示样式子控件
  void rebuildDisplayTypeControls();
  /// 重建编辑样式子控件
  void rebuildEditStyleControls();
  /// 从 tagItems 表格收集数据
  QList<TagItem> collectTagItems() const;
  /// 用 tagItems 填充表格
  void populateTagItems(const QList<TagItem> &items);
  /// 验证 tagItems 数据（检查空值和重复 value）
  bool validateTagItems(QString *error) const;

  EditStyle m_editStyle = EditStyle::Text;
  QueryInputStyle m_queryStyle = QueryInputStyle::Text;
  bool m_isColumnMode = true;  ///< true=列配置模式, false=查询字段模式

  QFormLayout *m_formLayout = nullptr;         ///< 主表单布局
  QVBoxLayout *m_displayTypeLayout = nullptr;  ///< 显示样式子控件容器布局
  QVBoxLayout *m_editStyleLayout = nullptr;    ///< 编辑样式子控件容器布局
  QWidget *m_displayTypeWidget = nullptr;      ///< 显示样式子控件容器
  QWidget *m_editStyleWidget = nullptr;        ///< 编辑样式子控件容器

  // ── 列配置模式：显示样式 ──
  QComboBox *m_displayTypeCombo = nullptr;

  // ── 列配置模式：编辑样式 ──
  QComboBox *m_editStyleCombo = nullptr;
  QCheckBox *m_editEditableCheck = nullptr;
  QCheckBox *m_switchEditableCheck = nullptr;

  // ── 编辑样式子控件（按需创建）──
  QLineEdit *m_placeholderEdit = nullptr;
  QComboBox *m_maxlengthCombo = nullptr;
  QComboBox *m_minValueCombo = nullptr;
  QComboBox *m_maxValueCombo = nullptr;
  QComboBox *m_precisionCombo = nullptr;
  QComboBox *m_dateFormatCombo = nullptr;
  QComboBox *m_textareaRowsCombo = nullptr;
  // 下拉框数据源配置（editStyle == Select 时显示"数据源"按钮）
  QPushButton *m_selectSourceBtn = nullptr;

  // ── 显示样式子控件（按需创建）──
  QTableWidget *m_tagItemsTable = nullptr;  ///< tag 标签映射表（动态增删行）
  QLineEdit *m_boolTrueTextEdit = nullptr;
  QLineEdit *m_boolFalseTextEdit = nullptr;

  // ── 通用配置控件（仅列配置模式）──
  QCheckBox *m_requiredCheck = nullptr;
  QComboBox *m_columnWidthCombo = nullptr;
  QComboBox *m_columnFixedCombo = nullptr;
  QComboBox *m_formatterCombo = nullptr;
  QComboBox *m_formSpanCombo = nullptr;

  // ── 通用配置 ──
  QLineEdit *m_defaultValueEdit = nullptr;
  QComboBox *m_defaultSortCombo = nullptr;

  /// 缓存当前值（重建控件时用于恢复）
  QString m_cachedPlaceholder;
  int m_cachedMaxlength = 0;
  double m_cachedMinValue = 0;
  double m_cachedMaxValue = 0;
  int m_cachedPrecision = 2;
  QString m_cachedDateFormat;
  int m_cachedTextareaRows = 3;
  QList<TagItem> m_cachedTagItems;  ///< tag 标签映射缓存
  QString m_cachedBoolTrueText;
  QString m_cachedBoolFalseText;
  bool m_cachedSwitchEditable = true;
  // 下拉框数据源缓存
  QString m_cachedSelectUrl;
  QString m_cachedSelectValueField;
  QString m_cachedSelectLabelField;
  // HTTP 配置（供下拉框数据源测试按钮使用）
  QString m_baseUrl;
  QString m_authHeader;
  QString m_postData;
};
