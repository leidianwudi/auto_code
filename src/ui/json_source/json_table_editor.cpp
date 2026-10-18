/**
 * @file json_table_editor.cpp
 * @brief .jsontable 可视化编辑器面板实现（内嵌表设计器）
 */

#include "json_table_editor.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QVBoxLayout>

#include "json_table_designer.h"
#include "src/util/common/util_json.h"

// ════════════════════════════════════════════════════════════
//  构造 / 界面构建
// ════════════════════════════════════════════════════════════

JsonTableEditor::JsonTableEditor(QWidget *parent) : QWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_designer = new JsonTableDesigner(JsonTableTable(), JsonTableConfig(), QString(), this);
  layout->addWidget(m_designer);

  // 设计器逐编辑广播 → 写回代码侧（防抖由 CodeVisualSyncWidget 基类统一处理）
  connect(m_designer, &JsonTableDesigner::configChanged, this, [this]() {
    if (!m_loading) emit configChanged();
  });
}

// ════════════════════════════════════════════════════════════
//  项目上下文（proj meta + 跨文件表清单）
// ════════════════════════════════════════════════════════════

void JsonTableEditor::ensureProjectContext() {
  if (m_ctxBuilt) return;
  m_ctxBuilt = true;
  if (m_filePath.isEmpty()) return;
  const QString dir = QFileInfo(m_filePath).absolutePath();
  const QString proj = findJsontableProjPath(dir);
  if (!proj.isEmpty()) {
    m_ctx = jsontableProjFromFile(proj);  // 读取失败返回空配置（表编辑不受影响）
  }
  m_ctx.tables = listProjectTables(dir);  // 联表下拉/交叉校验的跨文件目标
}

// ════════════════════════════════════════════════════════════
//  加载 / 收集配置
// ════════════════════════════════════════════════════════════

void JsonTableEditor::loadConfig(const JsonTableTable &table) {
  m_loading = true;
  m_hasTable = !table.tableName.isEmpty() || !table.columns.isEmpty();
  ensureProjectContext();
  m_designer->load(table, m_ctx, m_filePath);
  m_loading = false;
}

JsonTableTable JsonTableEditor::collectTable() const { return m_designer->collectTable(); }

void JsonTableEditor::setPreservedSource(const QString &src) {
  if (src.trimmed().isEmpty()) {
    m_preserved = QJsonObject();
    return;
  }
  QJsonParseError perr;
  QJsonDocument doc = UtilJson::fromJson(src, &perr);
  if (perr.error == QJsonParseError::NoError && doc.isObject()) {
    m_preserved = doc.object();
  } else {
    m_preserved = QJsonObject();
  }
}

QJsonObject JsonTableEditor::collectMergedObject() const {
  // 旧多表格式保护：未解析出表（tables 键存在）时写回磁盘原文，
  // 避免可视化端以空表覆盖旧文件（生成端对旧格式有明确报错指路拆分脚本）
  if (!m_hasTable && m_preserved.contains(QString::fromLatin1(JsonTableKey::kTables))) {
    return m_preserved;
  }

  QJsonObject root;
  root[QString::fromLatin1(JsonTableKey::kTable)] = m_designer->collectTable().toJsonObject();

  if (m_preserved.isEmpty()) return root;

  // 顶层保留其它未知键（一文件一表后旧 meta/tables 键不写回，避免新格式被污染）
  for (auto it = m_preserved.begin(); it != m_preserved.end(); ++it) {
    if (it.key() == QString::fromLatin1(JsonTableKey::kTable)) continue;
    if (it.key() == QString::fromLatin1(JsonTableKey::kMeta)) continue;
    if (it.key() == QString::fromLatin1(JsonTableKey::kTables)) continue;
    if (!root.contains(it.key())) root.insert(it.key(), it.value());
  }
  return root;
}
