/**
 * @file button_config_dialog.h
 * @brief 自定义操作按钮配置对话框
 *
 * 配置自定义按钮的外观（文字/图标/位置/样式）和行为（Ajax/Confirm/Dialog/Link）。
 * Dialog 行为支持管理对话框表单字段列表（fieldName/label/editStyle/required）。
 */

#pragma once

#include <QDialog>

#include "json_vue_model.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

/**
 * @class ButtonConfigDialog
 * @brief 自定义操作按钮配置对话框
 */
class ButtonConfigDialog : public QDialog {
  Q_OBJECT

public:
  /// 构造为新建模式
  explicit ButtonConfigDialog(QWidget *parent = nullptr);
  /// 构造为编辑模式（加载已有配置）
  explicit ButtonConfigDialog(const ButtonConfig &config, QWidget *parent = nullptr);
  ~ButtonConfigDialog() override = default;

  /// 获取配置数据
  ButtonConfig getData() const;
  /// 设置配置数据
  void setData(const ButtonConfig &config);

private:
  /// 构建界面
  void setupUI();
  /// 根据行为类型更新配置区可见性
  void updateBehaviorVisibility();
  /// 从 label 自动推导 actionKey（首字母小写驼峰）
  static QString deriveActionKey(const QString &label);
  /// 设置图标名（更新按钮显示并异步加载真实图标）
  void setIconName(const QString &iconName);

  // ── dialogFields 表格操作 ──
  void onAddDialogField();
  void onRemoveDialogField(int row);
  void onConfigureFieldStyle(int row);
  /// 从表格收集 dialogFields 数据
  QVector<DialogFieldConfig> collectDialogFields() const;
  /// 将 dialogFields 数据填充到表格
  void fillDialogFields(const QVector<DialogFieldConfig> &fields);

  // ── 基本信息控件 ──
  QLineEdit *m_labelEdit = nullptr;
  QLineEdit *m_actionKeyEdit = nullptr;
  QPushButton *m_iconBtn = nullptr;       ///< 图标预览按钮（点击弹出选择器）
  QPushButton *m_iconClearBtn = nullptr;  ///< 清除图标
  QString m_currentIconName;              ///< 当前图标名（空=无图标）
  QComboBox *m_positionCombo = nullptr;
  QComboBox *m_buttonTypeCombo = nullptr;
  QComboBox *m_actionTypeCombo = nullptr;

  // ── 行为配置区容器（整体显隐，避免布局错乱）──
  QWidget *m_ajaxWidget = nullptr;
  QWidget *m_confirmWidget = nullptr;
  QWidget *m_dialogWidget = nullptr;
  QWidget *m_linkWidget = nullptr;

  // ── Ajax / Confirm 行为控件 ──
  QLineEdit *m_apiNameEdit = nullptr;
  QLineEdit *m_confirmTextEdit = nullptr;
  QLineEdit *m_confirmApiEdit = nullptr;  ///< Confirm 行为专用 API 函数名

  // ── Dialog 行为控件 ──
  QLineEdit *m_dialogTitleEdit = nullptr;
  QLineEdit *m_dialogApiEdit = nullptr;
  QTableWidget *m_dialogFieldsTable = nullptr;
  QPushButton *m_addFieldBtn = nullptr;

  // ── Link 行为控件 ──
  QLineEdit *m_linkPathEdit = nullptr;

  /// 临时存储 dialogFields 的样式特定配置（表格只管理基本属性）
  QVector<DialogFieldConfig> m_dialogFieldsData;
};
