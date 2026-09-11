/**
 * @file json_source_widget.cpp
 * @brief .jsonsource 编辑器包装器实现
 */

#include "json_source_widget.h"

#include "json_source_editor.h"
#include "src/util/ui/code/code_editor.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

JsonSourceWidget::JsonSourceWidget(QWidget *parent) : CodeVisualSyncWidget(parent) {
  // 可视化编辑器（基类已创建代码页 index 0，高亮器与普通 .json 一致）
  m_visual = new JsonSourceEditor;
  addWidget(m_visual);

  setCurrentIndex(0);

  // 可视化编辑器配置变化时，写回代码编辑器并广播（基类统一入口）
  connect(m_visual, &JsonSourceEditor::configChanged, this,
          &CodeVisualSyncWidget::onVisualContentChanged);
}

// ════════════════════════════════════════════════════════════
//  数据同步（基类骨架回调）
// ════════════════════════════════════════════════════════════

QWidget *JsonSourceWidget::visualView() const { return m_visual; }

void JsonSourceWidget::syncCodeToVisualImpl() {
  QString jsonStr = m_editor->toPlainText();
  // 以当前代码内容刷新保真底：用户可能在代码视图删除过自定义键，
  // 若沿用打开文件时的旧快照，写回时会把这些已删除的键"复活"
  m_visual->setPreservedSource(jsonStr);
  JsonSourceConfig config = JsonSourceConfig::fromJsonString(jsonStr);
  m_visual->loadConfig(config);
}

void JsonSourceWidget::syncVisualToCodeImpl() {
  setPlainTextIfChanged(JsonSourceConfig::toJsonString(m_visual->collectMergedObject()));
}

// ════════════════════════════════════════════════════════════
//  透传接口
// ════════════════════════════════════════════════════════════

void JsonSourceWidget::setPreservedSource(const QString &src) { m_visual->setPreservedSource(src); }

void JsonSourceWidget::setHttpConfig(const QString &baseUrl, const QString &authHeader,
                                     const QString &postData) {
  m_visual->setHttpConfig(baseUrl, authHeader, postData);
}
