/**
 * @file debug_controller.cpp
 * @brief 调试与脚本执行控制器实现
 */

#include "debug_controller.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrent>

#include "debug_panel.h"
#include "editor_lookup.h"
#include "main_dev_ui.h"
#include "src/engine/script/ac_engine.h"
#include "src/util/common/code_constants.h"
#include "src/util/ui/code/code_editor.h"

// ════════════════════════════════════════════════════════════
//  构造 / 析构 / 初始化
// ════════════════════════════════════════════════════════════

DebugController::DebugController(MainDevUi *ui, QObject *parent)
    : QObject(parent), m_ui(ui) {}

DebugController::~DebugController() { shutdownAndWait(); }

void DebugController::init() {
  // ── 创建调试器并接入引擎（工作线程中的解释器通过它执行断点/单步） ──
  m_debugger = new AcDebugger(this);
  AcEngine::ins().setDebugger(m_debugger);
  connect(m_debugger, &AcDebugger::paused, this, &DebugController::onDebuggerPaused);
  connect(m_debugger, &AcDebugger::resumed, this, &DebugController::onDebuggerResumed);
  connect(m_debugger, &AcDebugger::finished, this, &DebugController::onDebuggerFinished);

  // 应用退出前（aboutToQuit 在事件循环退出前、静态析构之前触发），
  // 先唤醒可能阻塞在调试等待条件上的工作线程并请求取消，
  // 否则 Qt 全局线程池析构时 waitForDone() 会因线程阻塞而挂起，导致进程无法退出
  connect(qApp, &QCoreApplication::aboutToQuit, this, &DebugController::shutdownAndWait);

  // ── 调试按钮 ──
  connect(m_ui->debugBtn(), &QPushButton::clicked, this, &DebugController::startOrContinue);

  // ── 调试面板按钮 ──
  DebugPanel *panel = m_ui->debugPanel();
  connect(panel, &DebugPanel::continueClicked, this, [this]() {
    if (m_debugger && m_debugger->isPaused()) m_debugger->continueRun();
  });
  connect(panel, &DebugPanel::stepOverClicked, this, &DebugController::stepOver);
  connect(panel, &DebugPanel::stepIntoClicked, this, &DebugController::stepInto);
  connect(panel, &DebugPanel::stepOutClicked, this, &DebugController::stepOut);
  // 双击断点/调用栈/变量条目：请求打开对应文件并定位到行
  connect(panel, &DebugPanel::breakpointActivated, this, &DebugController::navigateToRequested);
  connect(panel, &DebugPanel::stackFrameActivated, this, &DebugController::navigateToRequested);
  connect(panel, &DebugPanel::varActivated, this, &DebugController::navigateToRequested);
  // 断点面板：切换生效状态 / 删除单个 / 删除全部
  connect(panel, &DebugPanel::breakpointToggleEnabledRequested, this,
          &DebugController::setBreakpointEnabled);
  connect(panel, &DebugPanel::breakpointDeleteRequested, this, &DebugController::removeBreakpoint);
  connect(panel, &DebugPanel::breakpointRemoveAllRequested, this,
          &DebugController::removeAllBreakpoints);
}

// ════════════════════════════════════════════════════════════
//  脚本执行（执行按钮 / 调试会话共用的工作线程运行器）
// ════════════════════════════════════════════════════════════

void DebugController::runScript(const QString &scriptPath, const QString &rootDir, bool debug) {
  m_scriptRunning = true;
  m_debugging = debug;
  if (debug) {
    // 当前编辑器可为空：断点命中未打开文件时会自动打开并定位（类似 VSCode）
    m_debugEditor = m_currentEditor ? m_currentEditor() : nullptr;
    m_ui->appendOutput(QStringLiteral("开始调试: %1").arg(scriptPath), false);
    m_ui->debugPanel()->setActive(true);
    m_ui->debugPanel()->setPaused(false);
    m_ui->debugPanel()->setStatus(QStringLiteral("运行中..."));
    // 收集全部断点（含其他文件）并告知调试器
    m_debugger->setBreakpoints(collectBreakpoints());
    m_debugger->begin();
  } else {
    m_ui->appendOutput(QStringLiteral("执行: %1").arg(scriptPath), false);
  }
  m_ui->buildBtn()->setEnabled(false);
  m_ui->stopBtn()->setEnabled(true);

  // 在工作线程执行脚本，避免卡住 GUI 线程
  m_scriptFuture = QtConcurrent::run([this, scriptPath, rootDir, debug]() {
    AcEngine::ins().setRootDir(rootDir);
    QString err = AcEngine::ins().execute(scriptPath);
    // 结果投递回 GUI 线程处理
    QMetaObject::invokeMethod(
        this,
        [this, err, debug]() {
          m_scriptRunning = false;
          m_debugging = false;
          m_ui->buildBtn()->setEnabled(true);
          m_ui->stopBtn()->setEnabled(false);
          if (debug) {
            m_ui->debugPanel()->setActive(false);
            m_ui->debugPanel()->clear();
            if (m_debugger) m_debugger->end();
          }
          if (m_debugEditor) {
            m_debugEditor->clearDebugLine();
            m_debugEditor->clearDebugVariables();
            m_debugEditor = nullptr;
          }
          if (AcEngine::ins().isCancelRequested()) {
            m_ui->appendOutput(debug ? QStringLiteral("调试已取消") : QStringLiteral("执行已取消"),
                               true);
          } else if (!err.isEmpty()) {
            m_ui->appendOutput(err, true);
          } else {
            m_ui->appendOutput(debug ? QStringLiteral("调试完成") : QStringLiteral("执行完成"),
                               false);
            const QStringList files = AcEngine::ins().generatedFiles();
            for (const QString &f : files)
              m_ui->appendOutput(QStringLiteral("  生成: %1").arg(f), false);
          }
        },
        Qt::QueuedConnection);
  });
}

void DebugController::stopScript() {
  if (!m_scriptRunning) return;
  // 调试中：唤醒被阻塞在工作线程的解释器，使其能检查取消标志
  if (m_debugging && m_debugger) {
    m_debugger->stop();
  }
  AcEngine::ins().requestCancel();
  m_ui->appendOutput(QStringLiteral("正在请求取消..."), false);
}

void DebugController::shutdownAndWait() {
  if (m_scriptFuture.isRunning()) {
    // 调试中：解释器可能阻塞在调试器的等待条件上（断点暂停），
    // 需先唤醒它（stop 会设置停止标志并 wakeAll），否则线程无法响应取消导致无法退出
    if (m_debugger) m_debugger->stop();
    AcEngine::ins().requestCancel();
    m_scriptFuture.waitForFinished();
  }
}

// ════════════════════════════════════════════════════════════
//  调试控制
// ════════════════════════════════════════════════════════════

void DebugController::startOrContinue() {
  if (m_debugging) {
    if (m_debugger && m_debugger->isPaused()) {
      m_debugger->continueRun();
    }
    return;
  }
  startDebugSession();
}

void DebugController::stepOver() {
  if (m_debugging && m_debugger && m_debugger->isPaused()) m_debugger->stepOver();
}

void DebugController::stepInto() {
  if (m_debugging && m_debugger && m_debugger->isPaused()) m_debugger->stepInto();
}

void DebugController::stepOut() {
  if (m_debugging && m_debugger && m_debugger->isPaused()) m_debugger->stepOut();
}

/// 启动一次调试会话：检查状态、读取启动项、运行脚本
void DebugController::startDebugSession() {
  if (m_scriptRunning) {
    m_ui->appendOutput(QStringLiteral("脚本正在执行中，请先停止"), true);
    return;
  }
  QString scriptPath = m_ui->startupCombo()->currentData().toString();
  if (scriptPath.isEmpty()) {
    m_ui->appendOutput(QStringLiteral("未选择启动项"), true);
    return;
  }
  runScript(scriptPath, m_ui->fileTree()->rootPath(), true);
}

// ════════════════════════════════════════════════════════════
//  调试器信号处理
// ════════════════════════════════════════════════════════════

void DebugController::onDebuggerPaused(const QString &filePath, int line,
                                       const QVector<AcDebugFrame> &stack,
                                       const QList<AcDebugVar> &vars) {
  // 定位到断点所在文件：若该文件未打开则自动打开，并滚动到断点行（类似 VSCode）
  CodeEditor *target = m_debugEditor;
  if (!filePath.isEmpty() && (!target || QFileInfo(target->objectName()).absoluteFilePath() !=
                                             QFileInfo(filePath).absoluteFilePath())) {
    CodeEditor *opened = m_openFile ? m_openFile(filePath) : nullptr;
    if (opened) {
      target = opened;
      m_debugEditor = opened;  // 后续继续/结束清除高亮针对新打开的编辑器
    }
  }
  if (target) {
    target->setDebugLine(line);
    target->setDebugVariables(vars);
  }
  m_ui->debugPanel()->setSnapshot(stack, vars);
  m_ui->debugPanel()->setPaused(true);
  m_ui->debugPanel()->setStatus(
      QStringLiteral("已暂停 @ 行 %1").arg(line > 0 ? QString::number(line) : QStringLiteral("?")));
}

void DebugController::onDebuggerResumed() {
  if (m_debugEditor) {
    m_debugEditor->clearDebugLine();
    m_debugEditor->clearDebugVariables();
  }
  m_ui->debugPanel()->setPaused(false);
  m_ui->debugPanel()->setStatus(QStringLiteral("运行中..."));
}

void DebugController::onDebuggerFinished() {
  if (m_debugEditor) {
    m_debugEditor->clearDebugLine();
    m_debugEditor->clearDebugVariables();
  }
  m_ui->debugPanel()->setPaused(false);
  m_ui->debugPanel()->setStatus(QStringLiteral("已停止"));
}

// ════════════════════════════════════════════════════════════
//  断点管理
// ════════════════════════════════════════════════════════════

/// @brief 断点存储文件路径（AppData 目录下，目录不存在则创建）
static QString breakpointStorePath() {
  QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (dir.isEmpty()) dir = QDir::homePath() + QString::fromUtf8(CodeConstants::Paths::kAppDataDirName);
  QDir().mkpath(dir);
  return dir + QString::fromUtf8(CodeConstants::Paths::kBreakpointsStoreFile);
}

void DebugController::loadBreakpointsFromDisk() {
  m_persistedBreakpoints.clear();
  QFile f(breakpointStorePath());
  if (!f.open(QIODevice::ReadOnly)) return;
  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  const QJsonArray arr =
      doc.object().value(QString::fromUtf8(CodeConstants::Paths::kBreakpointsJsonKey)).toArray();
  for (const QJsonValue &v : arr) {
    const QJsonObject entry = v.toObject();
    const QString file = entry.value(QStringLiteral("file")).toString();
    if (file.isEmpty()) continue;
    QMap<int, bool> lines;
    const QJsonArray lineArr =
        entry.value(QString::fromUtf8(CodeConstants::Paths::kBreakpointsJsonKey)).toArray();
    if (lineArr.isEmpty()) {
      // 兼容旧格式：直接是行号数组
      const QJsonArray oldArr = entry.value(QStringLiteral("lines")).toArray();
      for (const QJsonValue &lv : oldArr) lines.insert(lv.toInt(), true);
    } else {
      for (const QJsonValue &lv : lineArr) {
        const QJsonObject bpo = lv.toObject();
        lines.insert(bpo.value(QStringLiteral("line")).toInt(),
                     bpo.value(QStringLiteral("enabled")).toBool(true));
      }
    }
    if (!lines.isEmpty()) m_persistedBreakpoints[file] = lines;
  }
}

void DebugController::saveBreakpointsToDisk() {
  // 先从所有已打开编辑器同步到持久存储，再落盘
  forEachEditor(m_ui, [this](CodeEditor *editor) {
    const QString filePath = editor->objectName();
    if (!filePath.isEmpty()) m_persistedBreakpoints[filePath] = editor->breakpoints();
    return true;
  });

  QJsonObject root;
  QJsonArray arr;
  for (auto it = m_persistedBreakpoints.cbegin(); it != m_persistedBreakpoints.cend(); ++it) {
    if (it.value().isEmpty()) continue;
    QJsonArray lines;
    for (auto lit = it.value().cbegin(); lit != it.value().cend(); ++lit) {
      QJsonObject bp;
      bp[QStringLiteral("line")] = lit.key();
      bp[QStringLiteral("enabled")] = lit.value();
      lines.append(bp);
    }
    QJsonObject entry;
    entry[QStringLiteral("file")] = it.key();
    entry[QString::fromUtf8(CodeConstants::Paths::kBreakpointsJsonKey)] = lines;
    arr.append(entry);
  }
  root[QString::fromUtf8(CodeConstants::Paths::kBreakpointsJsonKey)] = arr;

  QFile f(breakpointStorePath());
  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  }
}

QMap<int, bool> DebugController::takeBreakpointsForFile(const QString &filePath, int totalLines) {
  QMap<int, bool> restoredBps;
  auto bpIt = m_persistedBreakpoints.find(filePath);
  if (bpIt == m_persistedBreakpoints.end()) return restoredBps;
  bool bpsChanged = false;
  for (auto lit = bpIt.value().cbegin(); lit != bpIt.value().cend(); ++lit) {
    if (lit.key() >= 1 && lit.key() <= totalLines) {
      restoredBps.insert(lit.key(), lit.value());
    } else {
      bpsChanged = true;  // 该断点行已不存在，清除
    }
  }
  if (bpsChanged) {
    if (restoredBps.isEmpty())
      m_persistedBreakpoints.erase(bpIt);
    else
      bpIt.value() = restoredBps;
  }
  return restoredBps;
}

void DebugController::refreshBreakpointList() {
  // 1) 将当前所有已打开编辑器的断点同步到持久存储（关闭文件时仍保留）
  forEachEditor(m_ui, [this](CodeEditor *editor) {
    const QString filePath = editor->objectName();
    if (!filePath.isEmpty()) m_persistedBreakpoints[filePath] = editor->breakpoints();
    return true;
  });

  // 2) 从持久存储构建断点列表（含已关闭文件），刷新调试面板
  QList<QPair<QString, AcBreakpoint>> bps;
  for (auto it = m_persistedBreakpoints.cbegin(); it != m_persistedBreakpoints.cend(); ++it) {
    for (auto lit = it.value().cbegin(); lit != it.value().cend(); ++lit) {
      bps.append({it.key(), AcBreakpoint{lit.key(), lit.value()}});
    }
  }
  m_ui->debugPanel()->setBreakpoints(bps);

  // 3) 调试期间将断点变更同步到调试器，否则删除/新增断点不会生效
  if (m_debugging && m_debugger) {
    m_debugger->setBreakpoints(collectBreakpoints());
  }

  // 4) 断点变更后持久化到磁盘，程序重启后还原
  saveBreakpointsToDisk();
}

void DebugController::setBreakpointEnabled(const QString &filePath, int line, bool enabled) {
  CodeEditor *editor = editorForFile(m_ui, filePath);
  if (editor) {
    editor->setBreakpointEnabled(line, enabled);
  } else if (m_persistedBreakpoints.contains(filePath)) {
    if (m_persistedBreakpoints[filePath].contains(line)) {
      m_persistedBreakpoints[filePath][line] = enabled;
    }
  }
  refreshBreakpointList();
}

void DebugController::removeBreakpoint(const QString &filePath, int line) {
  CodeEditor *editor = editorForFile(m_ui, filePath);
  if (editor) {
    if (editor->hasBreakpoint(line)) editor->toggleBreakpoint(line - 1);  // 存在则移除
  } else if (m_persistedBreakpoints.contains(filePath)) {
    m_persistedBreakpoints[filePath].remove(line);
    if (m_persistedBreakpoints[filePath].isEmpty()) m_persistedBreakpoints.remove(filePath);
  }
  refreshBreakpointList();
}

void DebugController::removeAllBreakpoints() {
  forEachEditor(m_ui, [](CodeEditor *editor) {
    editor->clearBreakpoints();
    return true;
  });
  m_persistedBreakpoints.clear();
  refreshBreakpointList();
}

QMap<QString, QMap<int, bool>> DebugController::collectBreakpoints() {
  QMap<QString, QMap<int, bool>> result;
  // 1) 已打开编辑器内的断点
  forEachEditor(m_ui, [&result](CodeEditor *editor) {
    const QString filePath = editor->objectName();
    if (!filePath.isEmpty()) {
      QMap<int, bool> lines = editor->breakpoints();
      if (!lines.isEmpty()) result.insert(filePath, lines);
    }
    return true;
  });
  // 2) 已关闭文件的持久化断点
  for (auto it = m_persistedBreakpoints.cbegin(); it != m_persistedBreakpoints.cend(); ++it) {
    if (it.value().isEmpty()) continue;
    auto &target = result[it.key()];
    for (auto lit = it.value().cbegin(); lit != it.value().cend(); ++lit) {
      target.insert(lit.key(), lit.value());
    }
  }
  return result;
}
