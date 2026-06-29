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
class CodeEditor;

// ──────────────────────────────────────────────────────────────
//  DimmableTabWidget
// ──────────────────────────────────────────────────────────────

class DimmableTabWidget : public QTabWidget {
public:
  explicit DimmableTabWidget(QWidget *parent = nullptr) : QTabWidget(parent) {
    setTabsClosable(true);
    setMovable(true);
    QStyle *fs = QStyleFactory::create("Fusion");
    if (fs) {
      fs->setParent(tabBar());
      tabBar()->setStyle(fs);
    }
  }
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