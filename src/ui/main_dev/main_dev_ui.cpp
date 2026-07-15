/**
 * @file main_dev_ui.cpp
 * @brief 代码编辑器主窗口（视图层）实现
 */

#include "main_dev_ui.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileInfo>
#include <QFileInfoList>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScrollBar>
#include <QSizeGrip>
#include <QStyle>
#include <QTabBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>

#include "main_dev_ui_ext.h"
#include "src/ui/demo/demo_mgr.h"
#include "src/util/ui/aui_window.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/code/code_log.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/component/aui_style.h"

//  构造
// ════════════════════════════════════════════════════════════

MainDevUi::MainDevUi(QWidget *parent) : QMainWindow(parent) {}

// ──────────────────────────────────────────────────────────────
//  界面布局 — 入口
// ──────────────────────────────────────────────────────────────

void MainDevUi::setupUI() {
  AuiWindow::setupFramelessWindow(this);

  // ── 标题栏 ──
  setupTitleBar();

  // ── 编辑器区域 ──
  setupEditorArea();

  // ── 内容容器（编辑器 + 状态栏） ──
  auto *contentWidget = new QWidget;
  auto *contentLayout = new QVBoxLayout(contentWidget);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(0);
  contentLayout->addWidget(m_mainSplitter, 1);

  // ── 底部状态栏 ──
  setupStatusBar(contentWidget, contentLayout);

  // ── 外层 QFrame 窗口框架 ──
  AuiWindow::applyWindowFrame(this, m_titleBar, contentWidget);
  AuiWindow::enableWin32Resize(this);
}

// ──────────────────────────────────────────────────────────────
//  构建自定义标题栏
// ──────────────────────────────────────────────────────────────

void MainDevUi::setupTitleBar() {
  TitleBarOptions opts;
  opts.title = windowTitle();
  opts.titleRightAligned = true;
  auto tb = AuiWindow::createTitleBar(this, opts);
  m_titleBar = tb.titleBar;
  m_titleLabel = tb.titleLabel;
  auto *titleLayout = qobject_cast<QHBoxLayout *>(tb.titleBar->layout());

  // ── 文件菜单 ──
  auto *fileMenu = new QMenu(m_titleBar);
  m_openAction = fileMenu->addAction(QStringLiteral("打开(&O)..."));
  m_openAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
  m_openFolderAction = fileMenu->addAction(QStringLiteral("打开文件夹(&F)..."));

  auto *fileBtn = new QToolButton;
  fileBtn->setText(QStringLiteral("文件(&F)"));
  fileBtn->setPopupMode(QToolButton::InstantPopup);
  fileBtn->setMenu(fileMenu);
  titleLayout->insertWidget(tb.contentInsertIndex, fileBtn);
  tb.contentInsertIndex++;

  // ── 视图菜单 ──
  auto *viewMenu = new QMenu(m_titleBar);
  m_splitAction = viewMenu->addAction(QStringLiteral("向右拆分编辑器"));
  m_splitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+\\")));
  m_closeAction = viewMenu->addAction(QStringLiteral("关闭标签页"));
  m_closeAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));

  auto *viewBtn = new QToolButton;
  viewBtn->setText(QStringLiteral("视图(&V)"));
  viewBtn->setPopupMode(QToolButton::InstantPopup);
  viewBtn->setMenu(viewMenu);
  titleLayout->insertWidget(tb.contentInsertIndex, viewBtn);
  tb.contentInsertIndex++;

  // ── 帮助菜单 ──
  auto *helpMenu = new QMenu(m_titleBar);
  m_helpExampleAction = helpMenu->addAction(QStringLiteral("例子(&E)..."));

  auto *helpBtn = new QToolButton;
  helpBtn->setText(QStringLiteral("帮助(&H)"));
  helpBtn->setPopupMode(QToolButton::InstantPopup);
  helpBtn->setMenu(helpMenu);
  titleLayout->insertWidget(tb.contentInsertIndex, helpBtn);
  tb.contentInsertIndex++;

  // ── 执行按钮 ──
  m_buildBtn = AuiButton::createBuildButton();
  m_buildBtn->setToolTip(QStringLiteral("执行 (F5)"));
  titleLayout->insertWidget(tb.contentInsertIndex, m_buildBtn);
  tb.contentInsertIndex++;

  // ── 启动项下拉框 ──
  m_startupCombo = AuiComboBox::create();
  m_startupCombo->setMinimumWidth(120);
  m_startupCombo->setToolTip(QStringLiteral("选择启动项"));
  titleLayout->insertWidget(tb.contentInsertIndex, m_startupCombo);
  tb.contentInsertIndex++;

  // ── 保存按钮 ──
  m_saveBtn = AuiButton::createSaveButton();
  m_saveBtn->setEnabled(false);
  titleLayout->insertWidget(tb.contentInsertIndex, m_saveBtn);
  tb.contentInsertIndex++;

  // ── 保存全部按钮 ──
  m_saveAllBtn = AuiButton::createSaveAllButton();
  m_saveAllBtn->setEnabled(false);
  titleLayout->insertWidget(tb.contentInsertIndex, m_saveAllBtn);

  // ── 拆分按钮（在标题标签之前插入） ──
  m_splitBtn = AuiButton::createSplitButton();
  int titleIdx = titleLayout->indexOf(m_titleLabel);
  titleLayout->insertWidget(titleIdx, m_splitBtn);

  connect(m_splitBtn, &QPushButton::clicked, m_splitAction, &QAction::trigger);

  // ── 帮助 → 例子 ──
  connect(m_helpExampleAction, &QAction::triggered, this, []() { DemoMgr::ins().open(); });
}

// ──────────────────────────────────────────────────────────────
//  构建编辑器区域（文件树 + 分割器 + 编辑器面板）
// ──────────────────────────────────────────────────────────────

void MainDevUi::setupEditorArea() {
  // ── 左侧文件树 ──
  m_fileTree = new TreeDir;

  connect(m_fileTree, &TreeDir::startupItemsChanged, this, &MainDevUi::refreshStartupCombo);

  connect(m_startupCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int /*idx*/) {
            QString path = m_startupCombo->currentData().toString();
            if (!path.isEmpty()) m_fileTree->setSelectedStartup(path);
          });

  // ── 右侧编辑器分割器 ──
  m_editorSplitter = new QSplitter(Qt::Horizontal);
  m_editorSplitter->setChildrenCollapsible(false);

  QTabWidget *initialTabs = createEditorPanel();
  m_editorSplitter->addWidget(initialTabs);
  m_editorSplitter->setStretchFactor(0, 1);

  // ── 输出面板 ──
  m_outputPanel = new CodeLog;
  m_outputPanel->installEventFilter(this);

  // ── 垂直分割器：编辑器 + 输出面板 ──
  m_contentSplitter = new QSplitter(Qt::Vertical);
  m_contentSplitter->addWidget(m_editorSplitter);
  m_contentSplitter->addWidget(m_outputPanel);
  m_contentSplitter->setStretchFactor(0, 1);
  m_contentSplitter->setStretchFactor(1, 0);

  // ── 主分割器：文件树 + 右侧区域 ──
  m_mainSplitter = new QSplitter(Qt::Horizontal);
  m_mainSplitter->addWidget(m_fileTree);
  m_mainSplitter->addWidget(m_contentSplitter);
  m_mainSplitter->setStretchFactor(0, 0);
  m_mainSplitter->setStretchFactor(1, 1);
}

// ──────────────────────────────────────────────────────────────
//  构建底部状态栏
// ──────────────────────────────────────────────────────────────

void MainDevUi::setupStatusBar(QWidget *contentWidget, QVBoxLayout *contentLayout) {
  auto *statusBarContent = new QWidget;
  auto *statusBarContentLayout = new QHBoxLayout(statusBarContent);
  statusBarContentLayout->setContentsMargins(4, 0, 4, 0);
  statusBarContentLayout->setSpacing(0);

  m_errorLabel = new QLabel;
  m_errorLabel->setStyleSheet(
      QStringLiteral("QLabel { color: %1; }").arg(AuiStyle::errorTextColor().name()));
  statusBarContentLayout->addWidget(m_errorLabel, 1);

  m_cursorPositionLabel = new QLabel(QStringLiteral("行: 1, 列: 1"));
  m_cursorPositionLabel->setMinimumWidth(120);
  m_cursorPositionLabel->setAlignment(Qt::AlignCenter);
  statusBarContentLayout->addWidget(m_cursorPositionLabel);

  contentLayout->addWidget(AuiWindow::createStatusBar(contentWidget, statusBarContent));
}

// ══════════════════════════════════════════════════════════════
//  窗口原生事件（WM_NCHITTEST — 可拉伸边框 + 标题栏拖拽）
// ══════════════════════════════════════════════════════════════

#if defined(Q_OS_WIN)
bool MainDevUi::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
  if (AuiWindow::handleNativeEvent(this, m_titleBar, eventType, message, result)) {
    return true;
  }
  return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

// ══════════════════════════════════════════════════════════════
//  窗口状态变化
// ══════════════════════════════════════════════════════════════

void MainDevUi::changeEvent(QEvent *ev) { QMainWindow::changeEvent(ev); }

// ──────────────────────────────────────────────────────────────
//  编辑器面板组创建
// ──────────────────────────────────────────────────────────────

QTabWidget *MainDevUi::createEditorPanel() {
  auto *tabs = new DimmableTabWidget;
  tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  tabs->setContentsMargins(0, 0, 0, 0);
  tabs->setStyleSheet(QStringLiteral("QTabWidget::pane { margin: 0; border: none; }"));
  return tabs;
}

void MainDevUi::showEvent(QShowEvent *event) {
  QMainWindow::showEvent(event);
  // 仅在首次显示时设置分割器初始比例
  static bool firstShow = true;
  if (firstShow) {
    firstShow = false;
    const int w = width();
    const int h = height();
    m_mainSplitter->setSizes(
        {qMax(TreeDir::kMinWidth, w / 5), w - qMax(TreeDir::kMinWidth, w / 5)});
    m_contentSplitter->setSizes({h - 120, 120});
  }
}

void MainDevUi::closeEvent(QCloseEvent *event) {
  QApplication::quit();
  QMainWindow::closeEvent(event);
}

// ══════════════════════════════════════════════════════════════
//  编辑器面板组操作
// ══════════════════════════════════════════════════════════════

int MainDevUi::editorPanelCount() const { return m_editorSplitter->count(); }

QTabWidget *MainDevUi::editorPanelAt(int index) const {
  return qobject_cast<QTabWidget *>(m_editorSplitter->widget(index));
}

int MainDevUi::editorPanelIndex(const QTabWidget *tabs) const {
  return m_editorSplitter->indexOf(const_cast<QTabWidget *>(tabs));
}

void MainDevUi::addEditorPanel(QTabWidget *panel) { m_editorSplitter->addWidget(panel); }

void MainDevUi::removeEditorPanelAt(int index) {
  QWidget *w = m_editorSplitter->widget(index);
  if (w) w->deleteLater();
}

void MainDevUi::setEditorPanelsUniformStretch() {
  int count = m_editorSplitter->count();
  for (int i = 0; i < count; ++i) m_editorSplitter->setStretchFactor(i, 1);
}

// ══════════════════════════════════════════════════════════════
//  主分割器操作
// ══════════════════════════════════════════════════════════════

void MainDevUi::adjustMainSplitter() {
  m_mainSplitter->setSizes(
      {m_fileTree->width(), m_mainSplitter->width() - m_fileTree->width() - 6});
}

int MainDevUi::mainSplitterWidth() const { return m_mainSplitter->width(); }

int MainDevUi::fileTreeWidth() const { return m_fileTree->width(); }

// ══════════════════════════════════════════════════════════════
//  状态栏
// ══════════════════════════════════════════════════════════════

void MainDevUi::setCursorStatusText(const QString &text) { m_cursorPositionLabel->setText(text); }

void MainDevUi::setErrorMessage(const QString &msg) { m_errorLabel->setText(msg); }

// ══════════════════════════════════════════════════════════════
//  标签页颜色
// ══════════════════════════════════════════════════════════════

void MainDevUi::applyTabDimming(QTabWidget *active) {
  for (int i = 0; i < editorPanelCount(); ++i) {
    auto *tabs = editorPanelAt(i);
    if (!tabs) continue;

    QTabBar *bar = tabs->tabBar();
    AuiStyle::ensureFusionTabBar(bar);

    bool isActive = (tabs == active);
    for (int j = 0; j < bar->count(); ++j)
      bar->setTabTextColor(j, isActive ? QColor() : AuiStyle::inactiveTabColor());
  }
}

// ══════════════════════════════════════════════════════════════
//  输出面板
// ══════════════════════════════════════════════════════════════

void MainDevUi::appendOutput(const QString &text, bool isError) {
  m_outputPanel->append(text, isError);
}

void MainDevUi::clearOutput() { m_outputPanel->clearLog(); }

bool MainDevUi::eventFilter(QObject *obj, QEvent *ev) {
  if (obj == m_outputPanel && ev->type() == QEvent::KeyPress) {
    auto *keyEv = static_cast<QKeyEvent *>(ev);
    if (keyEv->key() == Qt::Key_Backspace || keyEv->key() == Qt::Key_Delete) {
      clearOutput();
      return true;
    }
  }
  return QMainWindow::eventFilter(obj, ev);
}

/// @brief 刷新启动项下拉框
void MainDevUi::refreshStartupCombo() {
  if (!m_startupCombo || !m_fileTree) return;

  m_startupCombo->blockSignals(true);
  QString prev = m_startupCombo->currentData().toString();
  m_startupCombo->clear();

  QStringList files = m_fileTree->startupFiles();
  QFileInfo selInfo(m_fileTree->selectedStartup());

  for (const QString &path : files) {
    QFileInfo fi(path);
    m_startupCombo->addItem(fi.fileName(), path);
    if (path == m_fileTree->selectedStartup())
      m_startupCombo->setCurrentIndex(m_startupCombo->count() - 1);
  }

  m_startupCombo->blockSignals(false);
}