/**
 * @file main_dev_mgr.cpp
 * @brief MainDev 管理器层实现
 */

#include "main_dev_mgr.h"
#include "main_dev.h"
#include "main_dev_model.h"
#include "main_dev_ui.h"
#include "src/ui/tool_ui/code_editor.h"
#include "src/ui/tool_ui/highlighter/json_highlighter.h"
#include "src/ui/tool_ui/highlighter/template_highlighter.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMainWindow>
#include <QMessageBox>
#include <QPalette>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextStream>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// ──────────────────────────────────────────────────────────────
//  单例
// ──────────────────────────────────────────────────────────────

MainDevMgr *MainDevMgr::s_instance = nullptr;

MainDevMgr *MainDevMgr::instance() { return s_instance; }

void MainDevMgr::init(MainDevUi *ui, MainDevModel *model, QMainWindow *window) {
  if (!s_instance) {
    s_instance = new MainDevMgr;
  }
  s_instance->m_ui = ui;
  s_instance->m_model = model;
  s_instance->m_window = window;

  // ── 连接 UI 信号到管理器 ──

  // 菜单栏动作
  QObject::connect(ui->splitAction(), &QAction::triggered, s_instance,
                   &MainDevMgr::onSplitRight);
  QObject::connect(ui->closeAction(), &QAction::triggered, s_instance,
                   &MainDevMgr::onCloseEditor);

  // 文件树
  QObject::connect(ui->fileTree(), &QTreeWidget::itemClicked, s_instance,
                   &MainDevMgr::onTreeItemClicked);

  // 全局焦点变化
  QObject::connect(qApp, &QApplication::focusChanged, s_instance,
                   &MainDevMgr::onFocusChanged);

  // 连接初始编辑器面板组的信号（setupUI 中创建的）
  for (int i = 0; i < ui->editorSplitter()->count(); ++i) {
    auto *tabs = qobject_cast<QTabWidget *>(ui->editorSplitter()->widget(i));
    if (tabs) {
      QObject::connect(tabs, &QTabWidget::tabCloseRequested, s_instance,
                       &MainDevMgr::onTabCloseRequested);
      QObject::connect(tabs, &QTabWidget::currentChanged, s_instance,
                       &MainDevMgr::onCurrentTabChanged);
    }
  }
}

// ──────────────────────────────────────────────────────────────
//  静态方法：供其他模块调用
// ──────────────────────────────────────────────────────────────

void MainDevMgr::openFile(const QString &filePath) {
  if (s_instance) {
    s_instance->openFileInEditor(filePath);
  }
}

void MainDevMgr::splitRight() {
  if (s_instance) {
    s_instance->onSplitRight();
  }
}

void MainDevMgr::closeCurrentEditor() {
  if (s_instance) {
    s_instance->onCloseEditor();
  }
}

// ──────────────────────────────────────────────────────────────
//  文件扫描与树构建
// ──────────────────────────────────────────────────────────────

void MainDevMgr::loadFiles() {
  QDir baseDir;

  baseDir.setPath(QStringLiteral(PROJECT_SOURCE_DIR) + QStringLiteral("/file"));

  if (!baseDir.exists()) {
    baseDir.setPath(QCoreApplication::applicationDirPath() +
                    QStringLiteral("/file"));
  }
  if (!baseDir.exists()) {
    baseDir.setPath(QCoreApplication::applicationDirPath() +
                    QStringLiteral("/../../file"));
  }
  if (!baseDir.exists()) {
    baseDir.setPath(QDir::currentPath() + QStringLiteral("/file"));
  }

  if (!baseDir.exists()) {
    qWarning("未找到 file/ 目录");
    return;
  }

  addDirectoryToTree(nullptr, baseDir.absolutePath());
  m_ui->fileTree()->expandAll();
}

void MainDevMgr::addDirectoryToTree(QTreeWidgetItem *parentItem,
                                    const QString &dirPath) {
  QDir dir(dirPath);

  QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QFileInfo &info : dirs) {
    QTreeWidgetItem *dirItem;
    if (parentItem) {
      dirItem = new QTreeWidgetItem(parentItem);
    } else {
      dirItem = new QTreeWidgetItem(m_ui->fileTree());
    }
    dirItem->setText(0, info.fileName());
    dirItem->setIcon(
        0, m_ui->fileTree()->style()->standardIcon(QStyle::SP_DirIcon));
    dirItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    addDirectoryToTree(dirItem, info.absoluteFilePath());
  }

  QStringList nameFilters;
  nameFilters << QStringLiteral("*.ac") << QStringLiteral("*.json");
  QFileInfoList files = dir.entryInfoList(nameFilters, QDir::Files);
  for (const QFileInfo &info : files) {
    QTreeWidgetItem *fileItem;
    if (parentItem) {
      fileItem = new QTreeWidgetItem(parentItem);
    } else {
      fileItem = new QTreeWidgetItem(m_ui->fileTree());
    }
    fileItem->setText(0, info.fileName());
    fileItem->setIcon(
        0, m_ui->fileTree()->style()->standardIcon(QStyle::SP_FileIcon));
    fileItem->setData(0, Qt::UserRole + 1, info.absoluteFilePath());
  }
}

// ──────────────────────────────────────────────────────────────
//  树节点点击 → 打开文件
// ──────────────────────────────────────────────────────────────

void MainDevMgr::onTreeItemClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  if (!item)
    return;

  QString filePath = item->data(0, Qt::UserRole + 1).toString();
  if (filePath.isEmpty())
    return;

  openFileInEditor(filePath);
}

// ──────────────────────────────────────────────────────────────
//  创建 / 打开编辑器
// ──────────────────────────────────────────────────────────────

CodeEditor *MainDevMgr::createEditorForFile(const QString &filePath) {
  auto *editor = new CodeEditor;

  if (filePath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
    new JsonHighlighter(editor->document());
  } else {
    new TemplateHighlighter(editor->document());
  }

  return editor;
}

CodeEditor *MainDevMgr::openFileInEditor(const QString &filePath) {
  // ── 检查文件是否已打开：遍历所有面板组查找对应标签页 ──
  for (int i = 0; i < m_ui->editorSplitter()->count(); ++i) {
    auto *tabs = qobject_cast<QTabWidget *>(m_ui->editorSplitter()->widget(i));
    if (!tabs)
      continue;
    for (int j = 0; j < tabs->count(); ++j) {
      auto *editor = qobject_cast<CodeEditor *>(tabs->widget(j));
      if (editor && editor->objectName() == filePath) {
        tabs->setCurrentIndex(j);
        tabs->setFocus();
        editor->setFocus();
        return editor;
      }
    }
  }

  // ── 读取文件内容 ──
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::warning(m_window, QStringLiteral("打开失败"),
                         QStringLiteral("无法打开文件: %1").arg(filePath));
    return nullptr;
  }
  QTextStream in(&file);
  QString content = in.readAll();
  file.close();

  // ── 创建编辑器 ──
  CodeEditor *editor = createEditorForFile(filePath);
  editor->setPlainText(content);

  if (filePath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
    editor->setValidationMode(CodeEditor::JsonValidation);
  } else {
    editor->setValidationMode(CodeEditor::TemplateValidation);
  }

  // ── 获取当前面板组，添加到新标签页 ──
  QTabWidget *tabs = currentTabWidget();
  if (!tabs) {
    tabs = m_ui->createEditorPanel();
    m_ui->editorSplitter()->addWidget(tabs);
  }

  QFileInfo fi(filePath);
  int idx = tabs->addTab(editor, fi.fileName());
  tabs->setTabToolTip(idx, filePath);
  tabs->setCurrentIndex(idx);

  // ── 首次加载文件时强制刷新布局 ──
  if (m_model->openFiles.isEmpty()) {
    m_ui->mainSplitter()->setSizes(
        {m_ui->fileTree()->width(),
         m_ui->mainSplitter()->width() - m_ui->fileTree()->width() - 6});
  }

  // ── 记录状态 ──
  m_model->registerFile(filePath, content, editor);
  editor->setObjectName(filePath);
  editor->setFocus();
  m_window->setWindowTitle(QStringLiteral("Auto Code - %1").arg(fi.fileName()));

  return editor;
}

CodeEditor *MainDevMgr::currentEditor() const {
  for (int i = 0; i < m_ui->editorSplitter()->count(); ++i) {
    auto *tabs = qobject_cast<QTabWidget *>(m_ui->editorSplitter()->widget(i));
    if (tabs && tabs->isVisible()) {
      auto *editor = qobject_cast<CodeEditor *>(tabs->currentWidget());
      if (editor)
        return editor;
    }
  }
  return nullptr;
}

QTabWidget *MainDevMgr::currentTabWidget() const {
  if (m_model->lastActivePanel && m_model->lastActivePanel->isVisible() &&
      m_model->lastActivePanel->count() > 0) {
    return m_model->lastActivePanel;
  }

  QWidget *focus = QApplication::focusWidget();
  while (focus) {
    if (auto *tabs = qobject_cast<QTabWidget *>(focus))
      return tabs;
    focus = focus->parentWidget();
  }

  QTabWidget *emptyPanel = nullptr;
  for (int i = 0; i < m_ui->editorSplitter()->count(); ++i) {
    auto *tabs = qobject_cast<QTabWidget *>(m_ui->editorSplitter()->widget(i));
    if (tabs && tabs->isVisible()) {
      if (tabs->count() > 0)
        return tabs;
      if (!emptyPanel)
        emptyPanel = tabs;
    }
  }
  return emptyPanel;
}

// ──────────────────────────────────────────────────────────────
//  信号连接
// ──────────────────────────────────────────────────────────────

void MainDevMgr::connectEditor(CodeEditor *editor) {
  if (m_model->connectedEditor) {
    disconnect(m_model->connectedEditor, &QPlainTextEdit::cursorPositionChanged,
               this, &MainDevMgr::updateCursorPosition);
  }

  m_model->connectedEditor = editor;

  if (editor) {
    connect(editor, &QPlainTextEdit::cursorPositionChanged, this,
            &MainDevMgr::updateCursorPosition);
    updateCursorPosition();
  } else {
    m_ui->cursorPositionLabel()->setText(QStringLiteral("行: -, 列: -"));
  }
}

void MainDevMgr::onFocusChanged(QWidget * /*oldFocus*/, QWidget *newFocus) {
  if (!newFocus)
    return;

  QWidget *w = newFocus;
  CodeEditor *foundEditor = nullptr;

  while (w) {
    if (auto *tabs = qobject_cast<QTabWidget *>(w)) {
      m_model->lastActivePanel = tabs;
    }
    if (!foundEditor) {
      if (auto *editor = qobject_cast<CodeEditor *>(w)) {
        foundEditor = editor;
      }
    }
    w = w->parentWidget();
  }

  if (foundEditor) {
    connectEditor(foundEditor);
  }

  QTabWidget *activeTabs = nullptr;
  for (int i = 0; i < m_ui->editorSplitter()->count(); ++i) {
    auto *tabs = qobject_cast<QTabWidget *>(m_ui->editorSplitter()->widget(i));
    if (tabs && tabs->isAncestorOf(newFocus)) {
      activeTabs = tabs;
      break;
    }
  }

  applyTabDimming(activeTabs);
}

void MainDevMgr::updateCursorPosition() {
  if (!m_model->connectedEditor || !m_model->connectedEditor->isVisible()) {
    m_ui->cursorPositionLabel()->setText(QStringLiteral("行: -, 列: -"));
    return;
  }

  QTextCursor cursor = m_model->connectedEditor->textCursor();
  int line = cursor.blockNumber() + 1;
  int col = cursor.columnNumber() + 1;
  m_ui->cursorPositionLabel()->setText(
      QStringLiteral("行: %1, 列: %2").arg(line).arg(col));
}

void MainDevMgr::applyTabDimming(QTabWidget *active) {
  for (int i = 0; i < m_ui->editorSplitter()->count(); ++i) {
    auto *tabs = qobject_cast<QTabWidget *>(m_ui->editorSplitter()->widget(i));
    if (!tabs)
      continue;
    bool isActive = (tabs == active);
    QTabBar *bar = tabs->tabBar();
    if (isActive) {
      for (int j = 0; j < bar->count(); ++j)
        bar->setTabTextColor(j, QColor());
    } else {
      for (int j = 0; j < bar->count(); ++j)
        bar->setTabTextColor(j, QColor(0x88, 0x88, 0x88));
    }
  }
}

void MainDevMgr::setWindowTitles(const QString &text) {
  m_window->setWindowTitle(text);
  auto *mainDev = qobject_cast<MainDev *>(m_window);
  if (mainDev) {
    mainDev->setTitleText(text);
  }
}

// ──────────────────────────────────────────────────────────────
//  标签页操作
// ──────────────────────────────────────────────────────────────

void MainDevMgr::onTabCloseRequested(int index) {
  auto *tabs = qobject_cast<QTabWidget *>(sender());
  if (!tabs)
    return;

  auto *editor = qobject_cast<CodeEditor *>(tabs->widget(index));
  if (!editor)
    return;

  QString filePath = editor->objectName();
  if (!filePath.isEmpty()) {
    bool stillOpen = false;
    for (int i = 0; i < m_ui->editorSplitter()->count(); ++i) {
      auto *otherTabs =
          qobject_cast<QTabWidget *>(m_ui->editorSplitter()->widget(i));
      if (!otherTabs || otherTabs == tabs)
        continue;
      for (int j = 0; j < otherTabs->count(); ++j) {
        auto *otherEditor = qobject_cast<CodeEditor *>(otherTabs->widget(j));
        if (otherEditor && otherEditor->objectName() == filePath) {
          stillOpen = true;
          break;
        }
      }
      if (stillOpen)
        break;
    }
    if (!stillOpen) {
      m_model->unregisterFile(filePath);
    }
  }

  tabs->removeTab(index);
  editor->deleteLater();

  if (tabs->count() == 0 && m_ui->editorSplitter()->count() > 1) {
    int idx = m_ui->editorSplitter()->indexOf(tabs);
    if (idx >= 0) {
      m_ui->editorSplitter()->widget(idx)->deleteLater();
    }
    int count = m_ui->editorSplitter()->count();
    for (int i = 0; i < count; ++i) {
      m_ui->editorSplitter()->setStretchFactor(i, 1);
    }
    if (m_model->lastActivePanel == tabs)
      m_model->lastActivePanel = nullptr;
    m_window->setWindowTitle(QStringLiteral("Auto Code - 开发模式"));
    connectEditor(currentEditor());
    return;
  }

  if (tabs->count() == 0) {
    m_window->setWindowTitle(QStringLiteral("Auto Code - 开发模式"));
  } else {
    int newIdx = tabs->currentIndex();
    if (newIdx >= 0) {
      QString fullPath = tabs->tabToolTip(newIdx);
      if (!fullPath.isEmpty()) {
        QFileInfo fi(fullPath);
        m_window->setWindowTitle(
            QStringLiteral("Auto Code - %1").arg(fi.fileName()));
      }
    }
  }

  connectEditor(currentEditor());
}

void MainDevMgr::onCurrentTabChanged(int index) {
  auto *tabs = qobject_cast<QTabWidget *>(sender());
  if (!tabs)
    return;

  if (index < 0 || index >= tabs->count()) {
    m_window->setWindowTitle(QStringLiteral("Auto Code - 开发模式"));
    return;
  }

  QString fullPath = tabs->tabToolTip(index);
  if (!fullPath.isEmpty()) {
    QFileInfo fi(fullPath);
    m_window->setWindowTitle(
        QStringLiteral("Auto Code - %1").arg(fi.fileName()));
  }

  connectEditor(currentEditor());
  m_model->lastActivePanel = tabs;
  applyTabDimming(tabs);
}

// ──────────────────────────────────────────────────────────────
//  拆分 / 关闭 标签页
// ──────────────────────────────────────────────────────────────

void MainDevMgr::onSplitRight() {
  QTabWidget *newPanel = m_ui->createEditorPanel();

  QObject::connect(newPanel, &QTabWidget::tabCloseRequested, this,
                   &MainDevMgr::onTabCloseRequested);
  QObject::connect(newPanel, &QTabWidget::currentChanged, this,
                   &MainDevMgr::onCurrentTabChanged);

  CodeEditor *current = currentEditor();
  if (current) {
    auto *editor = new CodeEditor;
    editor->setPlainText(current->toPlainText());

    QString filePath = current->objectName();
    if (filePath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
      new JsonHighlighter(editor->document());
      editor->setValidationMode(CodeEditor::JsonValidation);
    } else {
      new TemplateHighlighter(editor->document());
      editor->setValidationMode(CodeEditor::TemplateValidation);
    }

    QFileInfo fi(filePath);
    QString tabLabel =
        filePath.isEmpty() ? QStringLiteral("拆分副本") : fi.fileName();
    int idx = newPanel->addTab(editor, tabLabel);
    newPanel->setTabToolTip(idx, filePath);
    newPanel->setCurrentIndex(idx);

    if (!filePath.isEmpty()) {
      editor->setObjectName(filePath);
      m_model->registerFile(filePath, current->toPlainText(), editor);
    }
  } else {
    auto *editor = new CodeEditor;
    int idx = newPanel->addTab(editor, QStringLiteral("新编辑器"));
    newPanel->setCurrentIndex(idx);
  }

  m_ui->editorSplitter()->addWidget(newPanel);

  int count = m_ui->editorSplitter()->count();
  for (int i = 0; i < count; ++i) {
    m_ui->editorSplitter()->setStretchFactor(i, 1);
  }

  newPanel->setFocus();
}

void MainDevMgr::onCloseEditor() {
  QTabWidget *tabs = currentTabWidget();
  if (!tabs)
    return;

  int idx = tabs->currentIndex();
  if (idx >= 0) {
    onTabCloseRequested(idx);
  }
}