/**
 * @file main_dev_ui_ext.h
 * @brief 编辑器面板扩展控件 — 可拖拽标签栏与接收拖放的面板
 *
 * DraggableTabBar  ：支持跨面板拖拽的标签栏（自绘样式继承自通用控件 AuiCodeTabBar）
 * DimmableTabWidget：内置 DraggableTabBar
 * 的标签页容器，标签页拖入时自动创建/销毁面板
 */

#pragma once

#include <QTabWidget>

#include "src/util/ui/component/aui_code_tab_bar.h"
#include "src/util/ui/component/aui_style.h"

class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QMouseEvent;
class QPaintEvent;

// ════════════════════════════════════════════════════════════
//  SplitSide — 拖拽拆分方向（VSCode 风格：拖到左/右边缘拆分）
// ════════════════════════════════════════════════════════════

enum class SplitSide { None = 0, Left, Right };

// ════════════════════════════════════════════════════════════
//  SplitOverlay — 拆分高亮覆盖层（半透明蓝色，覆盖左/右半区）
// ════════════════════════════════════════════════════════════

class SplitOverlay : public QWidget {
  Q_OBJECT

public:
  explicit SplitOverlay(QWidget *parent = nullptr);
  /// 在指定半区显示高亮（area 为覆盖层显示区域，由面板计算传入）
  void showForSide(SplitSide side, const QRect &area);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  SplitSide m_side = SplitSide::None;
};

// ════════════════════════════════════════════════════════════
//  DraggableTabBar  — 支持跨面板拖拽的标签栏
// ════════════════════════════════════════════════════════════

class DraggableTabBar : public AuiCodeTabBar {
  Q_OBJECT

public:
  explicit DraggableTabBar(QWidget *parent = nullptr);

signals:
  /// 标签从 fromBar(fromIndex) 拖拽到此 bar 的 toIndex 位置
  void tabDropped(int fromIndex, DraggableTabBar *fromBar, int toIndex);
  /// 标签被拖拽到 tab 头左/右边缘后松开 → 触发拆分
  void tabSplitDropped(int fromIndex, DraggableTabBar *fromBar, SplitSide side);

  /// 右键菜单：关闭其它标签页
  void closeOthersRequested(int index);
  /// 右键菜单：关闭所有标签页
  void closeAllRequested();

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dragLeaveEvent(QDragLeaveEvent *event) override;
  void dropEvent(QDropEvent *event) override;

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
  SplitSide m_splitSide = SplitSide::None;  ///< 拖拽过程中命中的拆分方向

  static DraggableTabBar *s_sourceBar;
  static int s_sourceIndex;
};

// ════════════════════════════════════════════════════════════
//  BottomTabWidget — 底部面板 tab 容器（输出 / 问题）
//  复用 AuiCodeTabBar 的自绘样式（选中蓝色指示条/主题背景/文字），
//  纯标签页不显示关闭按钮；底部面板不参与编辑器面板聚焦切换，始终高亮选中标签。
// ════════════════════════════════════════════════════════════

class BottomTabWidget : public QTabWidget {
public:
  explicit BottomTabWidget(QWidget *parent = nullptr) : QTabWidget(parent) {
    setDocumentMode(true);
    auto *bar = new AuiCodeTabBar;
    bar->setShowCloseButton(false);
    bar->setProperty("aui_focus_tab", true);
    bar->setTabHeight(22);  // 比代码标签栏更紧凑（默认约 30px）
    // 与代码标签栏一致：Fusion 风格 + 四边空白，保证未选中标签文字完整显示
    AuiStyle::applyTabBarPadding(bar, 12, 4, 12, 2);
    setTabBar(bar);
  }
};

// ════════════════════════════════════════════════════════════
//  LeftPanelTabWidget — 左侧面板 tab 容器（文件 / 调试）
//  复用 AuiCodeTabBar 的自绘样式，纯标签页不显示关闭按钮；
//  padding 与代码编辑 tab 完全一致（4,4,0,4），仅高度压缩为 24px，
//  选中态跟随 currentIndex 正常交互。
// ════════════════════════════════════════════════════════════

class LeftPanelTabWidget : public QTabWidget {
public:
  explicit LeftPanelTabWidget(QWidget *parent = nullptr) : QTabWidget(parent) {
    setDocumentMode(true);
    auto *bar = new AuiCodeTabBar;
    bar->setShowCloseButton(false);
    bar->setTabHeight(26);  // 比代码标签栏（约 30px）紧凑
    // 与代码编辑 tab 一致：Fusion 风格 + 同一四边空白，保证视觉统一
    AuiStyle::applyTabBarPadding(bar, 14, 4, 14, 4);
    setTabBar(bar);
  }
};

// ════════════════════════════════════════════════════════════
//  DimmableTabWidget  — 内置 DraggableTabBar 的标签页容器
// ════════════════════════════════════════════════════════════

class DimmableTabWidget : public QTabWidget {
  Q_OBJECT

public:
  explicit DimmableTabWidget(QWidget *parent = nullptr);

  /// 显示/隐藏拆分高亮覆盖层（拖到内容区或 tab 头边缘时调用）
  void showSplitOverlay(SplitSide side);
  void hideSplitOverlay();

signals:
  /// 标签被拖到面板左/右边缘后松开 → 请求拆分
  void splitDropped(int fromIndex, DraggableTabBar *fromBar, SplitSide side);

protected:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dragLeaveEvent(QDragLeaveEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private:
  SplitOverlay *m_splitOverlay = nullptr;
  SplitSide m_splitSide = SplitSide::None;  ///< 拖拽过程中命中的拆分方向
};
