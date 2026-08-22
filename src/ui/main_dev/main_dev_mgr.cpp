/**
 * @file main_dev_mgr.cpp
 * @brief 代码编辑器控制器层实现（单例 UI 控制器）
 *
 * 本文件只包含核心方法：窗口创建、信号初始化、文件树加载、保存逻辑。
 * 其他方法拆分到：
 *   - main_dev_mgr_file.cpp     文件操作（打开/创建/重命名/删除）
 *   - main_dev_mgr_tab.cpp      标签页管理（关闭/拆分/切换）
 *   - main_dev_mgr_connect.cpp  编辑器信号连接与事件过滤
 *   - main_dev_mgr_navigate.cpp 导航历史
 *   - main_dev_mgr_session.cpp  会话管理（文件列表/编辑器现场保存与还原）
 *   - main_dev_mgr_theme.cpp    主题/字体刷新
 */

#include "main_dev_mgr.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QShortcut>
#include <QStringList>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QtConcurrent/QtConcurrent>

#include "debug_controller.h"
#include "editor_lookup.h"
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
//  析构 — 脚本线程的等待清理由 DebugController 析构处理
// ──────────────────────────────────────────────────────────────

MainDevMgr::~MainDevMgr() = default;

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

  // ── 创建调试控制器（调试会话/脚本执行/断点管理）并注入协作回调 ──
  m_debug = new DebugController(m_ui, this);
  m_debug->setEditorProvider([this]() { return currentEditor(); });
  m_debug->setFileOpener([this](const QString &fp) { return openFileInEditor(fp); });
  m_debug->init();
  // 双击断点/调用栈/变量条目：打开对应文件并定位到行
  connect(m_debug, &DebugController::navigateToRequested, this,
          [this](const QString &filePath, int line) {
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
          });

  // ── 连接信号 ──
  initUi();

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
  m_debug->loadBreakpointsFromDisk();
  m_debug->refreshBreakpointList();

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
  // 窗口关闭前保存断点与会话状态
  connect(m_ui, &MainDevUi::uiClosing, this, [this]() {
    m_debug->saveBreakpointsToDisk();
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
    // 作用于当前获得焦点（或最后活跃）的编辑器面板，而不是第一个可见面板
    auto *tabs = currentTabWidget();
    if (!tabs) return;
    auto *jvw = qobject_cast<JsonVueWidget *>(tabs->currentWidget());
    if (!jvw) return;
    if (checked) {
      jvw->switchToVisual();
    } else {
      jvw->switchToCode();
    }
  });
}

/// 执行按钮（实际执行/状态管理由 DebugController 负责）
void MainDevMgr::connectBuildAction() {
  connect(m_ui->buildBtn(), &QPushButton::clicked, this, [this]() {
    if (m_debug->isScriptRunning()) {
      m_ui->appendOutput(QStringLiteral("脚本正在执行中，请先停止"), true);
      return;
    }
    QString scriptPath = m_ui->startupCombo()->currentData().toString();
    if (scriptPath.isEmpty()) {
      m_ui->appendOutput(QStringLiteral("未选择启动项"), true);
      return;
    }
    m_debug->runScript(scriptPath, m_ui->fileTree()->rootPath(), false);
  });

  // 停止按钮：设置取消标志，由工作线程轮询检查
  connect(m_ui->stopBtn(), &QPushButton::clicked, m_debug, &DebugController::stopScript);
}

// ──────────────────────────────────────────────────────────────
//  编辑器查找
// ──────────────────────────────────────────────────────────────

/// 在所有编辑面板中查找已打开指定文件的编辑器
CodeEditor *MainDevMgr::findEditorForFile(const QString &filePath) const {
  return editorForFile(m_ui, filePath);
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
  m_ui->appendOutput(
      QStringLiteral("工作区错误检查完成，共 %1 个问题（扫描时点快照；此后编辑产生的实时错误"
                     "以「问题」面板为准）")
          .arg(problemCount),
      false);
}

// ──────────────────────────────────────────────────────────────
//  保存按钮状态
// ──────────────────────────────────────────────────────────────

void MainDevMgr::updateSaveButtonState() {
  CodeEditor *cur = currentEditor();
  m_ui->saveBtn()->setEnabled(cur && cur->document()->isModified());
  // 全部保存：遍历所有面板的所有编辑器（含拆分副本，拆分副本不在 openFiles 中）
  bool anyModified = false;
  forEachEditor(m_ui, [&anyModified](CodeEditor *editor) {
    if (editor->document()->isModified()) {
      anyModified = true;
      return false;  // 提前结束
    }
    return true;
  });
  m_ui->saveAllBtn()->setEnabled(anyModified);
}

void MainDevMgr::syncJsonVueBeforeSave() {
  auto *tabs = currentTabWidget();
  if (!tabs) return;
  auto *w = tabs->currentWidget();
  auto *jvw = qobject_cast<JsonVueWidget *>(w);
  if (jvw) jvw->syncVisualToCode();
}
