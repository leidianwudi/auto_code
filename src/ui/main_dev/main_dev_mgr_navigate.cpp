/**
 * @file main_dev_mgr_navigate.cpp
 * @brief 导航历史功能实现（MainDevMgr 的跨文件跳转与导航历史方法）
 */

#include "main_dev_mgr.h"

#include <QDebug>
#include <QTextBlock>
#include <QTextCursor>

#include "src/util/ui/code/code_editor.h"

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
