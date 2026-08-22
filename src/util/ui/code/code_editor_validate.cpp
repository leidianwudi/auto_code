/**
 * @file code_editor_validate.cpp
 * @brief 代码编辑器语法校验与格式化实现（从 code_editor.cpp 拆分）
 *
 * 包含以下功能：
 * - 验证模式设置（setValidationMode）与防抖验证调度（scheduleValidation / performValidation）
 * - 统一 IValidator 验证（validateWithValidator / applyValidationResults）
 * - JSON $schema 结构校验（runSchemaValidation）
 * - 代码格式化（formatCode）
 */

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

#include "code_editor.h"
#include "src/engine/json_validator.h"
#include "src/engine/script/ac_validator.h"
#include "src/engine/tpl/tpl_validator.h"
#include "src/util/common/code_constants.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/code/format_code.h"

// ──────────────────────────────────────────────────────────────
//  验证模式与防抖调度
// ──────────────────────────────────────────────────────────────

void CodeEditor::setValidationMode(ValidationMode mode) {
  m_validationMode = mode;
  // 切换模式后立即执行一次验证
  performValidation();
  // 初始化对应的代码补全器
  initCompleter(mode);
  // 折叠区间依赖验证模式（NoValidation 不生成）；模式确定后立即重建，避免图标缺失
  if (m_foldValid) m_foldValid = false;
  QTimer::singleShot(0, this, &CodeEditor::rebuildFold);
}

void CodeEditor::validate() { performValidation(); }

void CodeEditor::scheduleValidation() {
  if (m_validationMode == NoValidation) return;
  m_validationTimer->start();
}

void CodeEditor::performValidation() {
  if (m_validationMode == NoValidation) return;

  // 清除旧的错误标记
  m_errorLines.clear();
  m_errorRanges.clear();  // 清除错误位置范围（供 paintEvent 使用）

  // 根据验证模式创建对应的验证器，使用统一的 IValidator 接口
  switch (m_validationMode) {
    case JsonValidation: {
      // 语法校验
      JsonValidator validator;
      QVector<ValidationResult> results = validator.validate(cachedText());
      // 语法通过后，再按 $schema 做结构校验
      QJsonParseError perr;
      QJsonDocument doc = UtilJson::fromJson(cachedText(), &perr);
      if (perr.error == QJsonParseError::NoError && doc.isObject()) {
        runSchemaValidation(doc.object(), results);
      }
      applyValidationResults(results);
      break;
    }
    case TemplateValidation: {
      TplValidator validator;
      validateWithValidator(&validator);
      break;
    }
    case AcValidation: {
      AcValidator validator;
      validator.setFilePath(objectName());  // 设置文件路径用于解析 import
      validateWithValidator(&validator);
      // 验证后提取符号表数据，同步到导航器
      m_symbolTable = validator.symbolTable().allSymbols();
      m_symbolNavigator.setSymbolTable(m_symbolTable);
      break;
    }
    default:
      break;
  }

  // 刷新 ExtraSelections（重新绘制行高亮 + 括号匹配 + 错误标记）
  refreshExtraSelections();

  // 刷新行号区域（错误行号显示红色）
  m_lineNumberArea->update();

  // 强制触发一次完整重绘（确保自定义波浪线立即完整显示/清除）
  // 不带参数的 update() 会标记整个控件为脏区域；
  // 额外刷新视口，确保自定义波浪线的旧像素（含越界残留）被彻底覆盖
  update();
  viewport()->update();
}

// ──────────────────────────────────────────────────────────────
//  统一验证（使用 IValidator 接口）
// ──────────────────────────────────────────────────────────────

void CodeEditor::validateWithValidator(IValidator *validator) {
  const QString &text = cachedText();
  if (text.trimmed().isEmpty()) return;

  QVector<ValidationResult> results = validator->validate(text);
  applyValidationResults(results);
}

// 统一应用验证结果：标记错误 + 发出消息
void CodeEditor::applyValidationResults(const QVector<ValidationResult> &results) {
  // 收集错误信息字符串
  QStringList errors;
  for (const auto &result : results) {
    QString msg = result.message;
    if (result.line > 0) {
      msg += QStringLiteral(" at line %1").arg(result.line);
    }
    errors << msg;

    // 将错误位置标记为红色波浪下划线
    // 定位到文档中的对应行
    QTextBlock block = document()->findBlockByNumber(result.line - 1);
    if (block.isValid()) {
      int blockPos = block.position();
      int blockTextLen = block.length() - 1;  // 行文本长度（不含行尾换行符）
      int startOffset = qBound(0, result.column > 0 ? result.column - 1 : 0, blockTextLen);
      // 无明确长度时默认标记到行尾；有明确长度时按其值，但都钳制在行内，
      // 避免范围越过行尾把波浪线画到下一行（错误改正后下一行残留成红点）
      int length = result.length > 0 ? result.length : blockTextLen - startOffset;
      // 空行（blockTextLen==0）时长度为 0，不绘制；有内容时至少标记 1 个字符。
      // 关键：范围绝不越过行尾，否则空行/短行错误会把波浪线画到下一行（“有时画了 2 行”）
      int maxLen = blockTextLen - startOffset;
      length = qBound(maxLen > 0 ? 1 : 0, length, maxLen);

      // 统一记录错误范围（paintEvent 单行绘制，绝不跨行）
      int startPos = (result.column > 0) ? (blockPos + startOffset) : blockPos;
      m_errorRanges.append(ErrorRange(startPos, length, result.message));
      m_errorLines.insert(result.line);
    }
  }

  // 发出验证结果信号
  if (errors.isEmpty()) {
    emit validationMessage(QString(), 0);
  } else {
    emit validationMessage(errors.join(QLatin1Char('\n')), errors.size());
  }

  // 发出结构化验证结果（供底部“问题”面板展示与双击跳转）
  emit validationIssues(objectName(), results);
}

// ──────────────────────────────────────────────────────────────
//  JSON Schema 校验（$schema 字段驱动）
// ──────────────────────────────────────────────────────────────

namespace {
/// 从错误消息中提取出错路径，如 "'tables.0.modelName' is required" → "tables.0.modelName"
QString extractSchemaPath(const QString &msg) {
  int s = msg.indexOf(QLatin1Char('\''));
  int e = msg.indexOf(QLatin1Char('\''), s + 1);
  if (s < 0 || e < 0) return QString();
  return msg.mid(s + 1, e - s - 1);
}

/// 在 JSON5 文本中查找指定键（作为属性名）所在的行号；找不到返回 1
int findSchemaKeyLine(const QString &text, const QString &key) {
  if (key.isEmpty()) return 1;
  QRegularExpression re(QStringLiteral("(['\"]?)%1\\1\\s*:").arg(QRegularExpression::escape(key)));
  auto m = re.match(text);
  if (!m.hasMatch()) return 1;
  int matchPos = m.capturedStart();
  int line = 1;
  for (int i = 0; i < matchPos && i < text.size(); ++i) {
    if (text[i] == QLatin1Char('\n')) ++line;
  }
  return line;
}
}  // namespace

// 按 $schema 加载并校验，将错误追加到 results
void CodeEditor::runSchemaValidation(const QJsonObject &doc, QVector<ValidationResult> &results) {
  QString schemaRef = doc.value(QStringLiteral("$schema")).toString();
  if (schemaRef.isEmpty()) {
    m_schemaLoaded = false;
    return;
  }

  // 解析 schema 路径：
  //   - 以 / 开头 → 基于项目根目录（PROJECT_SOURCE_DIR/file），供所有 json 文件复制使用
  //   - 相对路径   → 基于当前文件所在目录
  QString schemaPath = schemaRef;
  if (schemaRef.startsWith(QLatin1Char('/'))) {
    schemaPath = QStringLiteral(PROJECT_SOURCE_DIR) +
                 QString::fromUtf8(CodeConstants::Paths::kFileDirName) + schemaRef;
  } else if (QFileInfo(schemaRef).isRelative()) {
    QFileInfo fi(objectName());
    schemaPath = fi.absolutePath() + QLatin1Char('/') + schemaRef;
  }
  schemaPath = QDir::cleanPath(schemaPath);

  // 缓存加载：路径变化才重新加载
  if (schemaPath != m_schemaPath || !m_schemaLoaded) {
    m_schemaPath = schemaPath;
    m_schemaLoaded = m_schema.load(schemaPath);
  }
  if (!m_schemaLoaded || !m_schema.hasRoot()) return;

  const QString &text = cachedText();
  const QVector<QString> errs = m_schema.validateDocument(doc);
  for (const QString &e : errs) {
    QString prop = extractSchemaPath(e).section(QLatin1Char('.'), -1);
    int line = findSchemaKeyLine(text, prop);
    results.append(ValidationResult(line, 1, 1, QStringLiteral("Schema: %1").arg(e)));
  }
}

// ──────────────────────────────────────────────────────────────
//  代码格式化
// ──────────────────────────────────────────────────────────────

void CodeEditor::formatCode() {
  const QString &src = cachedText();
  FormatCode::FormatMode mode;

  switch (m_validationMode) {
    case JsonValidation:
      mode = FormatCode::FormatJson5;
      break;
    case AcValidation:
      mode = FormatCode::FormatAc;
      break;
    case TemplateValidation:
      mode = FormatCode::FormatTpl;
      break;
    default:
      return;  // 不支持的模式，不做格式化
  }

  QString formatted = FormatCode::format(src, mode);
  if (formatted == src) return;

  // 替换文本，并保持光标位置
  int cursorPos = textCursor().position();
  selectAll();
  insertPlainText(formatted);

  // 恢复光标位置（不超过文档长度）
  QTextCursor c = textCursor();
  c.setPosition(qMin(cursorPos, document()->characterCount() - 1));
  setTextCursor(c);
}
