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
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
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
#include "src/engine/semantic/workspace_index.h"
#include "src/ui/json_source/json_source_widget.h"
#include "src/ui/json_vue/json_vue_editor.h"
#include "src/ui/json_vue/json_vue_widget.h"
#include "src/ui/schema_json/schema_json_widget.h"
#include "src/ui/setting/setting_mgr.h"
#include "src/util/common/code_constants.h"
#include "src/util/common/path_resolver.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_message_box.h"
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

  // 注册关闭前确认：有未保存修改时弹窗（保存/不保存/取消），取消则阻止关闭
  m_ui->setCloseConfirmer([this]() { return confirmExit(); });

  // ── 构建界面 ──
  m_ui->setupUI();
  m_ui->resize(1400, 850);
  m_ui->setWindowTitle(MainDevUi::defaultTitle());

  // 注册编辑器全局文件内容提供器：跨文件 import 解析时优先返回已打开文件（或未打开但有
  // 缓冲修改的文件）的实时缓冲，避免重命名等操作后磁盘与内存不一致导致假报错
  CodeEditor::setGlobalContentProvider([this](const QString &filePath) {
    CodeEditor *ed = findEditorForFile(filePath);
    if (ed) return ed->toPlainText();
    if (m_pendingChanges.contains(filePath)) return m_pendingChanges.value(filePath);
    return QString();
  });

  // 引用面板的后台收集：主线程构建实时内容快照（已打开编辑器 + 缓冲文件），优先读缓冲
  m_ui->referencePanel()->setLiveContentProvider([this]() { return collectLiveContents(); });
  // 查找面板的跨文件搜索：同样优先读缓冲（已打开编辑器 + 缓冲文件）
  m_ui->findPanel()->setLiveContentProvider([this]() { return collectLiveContents(); });

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
  // 编辑器相关色（hl.* + 不进全局的 editor.*）变化：走更轻量的刷新（只重建编辑器语法高亮），
  // 不重建全局 QSS / 调色板 / 面板，避免在颜色设置对话框里挑颜色时整套重刷卡顿
  m_hlColorTimer = new QTimer(this);
  m_hlColorTimer->setSingleShot(true);
  m_hlColorTimer->setInterval(60);
  connect(m_hlColorTimer, &QTimer::timeout, this, &MainDevMgr::refreshHighlightColors);
  connect(&store, &SettingStore::highlightColorsChanged, m_hlColorTimer,
          qOverload<>(&QTimer::start));
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
  connect(m_ui->fileTree(), &TreeDir::moveRequested, this, &MainDevMgr::onMoveFile);
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
    saveAllEditors();
    // 未打开文件的缓冲修改（重命名等操作产生）一并写盘
    flushPendingChanges();
  });
}

/// 保存所有已打开且被修改的编辑器（含拆分副本；jsonvue 需先可视化→代码同步）
void MainDevMgr::saveAllEditors() {
  for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
    auto *tabs = m_ui->editorPanelAt(pi);
    if (!tabs) continue;
    for (int ti = 0; ti < tabs->count(); ++ti) {
      auto *w = tabs->widget(ti);
      CodeEditor *editor = nullptr;
      bool wasModified = false;
      if (auto *cv = qobject_cast<CodeVisualSyncWidget *>(w)) {
        editor = cv->codeEditor();
        // syncVisualToCode 会重置 modified 为 false，需先记录
        wasModified = editor && editor->document()->isModified();
        cv->syncVisualToCode();
      } else {
        editor = qobject_cast<CodeEditor *>(w);
      }
      if (editor && (wasModified || editor->document()->isModified())) {
        saveAndSync(editor);
      }
    }
  }
}

/// 把未打开文件的缓冲修改写盘并清除树目录黄色标记
void MainDevMgr::flushPendingChanges() {
  if (m_pendingChanges.isEmpty()) return;
  const QHash<QString, QString> snapshot = m_pendingChanges.snapshot();  // 迭代中会清空，先拷贝
  for (auto it = snapshot.cbegin(); it != snapshot.cend(); ++it) {
    QFile f(it.key());
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
      f.write(it.value().toUtf8());
      f.close();
    }
  }
  m_pendingChanges.clearAll();
  for (auto it = snapshot.cbegin(); it != snapshot.cend(); ++it) {
    if (m_ui->fileTree()) m_ui->fileTree()->setFileModified(it.key(), false);
  }
  updateSaveButtonState();
}

/// 清除某文件的缓冲修改（保存或打开后，缓冲已由编辑器 document 接管）
void MainDevMgr::clearPendingChange(const QString &filePath) { m_pendingChanges.clear(filePath); }

/// 构建实时内容快照：已打开编辑器内容 + 未打开但有缓冲修改的文件内容。
/// 主线程调用；返回的 QHash 拷贝进后台线程按值使用（QtConcurrent），线程安全。
QHash<QString, QString> MainDevMgr::collectLiveContents() const {
  QHash<QString, QString> live;
  forEachEditor(m_ui, [&live](CodeEditor *editor) {
    const QString fp = editor->objectName();
    if (!fp.isEmpty()) live.insert(fp, editor->toPlainText());
    return true;
  });
  const QHash<QString, QString> pending = m_pendingChanges.snapshot();
  for (auto it = pending.cbegin(); it != pending.cend(); ++it) live.insert(it.key(), it.value());
  return live;
}

/// 退出确认：无未保存修改直接放行；有则弹窗三选（保存 / 不保存 / 取消）
bool MainDevMgr::confirmExit() {
  QStringList dirty;
  forEachEditor(m_ui, [&dirty](CodeEditor *editor) {
    if (editor->document()->isModified() && !editor->objectName().isEmpty())
      dirty << QFileInfo(editor->objectName()).fileName();
    return true;
  });
  for (const QString &fp : m_pendingChanges.filePaths()) dirty << QFileInfo(fp).fileName();
  dirty.removeDuplicates();
  if (dirty.isEmpty()) return true;

  // 统一使用封装的消息框（与项目其它提示风格一致）
  const AuiMessageBox::Choice ch =
      AuiMessageBox::question3(m_ui, QStringLiteral("未保存的修改"),
                               QStringLiteral("以下文件有未保存的修改：\n%1\n\n要保存这些修改吗？")
                                   .arg(dirty.join(QStringLiteral("、"))),
                               QStringLiteral("保存"), QStringLiteral("不保存"));
  switch (ch) {
    case AuiMessageBox::Choice::kFirst:
      saveAllEditors();
      flushPendingChanges();
      return true;
    case AuiMessageBox::Choice::kSecond: {
      // 丢弃：清除未打开文件的缓冲修改与树目录黄色标记（已打开编辑器的改动随进程退出丢弃）
      const QHash<QString, QString> snapshot = m_pendingChanges.snapshot();
      m_pendingChanges.clearAll();
      for (auto it = snapshot.cbegin(); it != snapshot.cend(); ++it) {
        if (m_ui->fileTree()) m_ui->fileTree()->setFileModified(it.key(), false);
      }
      return true;
    }
    default:
      return false;  // 取消
  }
}

/// 可视化/代码切换按钮
void MainDevMgr::connectVisualToggle() {
  connect(m_ui->visualToggleBtn(), &QPushButton::toggled, this, [this](bool checked) {
    // 保存按钮状态到 tree.config
    m_ui->fileTree()->setVisualToggle(checked);
    // 全局生效：遍历所有编辑面板的所有标签，对可视化包装器（schema json / jsonvue /
    // jsonsource）统一切换为可视化或代码视图。
    auto apply = [checked](QWidget *w) {
      if (auto *cv = qobject_cast<CodeVisualSyncWidget *>(w)) {
        checked ? cv->switchToVisual() : cv->switchToCode();
      }
    };
    for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
      auto *tabs = m_ui->editorPanelAt(pi);
      if (!tabs) continue;
      for (int ti = 0; ti < tabs->count(); ++ti) apply(tabs->widget(ti));
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

  // 跨文件搜索面板（查找）：单击结果 → 打开文件定位（不选中，与引用一致）
  connect(m_ui->findPanel(), &SearchPanel::openRequested, this, &MainDevMgr::onOpenHighlightResult);
  // 引用面板：单击引用 → 打开文件定位（不选中，避免蓝色选区盖住引用高亮）
  connect(m_ui->referencePanel(), &ReferencePanel::openRequested, this,
          &MainDevMgr::onOpenHighlightResult);
  // 引用面板语义收集完成 → 保存位置并应用编辑器语义高亮（VSCode 行为）
  connect(m_ui->referencePanel(), &ReferencePanel::referencesReady, this,
          [this](const QVector<RenameRef> &refs) {
            m_referenceRefs = refs;
            if (m_ui->leftTabs() && m_ui->leftTabs()->currentWidget() == m_ui->referencePanel()) {
              applyReferenceRefsToEditors();
            }
          });
  // 查找面板搜索完成 → 防抖后同步编辑器查找高亮（与引用面板一致，编辑器持续变色）
  m_searchHighlightTimer = new QTimer(this);
  m_searchHighlightTimer->setSingleShot(true);
  m_searchHighlightTimer->setInterval(200);
  connect(m_searchHighlightTimer, &QTimer::timeout, this, [this]() {
    // 触发时复核左侧 tab 仍为「查找」：防止输入后 200ms 内已切到引用/文件等
    // 面板，定时器仍强行应用查找高亮，造成两类高亮同时残留（与引用即时应用保持一致）
    if (!m_lastSearchText.isEmpty() && m_ui->leftTabs() &&
        m_ui->leftTabs()->currentWidget() == m_ui->findPanel()) {
      applySearchHighlightToEditors(m_lastSearchText);
    }
  });
  // 工作区扫描防抖定时器：编辑/保存/重命名产生的多次扫描请求合并为一次（避免重复全量扫描）
  m_scanTimer = new QTimer(this);
  m_scanTimer->setSingleShot(true);
  m_scanTimer->setInterval(500);
  connect(m_scanTimer, &QTimer::timeout, this, [this]() { startWorkspaceScan(true); });
  // 已打开文件重验防抖定时器：合并同一事件循环内的多次 textChanged 为一次重验
  // （0ms：对单次编辑无感知延迟；防止打开大文件/程序化批量变更触发 N×M 次验证风暴卡死）
  m_revalidateTimer = new QTimer(this);
  m_revalidateTimer->setSingleShot(true);
  m_revalidateTimer->setInterval(0);
  connect(m_revalidateTimer, &QTimer::timeout, this, [this]() {
    CodeEditor *src = m_revalidateSource;
    m_revalidateSource = nullptr;
    forEachEditor(m_ui, [src](CodeEditor *ed) {
      if (ed != src) ed->validate();
      return true;
    });
  });
  // 问题面板刷新防抖：同一 burst 内多个编辑器的验证结果合并为一次面板重建
  // （防止每个验证结果都触发一次全量重建；扫描完成时仍立即刷新一次）
  m_problemPanelTimer = new QTimer(this);
  m_problemPanelTimer->setSingleShot(true);
  m_problemPanelTimer->setInterval(150);
  connect(m_problemPanelTimer, &QTimer::timeout, this, &MainDevMgr::refreshProblemPanel);
  // 保存/重命名后防抖重扫（合并连续保存，避免每次保存都全量扫描）
  m_workspaceRescanTimer = new QTimer(this);
  m_workspaceRescanTimer->setSingleShot(true);
  m_workspaceRescanTimer->setInterval(500);
  connect(m_workspaceRescanTimer, &QTimer::timeout, this, [this]() {
    // 同步重建语义索引（模块表），保证跳转定义/引用与最新磁盘内容一致
    if (const QString root = m_ui ? m_ui->fileTree()->rootPath() : QString(); !root.isEmpty()) {
      WorkspaceIndex::ins().rebuild(root);
    }
    // 触发一次扫描（与编辑触发的扫描共用同一防抖，天然合并，不会扫两次）
    m_scanTimer->start();
  });
  connect(m_ui->findPanel(), &SearchPanel::searchPerformed, this, [this](const QString &text) {
    m_lastSearchText = text;
    if (text.isEmpty()) {
      clearSearchHighlightFromEditors();
    } else if (m_ui->leftTabs() && m_ui->leftTabs()->currentWidget() == m_ui->findPanel()) {
      m_searchHighlightTimer->start();
    }
  });
  // 左侧 tab 切换：查找/引用面板显示/隐藏时同步编辑器高亮（VSCode 行为）
  if (auto *leftTabs = m_ui->leftTabs()) {
    connect(leftTabs, &QTabWidget::currentChanged, this, &MainDevMgr::onLeftTabChanged);
  }

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

  // 跨文件搜索面板（查找 / 引用）的搜索范围 = 文件树根目录
  const QString root = m_ui->fileTree()->rootPath();
  m_ui->findPanel()->setSearchRoot(root);
  m_ui->referencePanel()->setSearchRoot(root);

  // 启动时构建工作区语义索引（模块表），供跳转定义/引用使用
  if (!root.isEmpty()) WorkspaceIndex::ins().rebuild(root);
}

/// 启动后台工作区全量错误扫描：收集所有可验证文件，在工作线程逐个验证，
/// 完成后通过 onWorkspaceScanFinished 合并到问题面板（不阻塞 UI）
void MainDevMgr::startWorkspaceScan(bool silent) {
  if (m_workspaceScanWatcher) return;  // 已有扫描任务，避免重复启动
  const QString rootDir = m_ui ? m_ui->fileTree()->rootPath() : QString();
  if (rootDir.isEmpty()) return;

  const QStringList files = collectWorkspaceFiles(rootDir);
  if (files.isEmpty()) return;

  // 已打开文件 + 未打开但有缓冲修改的文件：使用实时内存内容（VSCode 行为），
  // 扫描读缓冲而非磁盘快照，避免重命名等操作后磁盘与内存不一致导致假报错
  const QHash<QString, QString> liveContents = collectLiveContents();

  m_workspaceScanWatcher = new QFutureWatcher<QVector<WorkspaceFileDiag>>(this);
  connect(m_workspaceScanWatcher, &QFutureWatcherBase::finished, this,
          &MainDevMgr::onWorkspaceScanFinished);
  QFuture<QVector<WorkspaceFileDiag>> future =
      QtConcurrent::run(scanWorkspaceDiagnostics, files, liveContents);
  m_workspaceScanWatcher->setFuture(future);
  m_workspaceScanSilent = silent;  // 静默扫描完成时不打印"完成"提示（避免每次编辑刷屏）
  if (!silent) {
    m_ui->appendOutput(QStringLiteral("开始检查工作区错误（%1 个文件）...").arg(files.size()),
                       false);
  }
}

/// 保存/重命名后防抖重扫：合并连续保存，避免每次保存都全量扫描
void MainDevMgr::scheduleWorkspaceRescan() {
  if (m_workspaceRescanTimer) m_workspaceRescanTimer->start();
}

/// 后台扫描完成：将结果合并到工作区问题聚合。
/// 打开中的文件以实时验证结果为准（跳过扫描结果），避免覆盖正在编辑的内容。
void MainDevMgr::onWorkspaceScanFinished() {
  if (!m_workspaceScanWatcher) return;
  const QVector<WorkspaceFileDiag> results = m_workspaceScanWatcher->future().result();
  m_workspaceScanWatcher->deleteLater();
  m_workspaceScanWatcher = nullptr;

  // 批量合并：setFileError 只更新叶子，文件夹错误汇总在循环结束后一次性重算，
  // 避免对每个文件递归遍历整棵子树（O(文件数×子树大小)）造成卡顿
  m_ui->fileTree()->beginBulkErrorUpdate();
  for (const WorkspaceFileDiag &diag : results) {
    // 打开中的文件已由实时验证维护 m_fileIssues（最新按键即时更新），扫描快照可能稍旧，跳过
    if (findEditorForFile(diag.filePath)) continue;
    if (diag.issues.isEmpty()) {
      m_fileIssues.remove(diag.filePath);
      m_ui->fileTree()->clearFileError(diag.filePath);
    } else {
      m_fileIssues[diag.filePath] = diag.issues;
      m_ui->fileTree()->setFileError(diag.filePath, diag.issues.size());
    }
  }
  m_ui->fileTree()->endBulkErrorUpdate();
  refreshProblemPanel();

  // 静默模式（保存后自动重扫）不打印"完成"提示，避免保存一次刷屏
  if (m_workspaceScanSilent) {
    m_workspaceScanSilent = false;
    return;
  }

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
  // 全部保存：已打开编辑器的脏文档（含拆分副本，不在 openFiles 中）+ 未打开文件的缓冲修改
  bool anyModified = !m_pendingChanges.isEmpty();
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
  if (auto *cv = qobject_cast<CodeVisualSyncWidget *>(tabs->currentWidget())) {
    cv->syncVisualToCode();
  }
}
