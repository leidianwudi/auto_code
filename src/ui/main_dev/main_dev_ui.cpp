/**
 * @file main_dev_ui.cpp
 * @brief 代码编辑器主窗口（视图层）实现
 */

#include "main_dev_ui.h"

#include "src/tool/ui/aui_button.h"
#include "src/tool/ui/aui_style.h"

#include <QApplication>
#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileInfo>
#include <QFileInfoList>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QMouseEvent>
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
//  DraggableTabBar 实现
// ──────────────────────────────────────────────────────────────

DraggableTabBar *DraggableTabBar::s_sourceBar = nullptr;
int DraggableTabBar::s_sourceIndex = -1;

DraggableTabBar::DraggableTabBar(QWidget *parent) : QTabBar(parent) {
  setAcceptDrops(true);
  setMovable(false); // 自行处理跨面板拖拽
}

void DraggableTabBar::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_pressedIndex = tabAt(event->pos());
    m_dragStartPos = event->pos();
  }
  QTabBar::mousePressEvent(event);
}

void DraggableTabBar::mouseMoveEvent(QMouseEvent *event) {
  if (!(event->buttons() & Qt::LeftButton) || m_pressedIndex < 0) {
    QTabBar::mouseMoveEvent(event);
    return;
  }

  if ((event->pos() - m_dragStartPos).manhattanLength() <
      QApplication::startDragDistance()) {
    QTabBar::mouseMoveEvent(event);
    return;
  }

  // ── 开始跨面板拖拽 ──
  s_sourceBar = this;
  s_sourceIndex = m_pressedIndex;

  auto *mime = new QMimeData;
  mime->setData(QStringLiteral("application/x-auto-code-tab"),
                QByteArray::number(reinterpret_cast<quintptr>(this)));

  auto *drag = new QDrag(this);
  drag->setMimeData(mime);

  // 拖拽半透明缩略图
  QPixmap px = grab(tabRect(m_pressedIndex));
  if (!px.isNull()) {
    drag->setPixmap(px);
    drag->setHotSpot(event->pos() - tabRect(m_pressedIndex).topLeft());
  }

  if (drag->exec(Qt::MoveAction) != Qt::MoveAction) {
    s_sourceBar = nullptr;
    s_sourceIndex = -1;
  }
}

void DraggableTabBar::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasFormat(
          QStringLiteral("application/x-auto-code-tab")))
    event->acceptProposedAction();
}

void DraggableTabBar::dragMoveEvent(QDragMoveEvent *event) {
  if (event->mimeData()->hasFormat(
          QStringLiteral("application/x-auto-code-tab")))
    event->acceptProposedAction();
}

void DraggableTabBar::dropEvent(QDropEvent *event) {
  if (!event->mimeData()->hasFormat(
          QStringLiteral("application/x-auto-code-tab")) ||
      !s_sourceBar) {
    QTabBar::dropEvent(event);
    return;
  }

  int toIndex = tabAt(event->position().toPoint());
  if (toIndex < 0)
    toIndex = count(); // 追加到末尾

  emit tabDropped(s_sourceIndex, s_sourceBar, toIndex);
  s_sourceBar = nullptr;
  s_sourceIndex = -1;

  event->acceptProposedAction();
}

// ──────────────────────────────────────────────────────────────
//  DimmableTabWidget 实现
// ──────────────────────────────────────────────────────────────

DimmableTabWidget::DimmableTabWidget(QWidget *parent) : QTabWidget(parent) {
  setAcceptDrops(true); // 内容区也接受拖放

  // 用 DraggableTabBar 替换默认标签栏
  auto *bar = new DraggableTabBar;
  setTabBar(bar);

  // setTabBar 之后设置，确保作用到 DraggableTabBar
  setTabsClosable(true);

  // 强制 Fusion 风格使 setTabTextColor 生效
  AuiStyle::ensureFusionTabBar(bar);

  // ── 跨面板拖拽：标签移动 ──
  connect(bar, &DraggableTabBar::tabDropped, this,
          [this](int fromIndex, DraggableTabBar *fromBar, int toIndex) {
            // 定位源 DimmableTabWidget
            auto *fromWidget =
                qobject_cast<DimmableTabWidget *>(fromBar->parentWidget());
            if (!fromWidget)
              return;

            // 取出标签页内容
            QWidget *page = fromWidget->widget(fromIndex);
            if (!page)
              return;

            QString text = fromWidget->tabText(fromIndex);
            QString tip = fromWidget->tabToolTip(fromIndex);
            QIcon icon = fromWidget->tabIcon(fromIndex);

            // 从源面板移除
            fromWidget->removeTab(fromIndex);

            // ── 源面板没有标签页且还有其它面板 → 自动销毁 ──
            if (fromWidget->count() == 0 && fromWidget != this) {
              auto *splitter =
                  qobject_cast<QSplitter *>(fromWidget->parentWidget());
              if (splitter && splitter->count() > 1)
                fromWidget->deleteLater();
            }

            if (fromWidget == this) {
              // 同一面板内重新排序
              int insertIdx = toIndex;
              if (insertIdx > fromIndex)
                --insertIdx;
              int newIdx = insertTab(insertIdx, page, icon, text);
              setTabToolTip(newIdx, tip);
              setCurrentIndex(newIdx);
            } else {
              // 跨面板移动
              int newIdx = insertTab(toIndex, page, icon, text);
              setTabToolTip(newIdx, tip);
              setCurrentIndex(newIdx);
            }
          });
}

// ── DimmableTabWidget 内容区拖放 ──

void DimmableTabWidget::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasFormat(
          QStringLiteral("application/x-auto-code-tab")))
    event->acceptProposedAction();
  else
    QTabWidget::dragEnterEvent(event);
}

void DimmableTabWidget::dragMoveEvent(QDragMoveEvent *event) {
  if (event->mimeData()->hasFormat(
          QStringLiteral("application/x-auto-code-tab")))
    event->acceptProposedAction();
  else
    QTabWidget::dragMoveEvent(event);
}

void DimmableTabWidget::dropEvent(QDropEvent *event) {
  if (!event->mimeData()->hasFormat(
          QStringLiteral("application/x-auto-code-tab"))) {
    QTabWidget::dropEvent(event);
    return;
  }

  auto *fromBar = DraggableTabBar::dragSourceBar();
  int fromIndex = DraggableTabBar::dragSourceIndex();
  DraggableTabBar::clearDragSource();

  if (!fromBar)
    return;

  auto *fromWidget = qobject_cast<DimmableTabWidget *>(fromBar->parentWidget());
  if (!fromWidget || fromIndex < 0)
    return;

  // ── 内容区域放下 = 追加到末尾 ──
  QWidget *page = fromWidget->widget(fromIndex);
  if (!page)
    return;

  QString text = fromWidget->tabText(fromIndex);
  QString tip = fromWidget->tabToolTip(fromIndex);
  QIcon icon = fromWidget->tabIcon(fromIndex);

  fromWidget->removeTab(fromIndex);

  // ── 源面板没有标签页且还有其它面板 → 自动销毁 ──
  if (fromWidget->count() == 0 && fromWidget != this) {
    auto *splitter = qobject_cast<QSplitter *>(fromWidget->parentWidget());
    if (splitter && splitter->count() > 1)
      fromWidget->deleteLater();
  }

  int newIdx = addTab(page, icon, text);
  setTabToolTip(newIdx, tip);

  // 如果目标面板已有标签页则插入在最后，否则追加
  setCurrentIndex(newIdx);

  event->acceptProposedAction();
}

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

  m_mainSplitter = new QSplitter(Qt::Horizontal); // 主分割器, 水平方向
  m_mainSplitter->addWidget(m_fileTree);
  m_mainSplitter->addWidget(m_editorSplitter);
  m_mainSplitter->setStretchFactor(0, 0); // 左侧文件树固定宽度
  m_mainSplitter->setStretchFactor(1, 1); // 右侧编辑器区域可拉伸
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
  AuiButton::updateMaximizeIcon(m_maxBtn, isMaximized());
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

/// @brief  创建新的编辑器面板（DimmableTabWidget）
/// @return 新面板指针，已设置 Expanding 策略并去除外间距 / pane 内边距
QTabWidget *MainDevUi::createEditorPanel() {
  auto *tabs = new DimmableTabWidget;
  tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  tabs->setContentsMargins(0, 0, 0, 0);
  tabs->setStyleSheet(
      QStringLiteral("QTabWidget::pane { margin: 0; border: none; }"));
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
void MainDevUi::addEditorPanel(QTabWidget *panel) {
  m_editorSplitter->addWidget(panel);
}

/// @brief  移除并销毁指定索引处的面板
/// @param index 待移除的面板索引
/// @note 面板对象通过 deleteLater 延迟销毁，内部标签页会连带释放
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

void MainDevUi::applyTabDimming(QTabWidget *active) {
  for (int i = 0; i < editorPanelCount(); ++i) {
    auto *tabs = editorPanelAt(i);
    if (!tabs)
      continue;

    QTabBar *bar = tabs->tabBar();
    AuiStyle::ensureFusionTabBar(bar);

    bool isActive = (tabs == active);
    for (int j = 0; j < bar->count(); ++j)
      bar->setTabTextColor(j,
                           isActive ? QColor() : AuiStyle::inactiveTabColor());
  }
}