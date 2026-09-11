/**
 * @file json_vue_widget.cpp
 * @brief .jsonvue 编辑器包装器实现
 */

#include "json_vue_widget.h"

#include <QJsonObject>

#include "json_vue_editor.h"
#include "json_vue_model.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/code/code_editor.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

JsonVueWidget::JsonVueWidget(QWidget *parent) : CodeVisualSyncWidget(parent) {
  // 可视化编辑器（基类已创建代码页 index 0，高亮器与普通 .json 一致）
  m_visual = new JsonVueEditor;
  addWidget(m_visual);

  // 默认显示代码编辑器；打开文件时由 MainDevMgr 依据可视化开关决定
  // （switchToVisual() 会先同步内容再切换，避免可视化视图未填充）
  setCurrentIndex(0);

  // 可视化编辑器配置变化时，写回代码编辑器并广播（基类统一入口）
  connect(m_visual, &JsonVueEditor::configChanged, this,
          &CodeVisualSyncWidget::onVisualContentChanged);
}

// ════════════════════════════════════════════════════════════
//  数据同步（基类骨架回调）
// ════════════════════════════════════════════════════════════

QWidget *JsonVueWidget::visualView() const { return m_visual; }

void JsonVueWidget::syncCodeToVisualImpl() {
  QString jsonStr = m_editor->toPlainText();
  QString error;
  JsonVueConfig config = JsonVueConfig::fromJsonString(jsonStr, &error);
  // 内容与可视化页当前一致时跳过整表重建：反复切换/来回点击时不卡顿
  const QByteArray hash = UtilJson::fingerprint(config.toJsonObject());
  if (hash != m_lastVisualHash) {
    m_visual->loadConfig(config);
    m_lastVisualHash = hash;
  }
}

void JsonVueWidget::syncVisualToCodeImpl() {
  // 以界面配置为主、磁盘原文为底做保真合并，再序列化写回代码编辑器：
  // 避免可视化未表达的字段被丢弃 / 界面未加载时把整份配置清空
  QJsonObject merged = m_visual->collectMergedObject();
  QString jsonStr = JsonVueConfig::toJsonString(merged);
  setPlainTextIfChanged(jsonStr);
  // 同步可视化页内容指纹（下次切回时据此跳过重载）
  m_lastVisualHash = UtilJson::fingerprint(merged);
}

// ════════════════════════════════════════════════════════════
//  透传接口
// ════════════════════════════════════════════════════════════

void JsonVueWidget::setBaseUrl(const QString &baseUrl) { m_visual->setBaseUrl(baseUrl); }

void JsonVueWidget::setSourceFilePath(const QString &path) { m_visual->setJsonVueFilePath(path); }

void JsonVueWidget::setPreservedSource(const QString &src) { m_visual->setPreservedSource(src); }

void JsonVueWidget::loadHttpConfigFromAcFile(const QString &acFilePath) {
  m_visual->loadHttpConfigFromAcFile(acFilePath);
}

QString JsonVueWidget::findNearestApiAuthDataAc(const QString &jsonvueFilePath) {
  return JsonVueEditor::findNearestApiAuthDataAc(jsonvueFilePath);
}
