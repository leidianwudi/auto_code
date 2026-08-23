/**
 * @file json_source_widget.cpp
 * @brief .jsonsource 编辑器包装器实现
 */

#include "json_source_widget.h"

#include "json_source_editor.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/highlighter/light_json.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

JsonSourceWidget::JsonSourceWidget(QWidget *parent) : QStackedWidget(parent) {
  m_editor = new CodeEditor;
  auto *hl = new LightJson(m_editor->document());
  m_editor->setSyntaxHighlighter(hl);
  m_editor->setValidationMode(CodeEditor::JsonValidation);
  addWidget(m_editor);

  m_visual = new JsonSourceEditor;
  addWidget(m_visual);

  setCurrentIndex(0);

  // 可视化编辑器配置变化时，写回代码编辑器
  connect(m_visual, &JsonSourceEditor::configChanged, this, [this]() {
    if (m_syncing) return;
    syncVisualToCode();
    emit contentChanged();
  });
}

// ════════════════════════════════════════════════════════════
//  模式切换
// ════════════════════════════════════════════════════════════

void JsonSourceWidget::focusActiveView() {
  if (isVisualMode()) {
    m_visual->setFocus();
  } else {
    m_editor->setFocus();
  }
}

void JsonSourceWidget::switchToCode() {
  if (currentIndex() == 0) return;
  syncVisualToCode();
  setCurrentIndex(0);
  emit modeChanged(false);
}

void JsonSourceWidget::switchToVisual() {
  if (currentIndex() == 1) return;
  syncCodeToVisual();
  setCurrentIndex(1);
  emit modeChanged(true);
}

void JsonSourceWidget::toggleMode() {
  if (isVisualMode()) {
    switchToCode();
  } else {
    switchToVisual();
  }
}

// ════════════════════════════════════════════════════════════
//  数据同步
// ════════════════════════════════════════════════════════════

void JsonSourceWidget::syncCodeToVisual() {
  m_syncing = true;
  QString jsonStr = m_editor->toPlainText();
  // 以当前代码内容刷新保真底：用户可能在代码视图删除过自定义键，
  // 若沿用打开文件时的旧快照，写回时会把这些已删除的键"复活"
  m_visual->setPreservedSource(jsonStr);
  JsonSourceConfig config = JsonSourceConfig::fromJsonString(jsonStr);
  m_visual->loadConfig(config);
  m_syncing = false;
}

void JsonSourceWidget::syncVisualToCode() {
  m_syncing = true;
  QString jsonStr = JsonSourceConfig::toJsonString(m_visual->collectMergedObject());
  m_editor->setPlainText(jsonStr);
  m_syncing = false;
}

void JsonSourceWidget::setPreservedSource(const QString &src) { m_visual->setPreservedSource(src); }

void JsonSourceWidget::setHttpConfig(const QString &baseUrl, const QString &authHeader,
                                   const QString &postData) {
  m_visual->setHttpConfig(baseUrl, authHeader, postData);
}
