/**
 * @file main_dev_mgr_connect.cpp
 * @brief 编辑器信号连接实现（MainDevMgr 的编辑器信号、焦点、验证、事件过滤）
 */

#include <QApplication>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QTextCursor>

#include "main_dev_mgr.h"
#include "main_dev_model.h"
#include "main_dev_ui.h"
#include "src/ui/json_vue/json_vue_widget.h"
#include "src/util/ui/code/code_editor.h"

// ──────────────────────────────────────────────────────────────
//  编辑器信号连接
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
    disconnect(m_model->connectedEditor, &CodeEditor::requestFindReferencesAll, this, nullptr);
    disconnect(m_model->connectedEditor, &CodeEditor::requestWorkspaceSymbols, this, nullptr);
    disconnect(m_model->connectedEditor, &CodeEditor::requestDebugStart, this, nullptr);
    disconnect(m_model->connectedEditor, &CodeEditor::requestDebugStepOver, this, nullptr);
    disconnect(m_model->connectedEditor, &CodeEditor::requestDebugStepInto, this, nullptr);
    disconnect(m_model->connectedEditor, &CodeEditor::requestDebugStepOut, this, nullptr);
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
    // 跨文件查找引用
    connect(editor, &CodeEditor::requestFindReferencesAll, this, [this](const QString &name) {
      m_ui->clearOutput();
      m_ui->appendOutput(QStringLiteral("查找引用: ") + name, false);
      int totalRefs = 0;
      for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
        auto *tabs = m_ui->editorPanelAt(pi);
        if (!tabs) continue;
        for (int ti = 0; ti < tabs->count(); ++ti) {
          auto *ed = qobject_cast<CodeEditor *>(tabs->widget(ti));
          if (!ed) continue;
          QFileInfo fi(ed->objectName());
          auto refs = ed->findSymbolReferences(name);
          for (const auto &ref : refs) {
            m_ui->appendOutput(QStringLiteral("  %1:%2 → %3")
                                   .arg(fi.fileName())
                                   .arg(ref.first)
                                   .arg(ref.second.trimmed()),
                               false);
            ++totalRefs;
          }
        }
      }
      m_ui->appendOutput(QStringLiteral("共 %1 处引用").arg(totalRefs), false);
    });
    // 工作区符号搜索 (Ctrl+T)
    connect(editor, &CodeEditor::requestWorkspaceSymbols, this, [this]() {
      bool ok = false;
      QString query =
          QInputDialog::getText(m_ui, QStringLiteral("工作区符号搜索"),
                                QStringLiteral("输入符号名:"), QLineEdit::Normal, QString(), &ok);
      if (!ok || query.isEmpty()) return;

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
    // 调试快捷键：F5 启动/继续、F10 单步跳过、F11 单步进入、Shift+F11 单步跳出
    connect(editor, &CodeEditor::requestDebugStart, this, &MainDevMgr::onDebugStart);
    connect(editor, &CodeEditor::requestDebugStepOver, this, &MainDevMgr::onDebugStepOver);
    connect(editor, &CodeEditor::requestDebugStepInto, this, &MainDevMgr::onDebugStepInto);
    connect(editor, &CodeEditor::requestDebugStepOut, this, &MainDevMgr::onDebugStepOut);
    updateCursorPosition();
    editor->validate();
  } else {
    m_ui->setCursorStatusText(MainDevUi::cursorDefault());
    m_ui->setErrorMessage(QString());
  }
}

// ──────────────────────────────────────────────────────────────
//  焦点切换
// ──────────────────────────────────────────────────────────────

void MainDevMgr::onFocusChanged(QWidget * /*oldFocus*/, QWidget *newFocus) {
  if (!newFocus) return;

  QWidget *w = newFocus;
  CodeEditor *foundEditor = nullptr;

  while (w) {
    if (auto *tabs = qobject_cast<QTabWidget *>(w)) {
      // 仅记录编辑器面板，避免把左侧「文件/调试」tab 误当作活跃编辑器
      if (m_ui->editorPanelIndex(tabs) >= 0) m_model->lastActivePanel = tabs;
    }
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

// ──────────────────────────────────────────────────────────────
//  光标位置 / 验证消息
// ──────────────────────────────────────────────────────────────

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
