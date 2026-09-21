/**
 * @file main_dev_mgr_navigate.cpp
 * @brief 导航历史功能实现（MainDevMgr 的跨文件跳转与导航历史方法）
 */

#include <QDebug>
#include <QTextBlock>
#include <QTextCursor>

#include "main_dev_mgr.h"
#include "src/util/common/ac_log.h"
#include "src/util/ui/code/code_editor.h"

// ──────────────────────────────────────────────────────────────
//  跨文件跳转与导航历史
// ──────────────────────────────────────────────────────────────

void MainDevMgr::pushNavigationHistory(const QString &filePath, int line, int column) {
  NavigationEntry entry;
  entry.filePath = filePath;
  entry.line = line;
  entry.column = column;
  m_nav.push(entry);  // 导航中静默忽略 / 去重 / 清前进栈 / 上限裁剪（状态机内聚）
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

void MainDevMgr::onOpenHighlightResult(const QString &filePath, int line, int column, int length) {
  Q_UNUSED(length);
  CodeEditor *editor = openFileInEditor(filePath);
  if (!editor) return;

  QTextBlock block = editor->document()->findBlockByNumber(line - 1);
  if (!block.isValid()) return;

  // 定位到匹配处，但不选中匹配词：选中会呈蓝色选区，盖住查找/引用高亮的浅红色。
  // VSCode 点击结果后只移动光标，让高亮（浅红）直接可见。
  QTextCursor cursor(block);
  cursor.movePosition(QTextCursor::StartOfBlock);
  if (column > 0) cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);

  editor->setTextCursor(cursor);
  editor->ensureCursorVisible();
  editor->setFocus();
}

void MainDevMgr::onGoToLine(const QString &filePath, int line) {
  AC_LOG_INFO() << "onGoToLine() called:" << filePath << "line:" << line;

  // 注意：历史记录已在 onAboutToNavigate() 中保存，这里只需执行跳转
  jumpToLocation(filePath, line, 0);
}

void MainDevMgr::onAboutToNavigate(const QString &targetFilePath, int targetLine) {
  AC_LOG_INFO() << "onAboutToNavigate() called:" << targetFilePath << "targetLine:" << targetLine;

  // 记录当前位置到历史栈（用于后退）
  CodeEditor *current = currentEditor();
  if (current) {
    QTextCursor cursor = current->textCursor();
    int curLine = cursor.blockNumber() + 1;
    int curColumn = cursor.columnNumber() + 1;
    pushNavigationHistory(current->objectName(), curLine, curColumn);
    AC_LOG_INFO() << "Pushed to history:" << current->objectName() << "line:" << curLine;
  }
}

void MainDevMgr::navigateBack() {
  AC_LOG_INFO() << "navigateBack() called, history size:" << m_nav.backSize();
  if (!m_nav.canBack()) {
    AC_LOG_INFO() << "Navigation history is empty, cannot go back";
    return;
  }

  // 先弹出后退栈目标位置（必须在任何修改之前，防止引用失效）
  NavigationEntry entry = m_nav.popBack();
  AC_LOG_INFO() << "Navigating back to:" << entry.filePath << "line:" << entry.line;

  // 记录当前位置到前进栈
  CodeEditor *current = currentEditor();
  if (current) {
    QTextCursor cursor = current->textCursor();
    NavigationEntry forwardEntry;
    forwardEntry.filePath = current->objectName();
    forwardEntry.line = cursor.blockNumber() + 1;
    forwardEntry.column = cursor.columnNumber() + 1;
    m_nav.pushForward(forwardEntry);
    AC_LOG_INFO() << "Pushed to forward stack:" << forwardEntry.filePath
                  << "line:" << forwardEntry.line;
  }

  // 执行跳转
  m_nav.setNavigating(true);
  jumpToLocation(entry.filePath, entry.line, entry.column);
  m_nav.setNavigating(false);
}

void MainDevMgr::navigateForward() {
  AC_LOG_INFO() << "navigateForward() called, forward stack size:" << m_nav.forwardSize();
  if (!m_nav.canForward()) {
    AC_LOG_INFO() << "Forward stack is empty, cannot go forward";
    return;
  }

  // 先弹出前进栈目标位置（必须在 pushNavigationHistory 之前，因为后者会清空前进栈！）
  NavigationEntry entry = m_nav.popForward();
  AC_LOG_INFO() << "Navigating forward to:" << entry.filePath << "line:" << entry.line;

  // 记录当前位置到后退栈
  CodeEditor *current = currentEditor();
  if (current) {
    QTextCursor cursor = current->textCursor();
    pushNavigationHistory(current->objectName(), cursor.blockNumber() + 1,
                          cursor.columnNumber() + 1);
  }

  // 执行跳转
  m_nav.setNavigating(true);
  jumpToLocation(entry.filePath, entry.line, entry.column);
  m_nav.setNavigating(false);
}
