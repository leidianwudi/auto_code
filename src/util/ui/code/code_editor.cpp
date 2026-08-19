/**
 * @file code_editor.cpp
 * @brief 代码编辑器控件实现（重构后）
 */

#include "code_editor.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShortcut>
#include <QStringListModel>
#include <QToolTip>
#include <QVBoxLayout>

#include "code_find_bar.h"
#include "src/engine/ac_language.h"
#include "src/engine/json_validator.h"
#include "src/engine/script/ac_validator.h"
#include "src/engine/tpl/tpl_validator.h"
#include "src/util/common/code_constants.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/code/format_code.h"
#include "src/util/ui/component/aui_error_tool_tip.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/highlighter/light_ac.h"
#include "src/util/ui/highlighter/light_json.h"
#include "src/util/ui/highlighter/light_tpl.h"
#include "src/util/ui/highlighter/light_ts.h"
#include "src/util/ui/setting_store.h"

/**
 * @class FixedLineHeightLayout
 * @brief 统一行高的文档布局（消除中文输入导致的行高“抖动”）
 *
 * 背景：QPlainTextEdit 的块高度由 QTextLayout 按行内所有字体（含中文 fallback
 * 字体）的度量计算，且 QPlainTextDocumentLayout 忽略块级 BlockLineHeight。
 * 等宽字体（Consolas/Courier New 等）不含中文字形，输入中文时触发字体 fallback
 * （如微软雅黑），其 ascent/descent 与主字体不同，使行高随内容变化。
 *
 * 方案：覆写 blockBoundingRect 强制所有块等高（末块保留原生底部边距），
 * 实现 VSCode 式的统一行高，保证输入中文前后行高一致。
 */
class FixedLineHeightLayout : public QPlainTextDocumentLayout {
public:
  explicit FixedLineHeightLayout(QTextDocument *doc) : QPlainTextDocumentLayout(doc) {}

  /// 设置统一行高（非正数表示不启用）
  void setFixedLineHeight(qreal h) {
    if (qFuzzyCompare(m_h, h)) return;
    m_h = h;
    // 强制重新布局，使新行高立即生效
    document()->markContentsDirty(0, document()->characterCount());
  }

  QRectF blockBoundingRect(const QTextBlock &block) const override {
    QRectF r = QPlainTextDocumentLayout::blockBoundingRect(block);
    if (m_h > 0) {
      r.setHeight(m_h);
      if (!block.next().isValid())  // 末块保留底部边距（与原生行为一致）
        r.setHeight(m_h + document()->documentMargin());
    }
    return r;
  }

private:
  qreal m_h = 0;  ///< 统一行高（像素）
};

// ──────────────────────────────────────────────────────────────
//  构造与初始化（精简后）
// ──────────────────────────────────────────────────────────────

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent) {
  setMouseTracking(true);

  m_lineNumberArea = new LineNumberArea(this);

  connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
  connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);
  connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

  updateLineNumberAreaWidth(0);
  highlightCurrentLine();

  // 使用统一行高布局：修复等宽字体不含中文时，输入中文触发 fallback 导致的行高变化
  m_fixedLineHeightLayout = new FixedLineHeightLayout(document());
  document()->setDocumentLayout(m_fixedLineHeightLayout);

  // 使用常量配置
  applyFontFromSetting();

  // 字体大小变化时（设置界面修改 / 重置）即时刷新
  connect(&SettingStore::ins(), &SettingStore::fontsChanged, this,
          &CodeEditor::applyFontFromSetting);

  // 禁用自动换行，启用水平滚动条
  setLineWrapMode(QPlainTextEdit::NoWrap);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  setExtraSelections(QList<QTextEdit::ExtraSelection>());

  // 初始化悬停定时器
  m_hoverTimer = new QTimer(this);
  m_hoverTimer->setSingleShot(true);
  m_hoverTimer->setInterval(CodeConstants::Performance::kHoverDebounceMs);

  // 初始化验证定时器
  m_validationTimer = new QTimer(this);
  m_validationTimer->setSingleShot(true);
  m_validationTimer->setInterval(CodeConstants::Performance::kValidationDebounceMs);
  connect(m_validationTimer, &QTimer::timeout, this, &CodeEditor::performValidation);

  // 文本变化即触发验证：监听 QTextDocument::contentsChange，
  // 覆盖打字/删除/粘贴/撤销重做/IME 输入法/拖放等所有编辑方式。
  // 不再依赖 keyPressEvent 手动触发（避免个别输入路径漏触发导致“输入不提示”）；
  // 0ms 防抖保证输入后立即验证（实测验证 <1ms），同事件循环内多次变化合并为一次验证。
  connect(document(), &QTextDocument::contentsChange, this, &CodeEditor::scheduleValidation);

  // 初始化查找/替换栏（嵌入编辑器上方，默认隐藏）
  m_findBar = new CodeFindBar(this, this);
  connect(m_findBar, &CodeFindBar::findBarClosed, this, [this]() {
    // 查找栏关闭时恢复视口边距
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
  });
  connect(m_findBar, &CodeFindBar::layoutChanged, this, &CodeEditor::updateFindBarLayout);
}

CodeEditor::~CodeEditor() {
  // 析构时清理资源（如果有动态分配的对象）
}

// ──────────────────────────────────────────────────────────────
//  applyFontFromSetting — 从设置读取代码字体大小并应用
// ──────────────────────────────────────────────────────────────

void CodeEditor::applyFontFromSetting() {
  QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  // 字体族：跟随「代码字体」设置（未设置时用系统等宽字体）
  const QString fam = SettingStore::ins().fontFamily(QStringLiteral("font.code"));
  if (!fam.isEmpty()) font.setFamily(fam);
  font.setPointSize(SettingStore::ins().fontSize(QStringLiteral("font.code")));
  setFont(font);
  // 等宽字体变化后 Tab 宽度按新字体的空格宽度重新计算
  setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) *
                     CodeConstants::Editor::kTabWidthSpaces);
  // 字体变化后同步刷新统一行高
  updateFixedLineHeight(font);
}

// ──────────────────────────────────────────────────────────────
//  updateFixedLineHeight — 测量并设置统一行高
// ──────────────────────────────────────────────────────────────

void CodeEditor::updateFixedLineHeight(const QFont &font) {
  if (!m_fixedLineHeightLayout) return;
  // 用临时文档测量「拉丁+中文」混合行的自然行高（非末块，不含底部边距）。
  // 触发中文 fallback 的行是行内所有字体中度量最大者，作为统一行高可保证任何行不被裁剪。
  QTextDocument probe;
  probe.setDefaultFont(font);
  probe.setPlainText(QStringLiteral("a高\n"));  // 首行混合（非末块），避免末块底部边距
  auto *pl = new QPlainTextDocumentLayout(&probe);
  probe.setDocumentLayout(pl);  // probe 接管所有权
  pl->documentSize();           // 强制布局
  // 自然行高 + 额外间距，保证任何行不被裁剪且行不显得拥挤
  m_fixedLineHeightLayout->setFixedLineHeight(pl->blockBoundingRect(probe.firstBlock()).height() +
                                              CodeConstants::Editor::kLineHeightExtraSpacing);
}

// ──────────────────────────────────────────────────────────────
//  性能优化：文本缓存
// ──────────────────────────────────────────────────────────────

const QString &CodeEditor::cachedText() const {
  // 检查文档是否已更改（通过版本号检测）
  if (document()->revision() != m_cacheVersion) {
    m_cachedText = toPlainText();
    m_cacheVersion = document()->revision();
  }
  return m_cachedText;
}

void CodeEditor::setSyntaxHighlighter(QSyntaxHighlighter *h) { m_highlighter = h; }

void CodeEditor::reloadColors() {
  // 按高亮器具体类型刷新颜色（主题/自定义颜色变化时调用）
  if (auto *lj = dynamic_cast<LightJson *>(m_highlighter)) {
    lj->reloadColors();
  } else if (auto *la = dynamic_cast<LightAc *>(m_highlighter)) {
    la->reloadColors();
  } else if (auto *lt = dynamic_cast<LightTpl *>(m_highlighter)) {
    lt->reloadColors();
  } else if (auto *lts = dynamic_cast<LightTs *>(m_highlighter)) {
    lts->reloadColors();
  }
}

void CodeEditor::setValidationMode(ValidationMode mode) {
  m_validationMode = mode;
  // 切换模式后立即执行一次验证
  performValidation();
  // 初始化对应的代码补全器
  initCompleter(mode);
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
//  行号区域绘制
// ──────────────────────────────────────────────────────────────

void CodeEditor::updateLineNumberAreaWidth(int newBlockCount) {
  Q_UNUSED(newBlockCount);
  // 保留查找栏的顶部边距
  int topMargin =
      (m_findBar && m_findBar->isFindBarVisible()) ? m_findBar->sizeHint().height() + 4 : 0;
  setViewportMargins(lineNumberAreaWidth(), topMargin, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
  if (dy)
    m_lineNumberArea->scroll(0, dy);
  else
    m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

  if (rect.contains(viewport()->rect())) updateLineNumberAreaWidth(0);
}

int CodeEditor::lineNumberAreaWidth() const {
  int digits = 1;
  int max = qMax(1, blockCount());
  while (max >= 10) {
    max /= 10;
    ++digits;
  }

  // 加 12px 余量（左右各 6px）
  int space = 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
  return space;
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event, const QRect &area) {
  QPainter painter(m_lineNumberArea);
  painter.fillRect(event->rect(), AuiStyle::lineNumberBackground());

  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + qRound(blockBoundingRect(block).height());

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      int line = blockNumber + 1;
      QString number = QString::number(line);

      // 错误行号显示红色，否则用默认颜色
      if (m_errorLines.contains(line)) {
        painter.setPen(AuiStyle::errorTextColor());
      } else {
        painter.setPen(AuiStyle::lineNumberTextColor());
      }

      painter.drawText(0, top, m_lineNumberArea->width() - 6, fontMetrics().height(),
                       Qt::AlignRight, number);

      // 绘制断点圆点（生效=实心红圆，失效=空心红圆，位于行号左侧）
      if (m_breakpoints.contains(line)) {
        int dotR = 5;
        int cx = 5;
        int cy = top + fontMetrics().height() / 2;
        QColor red(0xf4, 0x47, 0x47);
        if (m_breakpoints.value(line)) {
          // 生效：实心红圆
          painter.setPen(Qt::NoPen);
          painter.setBrush(red);
          painter.drawEllipse(QPoint(cx, cy), dotR, dotR);
        } else {
          // 失效：空心红圆（仅描边）
          painter.setPen(QPen(red, 1.5));
          painter.setBrush(Qt::NoBrush);
          painter.drawEllipse(QPoint(cx, cy), dotR, dotR);
        }
      }
    }

    block = block.next();
    top = bottom;
    bottom = top + qRound(blockBoundingRect(block).height());
    ++blockNumber;
  }
}

// ── 行号区点击：切换断点 ──
void LineNumberArea::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_codeEditor->toggleBreakpointAtY(event->pos().y());
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

// ── 根据行号区 y 坐标返回所在行（1-based），无命中返回 0 ──
int CodeEditor::lineAtY(int y) const {
  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + qRound(blockBoundingRect(block).height());

  while (block.isValid()) {
    if (y >= top && y < bottom) return blockNumber + 1;
    block = block.next();
    top = bottom;
    bottom = top + qRound(blockBoundingRect(block).height());
    ++blockNumber;
  }
  return 0;
}

// ── 根据行号区 y 坐标切换断点 ──
void CodeEditor::toggleBreakpointAtY(int y) {
  const int line = lineAtY(y);
  if (line > 0) toggleBreakpoint(line - 1);
}

// ── 行号区右键：添加/移除/启用/禁用断点（模仿 VSCode）──
void LineNumberArea::contextMenuEvent(QContextMenuEvent *event) {
  // 仅 .ac 可调试文件支持断点操作
  if (!m_codeEditor->isDebuggableFile()) {
    event->ignore();
    return;
  }
  const int line = m_codeEditor->lineAtY(event->pos().y());
  if (line <= 0) {
    event->ignore();
    return;
  }
  const bool hasBp = m_codeEditor->hasBreakpoint(line);
  const bool bpEnabled = hasBp && m_codeEditor->isBreakpointEnabled(line);

  QMenu menu(this);
  QAction *toggleAction =
      menu.addAction(hasBp ? QString::fromUtf8(CodeConstants::UiText::kRemoveBreakpoint)
                           : QStringLiteral("添加断点"));
  connect(toggleAction, &QAction::triggered, this,
          [this, line]() { m_codeEditor->toggleBreakpoint(line - 1); });

  if (hasBp) {
    QAction *enableAction =
        menu.addAction(bpEnabled ? QString::fromUtf8(CodeConstants::UiText::kDisableBreakpoint)
                                 : QString::fromUtf8(CodeConstants::UiText::kEnableBreakpoint));
    connect(enableAction, &QAction::triggered, this,
            [this, line, bpEnabled]() { m_codeEditor->setBreakpointEnabled(line, !bpEnabled); });
  }

  if (!m_codeEditor->breakpoints().isEmpty()) {
    QAction *removeAllAction = menu.addAction(QStringLiteral("移除断点..."));
    connect(removeAllAction, &QAction::triggered, m_codeEditor, &CodeEditor::clearBreakpoints);
  }

  menu.exec(event->globalPos());
  event->accept();
}

// ── 断点调试接口 ──
bool CodeEditor::isDebuggableFile() const {
  return objectName().endsWith(AcFileSuffix::kAc, Qt::CaseInsensitive);
}

void CodeEditor::toggleBreakpoint(int blockNumber) {
  if (blockNumber < 0) return;
  // 仅 .ac 脚本支持断点调试；json/tpl 等数据文件不可调试，忽略点击
  if (!isDebuggableFile()) return;
  int line = blockNumber + 1;
  if (m_breakpoints.contains(line)) {
    m_breakpoints.remove(line);
  } else {
    m_breakpoints.insert(line, true);  // 新增断点默认生效
  }
  m_lineNumberArea->update();
  emit breakpointsChanged();
}

bool CodeEditor::hasBreakpoint(int line) const { return m_breakpoints.contains(line); }

bool CodeEditor::isBreakpointEnabled(int line) const {
  return m_breakpoints.contains(line) && m_breakpoints.value(line);
}

void CodeEditor::setBreakpointEnabled(int line, bool enabled) {
  if (!m_breakpoints.contains(line)) return;
  m_breakpoints[line] = enabled;
  m_lineNumberArea->update();
  emit breakpointsChanged();
}

QMap<int, bool> CodeEditor::breakpoints() const { return m_breakpoints; }

void CodeEditor::setBreakpoints(const QMap<int, bool> &lines) {
  m_breakpoints = lines;
  m_lineNumberArea->update();
}

void CodeEditor::clearBreakpoints() {
  m_breakpoints.clear();
  m_lineNumberArea->update();
}

void CodeEditor::setDebugLine(int line) {
  m_debugLine = line;
  highlightCurrentLine();
  if (line > 0) {
    // 将调试行滚动到可见区域
    QTextBlock block = document()->findBlockByNumber(line - 1);
    if (block.isValid()) {
      QTextCursor cursor(block);
      setTextCursor(cursor);
    }
  }
}

void CodeEditor::clearDebugLine() { setDebugLine(-1); }

void CodeEditor::setDebugVariables(const QList<AcDebugVar> &vars) { m_debugVars = vars; }

void CodeEditor::clearDebugVariables() { m_debugVars.clear(); }

// ──────────────────────────────────────────────────────────────
//  绘制与事件处理
// ──────────────────────────────────────────────────────────────

void CodeEditor::resizeEvent(QResizeEvent *event) {
  QPlainTextEdit::resizeEvent(event);

  QRect cr = contentsRect();
  // 行号区域从视口内容区顶部开始（跳过查找栏占用的顶部边距）
  int topMargin = viewportMargins().top();
  m_lineNumberArea->setGeometry(
      QRect(cr.left(), cr.top() + topMargin, lineNumberAreaWidth(), cr.height() - topMargin));

  // 查找栏定位：在视口顶部边距区域内，右对齐
  if (m_findBar && m_findBar->isVisible()) {
    int findBarH = m_findBar->sizeHint().height();
    int findBarW = qMin(m_findBar->sizeHint().width(), cr.width() - 10);
    m_findBar->setGeometry(cr.right() - findBarW - 5, cr.top() + topMargin - findBarH - 2, findBarW,
                           findBarH);
  }
}

void CodeEditor::paintEvent(QPaintEvent *event) {
  // 先调用标准绘制（背景、文本、默认波浪下划线等）
  QPlainTextEdit::paintEvent(event);

  // 绘制缩进参考线（在文本之下、错误波浪线之下）
  {
    int charWidth = fontMetrics().horizontalAdvance(QLatin1Char(' '));
    int tabW = tabStopDistance() / charWidth;
    if (tabW <= 0) tabW = 4;
    m_indentGuide.compute(cachedText(), document()->revision(), tabW);

    const auto &guideRanges = m_indentGuide.ranges();
    if (!guideRanges.isEmpty()) {
      QPainter guidePainter(viewport());

      // 可见行范围
      QTextBlock firstBlock = firstVisibleBlock();
      int firstLine = firstBlock.blockNumber() + 1;
      int lastLine = firstLine;
      {
        QTextBlock blk = firstBlock;
        while (blk.isValid()) {
          QRectF br = blockBoundingGeometry(blk).translated(contentOffset());
          if (br.top() > viewport()->rect().bottom()) break;
          lastLine = blk.blockNumber() + 1;
          blk = blk.next();
        }
      }

      // 当前行号和缩进
      int cursorLine = textCursor().blockNumber() + 1;
      int cursorIndent = IndentGuide::lineIndentLevel(textCursor().block().text(), tabW);

      // 光标矩形（视口坐标）：引导线在光标所在列让位，保证光标始终可见不被覆盖
      const QRect cr = cursorRect();
      const qreal caretX = cr.x() + cr.width() * 0.5;

      QColor normalColor = AuiStyle::indentGuideColor();
      QColor activeColor = AuiStyle::indentGuideActiveColor();

      for (const auto &range : guideRanges) {
        if (range.endLine < firstLine || range.startLine > lastLine) continue;

        int drawStart = qMax(range.startLine, firstLine);
        int drawEnd = qMin(range.endLine, lastLine);

        QTextBlock startBlk = document()->findBlockByNumber(drawStart - 1);
        QTextBlock endBlk = document()->findBlockByNumber(drawEnd - 1);
        if (!startBlk.isValid() || !endBlk.isValid()) continue;

        qreal y1 = blockBoundingGeometry(startBlk).translated(contentOffset()).top();
        qreal y2 = blockBoundingGeometry(endBlk).translated(contentOffset()).bottom();

        // 使用 QTextLayout::cursorToX 获取精确像素位置，避免 charWidth 估算误差；
        // 竖线相对缩进列左移 2 列（落在缩进空白处），避免紧贴/压住代码首字符
        qreal x = 0;
        const int guideCol = qMax(0, range.indent - 2);
        QTextLayout *layout = startBlk.layout();
        if (layout && layout->lineCount() > 0) {
          x = layout->lineAt(0).cursorToX(guideCol) + contentOffset().x();
        } else {
          x = guideCol * charWidth + contentOffset().x();
        }

        bool isActive = (cursorLine >= range.startLine && cursorLine <= range.endLine &&
                         cursorIndent >= range.indent);
        guidePainter.setPen(QPen(isActive ? activeColor : normalColor, 1, Qt::SolidLine));
        // 引导线与光标同列时，在光标所在行让位（断开一小段），避免覆盖光标
        if (qAbs(x - caretX) < 1.0 && cr.top() < y2 && cr.bottom() > y1) {
          if (y1 < cr.top()) guidePainter.drawLine(qRound(x), qRound(y1), qRound(x), cr.top());
          if (cr.bottom() < y2)
            guidePainter.drawLine(qRound(x), cr.bottom(), qRound(x), qRound(y2));
        } else {
          guidePainter.drawLine(qRound(x), qRound(y1), qRound(x), qRound(y2));
        }
      }
    }
  }

  // 统一绘制红色波浪线（唯一绘制机制：单行绘制，终点钳制在行尾，绝不跨行）
  if (m_errorRanges.isEmpty()) return;

  QPainter painter(viewport());
  painter.setRenderHint(QPainter::Antialiasing);

  const int viewH = viewport()->height();
  for (const auto &err : m_errorRanges) {
    // 取错误范围起点所在行的视口横坐标（cursorRect 会把整行错误缩成行尾小锯齿，不可用）
    QTextCursor startCursor(document());
    startCursor.setPosition(err.start);
    const QTextBlock blk = startCursor.block();
    if (!blk.isValid()) continue;
    const QRectF blkRect = blockBoundingGeometry(blk).translated(contentOffset());
    // 仅绘制可见行
    if (blkRect.bottom() < 0 || blkRect.top() > viewH) continue;

    // 错误终点：取 err 终点与该行行尾的较小者，确保绝不越界画到下一行
    const int lineEndPos = blk.position() + qMax(0, blk.length() - 1);
    QTextCursor endCursor(document());
    endCursor.setPosition(qBound(err.start, err.start + err.length, lineEndPos));
    const int x1 = cursorRect(startCursor).left();
    const int x2 = cursorRect(endCursor).right();
    // 与标签栏共用 VSCode 风格波浪线（样式/粗细统一）
    AuiStyle::drawErrorUnderline(painter, x1, x2, qRound(blkRect.bottom()) - 2,
                                 AuiStyle::errorUnderlineColor());
  }
}

bool CodeEditor::viewportEvent(QEvent *event) {
  // 处理鼠标悬停事件，显示错误提示或悬停符号提示
  if (event->type() == QEvent::ToolTip) {
    QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
    QTextCursor cursor = cursorForPosition(helpEvent->pos());
    int pos = cursor.position();

    // 检查是否在错误区域内
    for (const auto &err : m_errorRanges) {
      if (pos >= err.start && pos <= err.start + err.length) {
        showErrorTooltip(helpEvent->globalPos(), err.tooltip);
        event->accept();
        return true;
      }
    }

    // 不在错误区域，隐藏错误提示
    hideErrorTooltip();
  }
  return QPlainTextEdit::viewportEvent(event);
}

void CodeEditor::showErrorTooltip(const QPoint &pos, const QString &text) {
  if (text.isEmpty()) {
    hideErrorTooltip();
    return;
  }

  // 如果弹窗已存在，先关闭（WA_DeleteOnClose 会自动删除）
  if (m_errorTooltip) {
    m_errorTooltip->close();
    m_errorTooltip = nullptr;
  }

  // 创建新弹窗
  m_errorTooltip = new AuiErrorToolTip(text, this);
  m_errorTooltip->move(pos);
  m_errorTooltip->show();
}

void CodeEditor::hideErrorTooltip() {
  if (m_errorTooltip) {
    m_errorTooltip->close();
    m_errorTooltip = nullptr;
  }
}

// ──────────────────────────────────────────────────────────────
//  当前行高亮 + 括号匹配（使用 BracketMatcher 模块）
// ──────────────────────────────────────────────────────────────

void CodeEditor::highlightCurrentLine() {
  QList<QTextEdit::ExtraSelection> extra;

  if (!isReadOnly()) {
    QTextEdit::ExtraSelection selection;
    selection.format.setBackground(AuiStyle::currentLineBackground());
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    extra.append(selection);
  }

  QTextCursor cursor = textCursor();
  if (!cursor.hasSelection()) {
    int pos = cursor.position();
    const QString &text = cachedText();

    // 使用 BracketMatcher 进行括号匹配
    auto directMatch = BracketMatcher::findMatchAtCursor(pos, text);
    auto enclosingMatch =
        (directMatch.isValid()) ? directMatch : BracketMatcher::findEnclosingBrackets(pos, text);

    if (enclosingMatch.isValid()) {
      QColor color = AuiStyle::bracketColorForChar(enclosingMatch.openChar);

      QTextEdit::ExtraSelection sel1;
      sel1.cursor = cursor;
      sel1.cursor.setPosition(enclosingMatch.openPos);
      sel1.cursor.setPosition(enclosingMatch.openPos + 1, QTextCursor::KeepAnchor);
      sel1.format.setBackground(color);
      sel1.format.setFontWeight(QFont::Bold);
      extra.append(sel1);

      QTextEdit::ExtraSelection sel2;
      sel2.cursor = cursor;
      sel2.cursor.setPosition(enclosingMatch.closePos);
      sel2.cursor.setPosition(enclosingMatch.closePos + 1, QTextCursor::KeepAnchor);
      sel2.format.setBackground(color);
      sel2.format.setFontWeight(QFont::Bold);
      extra.append(sel2);
    }
  }

  // 错误行背景色高亮
  if (!m_errorLines.isEmpty()) {
    QTextBlock block = document()->firstBlock();
    while (block.isValid()) {
      int lineNum = block.blockNumber() + 1;
      if (m_errorLines.contains(lineNum)) {
        QTextEdit::ExtraSelection errorSel;
        errorSel.format.setBackground(AuiStyle::errorLineBackground());
        errorSel.format.setProperty(QTextFormat::FullWidthSelection, true);
        errorSel.cursor = QTextCursor(block);
        errorSel.cursor.clearSelection();
        extra.append(errorSel);
      }
      block = block.next();
    }
  }

  // 错误波浪下划线由 paintEvent 依据 m_errorRanges 统一绘制（单行、钳制行尾），
  // 不在此通过 ExtraSelection 绘制，避免与自定义绘制重叠（粗/细两条线并存）

  // 调试当前行高亮（黄色背景，标红箭头）
  if (m_debugLine > 0) {
    QTextBlock block = document()->findBlockByNumber(m_debugLine - 1);
    if (block.isValid()) {
      QTextEdit::ExtraSelection debugSel;
      debugSel.format.setBackground(QColor(0xff, 0xf0, 0x8a));  // 淡黄背景
      debugSel.format.setProperty(QTextFormat::FullWidthSelection, true);
      debugSel.cursor = QTextCursor(block);
      debugSel.cursor.clearSelection();
      extra.append(debugSel);
    }
  }

  // 查找匹配高亮（由 CodeFindBar 管理，追加到行高亮之后）
  if (m_findBar && m_findBar->isFindBarVisible()) {
    extra.append(m_findSelections);
  }

  setExtraSelections(extra);
}

void CodeEditor::refreshExtraSelections() { highlightCurrentLine(); }

// ──────────────────────────────────────────────────────────────
//  按键事件 — Enter 自动缩进 + F12 跳转
// ──────────────────────────────────────────────────────────────

void CodeEditor::keyPressEvent(QKeyEvent *event) {
  // ── 调试快捷键：F5 启动/继续、F10 单步执行、F11 单步进入、Shift+F11 单步跳出 ──
  if (event->key() == Qt::Key_F5 && !event->modifiers()) {
    emit requestDebugStart();
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_F10 && !event->modifiers()) {
    emit requestDebugStepOver();
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_F11 && !event->modifiers()) {
    emit requestDebugStepInto();
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_F11 && (event->modifiers() & Qt::ShiftModifier)) {
    emit requestDebugStepOut();
    event->accept();
    return;
  }

  // Ctrl+F 查找
  if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_F) {
    showFindBar();
    event->accept();
    return;
  }

  // Ctrl+H 替换（等同 Ctrl+F + 展开替换区域）
  if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_H) {
    showFindBar();
    event->accept();
    return;
  }

  // F12 转到定义
  if (event->key() == Qt::Key_F12 && m_validationMode == AcValidation) {
    QTextCursor cursor = textCursor();
    int pos = cursor.position();
    QString identifier = identifierAtCursor(pos);
    if (!identifier.isEmpty()) {
      goToDefinition(identifier);
      event->accept();
      return;
    }
  }

  // Ctrl+T 工作区符号搜索
  if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_T &&
      m_validationMode == AcValidation) {
    emit requestWorkspaceSymbols();
    event->accept();
    return;
  }

  // Shift+F12 跨文件查找引用
  if ((event->modifiers() & Qt::ShiftModifier) && event->key() == Qt::Key_F12 &&
      m_validationMode == AcValidation) {
    QTextCursor cursor = textCursor();
    QString identifier = identifierAtCursor(cursor.position());
    if (!identifier.isEmpty()) {
      emit requestFindReferencesAll(identifier);
      event->accept();
      return;
    }
  }

  // Ctrl+M / Ctrl+] 跳转到匹配括号（P2: 快捷键功能）
  if ((event->modifiers() & Qt::ControlModifier) &&
      (event->key() == Qt::Key_M || event->key() == Qt::Key_BracketRight)) {
    jumpToMatchingBracket();
    event->accept();
    return;
  }

  // Ctrl+Shift+M 选中括号内所有内容（P2: 快捷键功能）
  if ((event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) ==
          (Qt::ControlModifier | Qt::ShiftModifier) &&
      event->key() == Qt::Key_M) {
    selectBetweenBrackets();
    event->accept();
    return;
  }

  // 输入 ( 时显示函数签名提示
  if (event->key() == Qt::Key_ParenLeft && m_validationMode == AcValidation) {
    QPlainTextEdit::keyPressEvent(event);
    showSignatureHelp();
    if (m_completer) showCompleter();
    return;
  }

  // 补全器处理：补全弹窗可见时，Enter/Tab/Backtab 用于选中补全项
  // 必须放在 Enter 自动缩进之前，否则弹窗可见时按 Enter 会变成换行
  if (m_completer && m_completer->popup() && m_completer->popup()->isVisible() &&
      (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return ||
       event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab)) {
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
      QModelIndex idx = m_completer->popup()->currentIndex();
      if (idx.isValid()) {
        insertCompletion(idx.data(Qt::DisplayRole).toString());
      }
    }
    m_completer->popup()->hide();
    event->accept();
    return;
  }

  // Enter 自动缩进
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    QTextCursor cursor = textCursor();
    QString currentLineText = cursor.block().text().left(cursor.positionInBlock());
    int indent = calculateNewLineIndent(currentLineText);
    QString indentStr(indent, QLatin1Char(' '));
    insertPlainText(QLatin1Char('\n') + indentStr);
    event->accept();
    return;
  }

  // Tab 插入空格（按 Tab 宽度对齐，与 kTabWidthSpaces 保持一致）
  if (event->key() == Qt::Key_Tab && !event->modifiers()) {
    QTextCursor cursor = textCursor();
    int tabW = CodeConstants::Editor::kTabWidthSpaces;
    int spaces = tabW - (cursor.positionInBlock() % tabW);
    insertPlainText(QString(spaces, QLatin1Char(' ')));
    event->accept();
    return;
  }

  QPlainTextEdit::keyPressEvent(event);

  // 显示补全列表
  if (m_completer) {
    showCompleter();
  }
}

int CodeEditor::calculateNewLineIndent(const QString &linePrefix) const {
  int indent = 0;
  for (QChar ch : linePrefix) {
    if (ch == QLatin1Char(' '))
      ++indent;
    else
      break;
  }

  // 如果上一行以 { 结尾，增加缩进
  QString trimmed = linePrefix.trimmed();
  if (trimmed.endsWith(QLatin1Char('{'))) {
    indent += CodeConstants::Editor::kIndentSpaces;
  }

  return indent;
}

// ──────────────────────────────────────────────────────────────
//  右键菜单 — 增加「格式化代码」「转到定义」「查找引用」等
// ──────────────────────────────────────────────────────────────

/// 将 Qt 标准右键菜单的英文项本地化为中文（撤销/剪切/复制/粘贴等）
static void localizeStandardMenu(QMenu *menu) {
  struct EnZh {
    const char *en;
    const char *zh;
  };
  static const EnZh kMap[] = {
      {"Undo", "撤销"},  {"Redo", "重做"},   {"Cut", "剪切"},        {"Copy", "复制"},
      {"Paste", "粘贴"}, {"Delete", "删除"}, {"Select All", "全选"},
  };
  auto stripAmp = [](const QString &s) {
    QString r = s;
    r.remove(QLatin1Char('&'));
    r.replace(QLatin1String("..."), QString());
    return r;
  };
  for (QAction *act : menu->actions()) {
    if (act->isSeparator()) continue;
    const QString key = stripAmp(act->text());
    for (const auto &m : kMap) {
      if (key == QLatin1String(m.en)) {
        act->setText(QString::fromUtf8(m.zh));
        break;
      }
    }
  }
}

void CodeEditor::contextMenuEvent(QContextMenuEvent *event) {
  // 创建标准右键菜单
  QMenu *menu = createStandardContextMenu();
  localizeStandardMenu(menu);

  // 只在支持的验证模式下添加格式化等功能（JSON / AC / TPL）
  if (m_validationMode != NoValidation) {
    menu->addSeparator();

    // 获取光标下的标识符
    QTextCursor cursor = cursorForPosition(event->pos());
    int pos = cursor.position();
    int idStart = 0, idEnd = 0;
    QString identifier = identifierAtCursor(pos, &idStart, &idEnd);

    if (!identifier.isEmpty()) {
      // ── 转到定义 ──
      QAction *goDefAction = menu->addAction(QStringLiteral("转到定义"));
      goDefAction->setShortcut(QKeySequence(QStringLiteral("F12")));
      connect(goDefAction, &QAction::triggered, this,
              [this, identifier]() { goToDefinition(identifier); });

      // ── 转到类型定义 ──
      QAction *goTypeDefAction = menu->addAction(QStringLiteral("转到类型定义"));
      connect(goTypeDefAction, &QAction::triggered, this,
              [this, identifier]() { goToTypeDefinition(identifier); });

      // ── 查找所有引用（跨文件）──
      QAction *findRefsAction = menu->addAction(QStringLiteral("查找所有引用"));
      findRefsAction->setShortcut(QKeySequence(QStringLiteral("Shift+F12")));
      connect(findRefsAction, &QAction::triggered, this,
              [this, identifier]() { emit requestFindReferencesAll(identifier); });
    }

    menu->addSeparator();

    QAction *fmtAction = menu->addAction(QStringLiteral("格式化代码"));
    fmtAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")));
    connect(fmtAction, &QAction::triggered, this, &CodeEditor::formatCode);
  }

  // ── 断点操作（仅 .ac 可调试文件，模仿 VSCode）──
  if (isDebuggableFile()) {
    // 计算右键位置对应的行号（1-based）
    int line = cursorForPosition(event->pos()).blockNumber() + 1;
    bool hasBp = hasBreakpoint(line);
    bool bpEnabled = hasBp && isBreakpointEnabled(line);

    menu->addSeparator();
    QAction *toggleBpAction =
        menu->addAction(hasBp ? QString::fromUtf8(CodeConstants::UiText::kRemoveBreakpoint)
                              : QStringLiteral("添加断点"));
    toggleBpAction->setShortcut(QKeySequence(QStringLiteral("F9")));
    connect(toggleBpAction, &QAction::triggered, this,
            [this, line]() { toggleBreakpoint(line - 1); });

    // 仅在已有断点时才提供启用/禁用
    if (hasBp) {
      QAction *enableBpAction =
          menu->addAction(bpEnabled ? QString::fromUtf8(CodeConstants::UiText::kDisableBreakpoint)
                                    : QString::fromUtf8(CodeConstants::UiText::kEnableBreakpoint));
      enableBpAction->setEnabled(true);
      connect(enableBpAction, &QAction::triggered, this,
              [this, line, bpEnabled]() { setBreakpointEnabled(line, !bpEnabled); });
    }

    // 移除当前文件所有断点（仅当存在断点时）
    if (!m_breakpoints.isEmpty()) {
      QAction *removeAllBpAction = menu->addAction(QStringLiteral("移除断点..."));
      connect(removeAllBpAction, &QAction::triggered, this, &CodeEditor::clearBreakpoints);
    }
  }

  menu->exec(event->globalPos());
  delete menu;
}

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

void CodeEditor::mouseMoveEvent(QMouseEvent *event) {
  QPlainTextEdit::mouseMoveEvent(event);

  if (m_validationMode == NoValidation) return;

  QTextCursor cursor = cursorForPosition(event->pos());
  int pos = cursor.position();

  // ── 统一的光标样式：悬停手形与 Ctrl+点击跳转共用 navigationTargetAt ──
  //   这样 AC / JSON 各模式下"手形 ⇔ 可跳转"严格一致
  bool canNavigate = navigationTargetAt(pos);
  if (event->modifiers() & Qt::ControlModifier) {
    viewport()->setCursor(canNavigate ? Qt::PointingHandCursor : Qt::IBeamCursor);
  } else {
    viewport()->setCursor(Qt::IBeamCursor);
  }

  // ── JSON 模式：属性悬停提示（基于 $schema）──
  if (m_validationMode == JsonValidation && m_schemaLoaded) {
    QString path = m_schema.propertyPathAt(cachedText(), pos);
    if (!path.isEmpty()) {
      m_hoverTimer->stop();
      QPoint gpos = event->globalPosition().toPoint();
      m_hoverTimer->start();
      disconnect(m_hoverTimer, &QTimer::timeout, this, nullptr);
      connect(m_hoverTimer, &QTimer::timeout, this,
              [this, path, gpos]() { showJsonPropertyHover(path, gpos); });
    } else {
      m_currentHoverSymbol.clear();
      m_hoverTimer->stop();
      QToolTip::hideText();
    }
    return;
  }

  if (m_validationMode != AcValidation) return;

  // ── AC 模式：悬停提示（不需要 Ctrl，始终显示）──
  int idStart = 0, idEnd = 0;
  QString identifier = identifierAtCursor(pos, &idStart, &idEnd);
  if (!identifier.isEmpty()) {
    m_hoverTimer->stop();
    QPoint gpos = event->globalPosition().toPoint();
    m_hoverTimer->start();
    disconnect(m_hoverTimer, &QTimer::timeout, this, nullptr);
    connect(m_hoverTimer, &QTimer::timeout, this,
            [this, pos, gpos]() { showSymbolHover(pos, gpos); });
  } else {
    m_currentHoverSymbol.clear();
    m_hoverTimer->stop();
    QToolTip::hideText();
  }
}

void CodeEditor::mouseReleaseEvent(QMouseEvent *event) {
  // Ctrl+点击跳转
  if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ControlModifier)) {
    QTextCursor cursor = cursorForPosition(event->pos());
    int pos = cursor.position();

    // ── JSON 模式：Ctrl+点击属性 → 跳转到 schema 中对应字段 ──
    if (m_validationMode == JsonValidation && m_schemaLoaded) {
      QString path = m_schema.propertyPathAt(cachedText(), pos);
      QString className, propName;
      if (!path.isEmpty() && m_schema.propertyContext(path, &className, &propName) &&
          !propName.isEmpty()) {
        int line = findSchemaPropertyLine(className, propName);
        emit aboutToNavigate(m_schemaPath, line);
        emit requestGoToLine(m_schemaPath, line);
        return;
      }
    }

    // ── AC 模式：Ctrl+点击标识符 → 转到定义 ──
    //   （JSON 无 schema 时不再走标识符兜底，保证与手形判定一致）
    if (m_validationMode == AcValidation) {
      int idStart = 0, idEnd = 0;
      QString identifier = identifierAtCursor(pos, &idStart, &idEnd);
      if (!identifier.isEmpty()) {
        goToDefinition(identifier);
        return;
      }
    }
  }

  QPlainTextEdit::mouseReleaseEvent(event);
}

// ── 统一的导航目标判定（悬停手形与 Ctrl+点击跳转共用）──
bool CodeEditor::navigationTargetAt(int pos) const {
  switch (m_validationMode) {
    case JsonValidation: {
      // 与鼠标点击跳转完全一致：路径非空 + schema 中可解析到属性
      if (!m_schemaLoaded) return false;
      QString path = m_schema.propertyPathAt(cachedText(), pos);
      QString className, propName;
      return !path.isEmpty() && m_schema.propertyContext(path, &className, &propName) &&
             !propName.isEmpty();
    }
    case AcValidation: {
      // 与 AC 现状一致：任意标识符均可提示/尝试跳转
      int start = 0, end = 0;
      return !identifierAtCursor(pos, &start, &end).isEmpty();
    }
    default:
      return false;
  }
}

// ── JSON 属性悬停提示（基于 $schema）──
void CodeEditor::showJsonPropertyHover(const QString &jsonPath, const QPoint &gpos) {
  if (m_validationMode != JsonValidation || !m_schemaLoaded) return;
  QString desc = m_schema.propertyDescription(jsonPath);
  if (desc.isEmpty()) {
    QToolTip::hideText();
    return;
  }
  QToolTip::showText(gpos, desc, this);
}

// ── 在 schema 文件中定位 className 类下 propName 属性的行号 ──
int CodeEditor::findSchemaPropertyLine(const QString &className, const QString &propName) const {
  QString text;
  {
    QFile f(m_schemaPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return 1;
    text = QString::fromUtf8(f.readAll());
  }

  // 先定位类定义块
  QRegularExpression classRe(
      QStringLiteral("[\"']?%1[\"']?\\s*:").arg(QRegularExpression::escape(className)));
  auto cm = classRe.match(text);
  if (!cm.hasMatch()) return 1;
  int searchFrom = cm.capturedStart();

  // 类块内优先定位 properties 块，再在其中找属性
  int propsIdx = text.indexOf(QStringLiteral("properties"), searchFrom);
  if (propsIdx >= 0 && (propsIdx - searchFrom) < 2000) searchFrom = propsIdx;

  QRegularExpression propRe(
      QStringLiteral("[\"']?%1[\"']?\\s*:").arg(QRegularExpression::escape(propName)));
  auto pm = propRe.match(text, searchFrom);
  if (!pm.hasMatch()) return 1;

  int matchPos = pm.capturedStart();
  int line = 1;
  for (int i = 0; i < matchPos && i < text.size(); ++i) {
    if (text[i] == QLatin1Char('\n')) ++line;
  }
  return line;
}

void CodeEditor::setSymbolTable(const QHash<QString, AcSymbolEntry> &symbols) {
  m_symbolTable = symbols;
  m_symbolNavigator.setSymbolTable(symbols);
}

// ══════════════════════════════════════════════════════════════════════════════
//  查找/替换栏
// ══════════════════════════════════════════════════════════════════════════════

void CodeEditor::showFindBar() {
  if (!m_findBar) return;
  m_findBar->showFindBar();
  updateFindBarLayout();
}

void CodeEditor::hideFindBar() {
  if (m_findBar) m_findBar->hideFindBar();
  setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

bool CodeEditor::isFindBarVisible() const { return m_findBar && m_findBar->isFindBarVisible(); }

CodeFindBar *CodeEditor::findBar() const { return m_findBar; }

void CodeEditor::updateFindBarLayout() {
  if (!m_findBar || !m_findBar->isFindBarVisible()) return;
  // 强制刷新布局，确保 sizeHint 反映替换区域展开/收起后的实际高度
  m_findBar->layout()->activate();
  int findBarH = m_findBar->sizeHint().height();
  int topMargin = findBarH + 4;
  setViewportMargins(lineNumberAreaWidth(), topMargin, 0, 0);
  QRect cr = contentsRect();
  int findBarW = qMin(m_findBar->sizeHint().width(), cr.width() - 10);
  // 查找栏定位在顶部边距区域内，不覆盖代码
  m_findBar->setGeometry(cr.right() - findBarW - 5, cr.top() + topMargin - findBarH - 2, findBarW,
                         findBarH);
  // 行号区域也要同步调整
  m_lineNumberArea->setGeometry(
      QRect(cr.left(), cr.top() + topMargin, lineNumberAreaWidth(), cr.height() - topMargin));
}