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
#include <QShowEvent>
#include <QSplitter>
#include <QTabWidget>

class QVBoxLayout;

#include "main_dev_ui_ext.h"
#include "src/util/ui/code/code_log.h"
#include "src/util/ui/code/tree_dir.h"

// ════════════════════════════════════════════════════════════
//  MainDevUi
// ════════════════════════════════════════════════════════════

class MainDevUi : public QMainWindow {
  Q_OBJECT

public:
  explicit MainDevUi(QWidget *parent = nullptr);
  ~MainDevUi() override = default;

  /// 初始化界面布局
  void setupUI();

  // ──────────────────────────────────────────────────────────
  //  窗口标题常量
  // ──────────────────────────────────────────────────────────

  static QString defaultTitle() { return QStringLiteral("Auto Code"); }
  static QString devTitle() { return QStringLiteral("Auto Code - 开发模式"); }
  static QString fileTitle(const QString &fileName) {
    return QStringLiteral("Auto Code - %1").arg(fileName);
  }
  static QString cursorDefault() { return QStringLiteral("行: -, 列: -"); }

  // ──────────────────────────────────────────────────────────
  //  编辑器面板组
  // ──────────────────────────────────────────────────────────

  /// 创建一个新的编辑器面板组
  QTabWidget *createEditorPanel();

  /// 获取编辑器面板数量
  int editorPanelCount() const;
  /// 获取指定索引处的编辑器面板
  QTabWidget *editorPanelAt(int index) const;
  /// 查找指定面板在分割器中的索引
  int editorPanelIndex(const QTabWidget *tabs) const;
  /// 在分割器末尾添加一个新面板
  void addEditorPanel(QTabWidget *panel);
  /// 移除并销毁指定索引处的面板
  void removeEditorPanelAt(int index);
  /// 将所有面板设置为等宽拉伸
  void setEditorPanelsUniformStretch();

  // ════════════════════════════════════════════════════════════
  //  文件树
  // ════════════════════════════════════════════════════════════

  TreeDir *fileTree() const { return m_fileTree; }

  // ════════════════════════════════════════════════════════════
  //  主分割器
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

  void appendOutput(const QString &text, bool isError = false);
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

  /// 刷新启动项下拉框
  void refreshStartupCombo();

private:
  /// 构建自定义标题栏
  void setupTitleBar();
  /// 构建编辑器区域（文件树 + 分割器 + 编辑器面板）
  void setupEditorArea();
  /// 构建底部状态栏并返回内容控件
  void setupStatusBar(QWidget *contentWidget, QVBoxLayout *contentLayout);
  // ── 窗口事件 ──
  void showEvent(QShowEvent *event) override;
  void closeEvent(QCloseEvent *event) override;
  void changeEvent(QEvent *ev) override;
  bool eventFilter(QObject *obj, QEvent *ev) override;
#if defined(Q_OS_WIN)
  bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

  // ── 成员变量 ──
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
  QPushButton *m_buildBtn = nullptr;
  QComboBox *m_startupCombo = nullptr;
  QPushButton *m_saveBtn = nullptr;
  QPushButton *m_saveAllBtn = nullptr;

  // ── 输出面板 ──
  QSplitter *m_contentSplitter = nullptr;
  CodeLog *m_outputPanel = nullptr;
};