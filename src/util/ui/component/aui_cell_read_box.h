/**
 * @file aui_cell_read_box.h
 * @brief 表格单元格常驻「只读文本框」控件（只读单元格的选中/复制统一方案）
 *
 * 表格只读文本列（如 .jsontable 设计器「字段引用」列）使用，两态交互完全
 * 复用 AuiCellEditCombo 已验证的机制（该控件经多轮踩坑验证，勿另造一套）：
 * - 非编辑态（默认）：内部输入框鼠标穿透 → 点击落回容器 → 容器把所在格设为
 *   当前格 → 表格原生绘制选中高亮（整格蓝，与下拉框列一致）；输入框无文字，
 *   item 文字是唯一显示（无重影）；
 * - 编辑态（双击进入）：输入框恢复鼠标接收并聚焦，画白底（盖住行高亮），
 *   拖选/双击选词/Ctrl+A/Ctrl+C 与可编辑输入框完全一致，仅不可输入；
 * - Esc/回车/失焦退出编辑态。
 * item 仍是数据源，本控件只做显示层。
 */

#pragma once

#include <QWidget>

class QLineEdit;
class QPainter;
class QPaintEvent;
class QTableWidget;

/**
 * @class AuiCellReadBox
 * @brief 表格单元格内的只读文本框：单击整格选中、双击进入可选中复制的编辑态
 */
class AuiCellReadBox : public QWidget {
 public:
  /// table/row/col 用于选中定位同步；text 为只读文本
  AuiCellReadBox(QTableWidget *table, int row, int col, const QString &text,
                 QWidget *parent = nullptr);

  QString text() const;
  void setText(const QString &t);
  /// 提示文本（透传内部输入框；item 的 tooltip 被 widget 遮挡时用它）
  void setTip(const QString &tip);

  /// 把所在格设为表格当前格（表格据此绘制选中高亮）
  void activateCell();
  /// 进入编辑态（双击触发：白底 + 可拖选复制）
  void enterEdit();
  /// 退出编辑态（Esc/回车/失焦）
  void exitEdit();

 protected:
  void mousePressEvent(QMouseEvent *e) override;
  void mouseDoubleClickEvent(QMouseEvent *e) override;
  void paintEvent(QPaintEvent *e) override;

 private:
  /// 是否处于编辑态（编辑态 = 输入框恢复接收鼠标）
  bool editing() const;
  /// 按当前态同步输入框文字（非编辑态清空，item 文字是唯一显示）
  void syncEditVisibleText();

  QLineEdit *m_edit = nullptr;
  QTableWidget *m_table = nullptr;
  int m_row = 0;   ///< 所在物理行（cellWidget 固定行位，构造后不变）
  int m_col = 0;   ///< 所在列
  QString m_text;  ///< 只读文本（编辑态由输入框显示）
};
