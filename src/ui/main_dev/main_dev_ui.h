/**
 * @file main_dev_ui.h
 * @brief MainDev UI 层
 *
 * 负责所有界面控件的创建和布局，包括：
 * - 菜单栏
 * - 左侧文件树
 * - 右侧可拆分编辑器区域
 * - 底部状态栏
 *
 * 所有 Qt 控件由本类持有，控制器通过 getter 访问。
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

/**
 * @class DimmableTabWidget
 * @brief 支持 tab 文字变灰的 QTabWidget 子类
 *
 * 使用 Fusion 风格绘制 tab bar，确保 setTabTextColor 生效。
 * Windows 原生风格会忽略 setTabTextColor，必须用 Fusion 替代。
 */
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

/**
 * @class MainDevUi
 * @brief 编辑器 UI 管理层
 *
 * 创建并持有所有界面控件，提供 getter 供控制器访问。
 * 不包含任何业务逻辑或信号连接。
 */
class MainDevUi : public QObject {
  Q_OBJECT

public:
  explicit MainDevUi(QObject *parent = nullptr);
  ~MainDevUi() override = default;

  /// 初始化界面布局（菜单栏、文件树、编辑器区域、状态栏）
  void setupUI(QMainWindow *window);

  /// 创建一个新的编辑器面板组（QTabWidget）
  QTabWidget *createEditorPanel();

  // ── 控件 getter ──
  QTreeWidget *fileTree() const { return m_fileTree; }
  QSplitter *mainSplitter() const { return m_mainSplitter; }
  QSplitter *editorSplitter() const { return m_editorSplitter; }
  QLabel *cursorPositionLabel() const { return m_cursorPositionLabel; }
  QAction *splitAction() const { return m_splitAction; }
  QAction *closeAction() const { return m_closeAction; }

private:
  QTreeWidget *m_fileTree = nullptr;       ///< 左侧文件树
  QSplitter *m_mainSplitter = nullptr;     ///< 主分割器（树 | 编辑器）
  QSplitter *m_editorSplitter = nullptr;   ///< 编辑器分割器
  QLabel *m_cursorPositionLabel = nullptr; ///< 状态栏光标位置标签
  QAction *m_splitAction = nullptr;        ///< 向右拆分菜单动作
  QAction *m_closeAction = nullptr;        ///< 关闭标签页菜单动作
};