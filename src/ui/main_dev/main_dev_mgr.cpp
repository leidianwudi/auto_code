/**
 * @file main_dev_mgr.cpp
 * @brief 代码编辑器控制器层实现（单例 UI 控制器）
 */

#include "main_dev_mgr.h"
#include "main_dev_model.h"
#include "main_dev_ui.h"
#include "src/tool/ui/code_editor.h"
#include "src/tool/ui/highlighter/json_highlighter.h"
#include "src/tool/ui/highlighter/template_highlighter.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextStream>

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
  m_ui->setWindowTitle(QStringLiteral("Auto Code"));

  // ── 连接信号 ──
  initUi();

  // ── 加载文件树 ──
  loadFiles();

  return m_ui;
}

// ──────────────────────────────────────────────────────────────
//  initUi — 连接所有信号槽
// ──────────────────────────────────────────────────────────────

void MainDevMgr::initUi() {
  connect(m_ui->openAction(), &QAction::triggered, this, [this]() {
    QString filePath = QFileDialog::getOpenFileName(
        m_ui, QStringLiteral("打开文件"), QStringLiteral(PROJECT_SOURCE_DIR));
    if (!filePath.isEmpty())
      openFileInEditor(filePath);
  });

  connect(m_ui->openFolderAction(), &QAction::triggered, this, [this]() {
    QString dir = QFileDialog::getExistingDirectory(
        m_ui, QStringLiteral("选择文件夹"), QStringLiteral(PROJECT_SOURCE_DIR));
    if (!dir.isEmpty())
      m_ui->fileTree()->buildTree(dir);
  });

  connect(m_ui->splitAction(), &QAction::triggered, this,
          &MainDevMgr::onSplitRight);
  connect(m_ui->closeAction(), &QAction::triggered, this,
          &MainDevMgr::onCloseEditor);
  connect(m_ui->fileTree(), &TreeDir::fileActivated, this,
          &MainDevMgr::openFileInEditor);
  connect(qApp, &QApplication::focusChanged, this, &MainDevMgr::onFocusChanged);

  // 连接初始编辑器面板组的信号
  for (int i = 0; i < m_ui->editorPanelCount(); ++i) {
    auto *tabs = m_ui->editorPanelAt(i);
    if (tabs) {
      connect(tabs, &QTabWidget::tabCloseRequested, this,
              &MainDevMgr::onTabCloseRequested);
      connect(tabs, &QTabWidget::currentChanged, this,
              &MainDevMgr::onCurrentTabChanged);
    }
  }
}

// ──────────────────────────────────────────────────────────────
//  文件扫描与树构建
// ──────────────────────────────────────────────────────────────

void MainDevMgr::loadFiles() {
  QDir baseDir;

  baseDir.setPath(QStringLiteral(PROJECT_SOURCE_DIR) + QStringLiteral("/file"));
  if (!baseDir.exists())
    baseDir.setPath(QCoreApplication::applicationDirPath() +
                    QStringLiteral("/file"));
  if (!baseDir.exists())
    baseDir.setPath(QCoreApplication::applicationDirPath() +
                    QStringLiteral("/../../file"));
  if (!baseDir.exists())
    baseDir.setPath(QDir::currentPath() + QStringLiteral("/file"));

  if (!baseDir.exists()) {
    qWarning("未找到 file/ 目录");
    return;
  }

  m_ui->fileTree()->buildTree(baseDir.absolutePath());
}

// ──────────────────────────────────────────────────────────────
//  创建 / 打开编辑器
// ──────────────────────────────────────────────────────────────

CodeEditor *MainDevMgr::createEditorForFile(const QString &filePath) {
  auto *editor = new CodeEditor;

  if (filePath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
    new JsonHighlighter(editor->document());
  else
    new TemplateHighlighter(editor->document());

  return editor;
}

CodeEditor *MainDevMgr::openFileInEditor(const QString &filePath) {
  // ── 查重：遍历所有面板组的所有标签页 ──
  for (int i = 0; i < m_ui->editorPanelCount(); ++i) {
    auto *tabs = m_ui->editorPanelAt(i);
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

  // ── 读取文件 ──
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::warning(m_ui, QStringLiteral("打开失败"),
                         QStringLiteral("无法打开文件: %1").arg(filePath));
    return nullptr;
  }
  QTextStream in(&file);
  QString content = in.readAll();
  file.close();

  // ── 创建编辑器 ──
  CodeEditor *editor = createEditorForFile(filePath);
  editor->setPlainText(content);

  if (filePath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
    editor->setValidationMode(CodeEditor::JsonValidation);
  else
    editor->setValidationMode(CodeEditor::TemplateValidation);

  // ── 获取 / 创建面板组 ──
  QTabWidget *tabs = currentTabWidget();
  if (!tabs) {
    tabs = m_ui->createEditorPanel();
    m_ui->addEditorPanel(tabs);
  }

  QFileInfo fi(filePath);
  int idx = tabs->addTab(editor, fi.fileName());
  tabs->setTabToolTip(idx, filePath);
  tabs->setCurrentIndex(idx);

  // 首次加载文件时刷新主分割器布局
  if (m_model->openFiles.isEmpty())
    m_ui->adjustMainSplitter();

  // ── 记录状态 ──
  m_model->registerFile(filePath, content, editor);
  editor->setObjectName(filePath);
  editor->setFocus();
  m_ui->setWindowTitle(QStringLiteral("Auto Code - %1").arg(fi.fileName()));

  return editor;
}

CodeEditor *MainDevMgr::currentEditor() const {
  for (int i = 0; i < m_ui->editorPanelCount(); ++i) {
    auto *tabs = m_ui->editorPanelAt(i);
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
  for (int i = 0; i < m_ui->editorPanelCount(); ++i) {
    auto *tabs = m_ui->editorPanelAt(i);
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
    m_ui->setCursorStatusText(QStringLiteral("行: -, 列: -"));
  }
}

void MainDevMgr::onFocusChanged(QWidget * /*oldFocus*/, QWidget *newFocus) {
  if (!newFocus)
    return;

  QWidget *w = newFocus;
  CodeEditor *foundEditor = nullptr;

  while (w) {
    if (auto *tabs = qobject_cast<QTabWidget *>(w))
      m_model->lastActivePanel = tabs;
    if (!foundEditor) {
      if (auto *editor = qobject_cast<CodeEditor *>(w))
        foundEditor = editor;
    }
    w = w->parentWidget();
  }

  if (foundEditor)
    connectEditor(foundEditor);

  // 找出焦点所在的面板组 → 应用 dimming
  QTabWidget *activeTabs = nullptr;
  for (int i = 0; i < m_ui->editorPanelCount(); ++i) {
    auto *tabs = m_ui->editorPanelAt(i);
    if (tabs && tabs->isAncestorOf(newFocus)) {
      activeTabs = tabs;
      break;
    }
  }

  m_ui->applyTabDimming(activeTabs);
}

void MainDevMgr::updateCursorPosition() {
  if (!m_model->connectedEditor || !m_model->connectedEditor->isVisible()) {
    m_ui->setCursorStatusText(QStringLiteral("行: -, 列: -"));
    return;
  }

  QTextCursor cursor = m_model->connectedEditor->textCursor();
  int line = cursor.blockNumber() + 1;
  int col = cursor.columnNumber() + 1;
  m_ui->setCursorStatusText(
      QStringLiteral("行: %1, 列: %2").arg(line).arg(col));
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

  // ── 清理数据层 ──
  QString filePath = editor->objectName();
  if (!filePath.isEmpty()) {
    bool stillOpen = false;
    for (int i = 0; i < m_ui->editorPanelCount(); ++i) {
      auto *otherTabs = m_ui->editorPanelAt(i);
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
    if (!stillOpen)
      m_model->unregisterFile(filePath);
  }

  tabs->removeTab(index);
  editor->deleteLater();

  // ── 空面板且存在多个面板 → 删除面板组 ──
  if (tabs->count() == 0 && m_ui->editorPanelCount() > 1) {
    int idx = m_ui->editorPanelIndex(tabs);
    if (idx >= 0)
      m_ui->removeEditorPanelAt(idx);
    m_ui->setEditorPanelsUniformStretch();
    if (m_model->lastActivePanel == tabs)
      m_model->lastActivePanel = nullptr;
    m_ui->setWindowTitle(QStringLiteral("Auto Code - 开发模式"));
    connectEditor(currentEditor());
    m_ui->applyTabDimming(currentTabWidget());
    return;
  }

  // ── 更新窗口标题 ──
  if (tabs->count() == 0) {
    m_ui->setWindowTitle(QStringLiteral("Auto Code - 开发模式"));
  } else {
    int newIdx = tabs->currentIndex();
    if (newIdx >= 0) {
      QString fullPath = tabs->tabToolTip(newIdx);
      if (!fullPath.isEmpty()) {
        QFileInfo fi(fullPath);
        m_ui->setWindowTitle(
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

  // ── 更新窗口标题 ──
  if (index < 0 || index >= tabs->count()) {
    m_ui->setWindowTitle(QStringLiteral("Auto Code - 开发模式"));
    return;
  }

  QString fullPath = tabs->tabToolTip(index);
  if (!fullPath.isEmpty()) {
    QFileInfo fi(fullPath);
    m_ui->setWindowTitle(QStringLiteral("Auto Code - %1").arg(fi.fileName()));
  }

  connectEditor(currentEditor());
  m_model->lastActivePanel = tabs;
  m_ui->applyTabDimming(tabs);
}

// ──────────────────────────────────────────────────────────────
//  拆分 / 关闭标签页
// ──────────────────────────────────────────────────────────────

void MainDevMgr::onSplitRight() {
  // 没有打开任何文件时不拆分
  CodeEditor *current = currentEditor();
  if (!current)
    return;

  QTabWidget *newPanel = m_ui->createEditorPanel();

  connect(newPanel, &QTabWidget::tabCloseRequested, this,
          &MainDevMgr::onTabCloseRequested);
  connect(newPanel, &QTabWidget::currentChanged, this,
          &MainDevMgr::onCurrentTabChanged);

  {
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
  }

  // ── 计算当前面板在分割器中的大小，用于平分 ──
  QTabWidget *curPanel = currentTabWidget();
  int curIdx = m_ui->editorPanelIndex(curPanel);
  QList<int> oldSizes =
      curIdx >= 0 ? m_ui->editorSplitter()->sizes() : QList<int>();

  m_ui->addEditorPanel(newPanel);
  m_ui->setEditorPanelsUniformStretch();

  // 仅平分当前面板：原面板和新面板各占一半
  if (curIdx >= 0 && curIdx < oldSizes.size()) {
    int half = oldSizes[curIdx] / 2;
    QList<int> newSizes = m_ui->editorSplitter()->sizes();
    newSizes[curIdx] = half;
    newSizes.insert(curIdx + 1, half);
    m_ui->editorSplitter()->setSizes(newSizes);
  }

  m_model->lastActivePanel = newPanel;
  m_ui->applyTabDimming(newPanel);

  auto *editorInPanel = qobject_cast<CodeEditor *>(newPanel->currentWidget());
  if (editorInPanel)
    editorInPanel->setFocus();
  else
    newPanel->setFocus();
}

void MainDevMgr::onCloseEditor() {
  QTabWidget *tabs = currentTabWidget();
  if (!tabs)
    return;

  int idx = tabs->currentIndex();
  if (idx >= 0)
    onTabCloseRequested(idx);
}