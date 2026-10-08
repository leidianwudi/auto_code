/**
 * @file json_table_widget.cpp
 * @brief .jsontable 编辑器包装器实现
 */

#include "json_table_widget.h"

#include "json_table_editor.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/code/format_code.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

JsonTableWidget::JsonTableWidget(QWidget *parent) : CodeVisualSyncWidget(parent) {
  // 可视化编辑器（基类已创建代码页 index 0，高亮器与普通 .json 一致）
  m_visual = new JsonTableEditor;
  addWidget(m_visual);

  setCurrentIndex(0);

  // 可视化编辑器配置变化时，写回代码编辑器并广播（基类统一入口）
  connect(m_visual, &JsonTableEditor::configChanged, this,
          &CodeVisualSyncWidget::onVisualContentChanged);
}

// ════════════════════════════════════════════════════════════
//  数据同步（基类骨架回调）
// ════════════════════════════════════════════════════════════

QWidget *JsonTableWidget::visualView() const { return m_visual; }

void JsonTableWidget::syncCodeToVisualImpl() {
  QString jsonStr = m_editor->toPlainText();
  // 以当前代码内容刷新保真底：用户可能在代码视图删除过自定义键，
  // 若沿用打开文件时的旧快照，写回时会把这些已删除的键"复活"
  m_visual->setPreservedSource(jsonStr);
  JsonTableConfig config = JsonTableConfig::fromJsonString(jsonStr);
  // 内容与可视化页一致时跳过整表重建：反复切换/来回点击时不卡顿（同 jsonvue）
  const QByteArray hash = UtilJson::fingerprint(config.toJsonObject());
  if (hash != m_lastVisualHash) {
    m_visual->loadConfig(config);
    m_lastVisualHash = hash;
  }
}

void JsonTableWidget::syncVisualToCodeImpl() {
  // 序列化后格式化（JSON5 风格，与右键"格式化代码"一致）再写回：
  // 紧凑单行 JSON 会让代码视图退化成一条 7KB 长行——缩进竖线错位、
  // 高亮/校验/括号匹配对超长块全量重算导致切换卡顿
  const QString jsonStr = FormatCode::format(
      JsonTableConfig::toJsonString(m_visual->collectMergedObject()), FormatCode::FormatJson5);
  setPlainTextIfChanged(jsonStr);
  m_lastVisualHash = UtilJson::fingerprint(m_visual->collectMergedObject());
}

// ════════════════════════════════════════════════════════════
//  透传接口
// ════════════════════════════════════════════════════════════

void JsonTableWidget::setPreservedSource(const QString &src) { m_visual->setPreservedSource(src); }
