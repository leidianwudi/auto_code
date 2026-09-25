/**
 * @file code_visual_sync_widget.cpp
 * @brief 代码/可视化双视图编辑器同步骨架基类实现
 */

#include "code_visual_sync_widget.h"

#include <QTimer>

#include "code_editor.h"
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
  syncCodeToVisual();
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

void CodeVisualSyncWidget::syncCodeToVisual() {
  m_syncing = true;
  syncCodeToVisualImpl();
  m_syncing = false;
}

void CodeVisualSyncWidget::syncVisualToCode() {
  // 仅当处于可视化模式时才把可视化数据写回代码编辑器：
  // 代码模式下用户可能直接改过代码，此时代码是权威来源，覆盖会导致修改丢失
  if (!isVisualMode()) return;
  m_syncing = true;
  syncVisualToCodeImpl();
  m_syncing = false;
}

void CodeVisualSyncWidget::onVisualContentChanged() {
  if (m_syncing) return;
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
      if (m_syncing) return;
      syncVisualToCode();
      emit contentChanged();
    });
  }
  m_syncDebounceTimer->start();
}

void CodeVisualSyncWidget::setPlainTextIfChanged(const QString &text) {
  if (text != m_editor->toPlainText()) m_editor->setPlainText(text);
}
