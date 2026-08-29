/**
 * @file aui_multi_check_combo.h
 * @brief 多选下拉框（下拉按钮 + 自管理复选弹出列表）
 *
 * 用于"按文件类型过滤"等多选场景：
 * - 点击弹出可勾选的列表（QListWidget，Qt::Popup 弹窗），勾选复选框不关闭弹层，可连续多选
 * - 不使用 QMenu / QComboBox 的内置弹出机制（它们点击菜单项会自动收起），
 *   弹层的显示/关闭完全由本控件控制，保证复选时绝不收起
 * - 按钮文字显示当前选择汇总（全部类型 / N 种类型 / 无类型）
 * - 样式随主题（深色/浅色）自适应，集中在 AuiStyle
 */

#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QToolButton>

class QListWidget;
class QListWidgetItem;
class QWidget;

/// 多选下拉框：QToolButton + 自管理 QListWidget 弹出列表
class AuiMultiCheckCombo : public QToolButton {
  Q_OBJECT

public:
  explicit AuiMultiCheckCombo(QWidget *parent = nullptr);

  /// 添加一个可选项（label 为显示文字，data 为关联数据，如文件后缀 ".ac"）
  void addOption(const QString &label, const QString &data);
  /// 是否存在某数据项（还原保存的勾选时用于校验选项是否仍存在）
  bool hasOption(const QString &data) const { return m_items.contains(data); }
  /// 设置某项勾选状态（不存在时忽略；不触发 checkedChanged，供程序化还原用）
  void setChecked(const QString &data, bool checked);
  /// 是否勾选了某项
  bool isChecked(const QString &data) const;
  /// 当前勾选的数据列表（按添加顺序）
  QStringList checkedData() const;
  /// 是否全部勾选
  bool allChecked() const;
  /// 全选 / 全不选（不触发 checkedChanged）
  void setAllChecked(bool all = true);

  /// 主题切换后刷新（按钮与弹出列表颜色随主题）
  void refreshStyle();

signals:
  /// 勾选状态变化（仅用户交互触发；程序化 setChecked/setAllChecked 不触发）
  void checkedChanged();

protected:
  void keyPressEvent(QKeyEvent *event) override;
  /// 监听弹窗 Hide（含点击外部自动收起），重置按钮聚焦/按下的残留变色
  bool eventFilter(QObject *watched, QEvent *event) override;
  /// 自绘：紧排"文字 + 小间隔 + 向下箭头"，避免原生布局文字与箭头间留白过大
  void paintEvent(QPaintEvent *event) override;
  /// 固定宽度（最宽汇总文字 + 箭头），避免文字切换导致宽度跳动 / 右侧多余空白
  QSize sizeHint() const override;

private:
  /// 展开 / 收起弹出列表
  void togglePopup();
  /// 重置按钮聚焦/按下态并强制重绘：Qt::Popup 不参与键盘焦点管理，
  /// 弹层展开/收起时若不清焦点，按钮的聚焦/按下"变色"会残留到点击应用外部才恢复
  void resetButtonVisualState();
  /// 按内容计算并更新弹窗尺寸
  void relayoutPopup();
  void updateDisplay();

  QWidget *m_popup = nullptr;              ///< Qt::Popup 弹窗（自管理，不自动收）
  QListWidget *m_list = nullptr;           ///< 可勾选列表
  QStringList m_dataOrder;                 ///< 选项数据顺序
  QHash<QString, QListWidgetItem *> m_items;  ///< data → 列表项
  QSet<QString> m_checked;                 ///< 当前勾选的数据
  bool m_updating = false;                 ///< 程序化改勾选期间抑制 checkedChanged
};
