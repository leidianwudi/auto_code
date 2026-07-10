/**
 * @file main_dev_ui.h
 * @brief 代码编辑器主窗口（视图层）
 *
 * 继承 QMainWindow 作为实际的主窗口。
 * 由 MainDevMgr 控制器创建和管理。
 */

#pragma once

#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>

#include "main_dev_ui_ext.h"
#include "src/tool/ui/code/code_log.h"
#include "src/tool/ui/code/tree_dir.h"

// ════════════════════════════════════════════════════════════
//  MainDevUi
// ════════════════════════════════════════════════════════════

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
  TreeDir *fileTree() const { return m_fileTree; }

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
  void setErrorMessage(const QString &msg);

  // ════════════════════════════════════════════════════════════
  //  标签页颜色
  // ════════════════════════════════════════════════════════════

  void applyTabDimming(QTabWidget *active);

  // ════════════════════════════════════════════════════════════
  //  输出面板
  // ════════════════════════════════════════════════════════════

  /// 向输出面板追加文本，isError 为 true 时显示红色
  void appendOutput(const QString &text, bool isError = false);
  /// 清空输出面板
  void clearOutput();
  CodeLog *outputPanel() const { return m_outputPanel; }

  // ════════════════════════════════════════════════════════════
  //  控件 getter
  // ════════════════════════════════════════════════════════════
  QSplitter *editorSplitter() const { return m_editorSplitter; }
  QAction *splitAction() const { return m_splitAction; }
  QAction *closeAction() const { return m_closeAction; }
  QAction *openAction() const { return m_openAction; }
  QAction *openFolderAction() const { return m_openFolderAction; }
  QAction *helpExampleAction() const { return m_helpExampleAction; }
  QPushButton *buildBtn() const { return m_buildBtn; }
  QComboBox *startupCombo() const { return m_startupCombo; }
  QPushButton *saveBtn() const { return m_saveBtn; }
  QPushButton *saveAllBtn() const { return m_saveAllBtn; }

  /// 刷新启动项下拉框（由外部在启动项变化后调用）
  void refreshStartupCombo();

private:
  /// 更新最大化/还原按钮图标
  void updateMaximizeIcon();

  // ── 窗口事件 ──
  void onMaximizeClicked();
  void closeEvent(QCloseEvent *event) override;
  void changeEvent(QEvent *ev) override;
  bool eventFilter(QObject *obj, QEvent *ev) override;
#if defined(Q_OS_WIN)
  bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

  TreeDir *m_fileTree = nullptr;
  QSplitter *m_mainSplitter = nullptr;
  QSplitter *m_editorSplitter = nullptr;
  QLabel *m_cursorPositionLabel = nullptr;
  QLabel *m_errorLabel = nullptr;
  QAction *m_splitAction = nullptr;
  QAction *m_closeAction = nullptr;
  QAction *m_openAction = nullptr;
  QAction *m_openFolderAction = nullptr;
  QAction *m_helpExampleAction = nullptr;

  // ── 自定义标题栏 ──
  QWidget *m_titleBar = nullptr;
  QLabel *m_titleLabel = nullptr;
  QPushButton *m_splitBtn = nullptr;
  QPushButton *m_minBtn = nullptr;
  QPushButton *m_maxBtn = nullptr;
  QPushButton *m_closeBtn = nullptr;
  QPushButton *m_buildBtn = nullptr;
  QComboBox *m_startupCombo = nullptr;  ///< 启动项下拉框
  QPushButton *m_saveBtn = nullptr;     ///< 保存按钮
  QPushButton *m_saveAllBtn = nullptr;  ///< 保存全部按钮

  // ── 输出面板 ──
  QSplitter *m_contentSplitter = nullptr;  ///< 垂直分割器：编辑器 + 输出面板
  CodeLog *m_outputPanel = nullptr;        ///< 脚本运行结果输出（带行号）
};