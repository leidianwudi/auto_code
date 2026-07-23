/**
 * @file code_editor_complete.cpp
 * @brief 代码编辑器补全功能实现（从 code_editor.cpp 拆分）
 *
 * 包含以下功能：
 * - 补全器初始化（initCompleter）
 * - 补全列表显示（showCompleter）
 * - 补全项插入（insertCompletion）
 */

#include <QAbstractItemView>
#include <QScrollBar>
#include <QStringListModel>

#include "code_editor.h"
#include "src/engine/script/ac_symbol_table.h"

// ──────────────────────────────────────────────────────────────
//  代码补全
// ──────────────────────────────────────────────────────────────

void CodeEditor::initCompleter(ValidationMode mode) {
  // 销毁旧补全器
  if (m_completer) {
    disconnect(m_completer, nullptr, this, nullptr);
    delete m_completer;
    m_completer = nullptr;
  }

  if (mode == NoValidation) return;

  m_completer = new QCompleter(this);
  m_completer->setWidget(this);
  m_completer->setCompletionMode(QCompleter::PopupCompletion);
  m_completer->setCaseSensitivity(Qt::CaseSensitive);
  // 必须设置 model，否则 model() 返回 nullptr 会导致崩溃
  m_completer->setModel(new QStringListModel(m_completer));

  connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated), this,
          &CodeEditor::insertCompletion);
}

void CodeEditor::showCompleter() {
  QTextCursor cursor = textCursor();
  int pos = cursor.position();
  int idStart = 0, idEnd = 0;
  QString identifier = identifierAtCursor(pos, &idStart, &idEnd);

  if (identifier.size() < 1) {
    m_completer->popup()->hide();
    return;
  }

  // 构建补全列表（从符号表）
  QStringList completions;
  for (auto it = m_symbolTable.begin(); it != m_symbolTable.end(); ++it) {
    if (it.key().startsWith(identifier, Qt::CaseInsensitive)) {
      completions.append(it.key());
    }
  }

  if (!m_completer || completions.isEmpty()) {
    if (m_completer && m_completer->popup()) {
      m_completer->popup()->hide();
    }
    return;
  }

  if (m_completer->model()) {
    m_completer->model()->sort(0);
  }
  m_completer->setCompletionPrefix(identifier);

  // 定位补全弹窗到光标位置
  QRect cr = cursorRect();
  cr.setWidth(m_completer->popup()->sizeHintForColumn(0) +
              m_completer->popup()->verticalScrollBar()->sizeHint().width());
  m_completer->complete(cr);
}

void CodeEditor::insertCompletion(const QString &completion) {
  if (!m_completer || m_completer->widget() != this) return;

  QTextCursor cursor = textCursor();
  int extra = completion.size() - m_completer->completionPrefix().size();
  cursor.movePosition(QTextCursor::Left);
  cursor.movePosition(QTextCursor::EndOfWord);
  cursor.insertText(completion.right(extra));
  setTextCursor(cursor);
}
