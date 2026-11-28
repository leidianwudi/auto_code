/**
 * @file button_config_dialog.h
 * @brief 自定义操作按钮配置对话框
 *
 * ButtonConfigForm 为单个按钮状态的配置表单（外观 + 行为 + 显示条件）；
 * ButtonConfigDialog 为统一入口：单状态直接显示一份表单，双状态（同一业务的
 * 两个互斥状态）在同一对话框内以页签并排显示，只弹出一层模态。
 */

#pragma once

#include <QDialog>
#include <QPair>
#include <QString>
#include <QVector>
#include <QWidget>

#include "json_vue_model.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTabWidget;
class QTableWidget;
class QVBoxLayout;

/**
 * @class ButtonConfigForm
 * @brief 单个按钮状态的配置表单（多状态时嵌入页签，各状态独立一份）
 */
class ButtonConfigForm : public QWidget {
  Q_OBJECT

public:
  explicit ButtonConfigForm(QWidget *parent = nullptr);
  ~ButtonConfigForm() override = default;

  /// 获取配置数据
  ButtonConfig getData() const;
  /// 设置配置数据
  void setData(const ButtonConfig &config);

  /// 设置 .jsonsource 文件搜索根目录（当前编辑 jsonvue 文件所在目录，可为空）
  void setSearchRoot(const QString &dir);

  /// 设置“显示条件”可选字段（ref, 显示标签）；行按钮才有意义
  void setVisibilityFields(const QVector<QPair<QString, QString>> &fields);

  /// 页签标题（按钮文字 + 显示条件值；未填文字时回退「状态 N」）
  QString stateTitle(int index) const;

signals:
  /// 标识信息（按钮文字 / 显示条件值）变化，供外层页签标题实时刷新
  void identityChanged();

private:
  /// 构建界面
  void setupUI();
  /// 根据行为类型更新配置区可见性
  void updateBehaviorVisibility();
  /// 根据位置/字段更新显示条件控件可用性
  void updateVisibilityControls();
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

  // ── 显示控制 ──
  QCheckBox *m_showLabelCheck = nullptr;      ///< 显示按钮文字（false=仅图标+tooltip）
  QComboBox *m_visibleFieldCombo = nullptr;   ///< 显示条件字段
  QComboBox *m_visibleOpCombo = nullptr;      ///< eq/ne
  QLineEdit *m_visibleValueEdit = nullptr;    ///< 显示条件值
  QString m_pendingVisibleField;              ///< 候选字段尚未填充时的暂存值

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

  /// .jsonsource 文件搜索根目录（当前编辑 jsonvue 文件所在目录）
  QString m_searchRoot;
};

/**
 * @class ButtonConfigDialog
 * @brief 自定义操作按钮配置对话框（单状态无页签 / 多状态页签，只弹一层）
 */
class ButtonConfigDialog : public QDialog {
  Q_OBJECT

public:
  /// 构造为新建单状态模式
  explicit ButtonConfigDialog(QWidget *parent = nullptr);
  /// 构造为编辑单状态模式（加载已有配置）
  explicit ButtonConfigDialog(const ButtonConfig &config, QWidget *parent = nullptr);
  /// 构造为多状态模式（同一业务的两个互斥状态，每个元素一个页签）
  explicit ButtonConfigDialog(const QVector<ButtonConfig> &states, QWidget *parent = nullptr);
  ~ButtonConfigDialog() override = default;

  /// 获取第一个状态的配置数据（单状态场景）
  ButtonConfig getData() const;
  /// 获取全部状态的配置数据（多状态场景；stateGroup 沿用入参）
  QVector<ButtonConfig> getStates() const;

  /// 设置单个状态配置（单状态构造后使用）
  void setData(const ButtonConfig &config);
  /// 设置全部状态配置（顺序与页签一致）
  void setStates(const QVector<ButtonConfig> &states);

  /// 设置 .jsonsource 文件搜索根目录（应用到所有状态表单）
  void setSearchRoot(const QString &dir);

  /// 设置“显示条件”可选字段（应用到所有状态表单）
  void setVisibilityFields(const QVector<QPair<QString, QString>> &fields);

  /// 预设为双状态模式（创建双状态按钮的快捷入口；等价于用户在下拉框中选择）
  void setDualMode(bool dual);
  /// 当前是否为双状态模式
  bool isDualMode() const;

private:
  /// 构建界面（stateCount > 1 时默认双状态）
  void setupUI(int stateCount);
  /// 按状态模式切换单状态表单 / 双状态页签的呈现
  void updateModeUI();
  /// 懒创建第二个状态表单（并应用搜索根/显示条件候选）
  void ensureDualForm();
  /// 按各状态的文字/判定值刷新页签标题
  void refreshTabTitles();

  QComboBox *m_modeCombo = nullptr;          ///< 按钮状态：单状态 / 双状态
  QLabel *m_modeHint = nullptr;              ///< 双状态说明（单状态时隐藏）
  QWidget *m_contentHost = nullptr;          ///< 表单呈现区容器
  QVBoxLayout *m_contentLayout = nullptr;    ///< 表单呈现区布局（单表单 或 页签）
  QTabWidget *m_tabs = nullptr;              ///< 双状态页签（单状态时隐藏）
  QVector<ButtonConfigForm *> m_forms;        ///< 每个状态一份表单
  QVector<ButtonConfig> m_initialStates;      ///< 入参配置（保留 stateGroup 等页签外字段）
  QString m_searchRoot;                       ///< 搜索根（后创建的状态表单需要）
  QVector<QPair<QString, QString>> m_visibilityFields;  ///< 显示条件候选（同上）
};
