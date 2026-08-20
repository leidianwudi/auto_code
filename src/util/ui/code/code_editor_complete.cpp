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
#include "src/engine/ac_language.h"
#include "src/engine/script/ac_symbol_table.h"
#include "guess_code.h"

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
  // 大小写不敏感，与 showCompleter 的过滤规则保持一致：
  // 若这里用 CaseSensitive，而 showCompleter 用 CaseInsensitive 过滤，
  // 大小写不一致的项会在 QCompleter 二次过滤时被丢弃，导致补全弹窗内容不对/为空
  m_completer->setCaseSensitivity(Qt::CaseInsensitive);
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

  // 构建补全列表
  QStringList completions;
  if (m_validationMode == JsonValidation && m_schemaLoaded) {
    // JSON + schema：按 schema 提示属性名 / 枚举值
    completions = m_schema.completions(cachedText(), pos);
  } else if (m_validationMode == AcValidation) {
    // AC：关键字 + 内置函数 + 符号表符号 + 词法级容错提取。
    // 符号表仅在语法分析成功时填充（敲代码时编译不过 → 符号表为空），
    // 关键字/内置函数固定存在保证始终有提示；再用词法分析（不依赖完整语法，
    // 容忍语法错误）提取用户已定义的变量/函数/类，避免"编译不过就补不出自己的符号"
    const QString &text = cachedText();
    for (const QString &kw : AcKeyword::kAll) completions.append(kw);
    for (const QString &fn : AcBuiltin::kAll) completions.append(fn + QStringLiteral("()"));
    for (const QString &v : GuessCode::extractLetVariables(text)) completions.append(v);
    for (const QString &f : GuessCode::extractUserFunctions(text)) completions.append(f);
    for (const QString &c : GuessCode::extractUserClasses(text)) completions.append(c);
    for (auto it = m_symbolTable.begin(); it != m_symbolTable.end(); ++it) {
      completions.append(it.key());
    }
  } else {
    // 其它模式：从符号表提示
    for (auto it = m_symbolTable.begin(); it != m_symbolTable.end(); ++it) {
      completions.append(it.key());
    }
  }

  // 按已输入前缀过滤并去重
  QStringList filtered;
  for (const QString &c : completions) {
    if (c.startsWith(identifier, Qt::CaseInsensitive) && !filtered.contains(c)) {
      filtered.append(c);
    }
  }

  if (!m_completer || filtered.isEmpty()) {
    if (m_completer && m_completer->popup()) {
      m_completer->popup()->hide();
    }
    return;
  }

  auto *model = qobject_cast<QStringListModel *>(m_completer->model());
  if (model) {
    model->setStringList(filtered);
    model->sort(0);
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
