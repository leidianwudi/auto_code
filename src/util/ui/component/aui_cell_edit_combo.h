/**
 * @file aui_cell_edit_combo.h
 * @brief 表格单元格常驻「输入框 + 三角下拉」控件
 *
 * 与 AuiComboBox::createForTable()（不可编辑扁平下拉，文字自绘）互补：
 * 本控件用原生 QLineEdit 承载文字，选中/复制能力与普通输入框完全一致。
 */

#pragma once

#include <QElapsedTimer>
#include <QStringList>
#include <QWidget>

#include <functional>

class QLineEdit;
class QMenu;
class QTableWidget;

/**
 * @class AuiCellEditCombo
 * @brief 表格单元格内的「输入框 + 实心三角下拉」，表现模拟单元格编辑器
 *
 * 两态交互（与普通文本列的编辑器一致，避免重复踩坑）：
 * - 非编辑态（默认）：输入框只读且鼠标穿透 —— 单击只选中单元格（无光标），
 *   行背景透出（选中行 = 整格蓝，与其他列一致）；
 * - 编辑态（双击进入）：容器画白底（无边框，盖住行高亮），光标出现，
 *   拖选/双击选词/Ctrl+A/Ctrl+C 与普通输入框完全一致；失焦/回车退出，Esc 还原。
 * - 箭头：右缘 18px 热区，单击展开候选（QMenu，当前值打勾），再单击收起；
 *   两种状态下均可点。
 * readOnly=true 时编辑态也不可改字（纯选择 + 选中复制），false 时可自由输入。
 */
class AuiCellEditCombo : public QWidget {
 public:
  /// table/col 用于行激活同步；presets 为候选列表；readOnly=true 时编辑态不可改字
  AuiCellEditCombo(QTableWidget *table, int col, const QStringList &presets,
                   QWidget *parent = nullptr, bool readOnly = false);

  QString text() const;
  void setText(const QString &t);
  /// 动态更新候选列表（如候选跟随其他表格内容变化）
  void setPresets(const QStringList &presets);
  QLineEdit *lineEdit() const;

  /// 用户点候选改值后触发（值变化才调用；程序性 setText 不触发）。
  /// 只读列的唯一值变化入口；可编辑列的手输走 textEdited 自行接线
  std::function<void()> valueCommitted;

  /// 把所在行设为当前行（cellWidget 吞掉表格选中处理，行操作依赖当前行；
  /// public 供内部输入框过滤器调用）
  void activateRow();

  /// 进入编辑态（双击触发；只读列同样进入，用于选中复制）
  void enterEdit();
  /// 退出编辑态（revert=true 时还原进入前的文本，Esc 用；失焦/回车用 false）
  void exitEdit(bool revert);

 protected:
  void resizeEvent(QResizeEvent *e) override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseDoubleClickEvent(QMouseEvent *e) override;
  void paintEvent(QPaintEvent *e) override;

 private:
  /// 是否处于编辑态（白底 + 光标/拖选）
  bool editing() const;
  /// 展开/收起候选菜单（toggle）
  void toggleCandidates();

  QLineEdit *m_edit = nullptr;
  QTableWidget *m_table = nullptr;
  int m_col = 0;               ///< 所在列（行激活 / 选中态判定）
  bool m_editable = true;      ///< 编辑态是否允许改字（构造 readOnly 取反）
  QString m_editOriginal;      ///< 进入编辑态时的文本（Esc 还原用）
  QStringList m_presets;       ///< 候选列表（可动态更新，可编辑列仍可自由输入）
  QMenu *m_menu = nullptr;     ///< 当前打开的候选菜单（非模态，支持单击收起）
  QElapsedTimer m_closedAt;    ///< 菜单收起时刻（挡住收起点击的穿透重开）
};
