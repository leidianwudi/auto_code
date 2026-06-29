/**
 * @file main_dev.cpp
 * @brief 代码编辑主窗口实现
 */

#include "main_dev.h"
#include "src/ui/tool_ui/code_editor.h"
#include "src/ui/tool_ui/highlighter/json_highlighter.h"
#include "src/ui/tool_ui/highlighter/template_highlighter.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStyle>
#include <QTabWidget>
#include <QTextStream>
#include <QTreeWidget>
#include <QVBoxLayout>

// ──────────────────────────────────────────────────────────────
//  构造
// ──────────────────────────────────────────────────────────────

MainDev::MainDev(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("Auto Code - 开发模式"));
  resize(1400, 850);
  setupUI();
  loadFiles();
}

// ──────────────────────────────────────────────────────────────
//  界面初始化
// ──────────────────────────────────────────────────────────────

void MainDev::setupUI() {
  // ── 菜单栏 ──
  QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("视图(&V)"));

  QAction *splitAction = viewMenu->addAction(QStringLiteral("向右拆分编辑器"));
  splitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+\\")));
  connect(splitAction, &QAction::triggered, this, &MainDev::onSplitRight);

  QAction *closeAction = viewMenu->addAction(QStringLiteral("关闭标签页"));
  closeAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));
  connect(closeAction, &QAction::triggered, this, &MainDev::onCloseEditor);

  // ── 左侧文件树 ──
  m_fileTree = new QTreeWidget(this);
  m_fileTree->setHeaderLabel(QStringLiteral("文件"));
  m_fileTree->setMinimumWidth(200);
  m_fileTree->setMaximumWidth(400);
  m_fileTree->setAnimated(true);
  m_fileTree->setIndentation(16);
  m_fileTree->setSortingEnabled(false);
  connect(m_fileTree, &QTreeWidget::itemClicked, this,
          &MainDev::onTreeItemClicked);

  // ── 右侧编辑器区域 ──
  m_editorSplitter = new QSplitter(Qt::Horizontal, this);

  // 创建初始编辑器面板组
  QTabWidget *initialTabs = createEditorPanel();
  m_editorSplitter->addWidget(initialTabs);

  // ── 主分割器 ──
  m_mainSplitter = new QSplitter(Qt::Horizontal, this);
  m_mainSplitter->addWidget(m_fileTree);
  m_mainSplitter->addWidget(m_editorSplitter);
  m_mainSplitter->setStretchFactor(0, 0);
  m_mainSplitter->setStretchFactor(1, 1);
  m_mainSplitter->setSizes({250, 1150});

  setCentralWidget(m_mainSplitter);
}

// ──────────────────────────────────────────────────────────────
//  编辑器面板组创建
// ──────────────────────────────────────────────────────────────

/**
 * @brief 创建一个新的编辑器面板组
 *
 * 每个面板组是一个 QTabWidget，包含可关闭的标签页，
 * 每个标签页内是一个 CodeEditor。
 *
 * @return 新创建的 QTabWidget
 */
QTabWidget *MainDev::createEditorPanel() {
  auto *tabs = new QTabWidget;
  tabs->setTabsClosable(true);
  tabs->setDocumentMode(true);
  tabs->setMovable(true);

  // 连接信号
  connect(tabs, &QTabWidget::tabCloseRequested, this,
          &MainDev::onTabCloseRequested);
  connect(tabs, &QTabWidget::currentChanged, this,
          &MainDev::onCurrentTabChanged);

  return tabs;
}

// ──────────────────────────────────────────────────────────────
//  文件扫描与树构建
// ──────────────────────────────────────────────────────────────

/**
 * @brief 扫描 file/ 目录，递归加载 .ac 和 .json 文件
 *
 * 路径查找优先级：
 * 1. PROJECT_SOURCE_DIR（编译时注入的源码路径）
 * 2. 可执行文件所在目录的 ../../
 * 3. 当前工作目录
 */
void MainDev::loadFiles() {
  QDir baseDir;

  // 1. 编译时注入的源码路径（最可靠）
  baseDir.setPath(QStringLiteral(PROJECT_SOURCE_DIR) + QStringLiteral("/file"));

  // 2. 可执行文件旁
  if (!baseDir.exists()) {
    baseDir.setPath(QCoreApplication::applicationDirPath() +
                    QStringLiteral("/file"));
  }
  if (!baseDir.exists()) {
    baseDir.setPath(QCoreApplication::applicationDirPath() +
                    QStringLiteral("/../../file"));
  }

  // 3. 当前工作目录
  if (!baseDir.exists()) {
    baseDir.setPath(QDir::currentPath() + QStringLiteral("/file"));
  }

  if (!baseDir.exists()) {
    qWarning("未找到 file/ 目录");
    return;
  }

  // 递归遍历所有子目录
  addDirectoryToTree(nullptr, baseDir.absolutePath());
  m_fileTree->expandAll();
}

/**
 * @brief 递归将目录下的文件添加到树节点
 *
 * 先处理子目录（递归），再处理文件（使用名称过滤器）。
 *
 * @param parentItem 父节点，nullptr 时表示根节点
 * @param dirPath 目录绝对路径
 */
void MainDev::addDirectoryToTree(QTreeWidgetItem *parentItem,
                                 const QString &dirPath) {
  QDir dir(dirPath);

  // ── 先处理子目录 ──
  QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QFileInfo &info : dirs) {
    QTreeWidgetItem *dirItem;
    if (parentItem) {
      dirItem = new QTreeWidgetItem(parentItem);
    } else {
      dirItem = new QTreeWidgetItem(m_fileTree);
    }
    dirItem->setText(0, info.fileName());
    dirItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    dirItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    addDirectoryToTree(dirItem, info.absoluteFilePath());
  }

  // ── 再处理文件（仅 .ac 和 .json） ──
  QStringList nameFilters;
  nameFilters << QStringLiteral("*.ac") << QStringLiteral("*.json");
  QFileInfoList files = dir.entryInfoList(nameFilters, QDir::Files);
  for (const QFileInfo &info : files) {
    QTreeWidgetItem *fileItem;
    if (parentItem) {
      fileItem = new QTreeWidgetItem(parentItem);
    } else {
      fileItem = new QTreeWidgetItem(m_fileTree);
    }
    fileItem->setText(0, info.fileName());
    fileItem->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
    fileItem->setData(0, FilePathRole, info.absoluteFilePath());
  }
}

// ──────────────────────────────────────────────────────────────
//  树节点点击 → 打开文件
// ──────────────────────────────────────────────────────────────

/**
 * @brief 点击文件树节点时，在编辑器中打开对应文件
 *
 * 如果文件已打开（在任意面板组的任意标签页中），则切换到该标签页；
 * 否则在当前面板组的新标签页中打开。
 */
void MainDev::onTreeItemClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  if (!item)
    return;

  QString filePath = item->data(0, FilePathRole).toString();
  if (filePath.isEmpty())
    return; // 目录节点

  openFileInEditor(filePath);
}

// ──────────────────────────────────────────────────────────────
//  创建 / 打开编辑器
// ──────────────────────────────────────────────────────────────

/**
 * @brief 根据文件类型创建 CodeEditor 并绑定语法高亮器
 *
 * - .ac 文件 → TemplateHighlighter
 * - .json 文件 → JsonHighlighter
 *
 * @param filePath 文件路径（用于判断后缀）
 * @return 配置好的 CodeEditor 指针
 */
CodeEditor *MainDev::createEditorForFile(const QString &filePath) {
  auto *editor = new CodeEditor;

  if (filePath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
    new JsonHighlighter(editor->document());
  } else {
    new TemplateHighlighter(editor->document());
  }

  return editor;
}

/**
 * @brief 在编辑器中打开指定文件
 *
 * 流程：
 * 1. 检查 m_openFiles 是否已打开 → 若已打开则切换到对应标签页
 * 2. 读取文件内容
 * 3. 在当前面板组创建新标签页，绑定高亮器和验证模式
 * 4. 记录到 m_openFiles
 *
 * @param filePath 文件绝对路径
 * @return 编辑该文件的 CodeEditor 指针
 */
CodeEditor *MainDev::openFileInEditor(const QString &filePath) {
  // ── 检查文件是否已在某标签页中打开 ──
  if (m_openFiles.contains(filePath)) {
    CodeEditor *editor = m_openFiles.value(filePath);
    // 找到该编辑器所属的 QTabWidget
    QTabWidget *parentTabs = qobject_cast<QTabWidget *>(editor->parentWidget());
    if (parentTabs) {
      int idx = parentTabs->indexOf(editor);
      if (idx >= 0) {
        parentTabs->setCurrentIndex(idx);
        parentTabs->setFocus();
        editor->setFocus();
        return editor;
      }
    }
    // 编辑器已被移除，重新打开
    m_openFiles.remove(filePath);
  }

  // ── 读取文件内容 ──
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::warning(this, QStringLiteral("打开失败"),
                         QStringLiteral("无法打开文件: %1").arg(filePath));
    return nullptr;
  }
  QTextStream in(&file);
  QString content = in.readAll();
  file.close();

  // ── 创建编辑器 ──
  CodeEditor *editor = createEditorForFile(filePath);
  editor->setPlainText(content);

  // 设置验证模式
  if (filePath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
    editor->setValidationMode(CodeEditor::JsonValidation);
  } else {
    editor->setValidationMode(CodeEditor::TemplateValidation);
  }

  // ── 获取当前面板组，添加到新标签页 ──
  QTabWidget *tabs = currentTabWidget();
  if (!tabs) {
    tabs = createEditorPanel();
    m_editorSplitter->addWidget(tabs);
  }

  QFileInfo fi(filePath);
  int idx = tabs->addTab(editor, fi.fileName());
  tabs->setTabToolTip(idx, filePath);
  tabs->setCurrentIndex(idx);

  // ── 记录状态 ──
  m_openFiles.insert(filePath, editor);
  editor->setObjectName(filePath);
  editor->setFocus();
  setWindowTitle(QStringLiteral("Auto Code - %1").arg(fi.fileName()));

  return editor;
}

/**
 * @brief 获取当前激活的编辑器
 * @return 当前获得焦点的 CodeEditor，若无则返回 nullptr
 */
CodeEditor *MainDev::currentEditor() const {
  // 遍历所有编辑器面板组，查找当前显示的标签页
  for (int i = 0; i < m_editorSplitter->count(); ++i) {
    auto *tabs = qobject_cast<QTabWidget *>(m_editorSplitter->widget(i));
    if (tabs && tabs->isVisible()) {
      auto *editor = qobject_cast<CodeEditor *>(tabs->currentWidget());
      if (editor)
        return editor;
    }
  }
  return nullptr;
}

/**
 * @brief 获取当前激活的标签页控件
 * @return 当前获得焦点的 QTabWidget，或第一个可见的
 */
QTabWidget *MainDev::currentTabWidget() const {
  // 从焦点小部件向上查找 QTabWidget
  QWidget *w = QApplication::focusWidget();
  while (w) {
    if (auto *tabs = qobject_cast<QTabWidget *>(w))
      return tabs;
    w = w->parentWidget();
  }
  // 回退：返回第一个可见的
  for (int i = 0; i < m_editorSplitter->count(); ++i) {
    auto *tabs = qobject_cast<QTabWidget *>(m_editorSplitter->widget(i));
    if (tabs && tabs->isVisible())
      return tabs;
  }
  return nullptr;
}

// ──────────────────────────────────────────────────────────────
//  标签页操作
// ──────────────────────────────────────────────────────────────

/**
 * @brief 标签页关闭按钮被点击
 *
 * 关闭指定索引的标签页，从 m_openFiles 中移除记录。
 * 如果标签页内容是文件，则关闭前不做保存提示。
 */
void MainDev::onTabCloseRequested(int index) {
  auto *tabs = qobject_cast<QTabWidget *>(sender());
  if (!tabs)
    return;

  auto *editor = qobject_cast<CodeEditor *>(tabs->widget(index));
  if (!editor)
    return;

  // 从打开文件映射中移除
  QString filePath = editor->objectName();
  if (!filePath.isEmpty()) {
    m_openFiles.remove(filePath);
  }

  tabs->removeTab(index);
  editor->deleteLater();

  // 如果当前面板组没有标签页了，更新窗口标题
  if (tabs->count() == 0) {
    setWindowTitle(QStringLiteral("Auto Code - 开发模式"));
  }
}

/**
 * @brief 当前标签页切换时更新窗口标题
 */
void MainDev::onCurrentTabChanged(int index) {
  auto *tabs = qobject_cast<QTabWidget *>(sender());
  if (!tabs)
    return;

  if (index < 0 || index >= tabs->count()) {
    setWindowTitle(QStringLiteral("Auto Code - 开发模式"));
    return;
  }

  QString fullPath = tabs->tabToolTip(index);
  if (!fullPath.isEmpty()) {
    QFileInfo fi(fullPath);
    setWindowTitle(QStringLiteral("Auto Code - %1").arg(fi.fileName()));
  }
}

// ──────────────────────────────────────────────────────────────
//  拆分 / 关闭 标签页
// ──────────────────────────────────────────────────────────────

/**
 * @brief 向右拆分编辑器
 *
 * 创建一个新的编辑器面板组（QTabWidget），里面包含一个空白的 CodeEditor。
 */
void MainDev::onSplitRight() {
  QTabWidget *newPanel = createEditorPanel();

  // 可以复制当前激活编辑器的内容
  CodeEditor *current = currentEditor();
  if (current) {
    auto *editor = new CodeEditor;
    editor->setPlainText(current->toPlainText());

    // 复制高亮器
    QString filePath = current->objectName();
    if (filePath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
      new JsonHighlighter(editor->document());
      editor->setValidationMode(CodeEditor::JsonValidation);
    } else {
      new TemplateHighlighter(editor->document());
      editor->setValidationMode(CodeEditor::TemplateValidation);
    }

    int idx = newPanel->addTab(editor, QStringLiteral("拆分副本"));
    newPanel->setCurrentIndex(idx);
  }

  m_editorSplitter->addWidget(newPanel);
  newPanel->setFocus();
}

/**
 * @brief 关闭当前激活的标签页
 *
 * 等价于点击当前标签页的关闭按钮。
 */
void MainDev::onCloseEditor() {
  QTabWidget *tabs = currentTabWidget();
  if (!tabs)
    return;

  int idx = tabs->currentIndex();
  if (idx >= 0) {
    onTabCloseRequested(idx);
  }
}