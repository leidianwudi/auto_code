/**
 * @file aui_combo_delete.h
 * @brief 可删除项下拉框控件
 *
 * 在普通下拉框基础上，为「下拉列表」的每一项右侧绘制一个可点击的删除按钮（×）。
 * 点击某项末尾的 × 时发出 itemDeleteRequested(index) 信号，由使用者决定删除逻辑。
 * 外观样式复用 AuiStyle 颜色常量，与 AuiComboBox 保持一致的页面风格。
 */

#pragma once

#include <QComboBox>

/**
 * @class AuiComboDelete
 * @brief 带项删除按钮的下拉框
 *
 * 用法示例：
 * @code
 * AuiComboDelete *combo = AuiComboDelete::create(this);
 * combo->addItem("项 1", "data1");
 * combo->addItem("项 2", "data2");
 * connect(combo, &AuiComboDelete::itemDeleteRequested, [](int index) {
 *   // 根据 index 删除对应项
 * });
 * @endcode
 */
class AuiComboDelete : public QComboBox {
  Q_OBJECT
public:
  /// 创建带删除按钮的下拉框（应用统一样式；三角箭头与 AuiComboBox 绘制方式一致）
  static AuiComboDelete *create(QWidget *parent = nullptr);

  /// 当前高亮的待删除项行号（-1 表示未悬停在任何删除按钮上）
  int hoverRow() const { return m_hoverRow; }

  /// 计算某列表项矩形内删除按钮的矩形（基于该项的 viewport 坐标）
  QRect buttonRect(const QRect &itemRect) const;

  /// 列表项内删除按钮的边长（正方形）
  static constexpr int kBtnSize = 16;
  /// 列表项右边留给删除按钮的空白宽度（含左右边距）
  static constexpr int kBtnScaffoldWidth = 24;
  /// 列表项文本左侧内边距
  static constexpr int kBtnPaddingLeft = 8;

signals:
  /// 点击某项右侧的删除按钮时发出，携带该项行号
  void itemDeleteRequested(int index);

protected:
  void paintEvent(QPaintEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  explicit AuiComboDelete(QWidget *parent = nullptr);

  int m_hoverRow = -1;  ///< 当前悬停的删除按钮所在行
};