/**
 * @file main_dev_ui.h
 * @brief MainDev 视图层
 *
 * 负责所有界面控件的创建、布局和直接操控。
 * 控制器只能通过本类提供的命名方法操作界面，不能直接访问内部控件。
 */

#pragma once

#include <QAction>
#include <QLabel>
#include <QObject>
#include <QSplitter>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTreeWidget>

class QMainWindow;
class QTreeWidgetItem;
class QTabBar;
class CodeEditor;

// ──────────────────────────────────────────────────────────────
//  DimmableTabWidget — 支持 tab 文字变灰
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

class MainDevUi : public QObject {
  Q_OBJECT

public:
  explicit MainDevUi(QObject *parent = nullptr);
  ~MainDevUi() override = default;

  /// 初始化界面布局（菜单栏、文件树、编辑器区域、状态栏）
  void setupUI(QMainWindow *window);

  /// 创建一个新的编辑器面板组
  QTabWidget *createEditorPanel();

  // ════════════════════════════════════════════════════════════
  //  文件树操作
  // ════════════════════════════════════════════════════════════

  /// 展开整个文件树
  void expandFileTree();

  /// 从目录路径构建文件树（递归遍历）
  void buildFileTree(const QString &dirPath);

  // ════════════════════════════════════════════════════════════
  //  编辑器面板组操作
  // ════════════════════════════════════════════════════════════

  /// 当前编辑器面板数量
  int editorPanelCount() const;

  /// 获取第 i 个面板组
  QTabWidget *editorPanelAt(int index) const;

  /// 获取面板组在分割器中的索引
  int editorPanelIndex(const QTabWidget *tabs) const;

  /// 添加一个面板组到分割器中
  void addEditorPanel(QTabWidget *panel);

  /// 移除并删除第 i 个面板组
  void removeEditorPanelAt(int index);

  /// 将所有面板组的拉伸因子设为 1（均匀分配空间）
  void setEditorPanelsUniformStretch();

  // ════════════════════════════════════════════════════════════
  //  主分割器操作
  // ════════════════════════════════════════════════════════════

  /// 根据当前实际宽度刷新主分割器尺寸
  void adjustMainSplitter();

  /// 主分割器当前宽度
  int mainSplitterWidth() const;

  /// 文件树当前宽度
  int fileTreeWidth() const;

  // ════════════════════════════════════════════════════════════
  //  状态栏
  // ════════════════════════════════════════════════════════════

  /// 设置状态栏光标位置文字
  void setCursorStatusText(const QString &text);

  // ════════════════════════════════════════════════════════════
  //  标签页颜色
  // ════════════════════════════════════════════════════════════

  /// 对所有面板组应用 tab 颜色：焦点面板默认色，非焦点灰色
  void applyTabDimming(QTabWidget *active);

  // ════════════════════════════════════════════════════════════
  //  控件 getter（仅限控制器需要直接访问的场合）
  // ════════════════════════════════════════════════════════════

  QTreeWidget *fileTree() const { return m_fileTree; }
  QSplitter *editorSplitter() const { return m_editorSplitter; }
  QAction *splitAction() const { return m_splitAction; }
  QAction *closeAction() const { return m_closeAction; }

private:
  /// 递归构建文件树的内部方法
  void addDirectoryToTree(QTreeWidgetItem *parentItem, const QString &dirPath);

  QTreeWidget *m_fileTree = nullptr;
  QSplitter *m_mainSplitter = nullptr;
  QSplitter *m_editorSplitter = nullptr;
  QLabel *m_cursorPositionLabel = nullptr;
  QAction *m_splitAction = nullptr;
  QAction *m_closeAction = nullptr;
};