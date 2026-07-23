/**
 * @file code_editor.cpp
 * @brief 代码编辑器控件实现（重构后）
 */

#include "code_editor.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QShortcut>
#include <QStringListModel>
#include <QToolTip>
#include <QVBoxLayout>

#include "code_find_bar.h"
#include "src/engine/json_validator.h"
#include "src/engine/script/ac_validator.h"
#include "src/engine/tpl/tpl_validator.h"
#include "src/util/common/code_constants.h"
#include "src/util/ui/code/format_code.h"
#include "src/util/ui/component/aui_error_tool_tip.h"
#include "src/util/ui/component/aui_style.h"

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

  // 使用常量配置
  QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  font.setPointSize(CodeConstants::Editor::kDefaultFontSize);
  setFont(font);

  setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) *
                     CodeConstants::Editor::kTabWidthSpaces);

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

  // 强制触发一次完整重绘（确保自定义波浪线立即完整显示）
  // 不带参数的 update() 会标记整个控件为脏区域
  update();
}

// ──────────────────────────────────────────────────────────────
//  统一验证（使用 IValidator 接口）
// ──────────────────────────────────────────────────────────────

void CodeEditor::validateWithValidator(IValidator *validator) {
  const QString &text = cachedText();
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

        // 使用 QTextLayout::cursorToX 获取精确像素位置，避免 charWidth 估算误差
        qreal x = 0;
        QTextLayout *layout = startBlk.layout();
        if (layout && layout->lineCount() > 0) {
          int centerCol = range.indent - tabW / 2;
          x = layout->lineAt(0).cursorToX(centerCol) + contentOffset().x();
        } else {
          x = (range.indent - tabW / 2.0) * charWidth + contentOffset().x();
        }

        bool isActive = (cursorLine >= range.startLine && cursorLine <= range.endLine &&
                         cursorIndent >= range.indent);
        guidePainter.setPen(QPen(isActive ? activeColor : normalColor, 1, Qt::SolidLine));
        guidePainter.drawLine(qRound(x), qRound(y1), qRound(x), qRound(y2));
      }
    }
  }

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

  // 错误波浪下划线
  extra.append(m_errorSelections);

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
    scheduleValidation();
    if (m_completer) showCompleter();
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