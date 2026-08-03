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
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QShortcut>
#include <QTabWidget>

#include "main_dev_model.h"
#include "main_dev_ui.h"
#include "main_dev_ui_ext.h"
#include "src/engine/ac_language.h"
#include "src/engine/script/ac_engine.h"
#include "src/ui/json_vue/json_vue_widget.h"
#include "src/ui/main_dev/help_key/help_key_mgr.h"
#include "src/util/common/path_resolver.h"
#include "src/util/ui/code/code_editor.h"

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

  // ── 设置日志回调：脚本中 printLog() 输出到 UI 面板 ──
  AcEngine::ins().setLogCallback(
      [this](const QString &text, bool isError) { m_ui->appendOutput(text, isError); });

  // ── 加载文件树 ──
  loadFiles();

  // ── 恢复可视化编辑按钮状态 ──
  m_ui->visualToggleBtn()->setChecked(m_ui->fileTree()->visualToggle());

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
  connect(m_ui->fileTree(), &TreeDir::fileActivated, this, &MainDevMgr::openFileInEditor);

  // ── 帮助 → 快捷键 ──
  connect(m_ui->helpKeyAction(), &QAction::triggered, this, []() { HelpKeyMgr::ins().open(); });
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
    QString scriptPath = m_ui->startupCombo()->currentData().toString();
    if (scriptPath.isEmpty()) {
      m_ui->appendOutput(QStringLiteral("未选择启动项"), true);
      return;
    }
    m_ui->appendOutput(QStringLiteral("执行: %1").arg(scriptPath), false);
    AcEngine::ins().setRootDir(m_ui->fileTree()->rootPath());
    QString err = AcEngine::ins().execute(scriptPath);
    if (!err.isEmpty()) {
      m_ui->appendOutput(err, true);
    } else {
      m_ui->appendOutput(QStringLiteral("执行完成"), false);
      QStringList files = AcEngine::ins().generatedFiles();
      for (const QString &f : files) m_ui->appendOutput(QStringLiteral("  生成: %1").arg(f), false);
    }
  });
}

/// 编辑器面板信号 + 事件过滤器
void MainDevMgr::connectEditorPanels() {
  // 连接初始编辑器面板组的信号
  for (int i = 0; i < m_ui->editorPanelCount(); ++i) {
    auto *tabs = m_ui->editorPanelAt(i);
    if (tabs) {
      connect(tabs, &QTabWidget::tabCloseRequested, this, &MainDevMgr::onTabCloseRequested);
      connect(tabs, &QTabWidget::currentChanged, this, &MainDevMgr::onCurrentTabChanged);
      auto *bar = qobject_cast<DraggableTabBar *>(tabs->tabBar());
      if (bar) {
        connect(bar, &DraggableTabBar::closeOthersRequested, this, &MainDevMgr::onCloseOthers);
        connect(bar, &DraggableTabBar::closeAllRequested, this, &MainDevMgr::onCloseAll);
        connect(bar, &QTabBar::tabBarClicked, this, &MainDevMgr::onTabBarClicked);
      }
    }
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

void MainDevMgr::syncJsonVueBeforeSave() {
  auto *tabs = currentTabWidget();
  if (!tabs) return;
  auto *w = tabs->currentWidget();
  auto *jvw = qobject_cast<JsonVueWidget *>(w);
  if (jvw) jvw->syncVisualToCode();
}
