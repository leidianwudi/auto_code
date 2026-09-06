/**
 * @file main_dev_mgr_connect.cpp
 * @brief 编辑器信号连接实现（MainDevMgr 的编辑器信号、焦点、验证、事件过滤）
 */

#include <QApplication>
#include <QFileInfo>
#include <QLineEdit>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QTextCursor>
#include <QTimer>

#include <functional>

#include "debug_controller.h"
#include "main_dev_mgr.h"
#include "main_dev_model.h"
#include "main_dev_ui.h"
#include "main_dev_ui_ext.h"
#include "src/ui/json_source/json_source_widget.h"
#include "src/ui/json_vue/json_vue_widget.h"
#include "src/ui/schema_json/schema_json_widget.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/component/aui_input_dialog.h"

/// 遍历所有已打开编辑器（含拆分面板；本文件下方有同签名定义，此处先声明供上方使用）
static void forEachEditor(MainDevUi *ui, const std::function<void(CodeEditor *)> &func);

// ──────────────────────────────────────────────────────────────
//  编辑器信号连接
// ──────────────────────────────────────────────────────────────

void MainDevMgr::connectEditorSignals(CodeEditor *editor) {
  if (!editor) return;
  connect(editor, &QPlainTextEdit::cursorPositionChanged, this,
          &MainDevMgr::updateCursorPosition);
  connect(editor, &CodeEditor::validationMessage, this, &MainDevMgr::onValidationMessage);
  // 任何文件内容变化后，立即重验其它已打开文件；未打开文件通过防抖合并后的扫描刷新。
  // 当前文件由自身的防抖验证处理，这里跳过它避免重复校验。
  connect(editor, &QPlainTextEdit::textChanged, this, [this, editor]() {
    // 合并同一事件循环内的多次触发：打开文件/程序化批量变更可能一次连发多个
    // textChanged（如整篇行高格式批量应用），直接同步重验会造成 N×M 次全量验证
    // 风暴卡死界面。改用 0ms 单次定时器：对单次编辑无感知延迟，仅把同一 burst
    // 合并为一次重验，仍即时刷新其它已打开文件。
    m_revalidateSource = editor;
    if (m_revalidateTimer) m_revalidateTimer->start();
    // 扫描请求统一进防抖定时器：与保存/重命名触发共用，合并为一次，不会重复全量扫描
    if (m_scanTimer) m_scanTimer->start();
  });
  // 跨文件跳转信号
  connect(editor, &CodeEditor::requestGoToLine, this, &MainDevMgr::onGoToLine);
  // 即将导航信号（用于记录历史）
  connect(editor, &CodeEditor::aboutToNavigate, this, &MainDevMgr::onAboutToNavigate);
  // 跨文件查找引用（VSCode 风格：结果在「引用」面板展示，并自动切换到引用 tab）。
  // 语义收集（作用域 + 类型推断）在后台完成，完成后 referencesReady → 编辑器语义高亮
  connect(editor, &CodeEditor::requestFindReferencesAll, this,
          [this](const QString &filePath, int line, int column, const QString &name) {
            if (name.isEmpty()) return;
            // 先清掉上一轮引用的编辑器高亮，避免新查找期间旧颜色残留
            m_referenceRefs.clear();
            applyReferenceRefsToEditors();
            m_ui->referencePanel()->findReferences(filePath, line, column, name);
            if (m_ui->leftTabs()) m_ui->leftTabs()->setCurrentWidget(m_ui->referencePanel());
          });
  // F2 语义级重命名（AC/TPL）：作用域 + 跨文件 import
  connect(editor, &CodeEditor::requestRenameSymbol, this, &MainDevMgr::onRenameSymbol);
  // 工作区符号搜索 (Ctrl+T)
  connect(editor, &CodeEditor::requestWorkspaceSymbols, this, [this]() {
    QString query = AuiInputDialog::getText(m_ui, QStringLiteral("工作区符号搜索"),
                                            QStringLiteral("输入符号名:"));
    if (query.isEmpty()) return;

    m_ui->clearOutput();
    m_ui->appendOutput(QStringLiteral("符号搜索: ") + query, false);
    int totalFound = 0;
    for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
      auto *tabs = m_ui->editorPanelAt(pi);
      if (!tabs) continue;
      for (int ti = 0; ti < tabs->count(); ++ti) {
        auto *ed = qobject_cast<CodeEditor *>(tabs->widget(ti));
        if (!ed) continue;
        QFileInfo fi(ed->objectName());
        // 搜索当前编辑器的符号表
        QRegularExpression re(QStringLiteral("\\b") + QRegularExpression::escape(query),
                              QRegularExpression::CaseInsensitiveOption);
        const QString &text = ed->cachedText();
        QStringList lines = text.split(QLatin1Char('\n'));
        for (int i = 0; i < lines.size(); ++i) {
          if (re.match(lines[i]).hasMatch()) {
            m_ui->appendOutput(QStringLiteral("  %1:%2 → %3")
                                   .arg(fi.fileName())
                                   .arg(i + 1)
                                   .arg(lines[i].trimmed()),
                               false);
            ++totalFound;
          }
        }
      }
    }
    m_ui->appendOutput(QStringLiteral("共 %1 处匹配").arg(totalFound), false);
  });
  // 调试快捷键：F5 启动/继续、F10 单步执行、F11 单步进入、Shift+F11 单步跳出
  connect(editor, &CodeEditor::requestDebugStart, debugController(),
          &DebugController::startOrContinue);
  connect(editor, &CodeEditor::requestDebugStepOver, debugController(),
          &DebugController::stepOver);
  connect(editor, &CodeEditor::requestDebugStepInto, debugController(),
          &DebugController::stepInto);
  connect(editor, &CodeEditor::requestDebugStepOut, debugController(), &DebugController::stepOut);
}

void MainDevMgr::setActiveEditor(CodeEditor *editor) {
  if (m_model->connectedEditor == editor) {
    // 活跃编辑器未变化：无需重复刷新（避免焦点在控件间移动时反复全文重扫）
    return;
  }
  m_model->connectedEditor = editor;
  if (editor) {
    updateCursorPosition();
    // 刷新让编辑器由隐藏变为可见时，其光标括号配对高亮立即出现：
    // appendCursorContextHighlights 仅在 isVisible() 时才扫描，隐藏期间高亮是陈旧的；
    // 这里在切 tab/获焦（setActiveEditor 的两种触发路径）时强制重绘一次（仅当前编辑器，开销小）。
    editor->refreshExtraSelections();
  } else {
    m_ui->setCursorStatusText(MainDevUi::cursorDefault());
  }
}

// ──────────────────────────────────────────────────────────────
//  焦点切换
// ──────────────────────────────────────────────────────────────

void MainDevMgr::onFocusChanged(QWidget * /*oldFocus*/, QWidget *newFocus) {
  if (!newFocus) return;

  QWidget *w = newFocus;
  CodeEditor *foundEditor = nullptr;
  QString jsonVuePath;  // 焦点位于 .jsonvue/.jsonsource 可视化编辑器内部控件时对应的文件路径

  while (w) {
    if (auto *tabs = qobject_cast<QTabWidget *>(w)) {
      // 仅记录编辑器面板，避免把左侧「文件/调试」tab 误当作活跃编辑器
      if (m_ui->editorPanelIndex(tabs) >= 0) m_model->lastActivePanel = tabs;
    }
    if (!foundEditor) {
      if (auto *editor = qobject_cast<CodeEditor *>(w)) foundEditor = editor;
    }
    // 可视化编辑器（JsonVueEditor / JsonSourceEditor / SchemaFormEditor）内的控件获得焦点时，向上找到所属包装器
    if (jsonVuePath.isEmpty()) {
      if (auto *jvw = qobject_cast<JsonVueWidget *>(w)) {
        jsonVuePath = jvw->codeEditor()->objectName();
      } else if (auto *jdw = qobject_cast<JsonSourceWidget *>(w)) {
        jsonVuePath = jdw->codeEditor()->objectName();
      } else if (auto *sjw = qobject_cast<SchemaJsonWidget *>(w)) {
        jsonVuePath = sjw->codeEditor()->objectName();
      }
    }
    w = w->parentWidget();
  }

  QString filePath;
  if (foundEditor) {
    setActiveEditor(foundEditor);
    filePath = foundEditor->objectName();
  } else if (!jsonVuePath.isEmpty()) {
    filePath = jsonVuePath;
  }

  if (!filePath.isEmpty() && !m_restoringSession) {
    // 焦点切换到编辑器/可视化编辑器时，同步定位树形目录到当前文件
    // （处理拆分面板间切换、以及可视化编辑控件获得焦点的场景）
    m_ui->fileTree()->locateFile(filePath);
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

// ──────────────────────────────────────────────────────────────
//  光标位置 / 验证消息
// ──────────────────────────────────────────────────────────────

void MainDevMgr::updateCursorPosition() {
  // 连接是一次性的：cursorPositionChanged 来自任意编辑器；仅活跃编辑器更新状态栏
  if (auto *ed = qobject_cast<CodeEditor *>(sender())) {
    if (ed != m_model->connectedEditor) return;
  }
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
  // 错误文本已移至底部“问题”tab（由 onValidationIssues 填充），
  // 本槽仅维护文件树与标签栏的错误状态标记。
  // 连接是一次性的：用 sender() 标记发出校验结果的编辑器自身的文件/tab 错误状态
  auto *editor = qobject_cast<CodeEditor *>(sender());
  if (!editor) return;
  const QString filePath = editor->objectName();
  if (!filePath.isEmpty() && m_ui->fileTree()) {
    if (errorCount == 0) {
      m_ui->fileTree()->clearFileError(filePath);
    } else {
      m_ui->fileTree()->setFileError(filePath, errorCount);
    }
  }

  // 通知标签栏：该编辑器所在标签的错误状态（有错误则文字红色 + 波浪线，VSCode 风格）
  for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
    auto *tabs = m_ui->editorPanelAt(pi);
    if (!tabs) continue;
    auto *bar = qobject_cast<DraggableTabBar *>(tabs->tabBar());
    if (!bar) continue;
    for (int ti = 0; ti < tabs->count(); ++ti) {
      if (tabs->widget(ti) == editor || tabs->widget(ti) == editor->parentWidget()) {
        bar->setTabError(ti, errorCount > 0);
        break;
      }
    }
  }
}

/// 结构化验证结果：更新工作区问题聚合，防抖合并后重建「问题」面板
void MainDevMgr::onValidationIssues(const QString &filePath,
                                    const QVector<ValidationResult> &issues) {
  if (filePath.isEmpty()) return;
  if (issues.isEmpty())
    m_fileIssues.remove(filePath);
  else
    m_fileIssues[filePath] = issues;
  // 防抖合并：同一 burst 内多个编辑器验证结果只重建一次面板（避免逐个全量重建）
  if (m_problemPanelTimer) m_problemPanelTimer->start();
}

/// 从工作区聚合重建底部「问题」面板（跨文件汇总所有已打开文件的错误/警告）
void MainDevMgr::refreshProblemPanel() {
  QVector<IssueItem> all;
  for (auto it = m_fileIssues.begin(); it != m_fileIssues.end(); ++it) {
    for (const ValidationResult &r : it.value()) {
      IssueItem item;
      item.filePath = it.key();
      item.fileName = QFileInfo(it.key()).fileName();
      item.line = r.line;
      item.message = r.message;
      item.severity = r.severity;
      all.append(item);
    }
  }
  m_ui->problemPanel()->setIssues(all);
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
      navigateBack();
      return true;  // 事件已处理
    }
    if (mouseEvent->button() == Qt::XButton2) {
      navigateForward();
      return true;  // 事件已处理
    }
  }

  // 其他事件交给默认处理
  return QObject::eventFilter(obj, event);
}

// ──────────────────────────────────────────────────────────────
//  引用高亮（VSCode：引用面板可见时编辑器持续变色）
// ──────────────────────────────────────────────────────────────

/// 遍历所有已打开编辑器（含拆分面板）
static void forEachEditor(MainDevUi *ui,
                          const std::function<void(CodeEditor *)> &func) {
  for (int pi = 0; pi < ui->editorPanelCount(); ++pi) {
    auto *tabs = ui->editorPanelAt(pi);
    if (!tabs) continue;
    for (int ti = 0; ti < tabs->count(); ++ti) {
      auto *ed = qobject_cast<CodeEditor *>(tabs->widget(ti));
      if (ed) func(ed);
    }
  }
}

void MainDevMgr::applyReferenceRefsToEditors() {
  // 语义位置高亮：为空时等价于清除
  forEachEditor(m_ui, [this](CodeEditor *ed) { ed->setReferenceHighlightPositions(m_referenceRefs); });
}

void MainDevMgr::clearReferenceHighlightFromEditors() {
  forEachEditor(m_ui, [](CodeEditor *ed) { ed->highlightSymbolReferences(QString()); });
}

void MainDevMgr::applySearchHighlightToEditors(const QString &text) {
  if (text.isEmpty()) return;
  forEachEditor(m_ui, [&text](CodeEditor *ed) { ed->highlightSearchMatches(text); });
}

void MainDevMgr::clearSearchHighlightFromEditors() {
  forEachEditor(m_ui, [](CodeEditor *ed) { ed->highlightSearchMatches(QString()); });
}

/// 根据左侧面板当前选中项统一同步两类高亮（引用/查找互斥，VSCode 行为）：
/// - 引用面板可见 → 应用引用高亮，清除查找高亮
/// - 查找面板可见 → 应用查找高亮，清除引用高亮
/// - 其他面板 → 两类高亮都清除
void MainDevMgr::resyncPanelHighlights() {
  if (!m_ui->leftTabs()) {
    clearSearchHighlightFromEditors();
    clearReferenceHighlightFromEditors();
    return;
  }
  QWidget *current = m_ui->leftTabs()->currentWidget();
  if (current == m_ui->referencePanel()) {
    // 切回引用面板：恢复编辑器语义引用高亮（并清除查找高亮）
    clearSearchHighlightFromEditors();
    applyReferenceRefsToEditors();
  } else if (current == m_ui->findPanel()) {
    // 切回查找面板：若仍有搜索关键词，恢复编辑器查找高亮（并清除引用高亮）
    clearReferenceHighlightFromEditors();
    const QString text = m_ui->findPanel()->currentText();
    m_lastSearchText = text;
    if (!text.isEmpty()) applySearchHighlightToEditors(text);
  } else {
    // 离开查找/引用面板：清除两类编辑器高亮（与 VSCode 一致）
    clearSearchHighlightFromEditors();
    clearReferenceHighlightFromEditors();
  }
}

/// 依据左侧面板当前选中项，为单个编辑器应用对应高亮（新打开文件时调用）
void MainDevMgr::applyPanelHighlightToEditor(CodeEditor *editor) {
  if (!editor || !m_ui->leftTabs()) return;
  QWidget *left = m_ui->leftTabs()->currentWidget();
  if (left == m_ui->referencePanel()) {
    const QString sym = m_ui->referencePanel()->symbolName();
    if (!sym.isEmpty()) editor->highlightSymbolReferences(sym);
  } else if (left == m_ui->findPanel()) {
    const QString text = m_ui->findPanel()->currentText();
    if (!text.isEmpty()) editor->highlightSearchMatches(text);
  }
}

void MainDevMgr::onLeftTabChanged(int index) {
  Q_UNUSED(index);
  // 切页后延迟一帧同步高亮：resyncPanelHighlights 会遍历所有已打开编辑器并各自
  // 触发 highlightCurrentLine()，其中彩虹括号为全文 O(n) 扫描 + 强制重绘，多个
  // 编辑器时若放在 currentChanged 槽内同步执行会阻塞面板切换造成明显卡顿。
  // 改为异步：先让新面板立即显示，高亮刷新放到下个事件循环。
  QTimer::singleShot(0, this, [this]() { resyncPanelHighlights(); });
}
