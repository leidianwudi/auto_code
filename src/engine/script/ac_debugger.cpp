/**
 * @file ac_debugger.cpp
 * @brief AC 脚本调试器实现
 */

#include "ac_debugger.h"

AcDebugger::AcDebugger(QObject *parent) : QObject(parent) {}

bool AcDebugger::onStatement(const QString &filePath, int line, int callDepth,
                             const SnapshotBuilder &builder) {
  QMutexLocker lock(&m_mutex);
  if (!m_debugging) return true;

  m_currentLine = line;
  m_currentDepth = callDepth;

  // 恢复后跳过紧邻的同一文件同一行，避免再次立即命中（同一行多条语句的情况）
  if (m_skipNext && line == m_pausedLine && filePath == m_pausedFile) {
    m_skipNext = false;
    return true;
  }
  m_skipNext = false;

  bool shouldPause = false;
  if (m_breakpoints.value(filePath).contains(line)) {
    shouldPause = true;
  } else if (m_stepMode == StepInto) {
    shouldPause = true;
  } else if (m_stepMode == StepOver && callDepth == m_stepDepth) {
    shouldPause = true;
  } else if (m_stepMode == StepOut && callDepth < m_stepDepth) {
    shouldPause = true;
  }

  if (!shouldPause) return true;

  // ── 暂停 ──
  m_paused = true;
  m_pausedFile = filePath;
  m_pausedLine = line;
  m_stepMode = StepNone;
  m_skipNext = true;

  // 构建快照（解锁前构建，避免与 GUI 读状态竞争）
  QVector<AcDebugFrame> stack;
  QList<AcDebugVar> vars;
  if (builder) builder(stack, vars);

  // 解锁后发信号，GUI 槽可能回调本对象方法，避免死锁
  m_mutex.unlock();
  emit paused(filePath, line, stack, vars);
  m_mutex.lock();

  // 阻塞等待 GUI 指令（继续/单步/停止）
  while (m_paused && !m_stopRequested) {
    m_cond.wait(&m_mutex);
  }

  if (m_stopRequested) {
    m_paused = false;
    m_stopRequested = false;
    m_stepMode = StepNone;
    m_mutex.unlock();
    emit finished();
    m_mutex.lock();
    return false;  // 中止执行
  }

  m_paused = false;
  m_mutex.unlock();
  emit resumed();
  m_mutex.lock();
  return true;
}

void AcDebugger::begin() {
  QMutexLocker lock(&m_mutex);
  m_debugging = true;
  m_paused = false;
  m_stopRequested = false;
  m_skipNext = false;
  m_pausedFile.clear();
  m_pausedLine = 0;
  m_stepMode = StepNone;  // F5 启动调试：直接运行到命中断点，而非暂停在入口
  m_stepDepth = 0;
}

void AcDebugger::end() {
  QMutexLocker lock(&m_mutex);
  m_debugging = false;
  m_paused = false;
  m_stepMode = StepNone;
  m_cond.wakeAll();
}

void AcDebugger::setBreakpoints(const QMap<QString, QSet<int>> &breakpoints) {
  QMutexLocker lock(&m_mutex);
  m_breakpoints = breakpoints;
}

void AcDebugger::addBreakpoint(const QString &filePath, int line) {
  QMutexLocker lock(&m_mutex);
  if (line > 0 && !filePath.isEmpty()) m_breakpoints[filePath].insert(line);
}

void AcDebugger::removeBreakpoint(const QString &filePath, int line) {
  QMutexLocker lock(&m_mutex);
  auto it = m_breakpoints.find(filePath);
  if (it != m_breakpoints.end()) {
    it.value().remove(line);
    if (it.value().isEmpty()) m_breakpoints.erase(it);
  }
}

void AcDebugger::clearBreakpoints() {
  QMutexLocker lock(&m_mutex);
  m_breakpoints.clear();
}

void AcDebugger::continueRun() {
  QMutexLocker lock(&m_mutex);
  if (!m_paused) return;
  m_stepMode = StepNone;
  m_paused = false;
  m_cond.wakeAll();
}

void AcDebugger::stepInto() {
  QMutexLocker lock(&m_mutex);
  if (!m_paused) return;
  m_stepMode = StepInto;
  m_stepDepth = m_currentDepth;
  m_paused = false;
  m_cond.wakeAll();
}

void AcDebugger::stepOver() {
  QMutexLocker lock(&m_mutex);
  if (!m_paused) return;
  m_stepMode = StepOver;
  m_stepDepth = m_currentDepth;
  m_paused = false;
  m_cond.wakeAll();
}

void AcDebugger::stepOut() {
  QMutexLocker lock(&m_mutex);
  if (!m_paused) return;
  m_stepMode = StepOut;
  m_stepDepth = m_currentDepth;
  m_paused = false;
  m_cond.wakeAll();
}

void AcDebugger::stop() {
  QMutexLocker lock(&m_mutex);
  m_stopRequested = true;
  m_paused = false;
  m_cond.wakeAll();
}

bool AcDebugger::isDebugging() const {
  QMutexLocker lock(&m_mutex);
  return m_debugging;
}

bool AcDebugger::isPaused() const {
  QMutexLocker lock(&m_mutex);
  return m_paused;
}

int AcDebugger::currentLine() const {
  QMutexLocker lock(&m_mutex);
  return m_currentLine;
}