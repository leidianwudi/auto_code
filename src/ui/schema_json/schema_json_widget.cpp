/**
 * @file schema_json_widget.cpp
 * @brief 通用 schema 驱动的 JSON 编辑器包装器实现
 */

#include "schema_json_widget.h"

#include <QComboBox>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QVBoxLayout>

#include "schema_form_editor.h"
#include "src/util/common/code_constants.h"
#include "src/util/common/path_resolver.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/setting_store.h"

// ──────────────────────────────────────────────────────────────
//  $schema 引用解析
// ──────────────────────────────────────────────────────────────

/// 从 JSON/JSON5 文本中解析 $schema 引用值；找不到返回空串。
/// 兼容 `"$schema": "path"`、`$schema: 'path'`、`$schema: path` 等写法（key 可带可不带引号）。
QString SchemaJsonWidget::extractSchemaRef(const QString &content) {
  QRegularExpression re(QStringLiteral("[\"']?\\$schema[\"']?\\s*:\\s*[\"']?([^\"'\\s,]+)"),
                        QRegularExpression::CaseInsensitiveOption);
  QRegularExpressionMatch m = re.match(content);
  return m.hasMatch() ? m.captured(1) : QString();
}

/// 解析 $schema 引用为绝对路径，统一走 PathResolver::resolveSchemaPath
/// （/ 开头 → 项目根 file 目录；相对路径 → json 文件所在目录）
QString SchemaJsonWidget::resolveSchemaPathFor(const QString &schemaRef) const {
  return PathResolver::resolveSchemaPath(m_filePath, schemaRef);
}

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

SchemaJsonWidget::SchemaJsonWidget(QWidget *parent) : CodeVisualSyncWidget(parent) {
  // 最外围细边框：与代码编辑器面板边界区隔，突出可视化表单区域
  reloadStyle();

  // 可视化页：顶部「模板」选择行（标签 + 可编辑下拉）+ 表单编辑器。
  // 代码页 index 0 由基类创建（与普通 .json 文件同一个 LightJson，保证高亮/主题一致）。
  // 空文件/无 $schema 也能进可视化：从下拉选定 .schema.json 后即可编辑
  m_visual = new SchemaFormEditor;
  auto *visualPage = new QWidget;
  auto *pageLay = new QVBoxLayout(visualPage);
  pageLay->setContentsMargins(0, 0, 0, 0);
  pageLay->setSpacing(0);

  auto *header = new QWidget(visualPage);
  auto *headerLay = new QHBoxLayout(header);
  headerLay->setContentsMargins(12, 8, 12, 0);
  auto *lbl = new QLabel(QStringLiteral("模板"), header);
  lbl->setProperty("muted", true);
  m_templateCombo = AuiComboBox::create(header);
  m_templateCombo->setEditable(true);
  m_templateCombo->setInsertPolicy(QComboBox::NoInsert);
  m_templateHint = new QLabel(header);
  m_templateHint->setProperty("muted", true);
  QFont hf = m_templateHint->font();
  hf.setPixelSize(12);
  m_templateHint->setFont(hf);
  headerLay->addWidget(lbl);
  headerLay->addWidget(m_templateCombo, 1);
  headerLay->addWidget(m_templateHint);
  pageLay->addWidget(header);
  pageLay->addWidget(m_visual, 1);
  addWidget(visualPage);

  setCurrentIndex(0);

  // 可视化编辑器内容变化时，写回代码编辑器并广播（基类统一入口）
  connect(m_visual, &SchemaFormEditor::contentChanged, this,
          &CodeVisualSyncWidget::onVisualContentChanged);

  // 下拉选定模板 → 应用（activated 仅用户点击触发，程序刷新不触发）
  connect(m_templateCombo, &QComboBox::activated, this, [this](int idx) {
    const QString ref = m_templateCombo->itemData(idx).toString();
    if (!ref.isEmpty() && ref != m_currentRef) applyTemplateRef(ref);
  });
  // 可编辑输入框手输路径 → 回车/失焦后应用（与当前值相同则忽略）
  connect(m_templateCombo->lineEdit(), &QLineEdit::editingFinished, this, [this]() {
    const QString ref = m_templateCombo->currentText().trimmed();
    if (!ref.isEmpty() && ref != m_currentRef) applyTemplateRef(ref);
  });
}

// ════════════════════════════════════════════════════════════
//  模板选择
// ════════════════════════════════════════════════════════════

void SchemaJsonWidget::reloadSchemaFor(const QString &ref) {
  // 引用未变且 schema 状态已初始化 → 跳过重载与表单重建（下拉框已显示该引用，
  // 也无需重新扫描候选）：反复切换代码/可视化不卡顿。
  // 例外：schema 文件在磁盘上被修改过（mtime 变化）→ 继续走重载，保持表单与 schema 同步
  if (m_schemaInit && ref == m_currentRef) {
    if (ref.isEmpty() || QFileInfo(resolveSchemaPathFor(ref)).lastModified() == m_schemaMtime)
      return;
  }
  m_schemaInit = true;
  m_currentRef = ref;
  m_schema = SchemaValidator();
  m_schemaMtime = QDateTime();
  if (!ref.isEmpty()) {
    SchemaValidator schema;
    const QString absPath = resolveSchemaPathFor(ref);
    if (schema.load(absPath) && schema.hasRoot()) {
      m_schema = schema;
      m_schemaMtime = QFileInfo(absPath).lastModified();
      setTemplateError(QString());
    } else {
      setTemplateError(QStringLiteral("schema 加载失败"));
    }
  }
  if (m_schema.hasRoot()) setTemplateError(QString());
  m_visual->setSchema(m_schema);
  refreshTemplateCombo(ref);
}

void SchemaJsonWidget::refreshTemplateCombo(const QString &currentRef) {
  const QSignalBlocker blocker(m_templateCombo);

  struct Cand {
    QString ref;  ///< 下拉显示的引用写法（相对 json 目录，或 / 开头相对项目根）
    QString abs;  ///< 绝对路径（去重用）
  };
  QVector<Cand> cands;

  // 1) json 文件所在目录（不递归，显示文件名）
  const QFileInfo fi(m_filePath);
  if (fi.absoluteDir().exists()) {
    const QString jsonDir = fi.absolutePath();
    QDir dir(jsonDir);
    const QStringList names = dir.entryList({"*.schema.json"}, QDir::Files, QDir::Name);
    for (const QString &name : names) {
      cands.push_back({name, QDir::cleanPath(jsonDir + QLatin1Char('/') + name)});
    }
  }

  // 2) 项目根 file 目录（递归），显示 /相对路径（与 $schema 的 / 前缀解析规则一致）
  const QString fileRoot = QStringLiteral(PROJECT_SOURCE_DIR) + CodeConstants::Paths::fileDir();
  if (QDir(fileRoot).exists()) {
    QDirIterator it(fileRoot, {"*.schema.json"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
      const QString abs = QDir::cleanPath(it.next());
      cands.push_back({QLatin1Char('/') + abs.mid(fileRoot.size() + 1), abs});
    }
  }

  m_templateCombo->clear();
  // 当前引用排最前并置为显示值，保证下拉框始终如实显示 $schema 内容
  QSet<QString> seen;
  if (!currentRef.isEmpty()) {
    m_templateCombo->addItem(currentRef, currentRef);
    seen.insert(QDir::cleanPath(resolveSchemaPathFor(currentRef)));
  }
  for (const Cand &c : cands) {
    if (seen.contains(c.abs)) continue;
    seen.insert(c.abs);
    m_templateCombo->addItem(c.ref, c.ref);
  }
  m_templateCombo->setCurrentText(currentRef);
}

void SchemaJsonWidget::applyTemplateRef(const QString &ref) {
  SchemaValidator schema;
  if (!schema.load(resolveSchemaPathFor(ref)) || !schema.hasRoot()) {
    setTemplateError(QStringLiteral("schema 加载失败"));
    refreshTemplateCombo(m_currentRef);  // 恢复显示原引用
    return;
  }

  m_currentRef = ref;
  m_schemaInit = true;
  m_schemaMtime = QFileInfo(resolveSchemaPathFor(ref)).lastModified();
  setTemplateError(QString());
  // 写回 $schema、空文档生成必填骨架并重建表单；contentChanged → 自动同步回代码编辑器
  m_visual->applySchema(schema, ref, true);
  refreshTemplateCombo(ref);
}

/// 模板行右侧提示文本：空串清除，非空以错误色显示
void SchemaJsonWidget::setTemplateError(const QString &text) {
  if (text.isEmpty()) {
    m_templateHint->clear();
    m_templateHint->setStyleSheet(QString());
    return;
  }
  m_templateHint->setText(text);
  m_templateHint->setStyleSheet(
      QStringLiteral("color: %1;")
          .arg(SettingStore::ins().color(QStringLiteral("ui.errorTextColor")).name()));
}

// ════════════════════════════════════════════════════════════
//  主题
// ════════════════════════════════════════════════════════════

void SchemaJsonWidget::reloadStyle() {
  // 边框色取当前主题的边框色，保证深浅主题下都可读（直角不加圆角）
  setStyleSheet(QStringLiteral("SchemaJsonWidget { border: 1px solid %1; }")
                    .arg(AuiStyle::borderColor().name()));
}

// ════════════════════════════════════════════════════════════
//  数据同步（基类骨架回调）
// ════════════════════════════════════════════════════════════

QWidget *SchemaJsonWidget::visualView() const { return m_visual; }

void SchemaJsonWidget::syncCodeToVisualImpl() {
  QByteArray data = m_editor->toPlainText().toUtf8();
  QJsonParseError err;
  // 用支持 JSON5 的解析器：文件可能是无引号键/单引号/注释/尾随逗号的 JSON5，
  // 严格 QJsonDocument::fromJson 会解析失败导致可视化空白。
  QJsonDocument doc = UtilJson::fromJson(data, &err);
  QJsonObject root;
  if (err.error == QJsonParseError::NoError && doc.isObject()) root = doc.object();
  m_visual->loadJson(root);
  // 重新解析 $schema 引用并加载 schema：代码里手改 $schema 后切可视化自动跟随；
  // 解析失败（半编辑状态）时用正则从原文兜底提取引用
  QString ref = root.value(QStringLiteral("$schema")).toString();
  if (ref.isEmpty()) ref = extractSchemaRef(m_editor->toPlainText());
  reloadSchemaFor(ref);
}

void SchemaJsonWidget::syncVisualToCodeImpl() {
  QByteArray data = QJsonDocument(m_visual->mergedObject()).toJson(QJsonDocument::Indented);
  const QString text = data.isEmpty() ? QStringLiteral("{}") : QString::fromUtf8(data);
  setPlainTextIfChanged(text);
}
