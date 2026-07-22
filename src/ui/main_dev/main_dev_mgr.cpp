/**
 * @file main_dev_mgr.cpp
 * @brief 代码编辑器控制器层实现（单例 UI 控制器）
 */

#include "main_dev_mgr.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMouseEvent>
#include <QShortcut>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextStream>

#include "main_dev_model.h"
#include "main_dev_ui.h"
#include "main_dev_ui_ext.h"
#include "src/engine/ac_language.h"
#include "src/engine/script/ac_engine.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/code/code_find_bar.h"
#include "src/util/ui/component/aui_message_box.h"
#include "src/util/ui/highlighter/light_ac.h"
#include "src/util/ui/highlighter/light_json.h"
#include "src/util/ui/highlighter/light_tpl.h"

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

  return m_ui;
}

// ──────────────────────────────────────────────────────────────
//  initUi — 连接所有信号槽
// ──────────────────────────────────────────────────────────────

void MainDevMgr::initUi() {
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
  connect(m_ui->fileTree(), &TreeDir::renameRequested, this, &MainDevMgr::onRenameFile);
  connect(m_ui->fileTree(), &TreeDir::deleteRequested, this, &MainDevMgr::onDeleteFile);
  connect(qApp, &QApplication::focusChanged, this, &MainDevMgr::onFocusChanged);

  // ── 保存按钮 ──
  connect(m_ui->saveBtn(), &QPushButton::clicked, this,
          [this]() { m_model->saveEditor(currentEditor()); });

  // ── Ctrl+S 快捷键 ──
  auto *saveShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+S")), m_ui);
  connect(saveShortcut, &QShortcut::activated, this,
          [this]() { m_model->saveEditor(currentEditor()); });

  // ── 保存全部按钮 ──
  connect(m_ui->saveAllBtn(), &QPushButton::clicked, this, [this]() {
    for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
      auto *tabs = m_ui->editorPanelAt(pi);
      if (!tabs) continue;
      for (int ti = 0; ti < tabs->count(); ++ti) {
        auto *editor = qobject_cast<CodeEditor *>(tabs->widget(ti));
        if (editor && editor->document()->isModified()) m_model->saveEditor(editor);
      }
    }
  });

  // ── 执行按钮 ──
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
  QDir baseDir;

  baseDir.setPath(QStringLiteral(PROJECT_SOURCE_DIR) + QStringLiteral("/file"));
  if (!baseDir.exists())
    baseDir.setPath(QCoreApplication::applicationDirPath() + QStringLiteral("/file"));
  if (!baseDir.exists())
    baseDir.setPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../../file"));
  if (!baseDir.exists()) baseDir.setPath(QDir::currentPath() + QStringLiteral("/file"));

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

  if (filePath.endsWith(AcFileSuffix::kJson, Qt::CaseInsensitive)) {
    new LightJson(editor->document());
    editor->setValidationMode(CodeEditor::JsonValidation);
  } else if (filePath.endsWith(AcFileSuffix::kAc, Qt::CaseInsensitive)) {
    new LightAc(editor->document());
    editor->setValidationMode(CodeEditor::AcValidation);
  } else if (filePath.endsWith(AcFileSuffix::kTpl, Qt::CaseInsensitive)) {
    new LightTpl(editor->document());
    editor->setValidationMode(CodeEditor::TemplateValidation);
  } else {
    new LightTpl(editor->document());
    editor->setValidationMode(CodeEditor::TemplateValidation);
  }

  return editor;
}

CodeEditor *MainDevMgr::openFileInEditor(const QString &filePath) {
  // ── 查重：遍历所有面板组的所有标签页 ──
  for (int i = 0; i < m_ui->editorPanelCount(); ++i) {
    auto *tabs = m_ui->editorPanelAt(i);
    if (!tabs) continue;
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
    AuiMessageBox::show(m_ui, QStringLiteral("打开失败"),
                        QStringLiteral("无法打开文件: %1").arg(filePath));
    return nullptr;
  }
  QTextStream in(&file);
  QString content = in.readAll();
  file.close();

  // ── 创建编辑器（含高亮器 + 验证模式） ──
  CodeEditor *editor = createEditorForFile(filePath);
  editor->setPlainText(content);

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

  // ── 首次加载文件时刷新主分割器布局 ──
  if (m_model->openFiles.isEmpty()) m_ui->adjustMainSplitter();

  // ── 记录状态 ──
  m_model->registerFile(filePath, content, editor);
  editor->setObjectName(filePath);
  editor->setFocus();
  m_ui->setWindowTitle(MainDevUi::fileTitle(fi.fileName()));

  // ── 修改标记：内容变化时标签页和树节点绘制红色 "*" ──
  connect(editor->document(), &QTextDocument::modificationChanged, this,
          [this, tabs, editor, filePath](bool changed) {
            for (int i = 0; i < tabs->count(); ++i) {
              if (tabs->widget(i) == editor) {
                auto *bar = qobject_cast<DraggableTabBar *>(tabs->tabBar());
                if (bar) bar->setTabModified(i, changed);
                break;
              }
            }
            // 更新树形目录对应文件的修改状态
            m_ui->fileTree()->setFileModified(filePath, changed);
            // 更新保存按钮可用状态
            updateSaveButtonState();
          });

  return editor;
}

CodeEditor *MainDevMgr::currentEditor() const {
  for (int i = 0; i < m_ui->editorPanelCount(); ++i) {
    auto *tabs = m_ui->editorPanelAt(i);
    if (tabs && tabs->isVisible()) {
      auto *editor = qobject_cast<CodeEditor *>(tabs->currentWidget());
      if (editor) return editor;
    }
  }
  return nullptr;
}

QTabWidget *MainDevMgr::currentTabWidget() const {
  // 1. 优先从焦点控件向上查找 QTabWidget 祖先
  QWidget *focus = QApplication::focusWidget();
  while (focus) {
    if (auto *tabs = qobject_cast<QTabWidget *>(focus)) return tabs;
    focus = focus->parentWidget();
  }

  // 2. 再 fallback 到最后活跃面板
  if (m_model->lastActivePanel && m_model->lastActivePanel->isVisible() &&
      m_model->lastActivePanel->count() > 0) {
    return m_model->lastActivePanel;
  }

  // 3. 最后 fallback 到第一个可见非空面板
  QTabWidget *emptyPanel = nullptr;
  for (int i = 0; i < m_ui->editorPanelCount(); ++i) {
    auto *tabs = m_ui->editorPanelAt(i);
    if (tabs && tabs->isVisible()) {
      if (tabs->count() > 0) return tabs;
      if (!emptyPanel) emptyPanel = tabs;
    }
  }
  return emptyPanel;
}

// ──────────────────────────────────────────────────────────────
//  信号连接
// ──────────────────────────────────────────────────────────────

void MainDevMgr::connectEditor(CodeEditor *editor) {
  if (m_model->connectedEditor) {
    disconnect(m_model->connectedEditor, &QPlainTextEdit::cursorPositionChanged, this,
               &MainDevMgr::updateCursorPosition);
    disconnect(m_model->connectedEditor, &CodeEditor::validationMessage, this,
               &MainDevMgr::onValidationMessage);
    disconnect(m_model->connectedEditor, &CodeEditor::requestGoToLine, this,
               &MainDevMgr::onGoToLine);
    disconnect(m_model->connectedEditor, &CodeEditor::aboutToNavigate, this,
               &MainDevMgr::onAboutToNavigate);
  }

  m_model->connectedEditor = editor;

  if (editor) {
    connect(editor, &QPlainTextEdit::cursorPositionChanged, this,
            &MainDevMgr::updateCursorPosition);
    connect(editor, &CodeEditor::validationMessage, this, &MainDevMgr::onValidationMessage);
    // 跨文件跳转信号
    connect(editor, &CodeEditor::requestGoToLine, this, &MainDevMgr::onGoToLine);
    // 即将导航信号（用于记录历史）
    connect(editor, &CodeEditor::aboutToNavigate, this, &MainDevMgr::onAboutToNavigate);
    updateCursorPosition();
    editor->validate();
  } else {
    m_ui->setCursorStatusText(MainDevUi::cursorDefault());
    m_ui->setErrorMessage(QString());
  }
}

void MainDevMgr::onFocusChanged(QWidget * /*oldFocus*/, QWidget *newFocus) {
  if (!newFocus) return;

  QWidget *w = newFocus;
  CodeEditor *foundEditor = nullptr;

  while (w) {
    if (auto *tabs = qobject_cast<QTabWidget *>(w)) m_model->lastActivePanel = tabs;
    if (!foundEditor) {
      if (auto *editor = qobject_cast<CodeEditor *>(w)) foundEditor = editor;
    }
    w = w->parentWidget();
  }

  if (foundEditor) {
    connectEditor(foundEditor);
    // 焦点切换到编辑器时，同步定位树形目录到当前文件（处理拆分面板间切换的场景）
    QString filePath = foundEditor->objectName();
    if (!filePath.isEmpty()) {
      m_ui->fileTree()->locateFile(filePath);
    }
  }

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
  updateSaveButtonState();
}

void MainDevMgr::updateCursorPosition() {
  if (!m_model->connectedEditor || !m_model->connectedEditor->isVisible()) {
    m_ui->setCursorStatusText(MainDevUi::cursorDefault());
    return;
  }

  QTextCursor cursor = m_model->connectedEditor->textCursor();
  int line = cursor.blockNumber() + 1;
  int col = cursor.columnNumber() + 1;
  m_ui->setCursorStatusText(QStringLiteral("行: %1, 列: %2").arg(line).arg(col));
}

void MainDevMgr::onValidationMessage(const QString &msg, int errorCount) {
  m_ui->setErrorMessage(msg);

  // 通知 TreeDir 更新文件错误状态
  if (m_model->connectedEditor && m_ui->fileTree()) {
    QString filePath = m_model->connectedEditor->objectName();
    if (!filePath.isEmpty()) {
      if (errorCount == 0) {
        // 无错误，清除错误状态
        m_ui->fileTree()->clearFileError(filePath);
      } else {
        // 有错误，传递实际错误数量
        m_ui->fileTree()->setFileError(filePath, errorCount);
      }
    }
  }
}

// ──────────────────────────────────────────────────────────────
//  标签页操作
// ──────────────────────────────────────────────────────────────

void MainDevMgr::closeTab(QTabWidget *tabs, int index) {
  auto *editor = qobject_cast<CodeEditor *>(tabs->widget(index));
  if (!editor) return;

  // ── 清理数据层 ──
  QString filePath = editor->objectName();
  if (!filePath.isEmpty() && !m_model->isRegisteredElsewhere(filePath, editor)) {
    m_model->unregisterFile(filePath);
  }

  tabs->removeTab(index);
  editor->deleteLater();

  // ── 空面板且存在多个面板 → 删除面板组 ──
  if (tabs->count() == 0 && m_ui->editorPanelCount() > 1) {
    int idx = m_ui->editorPanelIndex(tabs);
    if (idx >= 0) m_ui->removeEditorPanelAt(idx);
    m_ui->setEditorPanelsUniformStretch();
    if (m_model->lastActivePanel == tabs) m_model->lastActivePanel = nullptr;
    m_ui->setWindowTitle(MainDevUi::devTitle());
    connectEditor(currentEditor());
    m_ui->applyTabDimming(currentTabWidget());
    return;
  }

  // ── 更新窗口标题 ──
  if (tabs->count() == 0) {
    m_ui->setWindowTitle(MainDevUi::devTitle());
  } else {
    int newIdx = tabs->currentIndex();
    if (newIdx >= 0) {
      QString fullPath = tabs->tabToolTip(newIdx);
      if (!fullPath.isEmpty()) {
        QFileInfo fi(fullPath);
        m_ui->setWindowTitle(MainDevUi::fileTitle(fi.fileName()));
      }
    }
  }

  connectEditor(currentEditor());
}

void MainDevMgr::onTabCloseRequested(int index) {
  auto *tabs = qobject_cast<QTabWidget *>(sender());
  if (!tabs) return;
  closeTab(tabs, index);
}

void MainDevMgr::onCurrentTabChanged(int index) {
  auto *tabs = qobject_cast<QTabWidget *>(sender());
  if (!tabs) return;

  // ── 同步查找栏显示/隐藏 ──
  // 遍历该面板所有编辑器：暂停非当前标签页的查找栏，恢复当前标签页的查找栏
  for (int i = 0; i < tabs->count(); ++i) {
    auto *editor = qobject_cast<CodeEditor *>(tabs->widget(i));
    if (!editor || !editor->findBar()) continue;
    if (i == index) {
      editor->findBar()->resumeVisible();
    } else {
      editor->findBar()->pauseVisible();
    }
  }

  // ── 更新窗口标题 ──
  if (index < 0 || index >= tabs->count()) {
    m_ui->setWindowTitle(MainDevUi::devTitle());
    return;
  }

  QString fullPath = tabs->tabToolTip(index);
  if (!fullPath.isEmpty()) {
    QFileInfo fi(fullPath);
    m_ui->setWindowTitle(MainDevUi::fileTitle(fi.fileName()));
    // 标签页切换时，同步定位树形目录到当前文件
    m_ui->fileTree()->locateFile(fullPath);
  }

  connectEditor(currentEditor());
  m_model->lastActivePanel = tabs;
  m_ui->applyTabDimming(tabs);
}

void MainDevMgr::onTabBarClicked(int index) {
  // 点击已选中的标签时 currentChanged 不会触发，需要手动激活该面板
  auto *bar = qobject_cast<QTabBar *>(sender());
  if (!bar) return;
  auto *tabs = qobject_cast<QTabWidget *>(bar->parentWidget());
  if (!tabs || index < 0 || index >= tabs->count()) return;

  // 将焦点设置到当前编辑器，触发 onFocusChanged 完成面板切换
  auto *editor = qobject_cast<CodeEditor *>(tabs->widget(index));
  if (editor) editor->setFocus();
}

// ──────────────────────────────────────────────────────────────
//  拆分 / 关闭标签页
// ──────────────────────────────────────────────────────────────

void MainDevMgr::onSplitRight() {
  // 没有打开任何文件时不拆分
  CodeEditor *current = currentEditor();
  if (!current) return;

  QTabWidget *newPanel = m_ui->createEditorPanel();

  connect(newPanel, &QTabWidget::tabCloseRequested, this, &MainDevMgr::onTabCloseRequested);
  connect(newPanel, &QTabWidget::currentChanged, this, &MainDevMgr::onCurrentTabChanged);

  {
    auto *bar = qobject_cast<DraggableTabBar *>(newPanel->tabBar());
    if (bar) {
      connect(bar, &DraggableTabBar::closeOthersRequested, this, &MainDevMgr::onCloseOthers);
      connect(bar, &DraggableTabBar::closeAllRequested, this, &MainDevMgr::onCloseAll);
      connect(bar, &QTabBar::tabBarClicked, this, &MainDevMgr::onTabBarClicked);
    }
  }

  {
    // 复用 createEditorForFile 创建高亮器 + 验证模式一致的编辑器
    QString filePath = current->objectName();
    auto *editor = createEditorForFile(filePath);
    editor->setPlainText(current->toPlainText());

    QFileInfo fi(filePath);
    QString tabLabel = filePath.isEmpty() ? QStringLiteral("拆分副本") : fi.fileName();
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
  QList<int> oldSizes = curIdx >= 0 ? m_ui->editorSplitter()->sizes() : QList<int>();

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
  if (!tabs) return;

  int idx = tabs->currentIndex();
  if (idx >= 0) {
    // 通过信号触发 onTabCloseRequested → closeTab，确保 sender() 兼容
    emit tabs->tabCloseRequested(idx);
  }
}

// ──────────────────────────────────────────────────────────────
//  工具栏状态更新
// ──────────────────────────────────────────────────────────────

void MainDevMgr::updateSaveButtonState() {
  m_ui->saveBtn()->setEnabled(currentEditor() && currentEditor()->document()->isModified());
  m_ui->saveAllBtn()->setEnabled(m_model->hasAnyModified());
}

// ──────────────────────────────────────────────────────────────
//  重命名
// ──────────────────────────────────────────────────────────────

void MainDevMgr::onRenameFile(const QString &oldPath, const QString &newName) {
  // 校验新名称
  if (newName.isEmpty()) return;

  static const QString kInvalidChars = QStringLiteral("\\/:*?\"<>|");
  for (const QChar &c : newName) {
    if (kInvalidChars.contains(c)) {
      AuiMessageBox::show(m_ui, QStringLiteral("重命名失败"),
                          QStringLiteral("文件名不能包含以下字符: %1").arg(kInvalidChars));
      return;
    }
  }

  QFileInfo oldInfo(oldPath);
  QString parentDir = oldInfo.absolutePath();
  QString newPath = QDir::cleanPath(parentDir + QStringLiteral("/") + newName);

  // 同名检查
  if (QFileInfo::exists(newPath)) {
    AuiMessageBox::show(m_ui, QStringLiteral("重命名失败"),
                        QStringLiteral("已存在同名文件或文件夹: %1").arg(newName));
    return;
  }

  bool isDir = oldInfo.isDir();

  if (isDir) {
    // 文件夹重命名
    QDir dir(parentDir);
    if (!dir.rename(oldInfo.fileName(), newName)) {
      AuiMessageBox::show(m_ui, QStringLiteral("重命名失败"),
                          QStringLiteral("无法重命名文件夹: %1").arg(oldInfo.fileName()));
      return;
    }

    // 更新所有以旧路径开头的已打开文件
    QString oldDirPath = QDir::cleanPath(oldPath);
    for (const QString &filePath : m_model->openFilePaths()) {
      if (filePath.startsWith(oldDirPath + QStringLiteral("/"))) {
        QString newFilePath = newPath + filePath.mid(oldDirPath.length());
        CodeEditor *editor = m_model->openFiles.value(filePath);
        if (editor) {
          editor->setObjectName(newFilePath);
          // 更新 tab 标题和 tooltip
          for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
            auto *tabs = m_ui->editorPanelAt(pi);
            if (!tabs) continue;
            for (int ti = 0; ti < tabs->count(); ++ti) {
              if (tabs->widget(ti) == editor) {
                tabs->setTabText(ti, QFileInfo(newFilePath).fileName());
                tabs->setTabToolTip(ti, newFilePath);
                break;
              }
            }
          }
        }
        // 更新 model 中的注册信息
        QString content = m_model->fileContents.value(filePath);
        m_model->openFiles.remove(filePath);
        m_model->fileContents.remove(filePath);
        m_model->openFiles.insert(newFilePath, editor);
        m_model->fileContents.insert(newFilePath, content);
      }
    }
  } else {
    // 文件重命名
    QFile file(oldPath);
    if (!file.rename(newPath)) {
      AuiMessageBox::show(m_ui, QStringLiteral("重命名失败"),
                          QStringLiteral("无法重命名文件: %1").arg(oldInfo.fileName()));
      return;
    }

    // 更新已打开的编辑器
    CodeEditor *editor = m_model->openFiles.value(oldPath);
    if (editor) {
      editor->setObjectName(newPath);
      for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
        auto *tabs = m_ui->editorPanelAt(pi);
        if (!tabs) continue;
        for (int ti = 0; ti < tabs->count(); ++ti) {
          if (tabs->widget(ti) == editor) {
            tabs->setTabText(ti, newName);
            tabs->setTabToolTip(ti, newPath);
            break;
          }
        }
      }
      // 更新 model 中的注册信息
      QString content = m_model->fileContents.value(oldPath);
      m_model->openFiles.remove(oldPath);
      m_model->fileContents.remove(oldPath);
      m_model->openFiles.insert(newPath, editor);
      m_model->fileContents.insert(newPath, content);
    }
  }

  // 更新启动项数据（文件重命名或文件夹重命名中的 .ac 文件路径）
  if (isDir) {
    QString oldDirPath = QDir::cleanPath(oldPath);
    QList<QPair<QString, QString>> renames;
    for (const QString &sp : m_ui->fileTree()->startupFiles()) {
      if (sp.startsWith(oldDirPath + QStringLiteral("/"))) {
        QString newSp = newPath + sp.mid(oldDirPath.length());
        renames.append({sp, newSp});
      }
    }
    m_ui->fileTree()->renameStartupPaths(renames);
  } else {
    m_ui->fileTree()->renameStartupPath(oldPath, newPath);
  }

  // 刷新树
  m_ui->fileTree()->refreshTree();
}

void MainDevMgr::onDeleteFile(const QString &path) {
  QFileInfo info(path);
  if (!info.exists()) return;

  bool isDir = info.isDir();
  QString name = info.fileName();

  // 确认对话框
  if (!AuiMessageBox::confirm(
          m_ui, QStringLiteral("确认删除"),
          isDir ? QStringLiteral("确定要删除文件夹 \"%1\" 及其所有内容吗？").arg(name)
                : QStringLiteral("确定要删除文件 \"%1\" 吗？").arg(name))) {
    return;
  }

  if (isDir) {
    // 删除文件夹前，关闭所有以该路径开头的已打开文件
    QString dirPath = QDir::cleanPath(path);
    QStringList toClose;
    for (const QString &filePath : m_model->openFilePaths()) {
      if (filePath.startsWith(dirPath + QStringLiteral("/"))) {
        toClose.append(filePath);
      }
    }
    for (const QString &filePath : toClose) {
      CodeEditor *editor = m_model->openFiles.value(filePath);
      if (editor) {
        for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
          auto *tabs = m_ui->editorPanelAt(pi);
          if (!tabs) continue;
          for (int ti = 0; ti < tabs->count(); ++ti) {
            if (tabs->widget(ti) == editor) {
              closeTab(tabs, ti);
              break;
            }
          }
        }
        m_model->openFiles.remove(filePath);
        m_model->fileContents.remove(filePath);
      }
    }

    QDir dir(path);
    if (!dir.removeRecursively()) {
      AuiMessageBox::show(m_ui, QStringLiteral("删除失败"),
                          QStringLiteral("无法删除文件夹: %1").arg(name));
      return;
    }
  } else {
    // 删除文件前，关闭已打开的编辑器
    CodeEditor *editor = m_model->openFiles.value(path);
    if (editor) {
      for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
        auto *tabs = m_ui->editorPanelAt(pi);
        if (!tabs) continue;
        for (int ti = 0; ti < tabs->count(); ++ti) {
          if (tabs->widget(ti) == editor) {
            closeTab(tabs, ti);
            break;
          }
        }
      }
      m_model->openFiles.remove(path);
      m_model->fileContents.remove(path);
    }

    QFile file(path);
    if (!file.remove()) {
      AuiMessageBox::show(m_ui, QStringLiteral("删除失败"),
                          QStringLiteral("无法删除文件: %1").arg(name));
      return;
    }
  }

  // 刷新树
  m_ui->fileTree()->refreshTree();
}

// ──────────────────────────────────────────────────────────────
//  右键菜单：关闭其它 / 关闭全部
// ──────────────────────────────────────────────────────────────

void MainDevMgr::onCloseOthers(int index) {
  auto *bar = qobject_cast<DraggableTabBar *>(sender());
  if (!bar) return;
  auto *tabs = qobject_cast<QTabWidget *>(bar->parentWidget());
  if (!tabs) return;
  for (int i = tabs->count() - 1; i >= 0; --i) {
    if (i != index) closeTab(tabs, i);
  }
}

void MainDevMgr::onCloseAll() {
  auto *bar = qobject_cast<DraggableTabBar *>(sender());
  if (!bar) return;
  auto *tabs = qobject_cast<QTabWidget *>(bar->parentWidget());
  if (!tabs) return;
  while (tabs->count() > 0) {
    closeTab(tabs, 0);
  }
}

// ──────────────────────────────────────────────────────────────
//  跨文件跳转与导航历史
// ──────────────────────────────────────────────────────────────

void MainDevMgr::pushNavigationHistory(const QString &filePath, int line, int column) {
  if (m_navigating) return;

  NavigationEntry entry;
  entry.filePath = filePath;
  entry.line = line;
  entry.column = column;

  // 避免重复记录相同位置
  if (!m_navHistory.isEmpty()) {
    const auto &last = m_navHistory.top();
    if (last.filePath == filePath && last.line == line && last.column == column) return;
  }

  m_navHistory.push(entry);

  // 新操作时清空前进栈
  m_navForwardStack.clear();

  // 限制历史栈大小（最多保存 100 条）
  while (m_navHistory.size() > 100) {
    m_navHistory.remove(0);
  }
}

void MainDevMgr::jumpToLocation(const QString &filePath, int line, int column) {
  CodeEditor *editor = openFileInEditor(filePath);
  if (editor) {
    QTextCursor cursor(editor->document());

    // 定位到指定行
    if (line > 0) {
      QTextBlock block = editor->document()->findBlockByNumber(line - 1);
      if (block.isValid()) {
        cursor.setPosition(block.position());
        // 定位到指定列（如果有的话）
        if (column > 0) {
          cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                              qMin(column - 1, block.length() - 1));
        }
      }
    }

    editor->setTextCursor(cursor);
    editor->ensureCursorVisible();
    editor->setFocus();
  }
}

void MainDevMgr::onGoToLine(const QString &filePath, int line) {
  qDebug() << "onGoToLine() called:" << filePath << "line:" << line;

  // 注意：历史记录已在 onAboutToNavigate() 中保存，这里只需执行跳转
  jumpToLocation(filePath, line, 0);
}

void MainDevMgr::onAboutToNavigate(const QString &targetFilePath, int targetLine) {
  qDebug() << "onAboutToNavigate() called:" << targetFilePath << "targetLine:" << targetLine;

  // 记录当前位置到历史栈（用于后退）
  CodeEditor *current = currentEditor();
  if (current) {
    QTextCursor cursor = current->textCursor();
    int curLine = cursor.blockNumber() + 1;
    int curColumn = cursor.columnNumber() + 1;
    pushNavigationHistory(current->objectName(), curLine, curColumn);
    qDebug() << "Pushed to history:" << current->objectName() << "line:" << curLine;
  }
}

void MainDevMgr::navigateBack() {
  qDebug() << "navigateBack() called, history size:" << m_navHistory.size();
  if (m_navHistory.isEmpty()) {
    qDebug() << "Navigation history is empty, cannot go back";
    return;
  }

  // 先弹出后退栈目标位置（必须在任何修改之前，防止引用失效）
  NavigationEntry entry = m_navHistory.pop();
  qDebug() << "Navigating back to:" << entry.filePath << "line:" << entry.line;

  // 记录当前位置到前进栈
  CodeEditor *current = currentEditor();
  if (current) {
    QTextCursor cursor = current->textCursor();
    NavigationEntry forwardEntry;
    forwardEntry.filePath = current->objectName();
    forwardEntry.line = cursor.blockNumber() + 1;
    forwardEntry.column = cursor.columnNumber() + 1;
    m_navForwardStack.push(forwardEntry);
    qDebug() << "Pushed to forward stack:" << forwardEntry.filePath << "line:" << forwardEntry.line;
  }

  // 执行跳转
  m_navigating = true;
  jumpToLocation(entry.filePath, entry.line, entry.column);
  m_navigating = false;
}

void MainDevMgr::navigateForward() {
  qDebug() << "navigateForward() called, forward stack size:" << m_navForwardStack.size();
  if (m_navForwardStack.isEmpty()) {
    qDebug() << "Forward stack is empty, cannot go forward";
    return;
  }

  // 先弹出前进栈目标位置（必须在 pushNavigationHistory 之前，因为后者会清空前进栈！）
  NavigationEntry entry = m_navForwardStack.pop();
  qDebug() << "Navigating forward to:" << entry.filePath << "line:" << entry.line;

  // 记录当前位置到后退栈
  CodeEditor *current = currentEditor();
  if (current) {
    QTextCursor cursor = current->textCursor();
    pushNavigationHistory(current->objectName(), cursor.blockNumber() + 1,
                          cursor.columnNumber() + 1);
  }

  // 执行跳转
  m_navigating = true;
  jumpToLocation(entry.filePath, entry.line, entry.column);
  m_navigating = false;
}

// ──────────────────────────────────────────────────────────────
//  事件过滤器（鼠标侧键导航）
// ──────────────────────────────────────────────────────────────

bool MainDevMgr::eventFilter(QObject *obj, QEvent *event) {
  // 只处理鼠标按钮释放事件
  if (event->type() == QEvent::MouseButtonRelease) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);

    // 鼠标侧键：XButton1 = 后退，XButton2 = 前进
    if (mouseEvent->button() == Qt::XButton1) {
      qDebug() << "Mouse XButton1 pressed (Back)";
      navigateBack();
      return true;  // 事件已处理
    }
    if (mouseEvent->button() == Qt::XButton2) {
      qDebug() << "Mouse XButton2 pressed (Forward)";
      navigateForward();
      return true;  // 事件已处理
    }
  }

  // 其他事件交给默认处理
  return QObject::eventFilter(obj, event);
}