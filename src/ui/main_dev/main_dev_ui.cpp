/**
 * @file main_dev_ui.cpp
 * @brief MainDev 视图层实现
 */

#include "main_dev_ui.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTreeWidgetItem>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// ──────────────────────────────────────────────────────────────
//  构造
// ──────────────────────────────────────────────────────────────

MainDevUi::MainDevUi(QObject *parent) : QObject(parent) {}

// ──────────────────────────────────────────────────────────────
//  界面布局
// ──────────────────────────────────────────────────────────────

void MainDevUi::setupUI(QMainWindow *window) {
  // ── 菜单栏 ──
  QMenu *viewMenu = window->menuBar()->addMenu(QStringLiteral("视图(&V)"));

  m_splitAction = viewMenu->addAction(QStringLiteral("向右拆分编辑器"));
  m_splitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+\\")));

  m_closeAction = viewMenu->addAction(QStringLiteral("关闭标签页"));
  m_closeAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));

  // ── 左侧文件树 ──
  m_fileTree = new QTreeWidget(window);
  m_fileTree->setHeaderLabel(QStringLiteral("文件"));
  m_fileTree->setMinimumWidth(200);
  m_fileTree->setMaximumWidth(400);
  m_fileTree->setAnimated(true);
  m_fileTree->setIndentation(16);
  m_fileTree->setSortingEnabled(false);

  // ── 右侧编辑器区域 ──
  m_editorSplitter = new QSplitter(Qt::Horizontal, window);

  QTabWidget *initialTabs = createEditorPanel();
  m_editorSplitter->addWidget(initialTabs);
  m_editorSplitter->setStretchFactor(0, 1);

  // ── 主分割器 ──
  m_mainSplitter = new QSplitter(Qt::Horizontal, window);
  m_mainSplitter->addWidget(m_fileTree);
  m_mainSplitter->addWidget(m_editorSplitter);
  m_mainSplitter->setStretchFactor(0, 0);
  m_mainSplitter->setStretchFactor(1, 1);
  m_mainSplitter->setSizes({250, 1150});

  window->setCentralWidget(m_mainSplitter);

  // ── 底部状态栏 ──
  m_cursorPositionLabel = new QLabel(QStringLiteral("行: 1, 列: 1"));
  m_cursorPositionLabel->setMinimumWidth(120);
  m_cursorPositionLabel->setAlignment(Qt::AlignCenter);
  window->statusBar()->addPermanentWidget(m_cursorPositionLabel);
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
  return m_editorSplitter->indexOf(
      const_cast<QTabWidget *>(tabs)); // indexOf 参数非 const
}

void MainDevUi::addEditorPanel(QTabWidget *panel) {
  m_editorSplitter->addWidget(panel);
}

void MainDevUi::removeEditorPanelAt(int index) {
  QWidget *w = m_editorSplitter->widget(index);
  if (w) {
    m_editorSplitter->widget(index)->deleteLater();
  }
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

/// 确保 QTabBar 使用 Fusion 风格（Windows 原生风格忽略 setTabTextColor）
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