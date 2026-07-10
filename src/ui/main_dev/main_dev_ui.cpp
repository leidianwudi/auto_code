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
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>

#include "main_dev_ui_ext.h"
#include "src/tool/ui/code/code_editor.h"
#include "src/tool/ui/code/code_log.h"
#include "src/tool/ui/component/aui_button.h"
#include "src/tool/ui/component/aui_style.h"
#include "src/ui/demo/demo_mgr.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

//  构造
// ════════════════════════════════════════════════════════════

MainDevUi::MainDevUi(QWidget *parent) : QMainWindow(parent) {}

// ──────────────────────────────────────────────────────────────
//  界面布局
// ──────────────────────────────────────────────────────────────

void MainDevUi::setupUI() {
  // ── 无边框窗口 ──
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint);

  // 基础样式
  setStyleSheet(AuiStyle::mainStyleSheet());

  // ════════════════════════════════════════════════════════════
  //  自定义标题栏（单行：菜单 + 窗口标题 + 控制按钮）
  // ════════════════════════════════════════════════════════════

  m_titleBar = new QWidget;
  m_titleBar->setObjectName(QStringLiteral("TitleBar"));
  auto *titleLayout = new QHBoxLayout(m_titleBar);
  titleLayout->setContentsMargins(6, 2, 6, 2);
  titleLayout->setSpacing(4);

  // ── 程序图标（AC 粗体字母） ──
  auto *appIconLabel = new QLabel;
  {
    QPixmap px(20, 20);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(AuiStyle::textColor(), 2.0);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);

    QFont f(QStringLiteral("Consolas"), 11, QFont::Bold);
    p.setFont(f);
    p.drawText(QRectF(0, 0, 20, 20), Qt::AlignCenter, QStringLiteral("AC"));

    p.end();
    appIconLabel->setPixmap(px);
  }
  titleLayout->addWidget(appIconLabel);

  // titleLayout->addSpacing(5);

  // ── 左侧：文件菜单 ──
  auto *fileMenu = new QMenu(m_titleBar);
  m_openAction = fileMenu->addAction(QStringLiteral("打开(&O)..."));
  m_openAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));

  m_openFolderAction = fileMenu->addAction(QStringLiteral("打开文件夹(&F)..."));

  auto *fileBtn = new QToolButton;
  fileBtn->setText(QStringLiteral("文件(&F)"));
  fileBtn->setPopupMode(QToolButton::InstantPopup);
  fileBtn->setMenu(fileMenu);
  titleLayout->addWidget(fileBtn);

  // ── 左侧：视图菜单 ──
  auto *viewMenu = new QMenu(m_titleBar);

  m_splitAction = viewMenu->addAction(QStringLiteral("向右拆分编辑器"));
  m_splitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+\\")));

  m_closeAction = viewMenu->addAction(QStringLiteral("关闭标签页"));
  m_closeAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));

  auto *viewBtn = new QToolButton;
  viewBtn->setText(QStringLiteral("视图(&V)"));
  viewBtn->setPopupMode(QToolButton::InstantPopup);
  viewBtn->setMenu(viewMenu);
  titleLayout->addWidget(viewBtn);

  // ── 右侧：帮助菜单 ──
  auto *helpMenu = new QMenu(m_titleBar);
  m_helpExampleAction = helpMenu->addAction(QStringLiteral("例子(&E)..."));

  auto *helpBtn = new QToolButton;
  helpBtn->setText(QStringLiteral("帮助(&H)"));
  helpBtn->setPopupMode(QToolButton::InstantPopup);
  helpBtn->setMenu(helpMenu);
  titleLayout->addWidget(helpBtn);

  // ── 执行按钮 ──
  m_buildBtn = AuiButton::createBuildButton();
  m_buildBtn->setToolTip(QStringLiteral("执行 (F5)"));
  titleLayout->addWidget(m_buildBtn);

  // ── 启动项下拉框 ──
  m_startupCombo = new QComboBox;
  m_startupCombo->setMinimumWidth(120);
  m_startupCombo->setToolTip(QStringLiteral("选择启动项"));
  m_startupCombo->setStyleSheet(
      QStringLiteral("QComboBox { background: #3c3c3c; color: #d4d4d4; "
                     "border: 1px solid #555; padding: 2px 6px; }"
                     "QComboBox::drop-down { border: none; }"
                     "QComboBox QAbstractItemView { background: #3c3c3c; "
                     "color: #d4d4d4; selection-background-color: #264f78; }"));
  titleLayout->addWidget(m_startupCombo);

  // ── 保存按钮 ──
  m_saveBtn = AuiButton::createSaveButton();
  m_saveBtn->setEnabled(false);
  titleLayout->addWidget(m_saveBtn);

  // ── 保存全部按钮 ──
  m_saveAllBtn = AuiButton::createSaveAllButton();
  m_saveAllBtn->setEnabled(false);
  titleLayout->addWidget(m_saveAllBtn);

  titleLayout->addStretch();

  // ── 右侧：窗口标题 + 控制按钮 ──
  m_titleLabel = new QLabel(windowTitle());
  m_titleLabel->setObjectName(QStringLiteral("TitleLabel"));
  titleLayout->addWidget(m_titleLabel);

  titleLayout->addSpacing(8);

  // ── 向右拆分按钮 ──
  m_splitBtn = AuiButton::createSplitButton();

  m_minBtn = AuiButton::createMinButton();
  m_maxBtn = AuiButton::createMaxButton();
  m_closeBtn = AuiButton::createCloseButton();

  // 初始化为最大化图标
  updateMaximizeIcon();

  titleLayout->addWidget(m_splitBtn);
  titleLayout->addWidget(m_minBtn);
  titleLayout->addWidget(m_maxBtn);
  titleLayout->addWidget(m_closeBtn);

  connect(m_closeBtn, &QPushButton::clicked, this, &QWidget::close);
  connect(m_minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
  connect(m_maxBtn, &QPushButton::clicked, this, &MainDevUi::onMaximizeClicked);
  connect(m_splitBtn, &QPushButton::clicked, m_splitAction, &QAction::trigger);

  // ── 帮助 → 例子：弹出 Demo 窗口 ──
  connect(m_helpExampleAction, &QAction::triggered, this, []() { DemoMgr::ins().open(); });

  // ════════════════════════════════════════════════════════════
  //  左侧文件树
  // ════════════════════════════════════════════════════════════
  m_fileTree = new TreeDir;

  // ── 启动项变化时刷新下拉框 ──
  connect(m_fileTree, &TreeDir::startupItemsChanged, this, &MainDevUi::refreshStartupCombo);

  // ── 下拉框切换时联动文件树的选中启动项 ──
  connect(m_startupCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int /*idx*/) {
            QString path = m_startupCombo->currentData().toString();
            if (!path.isEmpty()) m_fileTree->setSelectedStartup(path);
          });

  // ════════════════════════════════════════════════════════════
  //  右侧编辑器区域
  // ════════════════════════════════════════════════════════════

  m_editorSplitter = new QSplitter(Qt::Horizontal);
  m_editorSplitter->setChildrenCollapsible(false);

  QTabWidget *initialTabs = createEditorPanel();
  m_editorSplitter->addWidget(initialTabs);
  m_editorSplitter->setStretchFactor(0, 1);

  // ════════════════════════════════════════════════════════════
  //  输出面板（编辑器下方，脚本运行结果）
  // ════════════════════════════════════════════════════════════
  m_outputPanel = new CodeLog;
  m_outputPanel->installEventFilter(this);  // 监听按键事件（Backspace/Delete 清空）

  // ════════════════════════════════════════════════════════════
  //  右侧分割器（垂直：编辑器 + 输出面板）
  // ════════════════════════════════════════════════════════════
  m_contentSplitter = new QSplitter(Qt::Vertical);
  m_contentSplitter->addWidget(m_editorSplitter);
  m_contentSplitter->addWidget(m_outputPanel);
  m_contentSplitter->setStretchFactor(0, 1);  // 编辑器可拉伸
  m_contentSplitter->setStretchFactor(1, 0);  // 输出面板固定高度
  m_contentSplitter->setSizes({700, 120});    // 编辑器 700，输出面板 120

  // ════════════════════════════════════════════════════════════
  //  主分割器（水平：文件树 + 右侧区域）
  // ════════════════════════════════════════════════════════════
  m_mainSplitter = new QSplitter(Qt::Horizontal);
  m_mainSplitter->addWidget(m_fileTree);
  m_mainSplitter->addWidget(m_contentSplitter);
  m_mainSplitter->setStretchFactor(0, 0);  // 左侧文件树固定宽度
  m_mainSplitter->setStretchFactor(1, 1);  // 右侧区域可拉伸
  m_mainSplitter->setSizes({250, 1150});

  // ════════════════════════════════════════════════════════════
  //  外层 QFrame（2px 黑色边框）+ Windows DWM 阴影
  // ════════════════════════════════════════════════════════════

  auto *frame = new QFrame;
  frame->setObjectName(QStringLiteral("WindowFrame"));
  auto *frameLayout = new QVBoxLayout(frame);
  frameLayout->setContentsMargins(2, 0, 2, 0);
  frameLayout->setSpacing(0);
  frameLayout->addWidget(m_titleBar);
  frameLayout->addWidget(m_mainSplitter, 1);

  setCentralWidget(frame);

  // ── 通过 Win32 添加 WS_THICKFRAME 以支持拉伸 ──
#if defined(Q_OS_WIN)
  {
    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style |= WS_THICKFRAME;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
  }
#endif

  // ════════════════════════════════════════════════════════════
  //  底部状态栏
  // ════════════════════════════════════════════════════════════
  m_cursorPositionLabel = new QLabel(QStringLiteral("行: 1, 列: 1"));
  m_cursorPositionLabel->setMinimumWidth(120);
  m_cursorPositionLabel->setAlignment(Qt::AlignCenter);
  statusBar()->addPermanentWidget(m_cursorPositionLabel);

  m_errorLabel = new QLabel;
  m_errorLabel->setStyleSheet(QStringLiteral("QLabel { color: #f44747; }"));
  statusBar()->addWidget(m_errorLabel, 1);
}

// ══════════════════════════════════════════════════════════════
//  窗口原生事件（WM_NCHITTEST — 可拉伸边框 + 标题栏拖拽）
// ══════════════════════════════════════════════════════════════

#if defined(Q_OS_WIN)
bool MainDevUi::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
  Q_UNUSED(eventType)
  const auto *msg = static_cast<MSG *>(message);

  // ── 抑制非客户区绘制，消除拉伸时黑色方块 ──
  if (msg->message == WM_NCPAINT) {
    *result = 0;
    return true;
  }
  if (msg->message == WM_ERASEBKGND) {
    *result = 1;
    return true;
  }

  if (msg->message == WM_NCHITTEST) {
    const int x = GET_X_LPARAM(msg->lParam);
    const int y = GET_Y_LPARAM(msg->lParam);
    const QPoint pt = mapFromGlobal(QPoint(x, y));
    const int bw = 5;  // 边框拉伸宽度

    // ── 四角 ──
    bool left = pt.x() < bw;
    bool right = pt.x() > width() - bw;
    bool top = pt.y() < bw;
    bool bottom = pt.y() > height() - bw;

    if (top && left) {
      *result = HTTOPLEFT;
      return true;
    }
    if (top && right) {
      *result = HTTOPRIGHT;
      return true;
    }
    if (bottom && left) {
      *result = HTBOTTOMLEFT;
      return true;
    }
    if (bottom && right) {
      *result = HTBOTTOMRIGHT;
      return true;
    }
    if (top) {
      *result = HTTOP;
      return true;
    }
    if (bottom) {
      *result = HTBOTTOM;
      return true;
    }
    if (left) {
      *result = HTLEFT;
      return true;
    }
    if (right) {
      *result = HTRIGHT;
      return true;
    }

    // ── 标题栏区域（排除按钮）→ HTCAPTION 实现原生拖拽 ──
    if (pt.y() < m_titleBar->height()) {
      QWidget *child = m_titleBar->childAt(m_titleBar->mapFromGlobal(QPoint(x, y)));
      if (!child || child == m_titleBar) {
        *result = HTCAPTION;
        return true;
      }
    }
  }
  return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

// ══════════════════════════════════════════════════════════════
//  窗口状态变化（最大化/还原更新按钮文字）
// ══════════════════════════════════════════════════════════════
void MainDevUi::updateMaximizeIcon() { AuiButton::updateMaximizeIcon(m_maxBtn, isMaximized()); }

void MainDevUi::changeEvent(QEvent *ev) {
  if (ev->type() == QEvent::WindowStateChange) {
    updateMaximizeIcon();
  }
  QMainWindow::changeEvent(ev);
}

void MainDevUi::onMaximizeClicked() {
  if (isMaximized()) {
    showNormal();
  } else {
    showMaximized();
  }
  updateMaximizeIcon();
}

// ──────────────────────────────────────────────────────────────
//  编辑器面板组创建
// ──────────────────────────────────────────────────────────────

/// @brief  创建新的编辑器面板（DimmableTabWidget）
/// @return 新面板指针，已设置 Expanding 策略、去内外边距、最小宽度 60
QTabWidget *MainDevUi::createEditorPanel() {
  auto *tabs = new DimmableTabWidget;
  tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  tabs->setContentsMargins(0, 0, 0, 0);
  tabs->setStyleSheet(QStringLiteral("QTabWidget::pane { margin: 0; border: none; }"));
  return tabs;
}

void MainDevUi::closeEvent(QCloseEvent *event) {
  QApplication::quit();
  QMainWindow::closeEvent(event);
}

// ══════════════════════════════════════════════════════════════
//  编辑器面板组操作
// ══════════════════════════════════════════════════════════════
/// @brief  获取编辑器面板数量（即分割器中的子控件数）
/// @return 当前 m_editorSplitter 中的面板个数
int MainDevUi::editorPanelCount() const { return m_editorSplitter->count(); }

/// @brief  获取指定索引处的编辑器面板
/// @param index 面板索引（0 ~ count-1）
/// @return 对应位置的 QTabWidget 指针，索引越界时返回 nullptr
QTabWidget *MainDevUi::editorPanelAt(int index) const {
  return qobject_cast<QTabWidget *>(m_editorSplitter->widget(index));
}

/// @brief  查找指定面板在分割器中的索引
/// @param tabs 待查找的面板指针
/// @return 面板在 m_editorSplitter 中的位置，未找到时返回 -1
int MainDevUi::editorPanelIndex(const QTabWidget *tabs) const {
  return m_editorSplitter->indexOf(const_cast<QTabWidget *>(tabs));
}

/// @brief  在分割器末尾添加一个新面板
/// @param panel 待添加的 QTabWidget 面板
void MainDevUi::addEditorPanel(QTabWidget *panel) { m_editorSplitter->addWidget(panel); }

/// @brief  移除并销毁指定索引处的面板
/// @param index 待移除的面板索引
/// @note 面板对象通过 deleteLater 延迟销毁，内部标签页会连带释放
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

/// @brief 向输出面板追加文本（委托给 CodeLog）
/// @param text 要显示的文本
/// @param isError 是否为错误信息，错误信息显示红色
void MainDevUi::appendOutput(const QString &text, bool isError) {
  m_outputPanel->append(text, isError);
}

/// @brief 清空输出面板（委托给 CodeLog）
void MainDevUi::clearOutput() { m_outputPanel->clearLog(); }

/// @brief 事件过滤器 — Backspace/Delete 清空日志
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

/// @brief 刷新启动项下拉框 — 从文件树获取所有启动项并填充
void MainDevUi::refreshStartupCombo() {
  if (!m_startupCombo || !m_fileTree) return;

  m_startupCombo->blockSignals(true);  // 避免触发 currentIndexChanged
  QString prev = m_startupCombo->currentData().toString();
  m_startupCombo->clear();

  QStringList files = m_fileTree->startupFiles();
  QFileInfo selInfo(m_fileTree->selectedStartup());

  for (const QString &path : files) {
    QFileInfo fi(path);
    m_startupCombo->addItem(fi.fileName(), path);
    // 恢复上次选中或选中第一个
    if (path == m_fileTree->selectedStartup())
      m_startupCombo->setCurrentIndex(m_startupCombo->count() - 1);
  }

  m_startupCombo->blockSignals(false);
}