/**
 * @file main_dev_mgr.h
 * @brief MainDev 管理器层
 *
 * 采用单例模式，管理所有编辑器的业务逻辑和信号处理。
 * 提供静态方法供其他模块调用，实现 UI 操作入口。
 *
 * 使用方式：
 *   MainDevMgr::openFile("path/to/file.ac");  // 其他模块直接调用
 */

#pragma once

#include <QObject>

class QTreeWidgetItem;
class QWidget;
class QMainWindow;
class QTabWidget;
class CodeEditor;
class MainDevUi;
class MainDevModel;

/**
 * @class MainDevMgr
 * @brief 编辑器管理器（单例）
 *
 * 职责：
 * 1. 连接 UI 控件信号到管理器槽函数
 * 2. 实现文件打开、拆分、关闭等所有业务逻辑
 * 3. 提供静态方法让其他模块调用 UI 功能
 */
class MainDevMgr : public QObject {
  Q_OBJECT

public:
  /// 获取单例实例
  static MainDevMgr *instance();

  /**
   * @brief 初始化管理器
   * @param ui UI 层指针
   * @param model 数据模型指针
   * @param window 主窗口指针（用于 setWindowTitle / statusBar）
   */
  static void init(MainDevUi *ui, MainDevModel *model, QMainWindow *window);

  /// 扫描并加载 file/ 目录下的文件树
  void loadFiles();

  // ── 静态方法：供其他模块调用 ──

  /// 在编辑器中打开指定文件
  static void openFile(const QString &filePath);

  /// 向右拆分编辑器
  static void splitRight();

  /// 关闭当前标签页
  static void closeCurrentEditor();

private slots:
  /// 点击文件树节点，打开对应文件
  void onTreeItemClicked(QTreeWidgetItem *item, int column);
  /// 向右拆分编辑器
  void onSplitRight();
  /// 关闭当前标签页
  void onCloseEditor();
  /// 标签页关闭按钮被点击
  void onTabCloseRequested(int index);
  /// 当前标签页切换时更新窗口标题
  void onCurrentTabChanged(int index);
  /// 更新状态栏光标位置（从当前焦点编辑器读取）
  void updateCursorPosition();
  /// 应用程序焦点变化时，连接新焦点的编辑器信号
  void onFocusChanged(QWidget *oldFocus, QWidget *newFocus);

private:
  MainDevMgr() = default;

  static MainDevMgr *s_instance;

  MainDevUi *m_ui = nullptr;
  MainDevModel *m_model = nullptr;
  QMainWindow *m_window = nullptr;

  // ── 内部辅助方法 ──

  void addDirectoryToTree(QTreeWidgetItem *parentItem, const QString &dirPath);
  CodeEditor *createEditorForFile(const QString &filePath);
  CodeEditor *openFileInEditor(const QString &filePath);
  CodeEditor *currentEditor() const;
  QTabWidget *currentTabWidget() const;
  void connectEditor(CodeEditor *editor);
  void applyTabDimming(QTabWidget *active);
  void setWindowTitles(const QString &text);
};