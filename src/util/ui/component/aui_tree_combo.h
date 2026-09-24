/**
 * @file aui_tree_combo.h
 * @brief 树形下拉框控件（分组可展开/收起）
 */

#ifndef AUI_TREE_COMBO_H
#define AUI_TREE_COMBO_H

#include <QComboBox>
#include <QStandardItem>
#include <QStandardItemModel>

class QMouseEvent;
class QPaintEvent;
class QStandardItemModel;
class QTreeView;

/**
 * @class AuiTreeCombo
 * @brief 树形下拉框：QComboBox + QTreeView 弹层，组节点可展开/收起
 *
 * 适用场景：候选内容按文件/类别分组且会持续增多的下拉选择。
 * - 组节点（addGroup）：加粗、不可选中，仅用于展开/收起与分组标题；
 * - 条目（addEntry）：可选项，data 存入 UserRole 并经 itemSelected 信号抛出；
 * - 点击控件任意区域弹出/收起弹层（与箭头一致）；
 * - 弹层高度自行管理：展开/收起时按实际内容高度自适应（超过 600px 出滚动条），
 *   全局 ComboPopDownFilter 对标记 auiTreePopup 属性的视图跳过高度钳制。
 *
 * 使用方式：
 * @code
 *   AuiTreeCombo *combo = new AuiTreeCombo(this);
 *   combo->addEntry(nullptr, "手动输入", QString());        // 顶层条目（不分组）
 *   QStandardItem *grp = combo->addGroup("▍全局枚举 · a.json");
 *   combo->addEntry(grp, "is_open - 是否开启", "a.json#id1");
 *   connect(combo, &AuiTreeCombo::itemSelected, this, [](const QVariant &data) {...});
 *   combo->selectByData("a.json#id1");                      // 恢复选中（可省略）
 * @endcode
 */
class AuiTreeCombo : public QComboBox {
  Q_OBJECT

public:
  explicit AuiTreeCombo(QWidget *parent = nullptr);

  /// 添加组节点（加粗、不可选中，仅展开/收起），返回组节点供 addEntry 挂子条目
  QStandardItem *addGroup(const QString &title);
  /// 向组节点添加可选项；group 为 nullptr 时挂顶层。data 存入 UserRole
  void addEntry(QStandardItem *group, const QString &text, const QVariant &data);
  /// 按 UserRole 数据选中条目（自动展开父级链），未找到时无动作
  void selectByData(const QVariant &data);

  /// 当前选中条目的 UserRole 数据（未选中/组标题返回空 QVariant）
  QVariant currentEntryData() const;

  /// 当前选中条目索引（弹层委托/分支区绘制高亮的依据；与内部选中引用同步维护）
  QModelIndex currentHighlightIndex() const { return m_currentIdx; }

signals:
  /// 选中某个条目（组标题与展开/收起不触发）；data 为条目 UserRole 数据
  void itemSelected(const QVariant &data);

protected:
  void showPopup() override;
  void hidePopup() override;
  void paintEvent(QPaintEvent *ev) override;
  void mousePressEvent(QMouseEvent *ev) override;
  void mouseReleaseEvent(QMouseEvent *ev) override;
  bool eventFilter(QObject *obj, QEvent *ev) override;

private:
  /// 深度优先查找 UserRole 匹配的条目索引，未找到返回无效索引
  QModelIndex findIndexByData(const QVariant &data) const;
  /// 按当前展开状态自适应弹层高度（可见行数 × 行高，超过 600px 出滚动条）
  void updatePopupHeight();

  QStandardItemModel *m_model = nullptr;
  QTreeView *m_treeView = nullptr;
  QPersistentModelIndex m_currentIdx;  ///< 当前选中条目索引（模型增删时自动失效）
  QVariant m_currentRef;           ///< 当前选中条目的 UserRole 数据（弹层打开时高亮用）
  int m_containerChrome = 0;       ///< 弹层窗口与视图的高度差（容器边距，弹出时测量）
  bool m_popupOpen = false;        ///< 弹层是否处于展开状态
  bool m_closedByOutsidePress = false;  ///< 弹层因"控件区域按下"而关闭（本次点击用于关闭）
  bool m_internalSelect = false;   ///< 程序性选中标记（selectByData/高亮时不触发 itemSelected）
  bool m_suppressNextShow = false; ///< 按下已关闭弹层时，抑制本次释放重新弹出
  bool m_needsExpand = false;      ///< 模型有新增内容，下次弹出时全部展开
};

#endif  // AUI_TREE_COMBO_H
