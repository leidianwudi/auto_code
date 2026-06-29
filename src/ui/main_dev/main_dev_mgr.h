/**
 * @file main_dev_mgr.h
 * @brief MainDev 控制器层（单例 + 主窗口）
 *
 * 继承 QMainWindow 作为主窗口，继承 Singleton 实现全局唯一实例。
 * 所有 UI 操作委托给 MainDevUi，不直接操控 Qt 控件。
 */

#pragma once

#include "src/tool/design/Singleton.h"
#include <QMainWindow>


class QTreeWidgetItem;
class QTabWidget;
class CodeEditor;
class MainDevUi;
class MainDevModel;

/**
 * @class MainDevMgr
 * @brief 编辑器管理器（单例主窗口）
 *
 * MVC 中的控制器层 + 壳：
 * - 创建 MainDevUi（视图）和 MainDevModel（数据）
 * - 处理所有业务逻辑和信号槽
 * - 提供静态方法供其他模块调用 UI 功能
 */
class MainDevMgr : public QMainWindow, public Singleton<MainDevMgr> {
  Q_OBJECT

public:
  /// 构造时自动完成窗口初始化
  MainDevMgr(QWidget *parent = nullptr);
  ~MainDevMgr() override = default;

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
  /// 查找并加载 file/ 目录
  void loadFiles();
  /// 为文件路径创建编辑器实例（含高亮器）
  CodeEditor *createEditorForFile(const QString &filePath);
  /// 在编辑器中打开文件（查重 → 读取 → 创建 → 显示）
  CodeEditor *openFileInEditor(const QString &filePath);
  /// 获取当前活跃的编辑器
  CodeEditor *currentEditor() const;
  /// 获取当前活跃的面板组
  QTabWidget *currentTabWidget() const;
  /// 连接编辑器的光标位置信号
  void connectEditor(CodeEditor *editor);

  MainDevUi *m_ui = nullptr;
  MainDevModel *m_model = nullptr;
};