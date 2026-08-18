/**
 * @file debug_controller.h
 * @brief 调试与脚本执行控制器（从 MainDevMgr 拆出）
 *
 * 职责（原 MainDevMgr 的调试相关部分）：
 * - 脚本执行：工作线程运行 AcEngine、运行状态维护、执行/停止按钮状态
 * - 调试会话：AcDebugger 生命周期、启动/继续/单步、暂停时行高亮与面板填充
 * - 断点管理：编辑器断点收集、跨文件持久化到磁盘、调试面板「断点」列表刷新
 *
 * 与 MainDevMgr 的协作通过回调注入（避免循环依赖）：
 * - setEditorProvider：提供当前活跃编辑器（启动调试时记录高亮目标）
 * - setFileOpener：按路径打开文件并返回编辑器（断点命中自动打开文件，VSCode 风格）
 * - navigateToRequested 信号：双击断点/调用栈/变量条目 → 主窗口打开文件并定位
 */

#pragma once

#include <QFuture>
#include <QHash>
#include <QMap>
#include <QObject>

#include <functional>

#include "src/engine/script/ac_debugger.h"

class CodeEditor;
class MainDevUi;

/**
 * @class DebugController
 * @brief 调试/脚本执行控制器
 *
 * 由 MainDevMgr 创建并持有（onCreateWindow 时构造 + init），
 * 界面元素（调试面板、执行/停止按钮、启动项下拉框）通过 MainDevUi 访问。
 */
class DebugController : public QObject {
  Q_OBJECT

public:
  explicit DebugController(MainDevUi *ui, QObject *parent = nullptr);
  ~DebugController() override;

  /// 初始化：创建调试器接入引擎、连接调试面板信号（onCreateWindow 时调用一次）
  void init();

  // ── 脚本执行 ──

  /// 脚本是否正在执行（含调试会话）
  bool isScriptRunning() const { return m_scriptRunning; }

  /// 在工作线程执行脚本（debug=true 时启动调试会话）
  void runScript(const QString &scriptPath, const QString &rootDir, bool debug);

  /// 请求停止正在执行的脚本/调试会话（唤醒调试阻塞 + 设置取消标志）
  void stopScript();

  /// 退出前清理：唤醒调试等待、请求取消并等待工作线程结束（阻塞，析构/aboutToQuit 调用）
  void shutdownAndWait();

  // ── 调试控制 ──

  /// 是否处于调试会话中
  bool isDebugging() const { return m_debugging; }

  /// 调试按钮 / F5：未调试则启动会话；已暂停则继续执行
  void startOrContinue();

  /// 单步执行（F10）
  void stepOver();
  /// 单步进入（F11）
  void stepInto();
  /// 单步跳出（Shift+F11）
  void stepOut();

  /// 注入「获取当前活跃编辑器」回调（启动调试时记录高亮目标）
  void setEditorProvider(std::function<CodeEditor *()> provider) {
    m_currentEditor = std::move(provider);
  }

  /// 注入「按路径打开文件并返回编辑器」回调（断点命中未打开文件时自动打开）
  void setFileOpener(std::function<CodeEditor *(const QString &)> opener) {
    m_openFile = std::move(opener);
  }

  // ── 断点管理 ──

  /// 从磁盘加载持久化断点到内存（程序启动时调用）
  void loadBreakpointsFromDisk();

  /// 同步所有编辑器断点 → 持久存储 → 调试面板列表 → 磁盘（断点任何变化后调用）
  void refreshBreakpointList();

  /// 保存持久化断点到磁盘（窗口关闭前调用）
  void saveBreakpointsToDisk();

  /// 打开文件时恢复该文件的持久化断点：
  /// 剔除超出总行数的失效行并同步更新持久存储，返回可恢复的断点集
  QMap<int, bool> takeBreakpointsForFile(const QString &filePath, int totalLines);

  /// 断点面板：切换某断点的生效状态（编辑器已打开则改编辑器，否则改持久存储）
  void setBreakpointEnabled(const QString &filePath, int line, bool enabled);

  /// 断点面板：删除单个断点
  void removeBreakpoint(const QString &filePath, int line);

  /// 断点面板：删除全部断点（所有编辑器 + 持久存储）
  void removeAllBreakpoints();

  /// 收集全部断点（已打开编辑器 + 已关闭文件的持久化断点），供调试器按文件命中
  QMap<QString, QMap<int, bool>> collectBreakpoints();

signals:
  /// 双击断点/调用栈/变量条目：请求主窗口打开文件并定位到行
  void navigateToRequested(const QString &filePath, int line);

private slots:
  /// 调试器暂停（工作线程阻塞中）：高亮当前行、填充调用栈/变量面板
  void onDebuggerPaused(const QString &filePath, int line, const QVector<AcDebugFrame> &stack,
                        const QList<AcDebugVar> &vars);
  /// 调试器恢复执行：清除行高亮
  void onDebuggerResumed();
  /// 调试会话结束：复位面板状态
  void onDebuggerFinished();

private:
  /// 启动一次调试会话（收集断点、运行脚本）
  void startDebugSession();

  MainDevUi *m_ui = nullptr;             ///< 主窗口视图（调试面板/按钮/输出）
  AcDebugger *m_debugger = nullptr;      ///< 调试器（接入引擎的工作线程解释器）
  bool m_debugging = false;              ///< 是否处于调试会话中
  bool m_scriptRunning = false;          ///< 脚本（含调试）是否正在执行
  QFuture<void> m_scriptFuture;          ///< 脚本执行任务（工作线程）
  CodeEditor *m_debugEditor = nullptr;   ///< 当前调试会话的行高亮目标编辑器
  QHash<QString, QMap<int, bool>> m_persistedBreakpoints;  ///< 文件路径 → 行号 → 是否生效

  std::function<CodeEditor *()> m_currentEditor;             ///< 当前活跃编辑器提供者
  std::function<CodeEditor *(const QString &)> m_openFile;   ///< 按路径打开文件
};
