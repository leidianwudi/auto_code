/**
 * @file style_config_dialog.h
 * @brief 字段样式配置对话框
 *
 * 根据编辑样式（text/int/float/date/textarea）或查询输入样式（text/date），
 * 显示不同的配置字段。select 样式不在此对话框处理（由 ComboboxConfigDialog 处理）。
 */

#pragma once

#include <QDialog>

#include "json_vue_model.h"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLineEdit;
class QLabel;

/**
 * @class StyleConfigDialog
 * @brief 字段样式配置对话框
 *
 * 支持两种模式：
 *   - 列配置模式：根据 EditStyle 显示 text/int/float/date/textarea 的配置
 *   - 查询字段模式：根据 QueryInputStyle 显示 text/date 的配置
 */
class StyleConfigDialog : public QDialog {
  Q_OBJECT

public:
  /// 构造为列配置模式
  explicit StyleConfigDialog(EditStyle style, QWidget *parent = nullptr);
  /// 构造为查询字段模式
  explicit StyleConfigDialog(QueryInputStyle style, QWidget *parent = nullptr);

  ~StyleConfigDialog() override = default;

  // ── 配置值读写 ──
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

  // ── 通用配置（3-1~3-4，仅列配置模式）──
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

private:
  /// 构建界面
  void setupUI();
  /// 添加一行标签+控件
  void addRow(const QString &labelText, QWidget *widget);

  EditStyle m_editStyle = EditStyle::Text;
  QueryInputStyle m_queryStyle = QueryInputStyle::Text;
  bool m_isColumnMode = true;  ///< true=列配置模式, false=查询字段模式

  QFormLayout *m_formLayout = nullptr;  ///< 表单布局

  // ── 控件（仅创建需要的那几个）──
  QLineEdit *m_placeholderEdit = nullptr;
  QComboBox *m_maxlengthCombo = nullptr;
  QComboBox *m_minValueCombo = nullptr;
  QComboBox *m_maxValueCombo = nullptr;
  QComboBox *m_precisionCombo = nullptr;
  QComboBox *m_dateFormatCombo = nullptr;
  QComboBox *m_textareaRowsCombo = nullptr;

  // ── 通用配置控件（仅列配置模式）──
  QCheckBox *m_requiredCheck = nullptr;     ///< 3-1: 是否必填
  QComboBox *m_columnWidthCombo = nullptr;  ///< 3-2: 表格列宽
  QComboBox *m_columnFixedCombo = nullptr;  ///< 3-2: 固定列
  QComboBox *m_formatterCombo = nullptr;    ///< 3-3: 格式化类型
  QComboBox *m_formSpanCombo = nullptr;     ///< 3-4: 表单布局占比
};
