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

#include <QFutureWatcher>
#include <QHash>
#include <QMap>
#include <QObject>
#include <QStack>

#include "src/engine/validation_result.h"
#include "src/engine/rename/symbol_rename.h"
#include "src/ui/main_dev/main_dev_ui_ext.h"
#include "src/util/common/workspace_diag.h"
#include "src/util/ui/aui_mgr.h"

class QTabWidget;
class QTimer;
class CodeEditor;
class DebugController;
class MainDevUi;
class MainDevModel;
class JsonVueWidget;

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
  /// 标签拖拽到面板左/右边缘后松开 → 拆分（移动标签到新面板）
  void onTabSplitDropped(int fromIndex, DraggableTabBar *fromBar, SplitSide side);
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
  /// 编辑器验证结果变化时，更新文件树/标签栏错误状态
  void onValidationMessage(const QString &msg, int errorCount);
  /// 编辑器结构化验证结果：填充底部“问题”面板
  void onValidationIssues(const QString &filePath, const QVector<ValidationResult> &issues);
  /// 后台工作区全量错误扫描完成：合并结果到问题面板
  void onWorkspaceScanFinished();
  /// 处理文件/文件夹重命名请求
  void onRenameFile(const QString &oldPath, const QString &newName);
  /// 处理文件/文件夹移动请求（目录树拖拽，移动到目标文件夹）
  void onMoveFile(const QString &oldPath, const QString &targetDir);
  /// 处理文件/文件夹删除请求
  void onDeleteFile(const QString &path);
  /// 右键菜单：关闭其它标签页
  void onCloseOthers(int index);
  /// 右键菜单：关闭所有标签页
  void onCloseAll();
  /// 跨文件跳转（从 CodeEditor 的 requestGoToLine 信号触发）
  void onGoToLine(const QString &filePath, int line);
  /// 查找/引用面板结果跳转：打开文件并定位到匹配处（不选中匹配词，
  /// 避免蓝色文本选区盖住查找/引用高亮的浅红色，与 VSCode 一致）
  void onOpenHighlightResult(const QString &filePath, int line, int column, int length);
  /// F2 重命名符号：弹窗输入新名 → 后台语义收集引用 → 应用替换
  void onRenameSymbol(const QString &filePath, int line, int column, const QString &name);
  /// 即将导航（记录当前位置到历史栈）
  void onAboutToNavigate(const QString &targetFilePath, int targetLine);
  /// 鼠标侧键：后退（XButton1）
  void navigateBack();
  /// 鼠标侧键：前进（XButton2）
  void navigateForward();
  /// 左侧 tab（文件/调试/查找/引用）切换：引用面板显示时恢复编辑器引用高亮，
  /// 隐藏时清除（与 VSCode「引用面板可见则编辑器持续变色」一致）
  void onLeftTabChanged(int index);
  /// 对所有已打开编辑器应用引用高亮（基于最近一次语义收集的位置）
  void applyReferenceRefsToEditors();
  /// 清除所有已打开编辑器的引用高亮
  void clearReferenceHighlightFromEditors();
  /// 对所有已打开编辑器应用查找面板搜索高亮（关键词）
  void applySearchHighlightToEditors(const QString &text);
  /// 清除所有已打开编辑器的查找搜索高亮
  void clearSearchHighlightFromEditors();
  /// 根据左侧面板当前选中项统一同步两类高亮（引用/查找互斥，VSCode 行为）
  void resyncPanelHighlights();
  /// 依据左侧面板当前选中项，为单个编辑器应用对应高亮（新打开文件时调用）
  void applyPanelHighlightToEditor(CodeEditor *editor);

private:
  /// 查找并加载 file/ 目录
  void loadFiles();
  /// 重命名/移动共用：文件系统变更 + 更新已打开编辑器 + 更新启动项 + 刷新树
  void applyRenameOrMove(const QString &oldPath, const QString &newPath, bool isDir,
                         const QString &displayName);
  /// 连接所有信号槽（在 onCreateWindow 中调用）
  void initUi();
  // ── initUi 子方法（按职责拆分）──
  void connectFileActions();   ///< 文件打开/帮助/重命名/删除
  void connectSaveActions();   ///< 保存/Ctrl+S/保存全部
  void connectVisualToggle();  ///< 可视化/代码切换按钮
  void connectBuildAction();   ///< 执行按钮
  void connectEditorPanels();  ///< 编辑器面板信号 + 事件过滤器
  /// 连接单个编辑器面板的信号（关闭/切换/标签栏交互）
  void connectEditorPanel(QTabWidget *tabs);
  /// 为文件路径创建编辑器实例（含高亮器 + 验证模式）
  CodeEditor *createEditorForFile(const QString &filePath);
  /// 在编辑器中打开文件（查重 → 读取 → 创建 → 显示）
  CodeEditor *openFileInEditor(const QString &filePath, QTabWidget *target = nullptr);
  /// 查重：在所有编辑面板中查找已打开指定文件的编辑器；未打开返回 nullptr。
  /// 命中时可选输出所在面板组与索引（供选中/聚焦该标签）
  CodeEditor *findOpenEditor(const QString &filePath, QTabWidget **outTabs = nullptr,
                             int *outIndex = nullptr);
  /// 创建编辑标签页（jsonvue 可视化包装器 / jsonsource / 普通编辑器），并输出 CodeEditor
  QWidget *createEditorTab(const QString &filePath, const QString &content,
                           CodeEditor **editorOut);
  /// 解析 HTTP 配置 AC 脚本路径：优先最近 api_auth_data.ac，回落启动项 AC 脚本
  QString resolveHttpConfigAcPath(const QString &filePath) const;
  /// 连接编辑器修改标记：tab 圆点 + 树节点圆点 + 保存按钮状态。
  /// 普通文件与 jsonvue 统一实现（都走 CodeEditor::document 的 modificationChanged）
  void connectModifiedTracking(QTabWidget *tabs, CodeEditor *editor, const QString &filePath);
  /// 获取当前活跃的编辑器
  CodeEditor *currentEditor() const;
  /// 在所有编辑面板中查找已打开指定文件的编辑器（未打开则返回 nullptr）
  CodeEditor *findEditorForFile(const QString &filePath) const;
  /// 保存当前打开的文件列表到设置（下次启动还原）
  void saveOpenFilesToSettings();
  /// 从设置还原上次打开的文件列表并重新打开
  void restoreOpenFilesFromSettings();
  /// 获取当前活跃的面板组
  QTabWidget *currentTabWidget() const;
  /// 连接编辑器的全部信号（编辑器创建时调用一次，不重复连接/断开）
  void connectEditorSignals(CodeEditor *editor);
  /// 设置当前活跃编辑器（焦点/tab 切换时调用，仅更新光标状态栏）
  void setActiveEditor(CodeEditor *editor);
  /// 重建底部「问题」面板（从 m_fileIssues 聚合全部已打开文件的问题）
  void refreshProblemPanel();
  /// 应用单个文件的重命名替换（已打开编辑器走 document 支持撤销；未打开直接改写文件）
  void applyRenameToFile(const QString &filePath, const QVector<RenameRef> &refs,
                         const QString &newName);
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
  /// 窗口字体变化后的轻量刷新：字体已由 SettingStore 应用到所有窗口，
  /// 这里只需重建标题栏文字样式（标题字号随窗口字号缩放），避免走 refreshTheme 重活
  void refreshWindowFont();
  /// 推入导航历史记录
  void pushNavigationHistory(const QString &filePath, int line, int column = 0);
  /// 跳转到指定位置（内部使用，不推入历史）
  void jumpToLocation(const QString &filePath, int line, int column = 0);

protected:
  /// 事件过滤器（用于捕获鼠标侧键）
  bool eventFilter(QObject *obj, QEvent *event) override;

  MainDevUi *m_ui = nullptr;
  MainDevModel *m_model = nullptr;

  /// 调试/脚本执行控制器（调试会话、断点管理，从本类拆出）
  DebugController *m_debug = nullptr;
  /// 调试控制器访问（供分文件 main_dev_mgr_*.cpp 使用）
  DebugController *debugController() const { return m_debug; }

  // 导航历史栈
  QStack<NavigationEntry> m_navHistory;       ///< 后退栈
  QStack<NavigationEntry> m_navForwardStack;  ///< 前进栈
  bool m_navigating = false;                  ///< 是否正在执行导航（避免循环记录）

  /// 工作区问题聚合：文件路径 → 验证结果列表（供底部「问题」面板跨文件汇总）
  QMap<QString, QVector<ValidationResult>> m_fileIssues;

  /// 未打开文件的缓冲修改（重命名等操作：未打开文件先存缓冲、不写盘，树目录标黄，
  /// 退出时提示保存——VSCode 行为）。文件路径 → 缓冲内容。
  QHash<QString, QString> m_pendingFileChanges;

  /// 保存所有已打开且被修改的编辑器（保存全部按钮 / 退出保存共用）
  void saveAllEditors();
  /// 把未打开文件的缓冲修改写盘并清除树目录黄色标记（保存全部 / 退出保存用）
  void flushPendingChanges();
  /// 清除某文件的缓冲修改（保存或打开后）
  void clearPendingChange(const QString &filePath);
  /// 退出确认：无未保存→true；有未保存则弹窗三选（保存/不保存/取消），取消→false
  bool confirmExit();
  /// 构建实时内容快照：已打开编辑器内容 + 未打开但有缓冲修改的文件内容
  ///（供后台引用收集 / 工作区扫描优先读缓冲而非磁盘；主线程调用）
  QHash<QString, QString> collectLiveContents() const;

  /// 工作区全量扫描的后台任务监视器（扫描在 Qt 全局线程池中执行，避免阻塞 UI）
  QFutureWatcher<QVector<WorkspaceFileDiag>> *m_workspaceScanWatcher = nullptr;
  /// 当前扫描是否静默（保存后自动重扫时 true，完成时不打印"完成"提示）
  bool m_workspaceScanSilent = false;

  /// 启动一次后台工作区全量错误扫描（应用启动时调用，不阻塞 UI）
  /// @param silent 静默模式：不向输出面板打印"开始检查"提示（保存后自动重扫用）
  void startWorkspaceScan(bool silent = false);

  /// 保存/重命名后防抖重扫：合并连续保存，避免每次保存都全量扫描
  void scheduleWorkspaceRescan();
  /// 工作区重扫防抖定时器（保存/重命名后：重建语义索引 + 触发一次扫描）
  QTimer *m_workspaceRescanTimer = nullptr;
  /// 工作区扫描防抖定时器：合并编辑/保存/重命名产生的多次扫描请求为一次
  QTimer *m_scanTimer = nullptr;

  /// 会话恢复：为 true 时抑制由打开文件触发的目录树定位，
  /// 避免启动还原上次打开的文件时自动展开/滚动目录树，破坏保存的展开状态
  bool m_restoringSession = false;

  /// 主题刷新防抖定时器：合并短时间内多次颜色变化，减少切换卡顿
  QTimer *m_themeTimer = nullptr;

  /// 查找面板搜索高亮防抖定时器（合并输入时的多次搜索，减少编辑器重扫）
  QTimer *m_searchHighlightTimer = nullptr;
  /// 最近一次查找面板搜索关键词（用于切换回查找 tab 时恢复高亮）
  QString m_lastSearchText;

  /// 递增的重命名请求序号：新一轮重命名开始即自增，过期的后台收集结果据此丢弃
  int m_renameRequestId = 0;

  /// 最近一次语义收集的引用位置（供切回引用面板时恢复编辑器语义高亮）
  QVector<RenameRef> m_referenceRefs;
};