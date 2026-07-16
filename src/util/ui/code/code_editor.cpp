/**
 * @file code_editor.cpp
 * @brief 代码编辑器控件实现
 */

#include "code_editor.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShortcut>
#include <QStringListModel>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTimer>
#include <QToolTip>

#include "src/engine/json_validator.h"
#include "src/engine/script/ac_validator.h"
#include "src/engine/tpl/tpl_validator.h"
#include "src/util/ui/code/format_code.h"
#include "src/util/ui/component/aui_error_tool_tip.h"
#include "src/util/ui/component/aui_style.h"

// ──────────────────────────────────────────────────────────────
//  构造与初始化
// ──────────────────────────────────────────────────────────────

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent) {
  // 启用鼠标追踪（不按下按键也能接收 mouseMoveEvent）
  setMouseTracking(true);

  // 创建行号区域
  m_lineNumberArea = new LineNumberArea(this);

  // 连接信号槽
  connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
  connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);
  connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

  // 初始化验证防抖定时器
  m_validationTimer.setSingleShot(true);
  m_validationTimer.setInterval(500);
  connect(&m_validationTimer, &QTimer::timeout, this, &CodeEditor::performValidation);

  // 初始化悬停提示防抖定时器
  m_hoverTimer.setSingleShot(true);
  m_hoverTimer.setInterval(500);

  // 初始设置
  updateLineNumberAreaWidth(0);
  highlightCurrentLine();

  // 设置等宽字体
  QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  font.setPointSize(10);
  setFont(font);

  // 设置 Tab 宽度
  setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);

  // 默认启用括号匹配
  setExtraSelections(QList<QTextEdit::ExtraSelection>());
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
  m_validationTimer.start();
}

void CodeEditor::performValidation() {
  if (m_validationMode == NoValidation) return;

  // 清除旧的错误标记
  m_errorSelections.clear();
  m_errorLines.clear();
  m_errorRanges.clear();  // 清除错误位置范围（供 paintEvent 使用）

  // 根据验证模式创建对应的验证器，使用统一的 IValidator 接口
  switch (m_validationMode) {
    case JsonValidation: {
      JsonValidator validator;
      validateWithValidator(&validator);
      break;
    }
    case TemplateValidation: {
      TplValidator validator;
      validateWithValidator(&validator);
      break;
    }
    case AcValidation: {
      AcValidator validator;
      validateWithValidator(&validator);
      // 验证后提取符号表数据
      m_symbolTable = validator.symbolTable().allSymbols();
      break;
    }
    default:
      break;
  }

  // 刷新 ExtraSelections（重新绘制行高亮 + 括号匹配 + 错误标记）
  refreshExtraSelections();

  // 刷新行号区域（错误行号显示红色）
  m_lineNumberArea->update();

  // 强制触发一次完整重绘（确保自定义波浪线立即完整显示）
  // 不带参数的 update() 会标记整个控件为脏区域
  update();
}

// ──────────────────────────────────────────────────────────────
//  统一验证（使用 IValidator 接口）
// ──────────────────────────────────────────────────────────────

void CodeEditor::validateWithValidator(IValidator *validator) {
  QString text = toPlainText();
  if (text.trimmed().isEmpty()) return;

  QVector<ValidationResult> results = validator->validate(text);

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
      int length = result.length > 0 ? result.length : block.length() - 1;
      if (result.column > 0) {
        // 有精确列号，从指定列开始标记
        applyErrorUnderline(blockPos + result.column - 1, length, result.message,
                            m_errorSelections);
        // 记录错误位置范围（供 paintEvent 自定义绘制）
        m_errorRanges.append(ErrorRange(blockPos + result.column - 1, length, result.message));
      } else {
        // 无列号，标记整行
        applyErrorUnderline(blockPos, length, result.message, m_errorSelections);
        m_errorRanges.append(ErrorRange(blockPos, length, result.message));
      }
      m_errorLines.insert(result.line);
    }
  }

  // 发出验证结果信号
  if (errors.isEmpty()) {
    emit validationMessage(QString(), 0);
  } else {
    emit validationMessage(errors.join(QLatin1Char('\n')), errors.size());
  }
}

void CodeEditor::applyErrorUnderline(int from, int length, const QString &tooltip,
                                     QList<QTextEdit::ExtraSelection> &selections) {
  QTextCursor cursor(document());
  cursor.setPosition(from);
  cursor.setPosition(from + length, QTextCursor::KeepAnchor);

  QTextEdit::ExtraSelection sel;
  sel.cursor = cursor;
  sel.format.setProperty(QTextFormat::TextUnderlineStyle, QTextCharFormat::WaveUnderline);
  sel.format.setProperty(QTextFormat::TextUnderlineColor, QColor(Qt::red));
  sel.format.setToolTip(tooltip);
  selections.append(sel);
}

// ──────────────────────────────────────────────────────────────
//  行号区域绘制
// ──────────────────────────────────────────────────────────────

void CodeEditor::updateLineNumberAreaWidth(int newBlockCount) {
  Q_UNUSED(newBlockCount);
  setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
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
      QString number = QString::number(blockNumber + 1);

      // 错误行号显示红色，否则用默认颜色
      if (m_errorLines.contains(blockNumber + 1)) {
        painter.setPen(AuiStyle::errorTextColor());
      } else {
        painter.setPen(AuiStyle::textColor());
      }

      painter.drawText(0, top, m_lineNumberArea->width() - 6, fontMetrics().height(),
                       Qt::AlignRight, number);
    }

    block = block.next();
    top = bottom;
    bottom = top + qRound(blockBoundingRect(block).height());
    ++blockNumber;
  }
}

// ──────────────────────────────────────────────────────────────
//  绘制与事件处理
// ──────────────────────────────────────────────────────────────

void CodeEditor::resizeEvent(QResizeEvent *event) {
  QPlainTextEdit::resizeEvent(event);

  QRect cr = contentsRect();
  m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::paintEvent(QPaintEvent *event) {
  // 先调用标准绘制（背景、文本、默认波浪下划线等）
  QPlainTextEdit::paintEvent(event);

  // 在标准绘制之上，额外绘制超粗红色波浪线（覆盖默认细波浪线）
  if (m_errorRanges.isEmpty()) return;

  QPainter painter(viewport());
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(AuiStyle::errorUnderlineColor(), 3, Qt::SolidLine, Qt::RoundCap));

  for (const auto &err : m_errorRanges) {
    QTextCursor cursor(document());
    cursor.setPosition(err.start);
    cursor.setPosition(err.start + err.length, QTextCursor::KeepAnchor);

    // 获取文本区域在视口中的矩形
    QRect rect = cursorRect(cursor);
    if (!rect.isValid()) continue;

    // 绘制粗波浪线（在文本下方）
    int y = rect.bottom() - 2;
    int x1 = rect.left();
    int x2 = rect.right();

    // 锯齿形波浪线
    QPainterPath path;
    path.moveTo(x1, y);
    int step = 4;
    bool up = true;
    for (int x = x1; x <= x2; x += step) {
      path.lineTo(x, up ? y - 3 : y + 3);
      up = !up;
    }
    painter.drawPath(path);
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
    m_errorTooltip->hide();
  }
}

// ──────────────────────────────────────────────────────────────
//  当前行高亮 + 括号匹配
// ──────────────────────────────────────────────────────────────

void CodeEditor::highlightCurrentLine() {
  QList<QTextEdit::ExtraSelection> extra;

  // 当前行高亮
  if (!isReadOnly()) {
    QTextEdit::ExtraSelection selection;
    selection.format.setBackground(AuiStyle::currentLineBackground());
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    extra.append(selection);
  }

  // 括号匹配高亮
  QTextCursor cursor = textCursor();
  if (!cursor.hasSelection()) {
    int pos = cursor.position();
    QString text = toPlainText();
    if (pos > 0 && pos <= text.size()) {
      QChar ch = text[pos - 1];
      QChar matchCh = QChar::Null;
      if (ch == QLatin1Char('('))
        matchCh = QLatin1Char(')');
      else if (ch == QLatin1Char('['))
        matchCh = QLatin1Char(']');
      else if (ch == QLatin1Char('{'))
        matchCh = QLatin1Char('}');
      else if (ch == QLatin1Char(')'))
        matchCh = QLatin1Char('(');
      else if (ch == QLatin1Char(']'))
        matchCh = QLatin1Char('[');
      else if (ch == QLatin1Char('}'))
        matchCh = QLatin1Char('{');

      if (!matchCh.isNull()) {
        // 查找匹配括号
        int matchPos = -1;
        int depth = 0;
        bool forward = (ch == QLatin1Char('(') || ch == QLatin1Char('[') || ch == QLatin1Char('{'));

        if (forward) {
          for (int i = pos; i < text.size(); ++i) {
            if (text[i] == ch)
              ++depth;
            else if (text[i] == matchCh) {
              --depth;
              if (depth == 0) {
                matchPos = i;
                break;
              }
            }
          }
        } else {
          for (int i = pos - 2; i >= 0; --i) {
            if (text[i] == matchCh)
              ++depth;
            else if (text[i] == ch) {
              --depth;
              if (depth == 0) {
                matchPos = i;
                break;
              }
            }
          }
        }

        if (matchPos >= 0) {
          QTextEdit::ExtraSelection sel1;
          sel1.cursor = cursor;
          sel1.cursor.setPosition(pos - 1);
          sel1.cursor.setPosition(pos, QTextCursor::KeepAnchor);
          sel1.format.setBackground(AuiStyle::bracketMatchColor());
          extra.append(sel1);

          QTextEdit::ExtraSelection sel2;
          sel2.cursor = cursor;
          sel2.cursor.setPosition(matchPos);
          sel2.cursor.setPosition(matchPos + 1, QTextCursor::KeepAnchor);
          sel2.format.setBackground(AuiStyle::bracketMatchColor());
          extra.append(sel2);
        }
      }
    }
  }

  // 合并所有 ExtraSelections
  if (!extra.isEmpty()) {
    QList<QTextEdit::ExtraSelection> all = extra;
    all.append(m_errorSelections);
    all.append(m_referenceSelections);
    setExtraSelections(all);
  } else {
    QList<QTextEdit::ExtraSelection> all;
    all.append(m_errorSelections);
    all.append(m_referenceSelections);
    setExtraSelections(all);
  }
}

void CodeEditor::refreshExtraSelections() { highlightCurrentLine(); }

// ──────────────────────────────────────────────────────────────
//  按键事件 — Enter 自动缩进 + F12 跳转
// ──────────────────────────────────────────────────────────────

void CodeEditor::keyPressEvent(QKeyEvent *event) {
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

  // Tab 插入空格
  if (event->key() == Qt::Key_Tab && !event->modifiers()) {
    QTextCursor cursor = textCursor();
    int spaces = 4 - (cursor.positionInBlock() % 4);
    insertPlainText(QString(spaces, QLatin1Char(' ')));
    event->accept();
    return;
  }

  // 补全器处理
  if (m_completer && (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return ||
                      event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab)) {
    if (m_completer->popup() && m_completer->popup()->isVisible()) {
      event->accept();
      return;
    }
  }

  QPlainTextEdit::keyPressEvent(event);

  // 触发验证防抖
  if (m_validationMode != NoValidation) {
    scheduleValidation();
  }

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
    indent += 4;
  }

  return indent;
}

// ──────────────────────────────────────────────────────────────
//  代码补全
// ──────────────────────────────────────────────────────────────

void CodeEditor::initCompleter(ValidationMode mode) {
  // 销毁旧补全器
  if (m_completer) {
    disconnect(m_completer, nullptr, this, nullptr);
    delete m_completer;
    m_completer = nullptr;
  }

  if (mode == NoValidation) return;

  m_completer = new QCompleter(this);
  m_completer->setWidget(this);
  m_completer->setCompletionMode(QCompleter::PopupCompletion);
  m_completer->setCaseSensitivity(Qt::CaseSensitive);
  // 必须设置 model，否则 model() 返回 nullptr 会导致崩溃
  m_completer->setModel(new QStringListModel(m_completer));

  connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated), this,
          &CodeEditor::insertCompletion);
}

void CodeEditor::showCompleter() {
  QTextCursor cursor = textCursor();
  int pos = cursor.position();
  int idStart = 0, idEnd = 0;
  QString identifier = identifierAtCursor(pos, &idStart, &idEnd);

  if (identifier.size() < 1) {
    m_completer->popup()->hide();
    return;
  }

  // 构建补全列表（从符号表）
  QStringList completions;
  for (auto it = m_symbolTable.begin(); it != m_symbolTable.end(); ++it) {
    if (it.key().startsWith(identifier, Qt::CaseInsensitive)) {
      completions.append(it.key());
    }
  }

  if (!m_completer || completions.isEmpty()) {
    if (m_completer && m_completer->popup()) {
      m_completer->popup()->hide();
    }
    return;
  }

  if (m_completer->model()) {
    m_completer->model()->sort(0);
  }
  m_completer->setCompletionPrefix(identifier);

  // 定位补全弹窗到光标位置
  QRect cr = cursorRect();
  cr.setWidth(m_completer->popup()->sizeHintForColumn(0) +
              m_completer->popup()->verticalScrollBar()->sizeHint().width());
  m_completer->complete(cr);
}

void CodeEditor::insertCompletion(const QString &completion) {
  if (!m_completer || m_completer->widget() != this) return;

  QTextCursor cursor = textCursor();
  int extra = completion.size() - m_completer->completionPrefix().size();
  cursor.movePosition(QTextCursor::Left);
  cursor.movePosition(QTextCursor::EndOfWord);
  cursor.insertText(completion.right(extra));
  setTextCursor(cursor);
}

// ──────────────────────────────────────────────────────────────
//  右键菜单 — 增加「格式化代码」「转到定义」「查找引用」等
// ──────────────────────────────────────────────────────────────

void CodeEditor::contextMenuEvent(QContextMenuEvent *event) {
  // 创建标准右键菜单
  QMenu *menu = createStandardContextMenu();

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

      // ── 查找所有引用 ──
      QAction *findRefsAction = menu->addAction(QStringLiteral("查找所有引用"));
      connect(findRefsAction, &QAction::triggered, this, [this, identifier]() {
        auto refs = findSymbolReferences(identifier);
        for (const auto &ref : refs) {
          emit requestFindReferences(QString(), ref.first, ref.second);
        }
      });
    }

    menu->addSeparator();

    QAction *fmtAction = menu->addAction(QStringLiteral("格式化代码"));
    fmtAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")));
    connect(fmtAction, &QAction::triggered, this, &CodeEditor::formatCode);
  }

  menu->exec(event->globalPos());
  delete menu;
}

void CodeEditor::formatCode() {
  QString src = toPlainText();
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

// ──────────────────────────────────────────────────────────────
//  符号导航：悬停提示、转到定义、查找引用、符号高亮
// ──────────────────────────────────────────────────────────────

QString CodeEditor::identifierAtCursor(int pos, int *startPos, int *endPos) const {
  QString text = toPlainText();
  if (pos < 0 || pos >= text.size()) {
    if (startPos) *startPos = pos;
    if (endPos) *endPos = pos;
    return QString();
  }

  // 向前扫描标识符字符（支持 :: 静态访问）
  int start = pos;
  while (start > 0) {
    QChar ch = text[start - 1];
    if (!ch.isLetterOrNumber() && ch != QLatin1Char('_') && ch != QLatin1Char('$') &&
        !text.mid(start - 1, 2).startsWith(QStringLiteral("::"))) {
      break;
    }
    --start;
  }

  // 向后扫描标识符字符（支持 :: 静态访问）
  int end = pos;
  while (end < text.size()) {
    QChar ch = text[end];
    if (!ch.isLetterOrNumber() && ch != QLatin1Char('_') && ch != QLatin1Char('$') &&
        !text.mid(end, 2).startsWith(QStringLiteral("::"))) {
      break;
    }
    ++end;
  }

  if (start == end) {
    if (startPos) *startPos = pos;
    if (endPos) *endPos = pos;
    return QString();
  }

  if (startPos) *startPos = start;
  if (endPos) *endPos = end;
  return text.mid(start, end - start);
}

const AcSymbolEntry *CodeEditor::findSymbolDefinition(const QString &name) const {
  if (name.isEmpty()) return nullptr;

  // 精确匹配
  auto it = m_symbolTable.find(name);
  if (it != m_symbolTable.end()) return &it.value();

  // 静态访问匹配：ClassName::member -> 尝试 ClassName.member
  if (name.contains(QStringLiteral("::"))) {
    QString key = name;
    key.replace(QStringLiteral("::"), QStringLiteral("."));
    auto it3 = m_symbolTable.find(key);
    if (it3 != m_symbolTable.end()) return &it3.value();

    // 也尝试只匹配类名
    QString className = name.section(QStringLiteral("::"), 0, 0);
    auto it4 = m_symbolTable.find(className);
    if (it4 != m_symbolTable.end()) return &it4.value();
  }

  // 类方法/属性匹配：同名方法/属性/参数
  for (auto it2 = m_symbolTable.begin(); it2 != m_symbolTable.end(); ++it2) {
    if (it2.value().name == name &&
        (it2.value().kind == AcSymbolKind::kMethod || it2.value().kind == AcSymbolKind::kProperty ||
         it2.value().kind == AcSymbolKind::kParameter)) {
      return &it2.value();
    }
  }

  return nullptr;
}

void CodeEditor::goToDefinition(const QString &name) {
  if (name.isEmpty() || !document()) return;

  const AcSymbolEntry *entry = findSymbolDefinition(name);
  if (!entry) return;

  // 确定目标位置
  QString targetPath = entry->filePath.isEmpty() ? objectName() : entry->filePath;
  int targetLine = entry->line;

  // 行号为 0 时，通过源码搜索定位
  if (targetLine <= 0) {
    targetLine = findSymbolLineByName(name);
    if (targetLine <= 0) targetLine = 1;  // 找不到时默认第 1 行
  }

  // 发射即将导航信号（用于记录历史，无论是否跨文件）
  emit aboutToNavigate(targetPath, targetLine);

  // 如果是当前文件，直接跳转
  if (targetPath == objectName()) {
    QTextCursor cursor(document());
    QTextBlock block = document()->findBlockByNumber(targetLine - 1);
    if (block.isValid()) {
      cursor.setPosition(block.position());
      setTextCursor(cursor);
      ensureCursorVisible();
      setFocus();
    }
  } else {
    // 跨文件跳转，发射信号让外部处理
    emit requestGoToLine(targetPath, targetLine);
  }
}

int CodeEditor::findSymbolLineByName(const QString &name) const {
  QString text = toPlainText();
  QStringList lines = text.split(QLatin1Char('\n'));

  // 搜索 "function name" 或 "class name" 或 "interface name" 等模式
  QRegularExpression funcRe(QStringLiteral("^\\s*function\\s+") + QRegularExpression::escape(name) +
                            QStringLiteral("\\b"));
  QRegularExpression classRe(QStringLiteral("^\\s*class\\s+") + QRegularExpression::escape(name) +
                             QStringLiteral("\\b"));
  QRegularExpression ifaceRe(QStringLiteral("^\\s*interface\\s+") +
                             QRegularExpression::escape(name) + QStringLiteral("\\b"));

  for (int i = 0; i < lines.size(); ++i) {
    if (funcRe.match(lines[i]).hasMatch() || classRe.match(lines[i]).hasMatch() ||
        ifaceRe.match(lines[i]).hasMatch()) {
      return i + 1;  // 返回 1-based 行号
    }
  }

  // 如果没找到精确匹配，尝试简单搜索标识符
  QRegularExpression identRe(QStringLiteral("\\b") + QRegularExpression::escape(name) +
                             QStringLiteral("\\b"));
  for (int i = 0; i < lines.size(); ++i) {
    if (identRe.match(lines[i]).hasMatch()) {
      return i + 1;
    }
  }

  return 0;
}

QVector<QPair<int, QString>> CodeEditor::findSymbolReferences(const QString &name) const {
  QVector<QPair<int, QString>> refs;
  if (name.isEmpty()) return refs;

  // 扫描全文查找标识符出现位置
  QString text = toPlainText();
  QStringList lines = text.split(QLatin1Char('\n'));
  QRegularExpression re(QStringLiteral("\\b") + QRegularExpression::escape(name) +
                        QStringLiteral("\\b"));

  for (int i = 0; i < lines.size(); ++i) {
    auto match = re.match(lines[i]);
    if (match.hasMatch()) {
      refs.append(qMakePair(i + 1, lines[i].trimmed()));
    }
  }

  return refs;
}

void CodeEditor::showSymbolHover(int pos, const QPoint &globalPos) {
  if (m_validationMode != AcValidation) return;

  int idStart = 0, idEnd = 0;
  QString identifier = identifierAtCursor(pos, &idStart, &idEnd);
  if (identifier.isEmpty()) {
    QToolTip::hideText();
    return;
  }

  // 同一符号不重复显示
  if (identifier == m_currentHoverSymbol) return;
  m_currentHoverSymbol = identifier;

  // 构建提示文本
  QString tipText = identifier;

  // 从符号表获取详细信息
  const AcSymbolEntry *entry = findSymbolDefinition(identifier);
  if (entry) {
    tipText = entry->signature;
    if (!entry->returnType.isEmpty()) {
      tipText += QStringLiteral("\n返回: ") + entry->returnType;
    }
  }

  // 查找引用数量
  auto refs = findSymbolReferences(identifier);
  if (!refs.isEmpty()) {
    tipText += QStringLiteral("\n%1 引用").arg(refs.size());
  }

  // 使用标准 QToolTip 显示悬停提示
  QToolTip::showText(globalPos, tipText, this);
}

void CodeEditor::highlightSymbolReferences(const QString &name) {
  m_referenceSelections.clear();

  if (name.isEmpty() || m_validationMode != AcValidation) {
    refreshExtraSelections();
    return;
  }

  auto refs = findSymbolReferences(name);
  QTextCursor cursor(document());

  QRegularExpression re(QStringLiteral("\\b") + QRegularExpression::escape(name) +
                        QStringLiteral("\\b"));
  QString text = toPlainText();
  int offset = 0;

  while (offset < text.size()) {
    auto match = re.match(text, offset);
    if (!match.hasMatch()) break;

    int start = match.capturedStart();
    int length = match.capturedLength();

    cursor.setPosition(start);
    cursor.setPosition(start + length, QTextCursor::KeepAnchor);

    QTextEdit::ExtraSelection sel;
    sel.cursor = cursor;
    sel.format.setBackground(AuiStyle::modifiedColor().lighter(180));
    sel.format.setForeground(AuiStyle::modifiedColor());
    m_referenceSelections.append(sel);

    offset = start + length;
  }

  refreshExtraSelections();
}

void CodeEditor::mouseMoveEvent(QMouseEvent *event) {
  QPlainTextEdit::mouseMoveEvent(event);

  if (m_validationMode != AcValidation) return;

  QTextCursor cursor = cursorForPosition(event->pos());
  int pos = cursor.position();

  int idStart = 0, idEnd = 0;
  QString identifier = identifierAtCursor(pos, &idStart, &idEnd);

  // 更新光标样式（仅 Ctrl 按下时变为手形）
  if (!identifier.isEmpty() && (event->modifiers() & Qt::ControlModifier)) {
    viewport()->setCursor(Qt::PointingHandCursor);
  } else {
    viewport()->setCursor(Qt::IBeamCursor);
  }

  // 悬停提示（不需要 Ctrl，始终显示）
  if (!identifier.isEmpty()) {
    m_hoverTimer.stop();
    QPoint gpos = event->globalPosition().toPoint();
    m_hoverTimer.start();
    disconnect(&m_hoverTimer, &QTimer::timeout, this, nullptr);
    connect(&m_hoverTimer, &QTimer::timeout, this,
            [this, pos, gpos]() { showSymbolHover(pos, gpos); });
  } else {
    m_currentHoverSymbol.clear();
    m_hoverTimer.stop();
    QToolTip::hideText();
  }
}

void CodeEditor::mouseReleaseEvent(QMouseEvent *event) {
  // Ctrl+点击跳转
  if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ControlModifier)) {
    QTextCursor cursor = cursorForPosition(event->pos());
    int pos = cursor.position();
    int idStart = 0, idEnd = 0;
    QString identifier = identifierAtCursor(pos, &idStart, &idEnd);
    if (!identifier.isEmpty()) {
      goToDefinition(identifier);
      return;
    }
  }

  QPlainTextEdit::mouseReleaseEvent(event);
}

void CodeEditor::setSymbolTable(const QHash<QString, AcSymbolEntry> &symbols) {
  m_symbolTable = symbols;
}