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

#include <QFuture>
#include <QHash>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QStack>

#include "src/engine/script/ac_debugger.h"
#include "src/util/ui/aui_mgr.h"

class QTabWidget;
class QTimer;
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
  ~MainDevMgr() override;

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
  /// 停止正在执行的脚本（设置取消标志，工作线程轮询检查）
  void onStopScript();
  /// 调试按钮点击：启动调试会话（若已暂停则继续执行）
  void onDebugBtnClicked();
  /// 编辑器 F5：启动调试/继续
  void onDebugStart();
  /// 编辑器 F10：单步执行
  void onDebugStepOver();
  /// 编辑器 F11：单步进入
  void onDebugStepInto();
  /// 编辑器 Shift+F11：单步跳出
  void onDebugStepOut();
  /// 双击断点面板条目：打开文件并定位到断点行
  void onBreakpointActivated(const QString &filePath, int line);
  /// 断点面板切换生效状态：更新对应编辑器中该断点的生效标记
  void onBreakpointToggleEnabledRequested(const QString &filePath, int line, bool enabled);
  /// 断点面板删除单个断点
  void onBreakpointDeleteRequested(const QString &filePath, int line);
  /// 断点面板删除全部断点
  void onBreakpointRemoveAllRequested();
  /// 调试器暂停（工作线程阻塞中），高亮当前行
  void onDebuggerPaused(const QString &filePath, int line, const QVector<AcDebugFrame> &stack,
                        const QList<AcDebugVar> &vars);
  /// 调试器恢复执行，清除行高亮
  void onDebuggerResumed();
  /// 调试会话结束，复位状态
  void onDebuggerFinished();

private:
  /// 查找并加载 file/ 目录
  void loadFiles();
  /// 连接所有信号槽（在 onCreateWindow 中调用）
  void initUi();
  // ── initUi 子方法（按职责拆分）──
  void connectFileActions();   ///< 文件打开/帮助/重命名/删除
  void connectSaveActions();   ///< 保存/Ctrl+S/保存全部
  void connectVisualToggle();  ///< 可视化/代码切换按钮
  void connectBuildAction();   ///< 执行按钮
  void connectDebugAction();   ///< 调试按钮与调试器信号
  void connectEditorPanels();  ///< 编辑器面板信号 + 事件过滤器
  /// 连接单个编辑器面板的信号（关闭/切换/标签栏交互）
  void connectEditorPanel(QTabWidget *tabs);
  /// 为文件路径创建编辑器实例（含高亮器 + 验证模式）
  CodeEditor *createEditorForFile(const QString &filePath);
  /// 在编辑器中打开文件（查重 → 读取 → 创建 → 显示）
  CodeEditor *openFileInEditor(const QString &filePath, QTabWidget *target = nullptr);
  /// 获取当前活跃的编辑器
  CodeEditor *currentEditor() const;
  /// 在所有编辑面板中查找已打开指定文件的编辑器（未打开则返回 nullptr）
  CodeEditor *findEditorForFile(const QString &filePath) const;
  /// 收集所有编辑器中的断点并刷新调试面板「断点」页
  void refreshBreakpointList();
  /// 将当前全部断点持久化到磁盘（程序重启后还原）
  void saveBreakpointsToDisk();
  /// 从磁盘加载断点到持久存储（程序启动时调用）
  void loadBreakpointsFromDisk();
  /// 保存当前打开的文件列表到设置（下次启动还原）
  void saveOpenFilesToSettings();
  /// 从设置还原上次打开的文件列表并重新打开
  void restoreOpenFilesFromSettings();
  /// 收集全部已打开编辑器及持久化断点（文件路径 → 行号 → 是否生效），供调试器使用
  QMap<QString, QMap<int, bool>> debugBreakpoints();
  /// 获取当前活跃的面板组
  QTabWidget *currentTabWidget() const;
  /// 连接编辑器的光标位置信号
  void connectEditor(CodeEditor *editor);
  /// 关闭指定面板中的指定标签页（不依赖 sender()）
  void closeTab(QTabWidget *tabs, int index);
  /// 检查所有编辑器的修改状态，更新保存按钮可用性
  void updateSaveButtonState();
  /// 保存编辑器并同步其他打开同一文件的编辑器实例内容
  /// （拆分副本场景：一个编辑器保存后，其他副本自动更新为最新内容）
  bool saveAndSync(CodeEditor *editor);
  /// 同步指定文件的所有其他编辑器实例内容（排除 sourceEditor）
  void syncEditorsForFile(const QString &filePath, const QString &content,
                          CodeEditor *sourceEditor);
  /// 保存前同步 JsonVueWidget 可视化数据到代码编辑器
  void syncJsonVueBeforeSave();
  /// 应用设置后刷新全局样式与编辑器高亮（主题/颜色变化时调用）
  void refreshTheme();
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

  // 脚本执行（工作线程）
  QFuture<void> m_scriptFuture;  ///< 脚本执行任务（工作线程）
  bool m_scriptRunning = false;  ///< 脚本是否正在执行

  // 调试会话
  AcDebugger *m_debugger = nullptr;     ///< 调试器（由 MainDevMgr 创建并持有）
  bool m_debugging = false;             ///< 是否处于调试模式
  CodeEditor *m_debugEditor = nullptr;  ///< 当前调试会话的目标编辑器（用于行高亮）

  // 断点持久化：按文件路径保存断点（行号 → 是否生效），关闭后重新打开仍保留
  QHash<QString, QMap<int, bool>> m_persistedBreakpoints;

  /// 会话恢复：为 true 时抑制由打开文件触发的目录树定位，
  /// 避免启动还原上次打开的文件时自动展开/滚动目录树，破坏保存的展开状态
  bool m_restoringSession = false;

  /// 主题刷新防抖定时器：合并短时间内多次颜色变化，减少切换卡顿
  QTimer *m_themeTimer = nullptr;

  /// 启动一次调试会话（收集当前编辑器断点，运行脚本）
  void startDebugSession();
};