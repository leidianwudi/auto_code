/**
 * @file schema_json_widget.cpp
 * @brief 通用 schema 驱动的 JSON 编辑器包装器实现
 */

#include "schema_json_widget.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include "schema_form_editor.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/highlighter/light_json.h"

SchemaJsonWidget::SchemaJsonWidget(QWidget *parent) : QStackedWidget(parent) {
  // 最外围细边框：与代码编辑器面板边界区隔，突出可视化表单区域
  reloadStyle();

  // 代码编辑器（与普通 .json 文件同一个 LightJson，保证高亮/主题一致）
  m_editor = new CodeEditor;
  auto *hl = new LightJson(m_editor->document());
  m_editor->setSyntaxHighlighter(hl);
  m_editor->setValidationMode(CodeEditor::JsonValidation);
  addWidget(m_editor);

  // 可视化（表单）编辑器
  m_visual = new SchemaFormEditor;
  addWidget(m_visual);

  setCurrentIndex(0);

  // 可视化编辑器内容变化时，写回代码编辑器
  connect(m_visual, &SchemaFormEditor::contentChanged, this, [this]() {
    if (m_syncing) return;
    syncVisualToCode();
    emit contentChanged();
  });
}

void SchemaJsonWidget::focusActiveView() {
  if (isVisualMode()) {
    m_visual->setFocus();
  } else {
    m_editor->setFocus();
  }
}

void SchemaJsonWidget::switchToCode() {
  if (currentIndex() == 0) return;
  syncVisualToCode();
  setCurrentIndex(0);
  emit modeChanged(false);
}

void SchemaJsonWidget::switchToVisual() {
  if (currentIndex() == 1) return;
  syncCodeToVisual();
  setCurrentIndex(1);
  emit modeChanged(true);
}

void SchemaJsonWidget::toggleMode() {
  if (isVisualMode()) {
    switchToCode();
  } else {
    switchToVisual();
  }
}

void SchemaJsonWidget::setSchema(const SchemaValidator &schema) {
  m_visual->setSchema(schema);
}

void SchemaJsonWidget::reloadStyle() {
  // 边框色取当前主题的边框色，保证深浅主题下都可读（直角不加圆角）
  setStyleSheet(QStringLiteral("SchemaJsonWidget { border: 1px solid %1; }")
                    .arg(AuiStyle::borderColor().name()));
}

void SchemaJsonWidget::syncCodeToVisual() {
  m_syncing = true;
  QByteArray data = m_editor->toPlainText().toUtf8();
  QJsonParseError err;
  // 用支持 JSON5 的解析器：文件可能是无引号键/单引号/注释/尾随逗号的 JSON5，
  // 严格 QJsonDocument::fromJson 会解析失败导致可视化空白。
  QJsonDocument doc = UtilJson::fromJson(data, &err);
  if (err.error == QJsonParseError::NoError && doc.isObject()) {
    m_visual->loadJson(doc.object());
  } else {
    // 代码为非法/非对象 JSON：清空表单，避免误用旧数据
    m_visual->loadJson(QJsonObject());
  }
  m_syncing = false;
}

void SchemaJsonWidget::syncVisualToCode() {
  // 仅当当前处于可视化模式时才把表单数据写回代码编辑器：
  // 代码模式下用户可能直接改过代码，此时代码是权威来源，覆盖会导致修改丢失。
  if (!isVisualMode()) return;
  m_syncing = true;
  QByteArray data =
      QJsonDocument(m_visual->mergedObject()).toJson(QJsonDocument::Indented);
  if (data.isEmpty()) data = "{}";
  m_editor->setPlainText(QString::fromUtf8(data));
  m_syncing = false;
}