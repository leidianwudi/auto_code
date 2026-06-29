/**
 * @file main_dev_ui.h
 * @brief 代码编辑器主窗口（视图层）
 *
 * 继承 QMainWindow 作为实际的主窗口。
 * 由 MainDevMgr 控制器创建和管理。
 */

#pragma once

#include <QAction>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSplitter>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTreeWidget>

class QTreeWidgetItem;
class QTabBar;
class QDrag;
class QMimeData;
class QMouseEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class CodeEditor;

// ──────────────────────────────────────────────────────────────
//  DraggableTabBar  — 支持跨面板拖拽的标签栏
// ──────────────────────────────────────────────────────────────

class DraggableTabBar : public QTabBar {
  Q_OBJECT

public:
  explicit DraggableTabBar(QWidget *parent = nullptr);

signals:
  /// 标签从 fromBar(fromIndex) 拖拽到此 bar 的 toIndex 位置
  void tabDropped(int fromIndex, DraggableTabBar *fromBar, int toIndex);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
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

  static DraggableTabBar *s_sourceBar;
  static int s_sourceIndex;
};

// ──────────────────────────────────────────────────────────────
//  DimmableTabWidget
// ──────────────────────────────────────────────────────────────

class DimmableTabWidget : public QTabWidget {
  Q_OBJECT

public:
  explicit DimmableTabWidget(QWidget *parent = nullptr);

protected:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;
};

// ──────────────────────────────────────────────────────────────
//  MainDevUi
// ──────────────────────────────────────────────────────────────

class MainDevUi : public QMainWindow {
  Q_OBJECT

public:
  explicit MainDevUi(QWidget *parent = nullptr);
  ~MainDevUi() override = default;

  /// 初始化界面布局（菜单栏、文件树、编辑器区域、状态栏）
  void setupUI();

  /// 创建一个新的编辑器面板组
  QTabWidget *createEditorPanel();

  // ════════════════════════════════════════════════════════════
  //  文件树操作
  // ════════════════════════════════════════════════════════════

  void expandFileTree();
  void buildFileTree(const QString &dirPath);

  // ════════════════════════════════════════════════════════════
  //  编辑器面板组操作
  // ════════════════════════════════════════════════════════════

  int editorPanelCount() const;
  QTabWidget *editorPanelAt(int index) const;
  int editorPanelIndex(const QTabWidget *tabs) const;
  void addEditorPanel(QTabWidget *panel);
  void removeEditorPanelAt(int index);
  void setEditorPanelsUniformStretch();

  // ════════════════════════════════════════════════════════════
  //  主分割器操作
  // ════════════════════════════════════════════════════════════

  void adjustMainSplitter();
  int mainSplitterWidth() const;
  int fileTreeWidth() const;

  // ════════════════════════════════════════════════════════════
  //  状态栏
  // ════════════════════════════════════════════════════════════

  void setCursorStatusText(const QString &text);

  // ════════════════════════════════════════════════════════════
  //  标签页颜色
  // ════════════════════════════════════════════════════════════

  void applyTabDimming(QTabWidget *active);

  // ════════════════════════════════════════════════════════════
  //  控件 getter
  // ════════════════════════════════════════════════════════════

  QTreeWidget *fileTree() const { return m_fileTree; }
  QSplitter *editorSplitter() const { return m_editorSplitter; }
  QAction *splitAction() const { return m_splitAction; }
  QAction *closeAction() const { return m_closeAction; }

private:
  void addDirectoryToTree(QTreeWidgetItem *parentItem, const QString &dirPath);

  /// 更新最大化/还原按钮图标
  void updateMaximizeIcon();

  // ── 窗口事件 ──
  void onMaximizeClicked();
  void changeEvent(QEvent *ev) override;
#if defined(Q_OS_WIN)
  bool nativeEvent(const QByteArray &eventType, void *message,
                   qintptr *result) override;
#endif

  QTreeWidget *m_fileTree = nullptr;
  QSplitter *m_mainSplitter = nullptr;
  QSplitter *m_editorSplitter = nullptr;
  QLabel *m_cursorPositionLabel = nullptr;
  QAction *m_splitAction = nullptr;
  QAction *m_closeAction = nullptr;

  // ── 自定义标题栏 ──
  QWidget *m_titleBar = nullptr;
  QLabel *m_titleLabel = nullptr;
  QPushButton *m_splitBtn = nullptr;
  QPushButton *m_minBtn = nullptr;
  QPushButton *m_maxBtn = nullptr;
  QPushButton *m_closeBtn = nullptr;
};