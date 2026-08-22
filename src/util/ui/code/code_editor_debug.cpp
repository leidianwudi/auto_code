/**
 * @file code_editor_debug.cpp
 * @brief 代码编辑器断点与调试功能实现（从 code_editor.cpp 拆分）
 *
 * 包含以下功能：
 * - 断点增删改查（toggleBreakpoint / setBreakpointEnabled / setBreakpoints ...）
 * - 调试暂停行与调试变量快照（setDebugLine / setDebugVariables ...）
 */

#include "code_editor.h"

#include "src/engine/ac_language.h"

// ── 断点调试接口 ──
bool CodeEditor::isDebuggableFile() const {
  return objectName().endsWith(AcFileSuffix::kAc, Qt::CaseInsensitive);
}

void CodeEditor::toggleBreakpoint(int blockNumber) {
  if (blockNumber < 0) return;
  // 仅 .ac 脚本支持断点调试；json/tpl 等数据文件不可调试，忽略点击
  if (!isDebuggableFile()) return;
  int line = blockNumber + 1;
  if (m_breakpoints.contains(line)) {
    m_breakpoints.remove(line);
  } else {
    m_breakpoints.insert(line, true);  // 新增断点默认生效
  }
  m_lineNumberArea->update();
  emit breakpointsChanged();
}

bool CodeEditor::hasBreakpoint(int line) const { return m_breakpoints.contains(line); }

bool CodeEditor::isBreakpointEnabled(int line) const {
  return m_breakpoints.contains(line) && m_breakpoints.value(line);
}

void CodeEditor::setBreakpointEnabled(int line, bool enabled) {
  if (!m_breakpoints.contains(line)) return;
  m_breakpoints[line] = enabled;
  m_lineNumberArea->update();
  emit breakpointsChanged();
}

QMap<int, bool> CodeEditor::breakpoints() const { return m_breakpoints; }

void CodeEditor::setBreakpoints(const QMap<int, bool> &lines) {
  m_breakpoints = lines;
  m_lineNumberArea->update();
}

void CodeEditor::clearBreakpoints() {
  m_breakpoints.clear();
  m_lineNumberArea->update();
}

void CodeEditor::setDebugLine(int line) {
  m_debugLine = line;
  highlightCurrentLine();
  if (line > 0) {
    // 将调试行滚动到可见区域
    QTextBlock block = document()->findBlockByNumber(line - 1);
    if (block.isValid()) {
      QTextCursor cursor(block);
      setTextCursor(cursor);
    }
  }
}

void CodeEditor::clearDebugLine() { setDebugLine(-1); }

void CodeEditor::setDebugVariables(const QList<AcDebugVar> &vars) { m_debugVars = vars; }

void CodeEditor::clearDebugVariables() { m_debugVars.clear(); }
