/**
 * @file code_visual_sync_widget.cpp
 * @brief 代码/可视化双视图编辑器同步骨架基类实现
 */

#include "code_visual_sync_widget.h"

#include <QScopedValueRollback>
#include <QTimer>

#include "code_editor.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/highlighter/light_json.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

CodeVisualSyncWidget::CodeVisualSyncWidget(QWidget *parent) : QStackedWidget(parent) {
  // 代码页固定为 index 0：与普通 .json 文件同一个 LightJson，保证高亮/主题一致
  m_editor = new CodeEditor;
  auto *hl = new LightJson(m_editor->document());
  m_editor->setSyntaxHighlighter(hl);
  m_editor->setValidationMode(CodeEditor::JsonValidation);
  addWidget(m_editor);
}

// ════════════════════════════════════════════════════════════
//  模式切换
// ════════════════════════════════════════════════════════════

void CodeVisualSyncWidget::focusActiveView() {
  // 可视化模式下代码编辑器页隐藏，setFocus 不会生效；必须聚焦可视化视图本身，
  // 才能触发主窗口的 onFocusChanged 完成面板激活
  if (isVisualMode()) {
    if (auto *v = visualView()) v->setFocus();
  } else {
    m_editor->setFocus();
  }
}

void CodeVisualSyncWidget::switchToCode() {
  if (currentIndex() == 0) return;
  syncVisualToCode();
  setCurrentIndex(0);
  emit modeChanged(false);
}

void CodeVisualSyncWidget::switchToVisual() {
  if (currentIndex() == 1) return;
  if (!syncCodeToVisual()) return;
  setCurrentIndex(1);
  emit modeChanged(true);
}

void CodeVisualSyncWidget::toggleMode() {
  if (isVisualMode()) {
    switchToCode();
  } else {
    switchToVisual();
  }
}

// ════════════════════════════════════════════════════════════
//  数据同步
// ════════════════════════════════════════════════════════════

bool CodeVisualSyncWidget::validateVisualSource(QString *error) const {
  return UtilJson::parseObject(m_editor->toPlainText(), nullptr, error, true);
}

bool CodeVisualSyncWidget::syncCodeToVisual() {
  if (m_syncDebounceTimer) m_syncDebounceTimer->stop();
  m_visualReady = false;
  if (!validateVisualSource(&m_syncError)) {
    emit synchronizationFailed(m_syncError);
    return false;
  }
  QScopedValueRollback<bool> syncing(m_syncing, true);
  syncCodeToVisualImpl();
  // 用回填后的收集结果作基线，避免控件补默认值或格式化导致首次交互误标修改。
  m_visualBaseline = visualFingerprint();
  m_visualReady = true;
  return true;
}

bool CodeVisualSyncWidget::syncVisualToCode() {
  if (m_syncDebounceTimer) m_syncDebounceTimer->stop();
  if (!isVisualMode() || !m_visualReady) return false;
  // 无实际编辑时不重新序列化：仅排版、键顺序和默认值差异不应改动源文件。
  if (visualFingerprint() == m_visualBaseline) return false;
  const QString previousText = m_editor->toPlainText();
  QScopedValueRollback<bool> syncing(m_syncing, true);
  syncVisualToCodeImpl();
  m_visualBaseline = visualFingerprint();
  return m_editor->toPlainText() != previousText;
}

void CodeVisualSyncWidget::onVisualContentChanged() {
  if (m_syncing || !isVisualMode() || !m_visualReady) return;
  if (visualFingerprint() == m_visualBaseline) {
    // 防抖期间改回原值，或点击/失焦只发出同值通知：撤销尚未发生的写回。
    if (m_syncDebounceTimer) m_syncDebounceTimer->stop();
    return;
  }
  // 防抖 300ms：可视化侧连续输入（每字符触发）时，全量序列化 + setPlainText
  // 全文重建 + 全文重高亮的开销是每键 O(文档)，停顿后合并为一次写回。
  // 顺序必须保持「先写回、后广播 contentChanged」：
  //   setPlainText 会把文档 modified 重置为 false，若先广播（置修改标记）后写回，
  //   标记会被清掉——用户看到"未保存"假象（黄点消失/保存按钮回弹），且
  //   此时 Ctrl+S 可能因文档未标记修改而丢失修改。
  //   切模式/保存路径直接调 syncVisualToCode() 即时 flush，不受防抖影响。
  if (!m_syncDebounceTimer) {
    m_syncDebounceTimer = new QTimer(this);
    m_syncDebounceTimer->setSingleShot(true);
    m_syncDebounceTimer->setInterval(300);
    connect(m_syncDebounceTimer, &QTimer::timeout, this, [this]() {
      if (m_syncing || !isVisualMode() || !m_visualReady) return;
      if (syncVisualToCode()) emit contentChanged();
    });
  }
  m_syncDebounceTimer->start();
}

void CodeVisualSyncWidget::setPlainTextIfChanged(const QString &text) {
  if (text != m_editor->toPlainText()) {
    m_editor->setPlainText(text);
    // 可视化写回产生的格式化文本可能与磁盘原文（如压缩单行 JSON）形态不同：
    // setPlainText 会重置修改标记，需重新置位——否则"看着变了却提示未修改"，
    // 规范化结果永远无法持久化，文件形态在单行/多行间反复摇摆
    m_editor->document()->setModified(true);
  }
}
