/**
 * @file json_vue_widget.cpp
 * @brief .jsonvue 编辑器包装器实现
 */

#include "json_vue_widget.h"

#include <QFile>
#include <QTextStream>

#include "json_vue_editor.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/highlighter/light_json.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

JsonVueWidget::JsonVueWidget(QWidget *parent) : QStackedWidget(parent) {
  // 代码编辑器（高亮器通过 setSyntaxHighlighter 注册，主题切换时同步刷新，
  // 与普通 .json 文件使用同一个 LightJson，保证文字颜色一致）
  m_editor = new CodeEditor;
  auto *hl = new LightJson(m_editor->document());
  m_editor->setSyntaxHighlighter(hl);
  m_editor->setValidationMode(CodeEditor::JsonValidation);
  addWidget(m_editor);

  // 可视化编辑器
  m_visual = new JsonVueEditor;
  addWidget(m_visual);

  // 默认显示代码编辑器；打开文件时由 MainDevMgr 依据可视化开关决定
  // （switchToVisual() 会先同步内容再切换，避免可视化视图未填充）
  setCurrentIndex(0);

  // 可视化编辑器配置变化时，写回代码编辑器
  connect(m_visual, &JsonVueEditor::configChanged, this, [this]() {
    if (m_syncing) return;
    syncVisualToCode();
    emit contentChanged();
  });
}

// ════════════════════════════════════════════════════════════
//  模式切换
// ════════════════════════════════════════════════════════════

void JsonVueWidget::switchToCode() {
  if (currentIndex() == 0) return;
  // 从可视化切到代码：把可视化配置写回代码
  syncVisualToCode();
  setCurrentIndex(0);
  emit modeChanged(false);
}

void JsonVueWidget::switchToVisual() {
  if (currentIndex() == 1) return;
  // 从代码切到可视化：解析代码内容加载到可视化
  syncCodeToVisual();
  setCurrentIndex(1);
  emit modeChanged(true);
}

void JsonVueWidget::toggleMode() {
  if (isVisualMode()) {
    switchToCode();
  } else {
    switchToVisual();
  }
}

// ════════════════════════════════════════════════════════════
//  数据同步
// ════════════════════════════════════════════════════════════

void JsonVueWidget::syncCodeToVisual() {
  m_syncing = true;
  QString jsonStr = m_editor->toPlainText();
  QString error;
  JsonVueConfig config = JsonVueConfig::fromJsonString(jsonStr, &error);
  m_visual->loadConfig(config);
  m_syncing = false;
}

void JsonVueWidget::syncVisualToCode() {
  m_syncing = true;
  JsonVueConfig config = m_visual->collectConfig();
  QString jsonStr = config.toJsonString();
  m_editor->setPlainText(jsonStr);
  m_syncing = false;
}

void JsonVueWidget::setBaseUrl(const QString &baseUrl) { m_visual->setBaseUrl(baseUrl); }

void JsonVueWidget::loadHttpConfigFromAcFile(const QString &acFilePath) {
  m_visual->loadHttpConfigFromAcFile(acFilePath);
}
