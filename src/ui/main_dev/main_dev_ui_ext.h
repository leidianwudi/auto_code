/**
 * @file main_dev_ui_ext.h
 * @brief 编辑器面板扩展控件 — 可拖拽标签栏与接收拖放的面板
 *
 * DraggableTabBar  ：支持跨面板拖拽的标签栏
 * DimmableTabWidget：内置 DraggableTabBar
 * 的标签页容器，标签页拖入时自动创建/销毁面板
 */

#pragma once

#include <QSet>
#include <QTabBar>
#include <QTabWidget>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QMouseEvent;

// ════════════════════════════════════════════════════════════
//  DraggableTabBar  — 支持跨面板拖拽的标签栏
// ════════════════════════════════════════════════════════════

class DraggableTabBar : public QTabBar {
  Q_OBJECT

public:
  explicit DraggableTabBar(QWidget *parent = nullptr);

  /// 设置标签页的修改状态（已修改则在标签右侧绘制红色 "*"）
  void setTabModified(int index, bool modified);
  bool isTabModified(int index) const;

signals:
  /// 标签从 fromBar(fromIndex) 拖拽到此 bar 的 toIndex 位置
  void tabDropped(int fromIndex, DraggableTabBar *fromBar, int toIndex);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

public:
  static DraggableTabBar *dragSourceBar() { return s_sourceBar; }
  static int dragSourceIndex() { return s_sourceIndex; }
  static void clearDragSource() {
    s_sourceBar = nullptr;
    s_sourceIndex = -1;
  }

private:
  int m_pressedIndex = -1;
  QPoint m_dragStartPos;
  QSet<int> m_modifiedTabs; ///< 已修改标签页索引集合

  static DraggableTabBar *s_sourceBar;
  static int s_sourceIndex;
};

// ════════════════════════════════════════════════════════════
//  DimmableTabWidget  — 内置 DraggableTabBar 的标签页容器
// ════════════════════════════════════════════════════════════

class DimmableTabWidget : public QTabWidget {
  Q_OBJECT

public:
  explicit DimmableTabWidget(QWidget *parent = nullptr);

protected:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;
};