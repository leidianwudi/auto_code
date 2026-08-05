/**
 * @file ac_debugger.h
 * @brief AC 脚本调试器 — 断点/暂停/单步/停止 的线程安全协调器
 *
 * 协调工作线程中的解释器与 GUI 线程：
 * - 解释器每执行一条语句调用 onStatement() 检查是否需要暂停
 * - 命中断点或单步时，工作线程阻塞等待 GUI 的继续/单步/停止指令
 * - GUI 线程通过 continueRun()/stepInto()/stepOver()/stepOut()/stop() 控制
 *
 * 线程安全基于 QMutex + QWaitCondition 实现。
 * 当未进入调试模式（m_debugging 为 false）时，onStatement 立即返回，零开销。
 */

#pragma once

#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>
#include <QWaitCondition>
#include <functional>

/// @brief 调用栈帧
struct AcDebugFrame {
  QString funcName;  ///< 函数/方法名
  QString filePath;  ///< 函数所在源文件（用于双击定位）
  int line = 0;      ///< 函数起始行号（用于双击定位）
};

/// @brief 调试变量条目
struct AcDebugVar {
  QString scope;     ///< 作用域名（如 "局部"/"全局"/"静态.类名"/"this"）
  QString name;      ///< 变量名
  QJsonValue value;  ///< 变量值
  QString filePath;  ///< 变量声明所在源文件（用于双击定位）
  int line = 0;      ///< 变量声明行号（用于双击定位）
  QString funcName;  ///< 变量所在函数名（用于位置列展示）
};

/// @brief 断点条目
struct AcBreakpoint {
  int line = 0;         ///< 断点行号（1-based）
  bool enabled = true;  ///< 是否生效（失效的断点不触发暂停）
};

/**
 * @class AcDebugger
 * @brief AC 脚本调试器
 *
 * 单次调试会话的协调者。由 GUI 线程创建并持有，
 * 工作线程中的解释器通过 onStatement() 接入。
 */
class AcDebugger : public QObject {
  Q_OBJECT

public:
  enum StepMode { StepNone = 0, StepInto, StepOver, StepOut };

  /// 快照构建回调：暂停时由解释器填充调用栈与变量
  using SnapshotBuilder =
      std::function<void(QVector<AcDebugFrame> &stack, QList<AcDebugVar> &vars)>;

  explicit AcDebugger(QObject *parent = nullptr);

  // ── 工作线程调用（解释器） ──
  /// 每执行一条语句前调用；返回 false 表示应中止执行（用户点击停止）
  bool onStatement(const QString &filePath, int line, int callDepth,
                   const SnapshotBuilder &builder);

  // ── GUI 线程调用 ──
  /// 开始一次调试会话（暂停在入口处，等待用户操作）
  void begin();
  /// 结束调试会话
  void end();
  /// 设置断点集合（文件路径 → 行号 → 是否生效，覆盖）
  void setBreakpoints(const QMap<QString, QMap<int, bool>> &breakpoints);
  /// 添加断点
  void addBreakpoint(const QString &filePath, int line);
  /// 移除断点
  void removeBreakpoint(const QString &filePath, int line);
  /// 清空断点
  void clearBreakpoints();
  /// 继续执行（F5）
  void continueRun();
  /// 单步进入（F11）
  void stepInto();
  /// 单步执行（F10）
  void stepOver();
  /// 单步跳出（Shift+F11）
  void stepOut();
  /// 停止调试并中止执行
  void stop();

  bool isDebugging() const;
  bool isPaused() const;
  int currentLine() const;

signals:
  /// 暂停（工作线程阻塞中），携带当前文件、行号、调用栈与变量快照
  void paused(const QString &filePath, int line, const QVector<AcDebugFrame> &stack,
              const QList<AcDebugVar> &vars);
  /// 恢复执行（当前行号高亮清除）
  void resumed();
  /// 调试会话结束（停止或脚本运行完毕）
  void finished();

private:
  mutable QMutex m_mutex;
  QWaitCondition m_cond;

  QMap<QString, QMap<int, bool>> m_breakpoints;  ///< 断点集合（文件 → 行号 → 是否生效）
  bool m_debugging = false;                      ///< 是否处于调试模式
  bool m_paused = false;                         ///< 是否已暂停
  bool m_stopRequested = false;
  bool m_skipNext = false;  ///< 恢复后跳过紧邻的同一行，避免立即再次命中
  QString m_pausedFile;     ///< 最近一次暂停的文件路径
  int m_pausedLine = 0;     ///< 最近一次暂停的行号

  StepMode m_stepMode = StepNone;
  int m_stepDepth = 0;  ///< 单步模式的目标调用深度

  int m_currentLine = 0;   ///< 当前执行行号
  int m_currentDepth = 0;  ///< 当前调用深度
};