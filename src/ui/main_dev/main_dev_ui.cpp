/**
 * @file main_dev_ui.cpp
 * @brief 代码编辑器主窗口（视图层）实现
 */

#include "main_dev_ui.h"

#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFileInfoList>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPixmap>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QToolButton>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

// ──────────────────────────────────────────────────────────────
//  构造
// ──────────────────────────────────────────────────────────────

MainDevUi::MainDevUi(QWidget *parent) : QMainWindow(parent) {}

// ──────────────────────────────────────────────────────────────
//  界面布局
// ──────────────────────────────────────────────────────────────

void MainDevUi::setupUI() {
  // ── 无边框窗口 ──
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint);

  // 基础样式
  setStyleSheet(QStringLiteral(
      "#TitleBar { background: #e0e0e0; }"
      "#TitleLabel { color: #333; font-size: 12px; }"
      "QToolButton { color: #333; border: none; padding: 2px 6px; }"
      "QToolButton:hover { background: #d0d0d0; }"
      "QPushButton { color: #333; border: none; }"
      "QPushButton:hover { background: #d0d0d0; }"
      "QStatusBar { background: #e0e0e0; color: #333; }"
      "QStatusBar::item { border: none; }"
      "#WindowFrame { background: #e0e0e0; }"));

  // ════════════════════════════════════════════════════════════
  //  自定义标题栏（单行：菜单 + 窗口标题 + 控制按钮）
  // ════════════════════════════════════════════════════════════

  m_titleBar = new QWidget;
  m_titleBar->setObjectName(QStringLiteral("TitleBar"));
  auto *titleLayout = new QHBoxLayout(m_titleBar);
  titleLayout->setContentsMargins(4, 2, 8, 2);
  titleLayout->setSpacing(4);

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

  titleLayout->addStretch();

  // ── 右侧：窗口标题 + 控制按钮 ──
  m_titleLabel = new QLabel(windowTitle());
  m_titleLabel->setObjectName(QStringLiteral("TitleLabel"));
  titleLayout->addWidget(m_titleLabel);

  titleLayout->addSpacing(8);

  // ── 向右拆分按钮 ──
  m_splitBtn = new QPushButton;
  {
    QPixmap px(20, 20);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor("#333"), 1.5));
    p.drawRect(2, 3, 16, 14);                    // 外框
    p.drawLine(QPointF(10, 3), QPointF(10, 17)); // 中分线
    p.end();
    m_splitBtn->setIcon(QIcon(px));
    m_splitBtn->setIconSize(QSize(20, 20));
  }
  m_splitBtn->setToolTip(QStringLiteral("向右拆分编辑器 (Ctrl+\\)"));

  m_minBtn = new QPushButton(QStringLiteral("—"));
  m_maxBtn = new QPushButton;
  m_closeBtn = new QPushButton(QStringLiteral("✕"));
  for (auto *btn : {m_splitBtn, m_minBtn, m_maxBtn, m_closeBtn}) {
    btn->setFixedSize(36, 26);
    btn->setFlat(true);
    btn->setFocusPolicy(Qt::NoFocus);
  }

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

  // ════════════════════════════════════════════════════════════
  //  左侧文件树
  // ════════════════════════════════════════════════════════════

  m_fileTree = new QTreeWidget;
  m_fileTree->setHeaderLabel(QStringLiteral("文件"));
  m_fileTree->setMinimumWidth(200);
  m_fileTree->setMaximumWidth(400);
  m_fileTree->setAnimated(true);
  m_fileTree->setIndentation(16);
  m_fileTree->setSortingEnabled(false);

  // ════════════════════════════════════════════════════════════
  //  右侧编辑器区域
  // ════════════════════════════════════════════════════════════

  m_editorSplitter = new QSplitter(Qt::Horizontal);

  QTabWidget *initialTabs = createEditorPanel();
  m_editorSplitter->addWidget(initialTabs);
  m_editorSplitter->setStretchFactor(0, 1);

  // ════════════════════════════════════════════════════════════
  //  主分割器
  // ════════════════════════════════════════════════════════════

  m_mainSplitter = new QSplitter(Qt::Horizontal);
  m_mainSplitter->addWidget(m_fileTree);
  m_mainSplitter->addWidget(m_editorSplitter);
  m_mainSplitter->setStretchFactor(0, 0);
  m_mainSplitter->setStretchFactor(1, 1);
  m_mainSplitter->setSizes({250, 1150});

  // ════════════════════════════════════════════════════════════
  //  外层 QFrame（2px 黑色边框）+ Windows DWM 阴影
  // ════════════════════════════════════════════════════════════

  auto *frame = new QFrame;
  frame->setObjectName(QStringLiteral("WindowFrame"));
  auto *frameLayout = new QVBoxLayout(frame);
  frameLayout->setContentsMargins(0, 0, 0, 0);
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
}

// ══════════════════════════════════════════════════════════════
//  窗口原生事件（WM_NCHITTEST — 可拉伸边框 + 标题栏拖拽）
// ══════════════════════════════════════════════════════════════

#if defined(Q_OS_WIN)
bool MainDevUi::nativeEvent(const QByteArray &eventType, void *message,
                            qintptr *result) {
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
    const int bw = 5; // 边框拉伸宽度

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
      QWidget *child =
          m_titleBar->childAt(m_titleBar->mapFromGlobal(QPoint(x, y)));
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

void MainDevUi::updateMaximizeIcon() {
  QPixmap px(14, 14);
  px.fill(Qt::transparent);
  QPainter p(&px);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(QColor("#333"), 1.2));

  if (isMaximized()) {
    p.drawRect(1, 4, 9, 7);
    p.drawRect(5, 1, 9, 7);
  } else {
    p.drawRect(1, 1, 12, 12);
  }
  p.end();
  m_maxBtn->setIcon(QIcon(px));
  m_maxBtn->setIconSize(QSize(14, 14));
}

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

QTabWidget *MainDevUi::createEditorPanel() {
  auto *tabs = new DimmableTabWidget;
  tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  return tabs;
}

// ══════════════════════════════════════════════════════════════
//  文件树操作
// ══════════════════════════════════════════════════════════════

void MainDevUi::expandFileTree() { m_fileTree->expandAll(); }

void MainDevUi::buildFileTree(const QString &dirPath) {
  addDirectoryToTree(nullptr, dirPath);
  expandFileTree();
}

void MainDevUi::addDirectoryToTree(QTreeWidgetItem *parentItem,
                                   const QString &dirPath) {
  QDir dir(dirPath);

  QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QFileInfo &info : dirs) {
    auto *dirItem = parentItem ? new QTreeWidgetItem(parentItem)
                               : new QTreeWidgetItem(m_fileTree);
    dirItem->setText(0, info.fileName());
    dirItem->setIcon(0, m_fileTree->style()->standardIcon(QStyle::SP_DirIcon));
    dirItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    addDirectoryToTree(dirItem, info.absoluteFilePath());
  }

  QStringList nameFilters;
  nameFilters << QStringLiteral("*.ac") << QStringLiteral("*.json");
  QFileInfoList files = dir.entryInfoList(nameFilters, QDir::Files);
  for (const QFileInfo &info : files) {
    auto *fileItem = parentItem ? new QTreeWidgetItem(parentItem)
                                : new QTreeWidgetItem(m_fileTree);
    fileItem->setText(0, info.fileName());
    fileItem->setIcon(0,
                      m_fileTree->style()->standardIcon(QStyle::SP_FileIcon));
    fileItem->setData(0, Qt::UserRole + 1, info.absoluteFilePath());
  }
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

void MainDevUi::addEditorPanel(QTabWidget *panel) {
  m_editorSplitter->addWidget(panel);
}

void MainDevUi::removeEditorPanelAt(int index) {
  QWidget *w = m_editorSplitter->widget(index);
  if (w)
    w->deleteLater();
}

void MainDevUi::setEditorPanelsUniformStretch() {
  int count = m_editorSplitter->count();
  for (int i = 0; i < count; ++i)
    m_editorSplitter->setStretchFactor(i, 1);
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

void MainDevUi::setCursorStatusText(const QString &text) {
  m_cursorPositionLabel->setText(text);
}

// ══════════════════════════════════════════════════════════════
//  标签页颜色
// ══════════════════════════════════════════════════════════════

static void ensureFusionTabBar(QTabBar *bar) {
#ifdef Q_OS_WIN
  if (bar->style() &&
      QString::fromLatin1(bar->style()->metaObject()->className())
          .contains(QStringLiteral("Fusion")))
    return;
  QStyle *fs = QStyleFactory::create(QStringLiteral("Fusion"));
  if (fs) {
    fs->setParent(bar);
    bar->setStyle(fs);
  }
#else
  Q_UNUSED(bar);
#endif
}

void MainDevUi::applyTabDimming(QTabWidget *active) {
  for (int i = 0; i < editorPanelCount(); ++i) {
    auto *tabs = editorPanelAt(i);
    if (!tabs)
      continue;

    QTabBar *bar = tabs->tabBar();
    ensureFusionTabBar(bar);

    bool isActive = (tabs == active);
    for (int j = 0; j < bar->count(); ++j)
      bar->setTabTextColor(j, isActive ? QColor() : QColor(0x88, 0x88, 0x88));
  }
}