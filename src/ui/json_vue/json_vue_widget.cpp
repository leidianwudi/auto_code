/**
 * @file json_vue_widget.cpp
 * @brief .jsonvue 编辑器包装器实现
 */

#include "json_vue_widget.h"

#include <QFile>
#include <QJsonDocument>
#include <QTextStream>

#include "json_vue_editor.h"
#include "src/util/common/util_json.h"
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

void JsonVueWidget::focusActiveView() {
  // 聚焦当前显示的页：可视化模式下代码编辑器页隐藏，setFocus 不会生效，
  // 必须聚焦可视化编辑器本身，才能触发主窗口的 onFocusChanged 完成面板激活
  if (isVisualMode()) {
    m_visual->setFocus();
  } else {
    m_editor->setFocus();
  }
}

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
  // 内容与可视化页当前一致时跳过整表重建：反复切换/来回点击时不卡顿
  const QByteArray hash = UtilJson::fingerprint(config.toJsonObject());
  if (hash != m_lastVisualHash) {
    m_visual->loadConfig(config);
    m_lastVisualHash = hash;
  }
  m_syncing = false;
}

void JsonVueWidget::syncVisualToCode() {
  m_syncing = true;
  // 以界面配置为主、磁盘原文为底做保真合并，再序列化写回代码编辑器：
  // 避免可视化未表达的字段被丢弃 / 界面未加载时把整份配置清空
  QJsonObject merged = m_visual->collectMergedObject();
  QString jsonStr = JsonVueConfig::toJsonString(merged);
  // 内容未变时不重设文本：避免无意义的 document 变更（误标修改、触发重排）
  if (jsonStr != m_editor->toPlainText()) m_editor->setPlainText(jsonStr);
  // 同步可视化页内容指纹（下次切回时据此跳过重载）
  m_lastVisualHash = UtilJson::fingerprint(merged);
  m_syncing = false;
}

void JsonVueWidget::setBaseUrl(const QString &baseUrl) { m_visual->setBaseUrl(baseUrl); }

void JsonVueWidget::setSourceFilePath(const QString &path) { m_visual->setJsonVueFilePath(path); }

void JsonVueWidget::setPreservedSource(const QString &src) { m_visual->setPreservedSource(src); }

void JsonVueWidget::loadHttpConfigFromAcFile(const QString &acFilePath) {
  m_visual->loadHttpConfigFromAcFile(acFilePath);
}

QString JsonVueWidget::findNearestApiAuthDataAc(const QString &jsonvueFilePath) {
  return JsonVueEditor::findNearestApiAuthDataAc(jsonvueFilePath);
}
