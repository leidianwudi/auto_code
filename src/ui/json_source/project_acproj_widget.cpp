/**
 * @file project_acproj_widget.cpp
 * @brief .acproj 编辑器包装器实现
 */

#include "project_acproj_widget.h"

#include <QJsonDocument>
#include <QJsonObject>

#include "project_config_editor.h"
#include "json_table_model.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/code/format_code.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

ProjectAcprojWidget::ProjectAcprojWidget(QWidget *parent) : CodeVisualSyncWidget(parent) {
  // 可视化编辑页（基类已创建代码页 index 0，高亮器与普通 .json 一致）
  m_visual = new ProjectConfigEditor;
  addWidget(m_visual);

  setCurrentIndex(0);

  // 表单任一字段变化时，写回代码编辑器并广播（基类统一入口，带 300ms 防抖）
  connect(m_visual, &ProjectConfigEditor::contentChanged, this,
          &CodeVisualSyncWidget::onVisualContentChanged);
}

// ════════════════════════════════════════════════════════════
//  数据同步（基类骨架回调）
// ════════════════════════════════════════════════════════════

QWidget *ProjectAcprojWidget::visualView() const { return m_visual; }

void ProjectAcprojWidget::syncCodeToVisualImpl() {
  const QString jsonStr = m_editor->toPlainText();
  // 从编辑器文本解析根对象（不依赖文件已保存）；
  // 解析失败（非法 JSON/空文件）时给表单空对象，由用户重新填写
  QJsonParseError perr;
  const QJsonDocument doc = UtilJson::fromJson(jsonStr, &perr);
  const QJsonObject root = (perr.error == QJsonParseError::NoError && doc.isObject())
                               ? doc.object()
                               : QJsonObject();
  // 内容与可视化页一致时跳过整页重建：反复切换/来回点击时不卡顿（同 jsontable）
  const QByteArray hash = UtilJson::fingerprint(root);
  if (hash != m_lastVisualHash) {
    m_visual->loadFromObject(root);
    m_lastVisualHash = hash;
  }
}

void ProjectAcprojWidget::syncVisualToCodeImpl() {
  // 序列化后格式化（JSON5 风格，与右键"格式化代码"一致）再写回，
  // 避免紧凑单行 JSON 造成代码视图高亮/校验卡顿
  QJsonObject root = m_visual->collectObject();
  const QString jsonStr =
      FormatCode::format(JsonTableConfig::toJsonString(root), FormatCode::FormatJson5);
  setPlainTextIfChanged(jsonStr);
  m_lastVisualHash = UtilJson::fingerprint(root);
}
