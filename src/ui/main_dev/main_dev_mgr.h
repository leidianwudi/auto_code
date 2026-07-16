/**
 * @file main_dev_mgr.h
 * @brief 代码编辑器控制器（单例）
 *
 * 继承 AuiMgr<MainDevMgr> 作为 UI 控制器，所有 UI 操作委托给 MainDevUi。
 * MainDevUi 负责实际的界面呈现（QMainWindow）。
 *
 * 架构：
 *   MainDevMgr (控制器 + 单例) ─── 创建并管理 ──→ MainDevUi (视图)
 *                                               └── 创建并读写 ──→ MainDevModel (数据)
 */

#pragma once

#include <QObject>
#include <QStack>

#include "src/util/ui/aui_mgr.h"

class QTabWidget;
class CodeEditor;
class MainDevUi;
class MainDevModel;

/// @brief 导航历史记录项
struct NavigationEntry {
  QString filePath;  ///< 文件路径
  int line = 0;      ///< 行号（1-based）
  int column = 0;    ///< 列号（1-based）
};

/**
 * @class MainDevMgr
 * @brief 编辑器管理器（单例 UI 控制器）
 *
 * MVC 中的控制器层：
 * - 继承 AuiMgr<MainDevMgr>，通过 ins() 获取全局唯一实例
 * - onCreateWindow() 创建 MainDevUi（QMainWindow）并初始化
 * - 处理所有业务逻辑和信号槽
 * - 提供静态方法供其他模块调用 UI 功能
 */
class MainDevMgr : public AuiMgr<MainDevMgr> {
  Q_OBJECT

  // CRTP 基类 AuiMgr<MainDevMgr> 需要访问 onCreateWindow()
  friend class AuiMgr<MainDevMgr>;

public:
  MainDevMgr() = default;
  ~MainDevMgr() override = default;

  // ── 静态方法：供其他模块调用 ──

  /// 在编辑器中打开指定文件
  static void openFile(const QString &filePath);

  /// 向右拆分编辑器
  static void splitRight();

  /// 关闭当前标签页
  static void closeCurrentEditor();

protected:
  /// 创建并初始化 MainDevUi 窗口（首次 open() 时调用）
  QWidget *onCreateWindow() override;

private slots:
  /// 向右拆分编辑器
  void onSplitRight();
  /// 关闭当前标签页
  void onCloseEditor();
  /// 标签页关闭按钮被点击
  void onTabCloseRequested(int index);
  /// 当前标签页切换时更新窗口标题
  void onCurrentTabChanged(int index);
  /// 标签栏被点击时激活对应面板（处理点击已选中标签的场景）
  void onTabBarClicked(int index);
  /// 更新状态栏光标位置（从当前焦点编辑器读取）
  void updateCursorPosition();
  /// 应用程序焦点变化时，连接新焦点的编辑器信号
  void onFocusChanged(QWidget *oldFocus, QWidget *newFocus);
  /// 编辑器验证结果变化时，更新状态栏错误信息
  void onValidationMessage(const QString &msg, int errorCount);
  /// 处理文件/文件夹重命名请求
  void onRenameFile(const QString &oldPath, const QString &newName);
  /// 处理文件/文件夹删除请求
  void onDeleteFile(const QString &path);
  /// 右键菜单：关闭其它标签页
  void onCloseOthers(int index);
  /// 右键菜单：关闭所有标签页
  void onCloseAll();
  /// 跨文件跳转（从 CodeEditor 的 requestGoToLine 信号触发）
  void onGoToLine(const QString &filePath, int line);
  /// 即将导航（记录当前位置到历史栈）
  void onAboutToNavigate(const QString &targetFilePath, int targetLine);
  /// 鼠标侧键：后退（XButton1）
  void navigateBack();
  /// 鼠标侧键：前进（XButton2）
  void navigateForward();

private:
  /// 查找并加载 file/ 目录
  void loadFiles();
  /// 连接所有信号槽（在 onCreateWindow 中调用）
  void initUi();
  /// 为文件路径创建编辑器实例（含高亮器 + 验证模式）
  CodeEditor *createEditorForFile(const QString &filePath);
  /// 在编辑器中打开文件（查重 → 读取 → 创建 → 显示）
  CodeEditor *openFileInEditor(const QString &filePath);
  /// 获取当前活跃的编辑器
  CodeEditor *currentEditor() const;
  /// 获取当前活跃的面板组
  QTabWidget *currentTabWidget() const;
  /// 连接编辑器的光标位置信号
  void connectEditor(CodeEditor *editor);
  /// 关闭指定面板中的指定标签页（不依赖 sender()）
  void closeTab(QTabWidget *tabs, int index);
  /// 检查所有编辑器的修改状态，更新保存按钮可用性
  void updateSaveButtonState();
  /// 推入导航历史记录
  void pushNavigationHistory(const QString &filePath, int line, int column = 0);
  /// 跳转到指定位置（内部使用，不推入历史）
  void jumpToLocation(const QString &filePath, int line, int column = 0);

protected:
  /// 事件过滤器（用于捕获鼠标侧键）
  bool eventFilter(QObject *obj, QEvent *event) override;

  MainDevUi *m_ui = nullptr;
  MainDevModel *m_model = nullptr;

  // 导航历史栈
  QStack<NavigationEntry> m_navHistory;       ///< 后退栈
  QStack<NavigationEntry> m_navForwardStack;  ///< 前进栈
  bool m_navigating = false;                  ///< 是否正在执行导航（避免循环记录）
};