/**
 * @file main_dev_mgr.cpp
 * @brief 代码编辑器控制器层实现（单例 UI 控制器）
 *
 * 本文件只包含核心方法：窗口创建、信号初始化、文件树加载、保存逻辑。
 * 其他方法拆分到：
 *   - main_dev_mgr_file.cpp    文件操作（打开/创建/重命名/删除）
 *   - main_dev_mgr_tab.cpp     标签页管理（关闭/拆分/切换）
 *   - main_dev_mgr_connect.cpp 编辑器信号连接与事件过滤
 *   - main_dev_mgr_navigate.cpp 导航历史
 */

#include "main_dev_mgr.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QShortcut>
#include <QStandardPaths>
#include <QStringList>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QToolButton>
#include <QtConcurrent/QtConcurrent>

#include "main_dev_model.h"
#include "main_dev_ui.h"
#include "main_dev_ui_ext.h"
#include "src/engine/ac_language.h"
#include "src/engine/script/ac_engine.h"
#include "src/ui/json_vue/json_vue_editor.h"
#include "src/ui/json_vue/json_vue_widget.h"
#include "src/ui/setting/setting_mgr.h"
#include "src/util/common/code_constants.h"
#include "src/util/common/path_resolver.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/setting_store.h"

// ──────────────────────────────────────────────────────────────
//  静态方法（通过单例转发）
// ──────────────────────────────────────────────────────────────

void MainDevMgr::openFile(const QString &filePath) {
  ins().open();
  ins().openFileInEditor(filePath);
}

void MainDevMgr::splitRight() { ins().onSplitRight(); }

void MainDevMgr::closeCurrentEditor() { ins().onCloseEditor(); }

// ──────────────────────────────────────────────────────────────
//  析构 — 等待脚本线程结束，避免异步回调访问已释放对象
// ──────────────────────────────────────────────────────────────

MainDevMgr::~MainDevMgr() {
  if (m_scriptFuture.isRunning()) {
    // 调试中：解释器可能阻塞在调试器的等待条件上（断点暂停），
    // 需先唤醒它（stop 会设置停止标志并 wakeAll），否则线程无法响应取消导致无法退出
    if (m_debugger) m_debugger->stop();
    AcEngine::ins().requestCancel();
    m_scriptFuture.waitForFinished();
  }
}

// ──────────────────────────────────────────────────────────────
//  onCreateWindow — 创建 MainDevUi 窗口（首次 open() 时调用）
// ──────────────────────────────────────────────────────────────

QWidget *MainDevMgr::onCreateWindow() {
  // ── 创建 MVC 组件 ──
  m_ui = new MainDevUi;
  m_model = new MainDevModel;

  // ── 构建界面 ──
  m_ui->setupUI();
  m_ui->resize(1400, 850);
  m_ui->setWindowTitle(MainDevUi::defaultTitle());

  // ── 连接信号 ──
  initUi();

  // ── 创建调试器并接入引擎（工作线程中的解释器通过它执行断点/单步） ──
  m_debugger = new AcDebugger(this);
  AcEngine::ins().setDebugger(m_debugger);
  connect(m_debugger, &AcDebugger::paused, this, &MainDevMgr::onDebuggerPaused);
  connect(m_debugger, &AcDebugger::resumed, this, &MainDevMgr::onDebuggerResumed);
  connect(m_debugger, &AcDebugger::finished, this, &MainDevMgr::onDebuggerFinished);

  // 应用退出前（aboutToQuit 在事件循环退出前、静态析构之前触发），
  // 先唤醒可能阻塞在调试等待条件上的工作线程并请求取消，
  // 否则 Qt 全局线程池析构时 waitForDone() 会因线程阻塞而挂起，导致进程无法退出
  connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
    if (m_debugger) m_debugger->stop();
    AcEngine::ins().requestCancel();
  });

  // ── 设置日志回调：脚本中 printLog() 输出到 UI 面板 ──
  // 脚本在工作线程执行，日志回调可能从工作线程触发，需投递到 GUI 线程
  AcEngine::ins().setLogCallback([this](const QString &text, bool isError) {
    QMetaObject::invokeMethod(
        m_ui, [this, text, isError]() { m_ui->appendOutput(text, isError); }, Qt::QueuedConnection);
  });

  // ── 加载文件树 ──
  loadFiles();

  // ── 启动后台工作区全量错误扫描（不阻塞 UI，完成后填充底部「问题」面板） ──
  startWorkspaceScan();

  // ── 恢复上次保存的断点（程序重启后还原） ──
  loadBreakpointsFromDisk();
  refreshBreakpointList();

  // ── 恢复可视化编辑按钮状态 ──
  // 必须在还原文件之前设置，否则 openFileInEditor 打开 .jsonvue 时
  // visualToggleBtn()->isChecked() 仍为 false，导致无法按按钮状态切到可视化模式
  m_ui->visualToggleBtn()->setChecked(m_ui->fileTree()->visualToggle());

  // ── 恢复上次打开的文件（程序重启后还原） ──
  restoreOpenFilesFromSettings();

  // ── 还原窗口几何与分割器大小（在还原文件/面板之后，确保面板数量匹配） ──
  m_ui->restoreLayout();

  return m_ui;
}

// ──────────────────────────────────────────────────────────────
//  initUi — 连接所有信号槽（按职责拆分为子方法）
// ──────────────────────────────────────────────────────────────

void MainDevMgr::initUi() {
  connectFileActions();
  connectSaveActions();
  connectVisualToggle();
  connectBuildAction();
  connectEditorPanels();
  connectDebugAction();
  // 窗口关闭前保存断点与会话状态
  connect(m_ui, &MainDevUi::uiClosing, this, [this]() {
    saveBreakpointsToDisk();
    saveOpenFilesToSettings();
  });

  // ── 设置：打开设置对话框，并在设置变化时实时刷新主题 ──
  connect(m_ui->settingsAction(), &QAction::triggered, this, []() { SettingMgr::ins().open(); });
  SettingStore &store = SettingStore::ins();
  // 防抖：主题/颜色变化时短暂延迟后一次性刷新，合并取色器拖动产生的连续信号，避免卡顿
  m_themeTimer = new QTimer(this);
  m_themeTimer->setSingleShot(true);
  m_themeTimer->setInterval(60);
  connect(m_themeTimer, &QTimer::timeout, this, &MainDevMgr::refreshTheme);
  connect(&store, &SettingStore::themeChanged, m_themeTimer, qOverload<>(&QTimer::start));
  connect(&store, &SettingStore::colorsChanged, m_themeTimer, qOverload<>(&QTimer::start));
  // 窗口字体变化：走轻量刷新（只重建标题栏样式），不触发 refreshTheme 的重活
  // （调色板重建、重新高亮所有编辑器、重建调试面板等对字体变化毫无必要）
  connect(&store, &SettingStore::windowFontChanged, this, &MainDevMgr::refreshWindowFont);
}

/// 文件打开、帮助、重命名、删除信号
void MainDevMgr::connectFileActions() {
  connect(m_ui->openAction(), &QAction::triggered, this, [this]() {
    QString filePath = QFileDialog::getOpenFileName(m_ui, QStringLiteral("打开文件"),
                                                    QStringLiteral(PROJECT_SOURCE_DIR));
    if (!filePath.isEmpty()) openFileInEditor(filePath);
  });

  connect(m_ui->openFolderAction(), &QAction::triggered, this, [this]() {
    QString dir = QFileDialog::getExistingDirectory(m_ui, QStringLiteral("选择文件夹"),
                                                    QStringLiteral(PROJECT_SOURCE_DIR));
    if (!dir.isEmpty()) m_ui->fileTree()->buildTree(dir);
  });

  connect(m_ui->splitAction(), &QAction::triggered, this, &MainDevMgr::onSplitRight);
  connect(m_ui->closeAction(), &QAction::triggered, this, &MainDevMgr::onCloseEditor);
  connect(m_ui->fileTree(), &TreeDir::fileActivated, this,
          [this](const QString &fp) { openFileInEditor(fp); });

  connect(m_ui->fileTree(), &TreeDir::renameRequested, this, &MainDevMgr::onRenameFile);
  connect(m_ui->fileTree(), &TreeDir::deleteRequested, this, &MainDevMgr::onDeleteFile);
  connect(qApp, &QApplication::focusChanged, this, &MainDevMgr::onFocusChanged);
}

/// 保存、Ctrl+S、保存全部信号
void MainDevMgr::connectSaveActions() {
  // ── 保存按钮 ──
  connect(m_ui->saveBtn(), &QPushButton::clicked, this, [this]() {
    syncJsonVueBeforeSave();
    saveAndSync(currentEditor());
  });

  // ── Ctrl+S 快捷键 ──
  auto *saveShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+S")), m_ui);
  connect(saveShortcut, &QShortcut::activated, this, [this]() {
    syncJsonVueBeforeSave();
    saveAndSync(currentEditor());
  });

  // ── Alt+Left 后退 / Alt+Right 前进 ──
  auto *backShortcut = new QShortcut(QKeySequence(QStringLiteral("Alt+Left")), m_ui);
  connect(backShortcut, &QShortcut::activated, this, &MainDevMgr::navigateBack);
  auto *forwardShortcut = new QShortcut(QKeySequence(QStringLiteral("Alt+Right")), m_ui);
  connect(forwardShortcut, &QShortcut::activated, this, &MainDevMgr::navigateForward);

  // ── 保存全部按钮 ──
  connect(m_ui->saveAllBtn(), &QPushButton::clicked, this, [this]() {
    for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
      auto *tabs = m_ui->editorPanelAt(pi);
      if (!tabs) continue;
      for (int ti = 0; ti < tabs->count(); ++ti) {
        auto *w = tabs->widget(ti);
        auto *jvw = qobject_cast<JsonVueWidget *>(w);
        CodeEditor *editor = nullptr;
        bool wasModified = false;
        if (jvw) {
          editor = jvw->codeEditor();
          // syncVisualToCode 会重置 modified 为 false，需先记录
          wasModified = editor && editor->document()->isModified();
          jvw->syncVisualToCode();
        } else {
          editor = qobject_cast<CodeEditor *>(w);
        }
        if (editor && (wasModified || editor->document()->isModified())) {
          saveAndSync(editor);
        }
      }
    }
  });
}

/// 可视化/代码切换按钮
void MainDevMgr::connectVisualToggle() {
  connect(m_ui->visualToggleBtn(), &QPushButton::toggled, this, [this](bool checked) {
    // 保存按钮状态到 tree.config
    m_ui->fileTree()->setVisualToggle(checked);
    // 获取当前 tab 的 widget
    for (int i = 0; i < m_ui->editorPanelCount(); ++i) {
      auto *tabs = m_ui->editorPanelAt(i);
      if (!tabs) continue;
      if (!tabs->isVisible()) continue;
      auto *w = tabs->currentWidget();
      auto *jvw = qobject_cast<JsonVueWidget *>(w);
      if (jvw) {
        if (checked) {
          jvw->switchToVisual();
        } else {
          jvw->switchToCode();
        }
        return;
      }
    }
  });
}

/// 执行按钮
void MainDevMgr::connectBuildAction() {
  connect(m_ui->buildBtn(), &QPushButton::clicked, this, [this]() {
    if (m_scriptRunning) {
      m_ui->appendOutput(QStringLiteral("脚本正在执行中，请先停止"), true);
      return;
    }
    QString scriptPath = m_ui->startupCombo()->currentData().toString();
    if (scriptPath.isEmpty()) {
      m_ui->appendOutput(QStringLiteral("未选择启动项"), true);
      return;
    }
    m_ui->appendOutput(QStringLiteral("执行: %1").arg(scriptPath), false);
    m_scriptRunning = true;
    m_ui->buildBtn()->setEnabled(false);
    m_ui->stopBtn()->setEnabled(true);

    // 在工作线程执行脚本，避免卡住 GUI 线程
    QString rootDir = m_ui->fileTree()->rootPath();
    m_scriptFuture = QtConcurrent::run([this, scriptPath, rootDir]() {
      AcEngine::ins().setRootDir(rootDir);
      QString err = AcEngine::ins().execute(scriptPath);
      // 结果投递回 GUI 线程处理
      QMetaObject::invokeMethod(
          this,
          [this, err]() {
            m_scriptRunning = false;
            m_ui->buildBtn()->setEnabled(true);
            m_ui->stopBtn()->setEnabled(false);
            if (AcEngine::ins().isCancelRequested()) {
              m_ui->appendOutput(QStringLiteral("执行已取消"), true);
            } else if (!err.isEmpty()) {
              m_ui->appendOutput(err, true);
            } else {
              m_ui->appendOutput(QStringLiteral("执行完成"), false);
              const QStringList files = AcEngine::ins().generatedFiles();
              for (const QString &f : files)
                m_ui->appendOutput(QStringLiteral("  生成: %1").arg(f), false);
            }
          },
          Qt::QueuedConnection);
    });
  });

  // 停止按钮：设置取消标志，由工作线程轮询检查
  connect(m_ui->stopBtn(), &QPushButton::clicked, this, &MainDevMgr::onStopScript);
}

/// 停止正在执行的脚本
void MainDevMgr::onStopScript() {
  if (!m_scriptRunning) return;
  // 调试中：唤醒被阻塞在工作线程的解释器，使其能检查取消标志
  if (m_debugging && m_debugger) {
    m_debugger->stop();
  }
  AcEngine::ins().requestCancel();
  m_ui->appendOutput(QStringLiteral("正在请求取消..."), false);
}

// ──────────────────────────────────────────────────────────────
//  调试会话
// ──────────────────────────────────────────────────────────────

/// 连接调试按钮与调试器信号
void MainDevMgr::connectDebugAction() {
  connect(m_ui->debugBtn(), &QPushButton::clicked, this, &MainDevMgr::onDebugBtnClicked);

  // 调试面板按钮
  DebugPanel *panel = m_ui->debugPanel();
  connect(panel, &DebugPanel::continueClicked, this, [this]() {
    if (m_debugger && m_debugger->isPaused()) m_debugger->continueRun();
  });
  connect(panel, &DebugPanel::stepOverClicked, this, &MainDevMgr::onDebugStepOver);
  connect(panel, &DebugPanel::stepIntoClicked, this, &MainDevMgr::onDebugStepInto);
  connect(panel, &DebugPanel::stepOutClicked, this, &MainDevMgr::onDebugStepOut);
  // 双击断点条目：打开对应文件并定位到断点行
  connect(panel, &DebugPanel::breakpointActivated, this, &MainDevMgr::onBreakpointActivated);
  // 双击调用栈条目：打开对应文件并定位到函数所在行
  connect(panel, &DebugPanel::stackFrameActivated, this, &MainDevMgr::onBreakpointActivated);
  // 双击变量条目：打开对应文件并定位到变量声明行
  connect(panel, &DebugPanel::varActivated, this, &MainDevMgr::onBreakpointActivated);
  // 断点面板：切换生效状态 / 删除单个 / 删除全部
  connect(panel, &DebugPanel::breakpointToggleEnabledRequested, this,
          &MainDevMgr::onBreakpointToggleEnabledRequested);
  connect(panel, &DebugPanel::breakpointDeleteRequested, this,
          &MainDevMgr::onBreakpointDeleteRequested);
  connect(panel, &DebugPanel::breakpointRemoveAllRequested, this,
          &MainDevMgr::onBreakpointRemoveAllRequested);
}

/// 调试按钮：未调试则启动会话；已暂停则继续执行
void MainDevMgr::onDebugBtnClicked() {
  if (m_debugging) {
    if (m_debugger && m_debugger->isPaused()) {
      m_debugger->continueRun();
    }
    return;
  }
  startDebugSession();
}

/// 编辑器 F5：未调试则启动会话；已暂停则继续执行
void MainDevMgr::onDebugStart() {
  if (m_debugging) {
    if (m_debugger && m_debugger->isPaused()) {
      m_debugger->continueRun();
    }
    return;
  }
  startDebugSession();
}

void MainDevMgr::onDebugStepOver() {
  if (m_debugging && m_debugger && m_debugger->isPaused()) m_debugger->stepOver();
}

void MainDevMgr::onDebugStepInto() {
  if (m_debugging && m_debugger && m_debugger->isPaused()) m_debugger->stepInto();
}

void MainDevMgr::onDebugStepOut() {
  if (m_debugging && m_debugger && m_debugger->isPaused()) m_debugger->stepOut();
}

/// 双击断点面板条目：打开对应文件（若未打开）并定位到断点行
void MainDevMgr::onBreakpointActivated(const QString &filePath, int line) {
  CodeEditor *editor = openFileInEditor(filePath);
  if (!editor) return;
  editor->setFocus();
  if (line > 0) {
    QTextBlock block = editor->document()->findBlockByNumber(line - 1);
    if (block.isValid()) {
      QTextCursor cursor(block);
      editor->setTextCursor(cursor);
    }
  }
}

/// 断点面板切换生效状态：更新对应编辑器（或持久存储）中该断点的生效标记
void MainDevMgr::onBreakpointToggleEnabledRequested(const QString &filePath, int line,
                                                    bool enabled) {
  CodeEditor *editor = findEditorForFile(filePath);
  if (editor) {
    editor->setBreakpointEnabled(line, enabled);
  } else if (m_persistedBreakpoints.contains(filePath)) {
    if (m_persistedBreakpoints[filePath].contains(line)) {
      m_persistedBreakpoints[filePath][line] = enabled;
    }
  }
  refreshBreakpointList();
}

/// 断点面板删除单个断点
void MainDevMgr::onBreakpointDeleteRequested(const QString &filePath, int line) {
  CodeEditor *editor = findEditorForFile(filePath);
  if (editor) {
    if (editor->hasBreakpoint(line)) editor->toggleBreakpoint(line - 1);  // 存在则移除
  } else if (m_persistedBreakpoints.contains(filePath)) {
    m_persistedBreakpoints[filePath].remove(line);
    if (m_persistedBreakpoints[filePath].isEmpty()) m_persistedBreakpoints.remove(filePath);
  }
  refreshBreakpointList();
}

/// 断点面板删除全部断点
void MainDevMgr::onBreakpointRemoveAllRequested() {
  for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
    auto *tabs = m_ui->editorPanelAt(pi);
    if (!tabs) continue;
    for (int ti = 0; ti < tabs->count(); ++ti) {
      auto *w = tabs->widget(ti);
      CodeEditor *editor = qobject_cast<CodeEditor *>(w);
      if (!editor) {
        auto *jvw = qobject_cast<JsonVueWidget *>(w);
        if (jvw) editor = jvw->codeEditor();
      }
      if (editor) editor->clearBreakpoints();
    }
  }
  m_persistedBreakpoints.clear();
  refreshBreakpointList();
}

/// 在所有编辑面板中查找已打开指定文件的编辑器
CodeEditor *MainDevMgr::findEditorForFile(const QString &filePath) const {
  for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
    auto *tabs = m_ui->editorPanelAt(pi);
    if (!tabs) continue;
    for (int ti = 0; ti < tabs->count(); ++ti) {
      auto *w = tabs->widget(ti);
      CodeEditor *editor = qobject_cast<CodeEditor *>(w);
      if (!editor) {
        auto *jvw = qobject_cast<JsonVueWidget *>(w);
        if (jvw) editor = jvw->codeEditor();
      }
      if (editor && editor->objectName() == filePath) return editor;
    }
  }
  return nullptr;
}

/// 启动一次调试会话：收集当前编辑器断点，运行脚本
void MainDevMgr::startDebugSession() {
  if (m_scriptRunning) {
    m_ui->appendOutput(QStringLiteral("脚本正在执行中，请先停止"), true);
    return;
  }
  QString scriptPath = m_ui->startupCombo()->currentData().toString();
  if (scriptPath.isEmpty()) {
    m_ui->appendOutput(QStringLiteral("未选择启动项"), true);
    return;
  }

  // 当前编辑器可为空：断点命中未打开文件时会自动打开并定位（类似 VSCode）
  m_debugEditor = currentEditor();
  m_debugging = true;
  m_scriptRunning = true;
  m_ui->appendOutput(QStringLiteral("开始调试: %1").arg(scriptPath), false);
  m_ui->buildBtn()->setEnabled(false);
  m_ui->stopBtn()->setEnabled(true);
  m_ui->debugPanel()->setActive(true);
  m_ui->debugPanel()->setPaused(false);
  m_ui->debugPanel()->setStatus(QStringLiteral("运行中..."));

  // 收集全部断点（含其他文件）并告知调试器
  m_debugger->setBreakpoints(debugBreakpoints());
  m_debugger->begin();

  QString rootDir = m_ui->fileTree()->rootPath();
  m_scriptFuture = QtConcurrent::run([this, scriptPath, rootDir]() {
    AcEngine::ins().setRootDir(rootDir);
    QString err = AcEngine::ins().execute(scriptPath);
    // 结果投递回 GUI 线程处理
    QMetaObject::invokeMethod(
        this,
        [this, err]() {
          m_scriptRunning = false;
          m_debugging = false;
          m_ui->buildBtn()->setEnabled(true);
          m_ui->stopBtn()->setEnabled(false);
          m_ui->debugPanel()->setActive(false);
          m_ui->debugPanel()->clear();
          if (m_debugger) m_debugger->end();
          if (m_debugEditor) {
            m_debugEditor->clearDebugLine();
            m_debugEditor->clearDebugVariables();
            m_debugEditor = nullptr;
          }
          if (AcEngine::ins().isCancelRequested()) {
            m_ui->appendOutput(QStringLiteral("调试已取消"), true);
          } else if (!err.isEmpty()) {
            m_ui->appendOutput(err, true);
          } else {
            m_ui->appendOutput(QStringLiteral("调试完成"), false);
          }
        },
        Qt::QueuedConnection);
  });
}

/// 调试器暂停（工作线程阻塞中），高亮当前行并填充面板
void MainDevMgr::onDebuggerPaused(const QString &filePath, int line,
                                  const QVector<AcDebugFrame> &stack,
                                  const QList<AcDebugVar> &vars) {
  // 定位到断点所在文件：若该文件未打开则自动打开，并滚动到断点行（类似 VSCode）
  CodeEditor *target = m_debugEditor;
  if (!filePath.isEmpty() && (!target || QFileInfo(target->objectName()).absoluteFilePath() !=
                                             QFileInfo(filePath).absoluteFilePath())) {
    CodeEditor *opened = openFileInEditor(filePath);
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

/// 调试器恢复执行，清除行高亮
void MainDevMgr::onDebuggerResumed() {
  if (m_debugEditor) {
    m_debugEditor->clearDebugLine();
    m_debugEditor->clearDebugVariables();
  }
  m_ui->debugPanel()->setPaused(false);
  m_ui->debugPanel()->setStatus(QStringLiteral("运行中..."));
}

/// 调试会话结束，复位状态
void MainDevMgr::onDebuggerFinished() {
  if (m_debugEditor) {
    m_debugEditor->clearDebugLine();
    m_debugEditor->clearDebugVariables();
  }
  m_ui->debugPanel()->setPaused(false);
  m_ui->debugPanel()->setStatus(QStringLiteral("已停止"));
}

/// 连接单个编辑器面板的信号（关闭/切换/标签栏交互）
void MainDevMgr::connectEditorPanel(QTabWidget *tabs) {
  if (!tabs) return;
  connect(tabs, &QTabWidget::tabCloseRequested, this, &MainDevMgr::onTabCloseRequested);
  connect(tabs, &QTabWidget::currentChanged, this, &MainDevMgr::onCurrentTabChanged);
  // 标签拖拽到面板左/右边缘 → 拆分（sender 需先转为具体类型，信号属于 DimmableTabWidget）
  if (auto *dimTabs = qobject_cast<DimmableTabWidget *>(tabs)) {
    connect(dimTabs, &DimmableTabWidget::splitDropped, this, &MainDevMgr::onTabSplitDropped);
  }
  auto *bar = qobject_cast<DraggableTabBar *>(tabs->tabBar());
  if (bar) {
    connect(bar, &DraggableTabBar::closeOthersRequested, this, &MainDevMgr::onCloseOthers);
    connect(bar, &DraggableTabBar::closeAllRequested, this, &MainDevMgr::onCloseAll);
    connect(bar, &QTabBar::tabBarClicked, this, &MainDevMgr::onTabBarClicked);
  }
}

/// 编辑器面板信号 + 事件过滤器
void MainDevMgr::connectEditorPanels() {
  // 连接所有已存在编辑器面板组的信号
  for (int i = 0; i < m_ui->editorPanelCount(); ++i) {
    connectEditorPanel(m_ui->editorPanelAt(i));
  }

  // 底部“问题”面板：双击错误项 → 打开对应文件并定位到出错行
  connect(m_ui->problemPanel(), &ProblemPanel::issueActivated, this, &MainDevMgr::onGoToLine);

  // 安装事件过滤器以捕获鼠标侧键（前进/后退）
  // 注意：需要在 QApplication 级别安装，因为鼠标事件可能被子控件消费
  qApp->installEventFilter(this);
}

// ──────────────────────────────────────────────────────────────
//  文件扫描与树构建
// ──────────────────────────────────────────────────────────────

void MainDevMgr::loadFiles() {
  // 复用 PathResolver 统一的文件搜索路径
  QStringList searchPaths = PathResolver::fileSearchPaths(QString());

  QDir baseDir;
  for (const auto &path : searchPaths) {
    if (QDir(path).exists()) {
      baseDir.setPath(path);
      break;
    }
  }

  if (!baseDir.exists()) {
    qWarning("未找到 file/ 目录");
    return;
  }

  m_ui->fileTree()->buildTree(baseDir.absolutePath());
}

/// 启动后台工作区全量错误扫描：收集所有可验证文件，在工作线程逐个验证，
/// 完成后通过 onWorkspaceScanFinished 合并到问题面板（不阻塞 UI）
void MainDevMgr::startWorkspaceScan() {
  if (m_workspaceScanWatcher) return;  // 已有扫描任务，避免重复启动
  const QString rootDir = m_ui ? m_ui->fileTree()->rootPath() : QString();
  if (rootDir.isEmpty()) return;

  const QStringList files = collectWorkspaceFiles(rootDir);
  if (files.isEmpty()) return;

  m_workspaceScanWatcher = new QFutureWatcher<QVector<WorkspaceFileDiag>>(this);
  connect(m_workspaceScanWatcher, &QFutureWatcherBase::finished, this,
          &MainDevMgr::onWorkspaceScanFinished);
  QFuture<QVector<WorkspaceFileDiag>> future = QtConcurrent::run(scanWorkspaceDiagnostics, files);
  m_workspaceScanWatcher->setFuture(future);
  m_ui->appendOutput(QStringLiteral("开始检查工作区错误（%1 个文件）...").arg(files.size()), false);
}

/// 后台扫描完成：将结果合并到工作区问题聚合。
/// 打开中的文件以实时验证结果为准（跳过扫描结果），避免覆盖正在编辑的内容。
void MainDevMgr::onWorkspaceScanFinished() {
  if (!m_workspaceScanWatcher) return;
  const QVector<WorkspaceFileDiag> results = m_workspaceScanWatcher->future().result();
  m_workspaceScanWatcher->deleteLater();
  m_workspaceScanWatcher = nullptr;

  for (const WorkspaceFileDiag &diag : results) {
    // 打开中的文件已由实时验证维护 m_fileIssues，扫描结果可能是读取时的旧内容，跳过
    if (findEditorForFile(diag.filePath)) continue;
    if (diag.issues.isEmpty())
      m_fileIssues.remove(diag.filePath);
    else
      m_fileIssues[diag.filePath] = diag.issues;
  }
  refreshProblemPanel();

  // 统计问题面板当前实际展示的问题总数（含已打开文件的实时验证结果，与面板保持一致）
  int problemCount = 0;
  for (auto it = m_fileIssues.cbegin(); it != m_fileIssues.cend(); ++it)
    problemCount += it.value().size();
  m_ui->appendOutput(QStringLiteral("工作区错误检查完成，共 %1 个问题").arg(problemCount), false);
}

// ──────────────────────────────────────────────────────────────
//  保存按钮状态
// ──────────────────────────────────────────────────────────────

void MainDevMgr::updateSaveButtonState() {
  CodeEditor *cur = currentEditor();
  m_ui->saveBtn()->setEnabled(cur && cur->document()->isModified());
  // 全部保存：遍历所有面板的所有编辑器（含拆分副本，拆分副本不在 openFiles 中）
  bool anyModified = false;
  for (int pi = 0; pi < m_ui->editorPanelCount() && !anyModified; ++pi) {
    auto *tabs = m_ui->editorPanelAt(pi);
    if (!tabs) continue;
    for (int ti = 0; ti < tabs->count() && !anyModified; ++ti) {
      auto *w = tabs->widget(ti);
      CodeEditor *editor = qobject_cast<CodeEditor *>(w);
      if (!editor) {
        auto *jvw = qobject_cast<JsonVueWidget *>(w);
        if (jvw) editor = jvw->codeEditor();
      }
      if (editor && editor->document()->isModified()) anyModified = true;
    }
  }
  m_ui->saveAllBtn()->setEnabled(anyModified);
}

void MainDevMgr::refreshBreakpointList() {
  // 1) 将当前所有已打开编辑器的断点同步到持久存储（关闭文件时仍保留）
  for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
    auto *tabs = m_ui->editorPanelAt(pi);
    if (!tabs) continue;
    for (int ti = 0; ti < tabs->count(); ++ti) {
      auto *w = tabs->widget(ti);
      CodeEditor *editor = qobject_cast<CodeEditor *>(w);
      if (!editor) {
        auto *jvw = qobject_cast<JsonVueWidget *>(w);
        if (jvw) editor = jvw->codeEditor();
      }
      if (!editor) continue;
      const QString filePath = editor->objectName();
      if (!filePath.isEmpty()) {
        m_persistedBreakpoints[filePath] = editor->breakpoints();
      }
    }
  }

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
    m_debugger->setBreakpoints(debugBreakpoints());
  }

  // 4) 断点变更后持久化到磁盘，程序重启后还原
  saveBreakpointsToDisk();
}

/// @brief 断点存储文件路径（AppData 目录下，目录不存在则创建）
static QString breakpointStorePath() {
  QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (dir.isEmpty())
    dir = QDir::homePath() + QString::fromUtf8(CodeConstants::Paths::kAppDataDirName);
  QDir().mkpath(dir);
  return dir + QString::fromUtf8(CodeConstants::Paths::kBreakpointsStoreFile);
}

void MainDevMgr::saveBreakpointsToDisk() {
  // 先从所有已打开编辑器同步到持久存储，再落盘
  for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
    auto *tabs = m_ui->editorPanelAt(pi);
    if (!tabs) continue;
    for (int ti = 0; ti < tabs->count(); ++ti) {
      auto *w = tabs->widget(ti);
      CodeEditor *editor = qobject_cast<CodeEditor *>(w);
      if (!editor) {
        auto *jvw = qobject_cast<JsonVueWidget *>(w);
        if (jvw) editor = jvw->codeEditor();
      }
      if (!editor) continue;
      const QString filePath = editor->objectName();
      if (!filePath.isEmpty()) m_persistedBreakpoints[filePath] = editor->breakpoints();
    }
  }

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

void MainDevMgr::loadBreakpointsFromDisk() {
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

/// @brief 会话状态存储文件（记录上次打开的文件列表）
static QString sessionSettingsPath() {
  QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (dir.isEmpty())
    dir = QDir::homePath() + QString::fromUtf8(CodeConstants::Paths::kAppDataDirName);
  QDir().mkpath(dir);
  return dir + QStringLiteral("/session.ini");
}

void MainDevMgr::saveOpenFilesToSettings() {
  // 按编辑器面板分组保存（还原拆分数量与每个面板的文件）
  QList<QVariant> groups;
  QList<QVariant> activeIdxes;
  for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
    auto *tabs = m_ui->editorPanelAt(pi);
    if (!tabs) continue;
    QStringList files;
    for (int ti = 0; ti < tabs->count(); ++ti) {
      auto *w = tabs->widget(ti);
      CodeEditor *editor = qobject_cast<CodeEditor *>(w);
      if (!editor) {
        auto *jvw = qobject_cast<JsonVueWidget *>(w);
        if (jvw) editor = jvw->codeEditor();
      }
      if (!editor) continue;
      const QString fp = editor->objectName();
      if (!fp.isEmpty()) files.append(fp);
    }
    if (files.isEmpty()) continue;  // 跳过空面板
    groups << QVariant(files);
    // 保存当前激活标签页，保证重启后仍停留在关闭前正在编辑的文件
    activeIdxes << tabs->currentIndex();
  }
  QSettings s(sessionSettingsPath(), QSettings::IniFormat);
  s.setValue(QStringLiteral("session/editorPanels"), QVariant(groups));
  s.setValue(QStringLiteral("session/activeIndexes"), QVariant(activeIdxes));
}

void MainDevMgr::restoreOpenFilesFromSettings() {
  // 还原期间抑制目录树定位：打开文件会触发 setCurrentIndex/焦点变化，
  // 进而 locateFile 自动展开并滚动目录树，破坏保存的展开状态
  m_restoringSession = true;

  QSettings s(sessionSettingsPath(), QSettings::IniFormat);
  const QList<QVariant> groups = s.value(QStringLiteral("session/editorPanels")).toList();
  const QList<QVariant> activeIdxes = s.value(QStringLiteral("session/activeIndexes")).toList();

  QTabWidget *target = nullptr;  // nullptr → 打开到默认（第一个）面板
  for (int gi = 0; gi < groups.size(); ++gi) {
    const QStringList files = groups[gi].toStringList();
    for (const QString &fp : files) {
      if (QFileInfo::exists(fp)) openFileInEditor(fp, target);
    }
    // 恢复该面板的当前（激活）标签，停留在关闭前正在编辑的文件
    int active = (gi < activeIdxes.size()) ? activeIdxes[gi].toInt() : files.size() - 1;
    QTabWidget *panel = target ? target : currentTabWidget();
    if (panel && active >= 0 && active < panel->count()) panel->setCurrentIndex(active);
    // 下一组文件应放到新建的编辑器面板中，还原拆分数量
    if (gi < groups.size() - 1) {
      auto *panel2 = m_ui->createEditorPanel();
      m_ui->addEditorPanel(panel2);
      connectEditorPanel(panel2);  // 连接关闭/切换等信号，否则标签关闭按钮无效
      target = panel2;
    }
  }

  // 清理还原过程中产生的空面板（其文件已不存在），避免在编辑器区出现空白条
  for (int pi = m_ui->editorPanelCount() - 1; pi >= 0; --pi) {
    if (m_ui->editorPanelCount() <= 1) break;  // 至少保留一个编辑面板
    auto *panel = m_ui->editorPanelAt(pi);
    if (panel && panel->count() == 0) m_ui->removeEditorPanelAt(pi);
  }

  m_restoringSession = false;
}

/// 收集全部断点（已打开编辑器 + 已关闭的持久化断点），供调试器按文件命中
QMap<QString, QMap<int, bool>> MainDevMgr::debugBreakpoints() {
  QMap<QString, QMap<int, bool>> result;
  // 1) 已打开编辑器内的断点
  for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
    auto *tabs = m_ui->editorPanelAt(pi);
    if (!tabs) continue;
    for (int ti = 0; ti < tabs->count(); ++ti) {
      auto *w = tabs->widget(ti);
      CodeEditor *editor = qobject_cast<CodeEditor *>(w);
      if (!editor) {
        auto *jvw = qobject_cast<JsonVueWidget *>(w);
        if (jvw) editor = jvw->codeEditor();
      }
      if (!editor) continue;
      const QString filePath = editor->objectName();
      if (filePath.isEmpty()) continue;
      QMap<int, bool> lines = editor->breakpoints();
      if (!lines.isEmpty()) result.insert(filePath, lines);
    }
  }
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

void MainDevMgr::syncJsonVueBeforeSave() {
  auto *tabs = currentTabWidget();
  if (!tabs) return;
  auto *w = tabs->currentWidget();
  auto *jvw = qobject_cast<JsonVueWidget *>(w);
  if (jvw) jvw->syncVisualToCode();
}

/// 应用设置后刷新全局样式与编辑器高亮
void MainDevMgr::refreshTheme() {
  if (!m_ui) return;

  // 全局 Fusion 风格 + 调色板（原生控件菜单/下拉/表格/滚动条等随主题变化）
  SettingStore::ins().applyGlobalStyle();

  // 重新应用主窗口全局样式表（背景、边框等随主题变化）
  m_ui->setStyleSheet(AuiStyle::mainStyleSheet());

  // 刷新标题栏及菜单按钮颜色（标题栏背景、文件/视图按钮文字等）
  m_ui->refreshTitleBarStyle();

  // 刷新主窗口 log 输出面板（背景/文字色随主题更新）
  if (m_ui->outputPanel()) m_ui->outputPanel()->reloadStyle();

  // 刷新问题面板（背景/文字/条目颜色随主题重建）
  if (m_ui->problemPanel()) m_ui->problemPanel()->reloadStyle();

  // 刷新调试面板（调用栈/变量/断点页签栏与列表的颜色随主题重建）
  if (m_ui->debugPanel()) m_ui->debugPanel()->refreshStyle();

  // 刷新所有已打开编辑器的高亮颜色（语法高亮 / 行号 / 当前行等），
  // 以及 .jsonvue 的代码编辑器与可视化编辑器样式
  for (int p = 0; p < m_ui->editorPanelCount(); ++p) {
    QTabWidget *tabs = m_ui->editorPanelAt(p);
    if (!tabs) continue;
    for (int i = 0; i < tabs->count(); ++i) {
      QWidget *w = tabs->widget(i);
      if (auto *ed = qobject_cast<CodeEditor *>(w)) {
        ed->reloadColors();
      } else if (auto *jvw = qobject_cast<JsonVueWidget *>(w)) {
        if (jvw->codeEditor()) jvw->codeEditor()->reloadColors();
        if (jvw->visualEditor()) jvw->visualEditor()->reloadStyle();
      }
    }
  }

  // ── 刷新所有打开的顶层窗口（主窗口 + 对话框），保证文字随主题变色 ──
  // 背景色由全局调色板自动变化，但文字控件（QLabel/QPushButton）若持有创建时
  // 固化的样式表，深色下仍是浅色主题的深色文字而看不清，这里统一用当前文字色重建。
  const QWidgetList toplevels = qApp->topLevelWidgets();
  for (QWidget *w : toplevels) {
    // 对话框重建窗口级样式表与标题栏
    if (auto *dlg = qobject_cast<QDialog *>(w)) {
      dlg->setStyleSheet(AuiStyle::mainStyleSheet() + AuiStyle::dialogStyleSheet());
      const auto bars = dlg->findChildren<QWidget *>(QStringLiteral("AuiTitleBar"));
      for (QWidget *tb : bars) {
        AuiStyle::applyTitleBarStyle(tb);
        tb->update();
        for (QWidget *child : tb->findChildren<QWidget *>()) child->update();
      }
      // 刷新标题文字（如「设置」）颜色
      if (QLabel *tl = dlg->findChild<QLabel *>(QStringLiteral("AuiTitleLabel")))
        AuiStyle::applyTitleLabelStyle(tl);
      // 重建对话框标准按钮样式 + 标题栏控制按钮图标
      AuiButton::refreshThemedButtons(dlg);
    }
    // 重建无专门样式表的普通 QLabel 文字色（标题文字与有自定义样式的标签除外），
    // 用 auiAutoLabel 属性标记，使每次切换主题都用当前文字色重建、不固化旧色
    for (QLabel *l : w->findChildren<QLabel *>()) {
      if (l->objectName() == QStringLiteral("AuiTitleLabel")) continue;
      if (l->styleSheet().isEmpty() || l->property("auiAutoLabel").toBool()) {
        l->setProperty("auiAutoLabel", true);
        l->setStyleSheet(QStringLiteral("color: %1;").arg(AuiStyle::textColor().name()));
      }
    }
    // 强制 repolish，确保已存在子控件（含 QToolButton/QPushButton/QLabel/QMenu 等）重新解析
    // 新的样式表颜色。只 repolish 顶层窗口不够，子控件的 QSS 颜色需各自 unpolish/polish 才会重算。
    auto repolish = [](QWidget *root) {
      QList<QWidget *> all;
      all.reserve(64);
      all << root;
      all << root->findChildren<QWidget *>();
      for (QWidget *c : all) {
        c->style()->unpolish(c);
        c->style()->polish(c);
        c->update();
      }
    };
    repolish(w);
  }
}

/// 窗口字体变化后的轻量刷新（区别于 refreshTheme 的重活）
void MainDevMgr::refreshWindowFont() {
  if (!m_ui) return;
  // 窗口字体已由 SettingStore::applyWindowFont 应用到 qApp 与所有窗口，
  // 并已触发全部子控件重排 + 重绘（见 AuiStyle::applyAppFont）。
  // 这里只需重建标题栏文字样式：标题字号随窗口字号缩放，
  // 且标题字号是固化在样式表里的，必须重建才能生效。
  m_ui->refreshTitleBarStyle();
}
