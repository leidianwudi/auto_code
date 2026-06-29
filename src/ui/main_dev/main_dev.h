/**
 * @file main_dev.h
 * @brief 代码编辑主窗口（类 VS Code 风格）
 *
 * 提供文件树浏览、多标签页编辑器、向右拆分编辑器等功能。
 * 自动加载 file/ 目录下的 .ac 和 .json 文件。
 */

#pragma once

#include <QHash>
#include <QMainWindow>
#include <QSplitter>
#include <QString>
#include <QTabWidget>
#include <QTreeWidget>

class CodeEditor;
class QTreeWidgetItem;

/**
 * @class MainDev
 * @brief 代码开发主窗口
 *
 * 类似 VS Code 的布局：
 * - 左侧：文件树（TreeWidget）
 * - 右侧：可拆分的编辑器区域（多组 QTabWidget，每组含多个 CodeEditor 标签页）
 * - 每个标签页显示文件名，带关闭按钮
 * - 根据文件后缀自动选择语法高亮器
 */
class MainDev : public QMainWindow {
  Q_OBJECT

public:
  explicit MainDev(QWidget *parent = nullptr);
  ~MainDev() override = default;

private slots:
  /// 点击文件树节点，打开对应文件
  void onTreeItemClicked(QTreeWidgetItem *item, int column);
  /// 向右拆分编辑器（创建新的编辑器面板组）
  void onSplitRight();
  /// 关闭当前标签页
  void onCloseEditor();
  /// 标签页关闭按钮被点击
  void onTabCloseRequested(int index);
  /// 当前标签页切换时更新窗口标题
  void onCurrentTabChanged(int index);

private:
  /// 初始化界面布局
  void setupUI();
  /// 扫描 file/ 目录，递归加载 .ac 和 .json 文件到文件树
  void loadFiles();
  /// 递归将目录下的文件添加到树节点
  void addDirectoryToTree(QTreeWidgetItem *parentItem, const QString &dirPath);
  /// 根据文件类型创建 CodeEditor 并绑定语法高亮器
  CodeEditor *createEditorForFile(const QString &filePath);
  /// 在编辑器中打开指定文件（若已打开则切换到对应标签页）
  CodeEditor *openFileInEditor(const QString &filePath);
  /// 获取当前激活的编辑器
  CodeEditor *currentEditor() const;
  /// 获取当前激活的标签页控件
  QTabWidget *currentTabWidget() const;
  /// 创建一个新的编辑器面板组（QTabWidget + 空 CodeEditor）
  QTabWidget *createEditorPanel();

  /// 用户数据角色：存储文件完整路径
  static constexpr int FilePathRole = Qt::UserRole + 1;

  QTreeWidget *m_fileTree;     ///< 左侧文件树
  QSplitter *m_mainSplitter;   ///< 主分割器（树 | 编辑器）
  QSplitter *m_editorSplitter; ///< 编辑器分割器（包含多个 QTabWidget）
  QHash<QString, CodeEditor *>
      m_openFiles; ///< filePath -> editor 映射（跨所有面板）
};