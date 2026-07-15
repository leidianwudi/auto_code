/**
 * @file code_editor.cpp
 * @brief 代码编辑器控件实现
 */

#include "code_editor.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QContextMenuEvent>
#include <QEvent>
#include <QHelpEvent>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QMap>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStack>
#include <QStringListModel>
#include <QTextBlock>

#include "src/engine/ac_language.h"
#include "src/engine/json_validator.h"
#include "src/engine/script/ac_validator.h"
#include "src/engine/tpl/tpl_validator.h"
#include "src/engine/validation_result.h"
#include "src/util/ui/code/format_code.h"
#include "src/util/ui/code/guess_code.h"
#include "src/util/ui/component/aui_error_tool_tip.h"
#include "src/util/ui/component/aui_style.h"

// ──────────────────────────────────────────────────────────────
//  构造 / 基本接口
// ──────────────────────────────────────────────────────────────

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent), m_lineNumberArea(new LineNumberArea(this)) {
  // 设置等宽编程字体，确保代码对齐和可读性（使用 AuiStyle 统一字体）
  setFont(AuiStyle::createEditorFont());

  // 去除文档外边距，使编辑区紧贴父容器
  document()->setDocumentMargin(0);

  // ── 连接信号槽 ──
  // blockCountChanged: 行数变化时重新计算行号区域宽度
  // updateRequest: 滚动/绘制时更新行号区域
  // cursorPositionChanged: 光标移动时刷新当前行高亮
  // textChanged: 文本变化时触发验证（经过防抖）
  connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
  connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);
  connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this]() { highlightCurrentLine(); });

  // 验证防抖定时器：用户停止输入 500ms 后才执行验证
  // 避免每次击键都进行完整的 JSON 解析或模板标签匹配，显著提升性能
  m_validationTimer.setSingleShot(true);
  m_validationTimer.setInterval(500);  // 500ms 防抖
  connect(&m_validationTimer, &QTimer::timeout, this, &CodeEditor::performValidation);

  // 任何文本变化都重新调度验证（每次变化会重置 500ms 计时器）
  connect(this, &QPlainTextEdit::textChanged, this, &CodeEditor::scheduleValidation);
  // 文本变化或滚动时自动关闭错误提示弹窗
  connect(this, &QPlainTextEdit::textChanged, this, &CodeEditor::hideErrorTooltip);
  connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::hideErrorTooltip);

  // 初始化：计算行号区域宽度并高亮当前行
  updateLineNumberAreaWidth(0);
  highlightCurrentLine();
}

void CodeEditor::setValidationMode(ValidationMode mode) {
  m_validationMode = mode;
  initCompleter(mode);
  performValidation();  // 立即做一次验证
}

void CodeEditor::validate() { performValidation(); }

// ──────────────────────────────────────────────────────────────
//  行号区域
// ──────────────────────────────────────────────────────────────

int CodeEditor::lineNumberAreaWidth() const {
  // 计算最大行号所需的位数
  // 例如：100 行需要 3 位，1000 行需要 4 位
  int digits = 1;
  int max = qMax(1, blockCount());
  while (max >= 10) {
    max /= 10;
    ++digits;
  }
  // 宽度 = 边距(3px) + 数字宽度 × 位数
  return 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void CodeEditor::updateLineNumberAreaWidth(int) {
  // 根据行号宽度预留左侧边距，将主编辑区域向右推移
  setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
  // dy > 0 表示滚动，直接滚动行号区域即可
  // dy == 0 表示需要重绘指定区域
  if (dy)
    m_lineNumberArea->scroll(0, dy);
  else
    m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

  // 如果整个视口可见区域都包含在更新矩形中，则重新计算行号宽度
  if (rect.contains(viewport()->rect())) updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *event) {
  QPlainTextEdit::resizeEvent(event);
  // 将行号区域放置在左侧，高度与内容区域一致
  QRect cr = contentsRect();
  m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event, const QRect &area) {
  QPainter painter(m_lineNumberArea);
  // 绘制行号区域背景色（浅灰）
  painter.fillRect(area, AuiStyle::lineNumberBackground());

  // 计算第一个可见文本块
  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  // 计算该块在视口中的顶部和底部坐标
  int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + qRound(blockBoundingRect(block).height());

  // 遍历所有可见块，绘制行号
  while (block.isValid() && top <= area.bottom()) {
    if (block.isVisible() && bottom >= area.top()) {
      QString number = QString::number(blockNumber + 1);
      // 如果该行有错误，用红色显示行号，否则用黑色
      if (m_errorLines.contains(blockNumber)) {
        painter.setPen(Qt::red);
      } else {
        painter.setPen(Qt::black);
      }
      // 右对齐绘制行号，保留 4px 间距
      painter.drawText(0, top, m_lineNumberArea->width() - 4, fontMetrics().height(),
                       Qt::AlignRight, number);
    }
    // 移动到下一个文本块
    block = block.next();
    top = bottom;
    bottom = top + qRound(blockBoundingRect(block).height());
    ++blockNumber;
  }
}

// ──────────────────────────────────────────────────────────────
//  当前行高亮 + 括号匹配
// ──────────────────────────────────────────────────────────────

void CodeEditor::highlightCurrentLine() { refreshExtraSelections(); }

void CodeEditor::refreshExtraSelections() {
  QList<QTextEdit::ExtraSelection> selections;

  // ── 1. 当前行高亮 ──
  // 使用淡黄色背景标记当前行，仅非只读模式下生效
  if (!isReadOnly()) {
    QTextEdit::ExtraSelection sel;
    sel.format.setBackground(AuiStyle::currentLineBackground());
    // FullWidthSelection 确保整行高亮（即使行尾有空行）
    sel.format.setProperty(QTextFormat::FullWidthSelection, true);
    sel.cursor = textCursor();
    sel.cursor.clearSelection();
    selections.append(sel);
  }

  // ── 2. 括号匹配高亮 ──
  // 算法：检查光标左侧字符，判断是否为括号
  // 如果是开括号 → 向右扫描找配对的闭括号
  // 如果是闭括号 → 向左扫描找配对的开括号
  // 使用计数器（depth）跟踪嵌套深度
  QTextCursor cursor = textCursor();
  QString lineText = cursor.block().text();
  int posInBlock = cursor.positionInBlock();

  // 确定检查位置：默认检查光标左侧字符
  int checkPos = posInBlock;
  if (checkPos >= lineText.length() && checkPos > 0) checkPos = posInBlock - 1;

  // Lambda 辅助函数：判断字符类型
  auto isOpen = [](QChar c) { return c == '(' || c == '{' || c == '['; };
  auto isClose = [](QChar c) { return c == ')' || c == '}' || c == ']'; };
  // 查找开括号对应的闭括号
  auto findMatch = [](QChar open) -> QChar {
    if (open == '(') return ')';
    if (open == '{') return '}';
    if (open == '[') return ']';
    return {};
  };
  // 查找闭括号对应的开括号
  auto findOpen = [](QChar close) -> QChar {
    if (close == ')') return '(';
    if (close == '}') return '{';
    if (close == ']') return '[';
    return {};
  };

  if (checkPos >= 0 && checkPos < lineText.length()) {
    QChar ch = lineText[checkPos];
    int matchBlockPos = -1;

    if (isOpen(ch)) {
      // 向右扫描找配对的闭括号
      QChar matchCh = findMatch(ch);
      int depth = 0;
      for (int i = checkPos; i < lineText.length(); ++i) {
        if (lineText[i] == ch)
          depth++;
        else if (lineText[i] == matchCh) {
          depth--;
          if (depth == 0) {
            matchBlockPos = i;
            break;
          }
        }
      }
    } else if (isClose(ch)) {
      // 向左扫描找配对的开括号
      QChar matchCh = findOpen(ch);
      int depth = 0;
      for (int i = checkPos; i >= 0; --i) {
        if (lineText[i] == ch)
          depth++;
        else if (lineText[i] == matchCh) {
          depth--;
          if (depth == 0) {
            matchBlockPos = i;
            break;
          }
        }
      }
    }

    // 找到匹配括号后，用青色背景高亮两个括号
    if (matchBlockPos >= 0) {
      QColor bracketColor = AuiStyle::bracketMatchColor();
      int blockPos = cursor.block().position();

      // 创建高亮选区的辅助 Lambda
      auto makeSel = [&](int pos) {
        QTextEdit::ExtraSelection s;
        s.format.setBackground(bracketColor);
        s.format.setForeground(Qt::white);
        s.format.setFontWeight(QFont::Bold);
        s.cursor = textCursor();
        s.cursor.setPosition(blockPos + pos);
        s.cursor.setPosition(blockPos + pos + 1, QTextCursor::KeepAnchor);
        selections.append(s);
      };
      // 高亮两个匹配的括号
      makeSel(checkPos);
      makeSel(matchBlockPos);
    }
  }

  // ── 3. 错误标记（由验证持久化保存） ──
  // 错误标记不会丢失，即使光标移动或行号变化也保持显示
  selections.append(m_errorSelections);

  setExtraSelections(selections);
}

// ──────────────────────────────────────────────────────────────
//  验证调度
// ──────────────────────────────────────────────────────────────

/**
 * @brief 调度验证（防抖入口）
 *
 * 每次文本变化时调用，重置 500ms 定时器。
 * 用户持续输入时定时器不断被重置，只有停止输入 500ms 后才真正触发验证。
 * 这种"防抖"（debounce）模式避免了频繁的完整文档解析。
 */
void CodeEditor::scheduleValidation() {
  if (m_validationMode == NoValidation) return;
  // 文本已变化，立即清除旧错误标记，防止指向失效位置导致崩溃
  m_errorSelections.clear();
  m_errorLines.clear();
  m_errorRanges.clear();
  m_validationTimer.start();  // 每次调用都会重置计时器
}

/**
 * @brief 执行验证
 *
 * 验证流程：
 * 1. 清除上次的错误标记和错误行号集合
 * 2. 根据 mode 调用对应的验证函数
 * 3. 发出验证结果信号（用于状态栏显示）
 * 4. 刷新 ExtraSelections（重新绘制行高亮、括号匹配、错误标记）
 * 5. 刷新行号区域（错误行号显示红色）
 */
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

/**
 * @brief 使用统一的 IValidator 接口执行验证
 *
 * 替换原来的三个独立验证方法（validateJson/validateTemplate/validateAc），
 * 所有验证器都实现 IValidator 接口，返回统一的 ValidationResult 列表。
 *
 * 验证流程：
 * 1. 调用 validator->validate() 获取验证结果
 * 2. 将每个 ValidationResult 映射为错误字符串
 * 3. 在对应位置标记红色波浪下划线
 * 4. 发出验证结果信号
 */
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
      } else {
        // 没有列号，标记整行
        applyErrorUnderline(blockPos, length, result.message, m_errorSelections);
      }
    }
  }

  // 发出验证结果信号（无错误时传空字符串）
  if (errors.isEmpty()) {
    emit validationMessage(QString());
  } else {
    emit validationMessage(errors.first());
  }
}

// ──────────────────────────────────────────────────────────────
//  错误标记辅助
// ──────────────────────────────────────────────────────────────

/**
 * @brief 将错误区间应用到 ExtraSelection 并标记波浪下划线
 *
 * 创建红色波浪下划线的 ExtraSelection，设置错误提示 Tooltip，
 * 并将错误所在行号记录到 m_errorLines 集合中，用于行号区域红色显示。
 *
 * @param from 错误起始位置（字符偏移量）
 * @param length 错误区间长度
 * @param tooltip 鼠标悬停时显示的提示信息
 * @param selections 输出参数，追加创建的 ExtraSelection
 */
void CodeEditor::applyErrorUnderline(int from, int length, const QString &tooltip,
                                     QList<QTextEdit::ExtraSelection> &selections) {
  QTextEdit::ExtraSelection sel;
  // 不设置下划线样式（由 paintEvent 自定义绘制超粗波浪线）
  // 只设置加粗字体和 Tooltip 提示
  sel.format.setFontWeight(QFont::Bold);
  sel.format.setToolTip(tooltip);
  sel.cursor = textCursor();
  // 设置选区，限制在文档范围内
  sel.cursor.setPosition(qMin(from, document()->characterCount() - 1));
  sel.cursor.setPosition(qMin(from + length, document()->characterCount()),
                         QTextCursor::KeepAnchor);
  selections.append(sel);

  // 记录错误行号，用于行号区域红色显示
  QTextBlock block = document()->findBlock(qMin(from, document()->characterCount() - 1));
  if (block.isValid()) {
    m_errorLines.insert(block.blockNumber());
  }

  // 同时记录错误位置范围（供 paintEvent 自定义绘制使用）
  m_errorRanges.append(ErrorRange(from, length, tooltip));
}

// ──────────────────────────────────────────────────────────────
//  自定义绘制：超粗红色波浪线
// ──────────────────────────────────────────────────────────────

/**
 * @brief 自定义绘制事件（覆盖 QPlainTextEdit 的默认绘制）
 *
 * 绘制流程：
 * 1. 先调用基类方法完成标准绘制（文本、行号、高亮等）
 * 2. 遍历所有错误选区，在每个错误位置额外绘制超粗波浪线
 *    （覆盖默认的细波浪线，实现更醒目的视觉效果）
 *
 * 波浪线参数：
 * - 线宽：3 像素（默认约 1 像素）
 * - 振幅：4 像素（波浪高度）
 * - 波长：8 像素（一个完整波形的宽度）
 * - 颜色：Qt::red
 */
void CodeEditor::paintEvent(QPaintEvent *event) {
  // 步骤1: 调用基类完成标准绘制
  // 这会绘制所有文本、背景、ExtraSelection（包括默认的细波浪线）
  QPlainTextEdit::paintEvent(event);

  // 步骤2: 如果没有错误标记，直接返回
  if (m_errorRanges.isEmpty()) return;

  // 创建画笔用于绘制超粗波浪线
  QPainter painter(viewport());
  painter.setRenderHint(QPainter::Antialiasing);               // 启用抗锯齿，使线条平滑
  painter.setPen(QPen(AuiStyle::errorUnderlineColor(), 1.5));  // 1.5 像素宽的红色画笔

  // 步骤3: 遍历所有错误位置范围，为每个位置绘制超粗波浪线
  for (const auto &range : m_errorRanges) {
    int start = range.start;
    int end = start + range.length;

    // 将文档位置转换为屏幕坐标
    QTextCursor cursor(document());
    cursor.setPosition(start);

    // 遍历选区覆盖的所有文本块（可能跨多行）
    while (!cursor.atEnd() && cursor.position() <= end) {
      QTextBlock block = cursor.block();
      QRect blockRect = blockBoundingGeometry(block).translated(contentOffset()).toRect();

      // 计算当前块内需要绘制的字符范围
      int blockStart = block.position();
      int selStartInBlock = qMax(0, start - blockStart);
      int selEndInBlock = qMin(block.length() - 1, end - blockStart);

      if (selStartInBlock < selEndInBlock) {
        QTextLayout *layout = block.layout();
        if (!layout) {
          cursor.movePosition(QTextCursor::NextBlock);
          continue;
        }

        // 获取起始字符的 x 坐标（使用 QTextLine 的 cursorToX 方法）
        QTextLine line = layout->lineForTextPosition(selStartInBlock);
        if (!line.isValid()) {
          cursor.movePosition(QTextCursor::NextBlock);
          continue;
        }
        int startX = static_cast<int>(line.cursorToX(selStartInBlock));
        // 获取结束字符的 x 坐标
        int endX = static_cast<int>(line.cursorToX(selEndInBlock));

        // 计算波浪线的 y 坐标（文字基线下方，留出足够空间）
        int y = blockRect.bottom() + 3;

        // 波浪线参数（集中定义，便于调整）
        const double waveAmplitude = 1.5;  // 振幅：波浪高度（px）
        const double waveLength = 8.0;     // 波长：一个完整波形的宽度（px）

        // 使用 QPainterPath 绘制连续的波浪线（避免 drawPoint 导致的糊状效果）
        QPainterPath path;
        for (int x = startX; x < endX; ++x) {
          double waveY = y + waveAmplitude * qSin(x * M_PI / (waveLength / 2));
          if (x == startX)
            path.moveTo(x, static_cast<int>(waveY));
          else
            path.lineTo(x, static_cast<int>(waveY));
        }
        painter.drawPath(path);
      }

      // 移动到下一个块
      if (!cursor.movePosition(QTextCursor::NextBlock)) break;
    }
  }
}

// ──────────────────────────────────────────────────────────────
//  ToolTip 事件：鼠标悬停时显示错误提示
// ──────────────────────────────────────────────────────────────

/**
 * @brief 视口事件处理（拦截 ToolTip 事件显示错误提示）
 *
 * 当鼠标悬停在有红色波浪线的错误代码上时，显示具体的错误信息。
 * 遍历 m_errorRanges 查找鼠标位置是否落在某个错误区间内，
 * 如果是则显示对应的 tooltip 文本。
 */
bool CodeEditor::viewportEvent(QEvent *event) {
  if (event->type() == QEvent::ToolTip) {
    QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
    // 将鼠标屏幕坐标转换为文档中的光标位置
    QTextCursor cursor = cursorForPosition(helpEvent->pos());
    int pos = cursor.position();  // 光标在文档中的位置

    // 如果已有自定义弹窗，先隐藏
    hideErrorTooltip();

    // 遍历所有错误范围，查找鼠标位置是否落在某个错误区间内
    for (const auto &range : m_errorRanges) {
      if (pos >= range.start && pos < range.start + range.length) {
        // 找到匹配的错误区间，显示自定义弹窗（可选中/复制）
        showErrorTooltip(helpEvent->globalPos(), range.tooltip);
        return true;
      }
    }

    // 没有匹配的错误，让基类继续处理
    event->ignore();
    return true;
  }
  return QPlainTextEdit::viewportEvent(event);
}

void CodeEditor::showErrorTooltip(const QPoint &pos, const QString &text) {
  hideErrorTooltip();
  auto *tip = new AuiErrorToolTip(text, this);
  tip->move(pos + QPoint(1, 6));  // 略微偏移，避免遮挡代码
  tip->adjustSize();
  tip->show();
  m_errorTooltip = tip;
}

void CodeEditor::hideErrorTooltip() {
  if (m_errorTooltip) {
    m_errorTooltip->close();
    m_errorTooltip.clear();
  }
}

// ──────────────────────────────────────────────────────────────
//  回车自动缩进
// ──────────────────────────────────────────────────────────────

int CodeEditor::calculateNewLineIndent(const QString &linePrefix) const {
  // 计算当前行的前导空格数
  int baseIndent = 0;
  for (const QChar &ch : linePrefix) {
    if (ch == QLatin1Char(' '))
      ++baseIndent;
    else if (ch == QLatin1Char('\t'))
      baseIndent += FormatCode::kIndentSize;
    else
      break;
  }

  QString trimmed = linePrefix.trimmed();
  if (trimmed.isEmpty()) return baseIndent;

  // ── AC / JSON：{ 结尾 → 加一级缩进 ──
  if (trimmed.endsWith(QLatin1Char('{'))) return baseIndent + FormatCode::kIndentSize;

  // ── JSON：[ 结尾 → 加一级缩进 ──
  if (m_validationMode == JsonValidation && trimmed.endsWith(QLatin1Char('[')))
    return baseIndent + FormatCode::kIndentSize;

  // ── TPL：${if ...} / ${each ...} 结尾 → 加一级缩进 ──
  if (m_validationMode == TemplateValidation) {
    QString kEachName = QString::fromLatin1(AcTemplate::kEachPrefix).trimmed();
    QString kIfName = QString::fromLatin1(AcTemplate::kIfPrefix).trimmed();
    static const QRegularExpression openTplRe(QStringLiteral("\\$\\{(") + kEachName +
                                              QStringLiteral("\\b|") + kIfName +
                                              QStringLiteral("\\b)[^}]*\\}$"));
    if (openTplRe.match(trimmed).hasMatch()) return baseIndent + FormatCode::kIndentSize;

    // ${/each} / ${/if} 单独一行 → 减一级缩进
    static const QRegularExpression closeTplRe(QStringLiteral("\\$\\{/(") + kEachName +
                                               QStringLiteral("|") + kIfName +
                                               QStringLiteral(")\\}$"));
    if (closeTplRe.match(trimmed).hasMatch()) return qMax(0, baseIndent - FormatCode::kIndentSize);
  }

  return baseIndent;
}

void CodeEditor::keyPressEvent(QKeyEvent *event) {
  // ── 补全器处于弹出状态时，优先由补全器处理 ──
  if (m_completer && m_completer->popup()->isVisible()) {
    switch (event->key()) {
      case Qt::Key_Enter:
      case Qt::Key_Return:
      case Qt::Key_Tab: {
        // 优先使用弹出列表中用户用方向键选中的行
        QModelIndex idx = m_completer->popup()->currentIndex();
        if (!idx.isValid()) idx = m_completer->currentIndex();  // 退回到默认第一行
        if (idx.isValid()) m_completer->activated(idx.data().toString());
        return;
      }
      case Qt::Key_Escape:
        m_completer->popup()->hide();
        return;
      case Qt::Key_Up:
      case Qt::Key_Down:
      case Qt::Key_PageUp:
      case Qt::Key_PageDown:
        // 方向键让补全器自己处理
        QPlainTextEdit::keyPressEvent(event);
        return;
      default:
        break;
    }
  }

  // ── Tab → 2 空格（补全弹出时由补全器优先处理，不走到这里） ──
  if (event->key() == Qt::Key_Tab) {
    // Shift+Tab：反向缩进
    if (event->modifiers() & Qt::ShiftModifier) {
      QTextCursor cursor = textCursor();
      cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
      QString line = cursor.selectedText();
      // 去掉开头的 kIndentSize 个空格
      int remove = 0;
      while (remove < FormatCode::kIndentSize && remove < line.length() &&
             line[remove] == QLatin1Char(' ')) {
        ++remove;
      }
      if (remove > 0) {
        cursor.beginEditBlock();
        for (int i = 0; i < remove; ++i) cursor.deleteChar();
        cursor.endEditBlock();
      }
      return;
    }
    // Tab → 插入 2 空格
    insertPlainText(QString(FormatCode::kIndentSize, QLatin1Char(' ')));
    return;
  }

  // ── 键入开字符时自动补全闭字符 ──
  if (event->text().length() == 1 && m_validationMode != NoValidation) {
    QChar ch = event->text()[0];
    if (FormatCode::isOpenChar(ch)) {
      // 获取光标前一个字符（用于 " 的特殊判断）
      QTextCursor tc = textCursor();
      tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 1);
      QChar charBefore = tc.selectedText().isEmpty() ? QChar() : tc.selectedText()[0];

      if (FormatCode::shouldAutoPair(ch, charBefore)) {
        QChar close = FormatCode::matchingCloseChar(ch);
        insertPlainText(QString(ch) + close);
        // 光标回到中间
        QTextCursor back = textCursor();
        back.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 1);
        setTextCursor(back);
        return;
      }
    }
  }

  // ── Enter 自动缩进 + 拆分配对块 ──
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    // 检查光标后是否是某个闭字符
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
    QString nextText = cursor.selectedText();
    QChar nextChar = nextText.isEmpty() ? QChar() : nextText[0];
    bool hasClosePair = !nextChar.isNull() && FormatCode::isCloseChar(nextChar);

    // 恢复光标位置
    cursor.setPosition(textCursor().position());
    cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
    QString linePrefix = cursor.selectedText();
    int indent = calculateNewLineIndent(linePrefix);

    if (hasClosePair) {
      // 删除后面的闭字符
      QTextCursor tc = textCursor();
      tc.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
      tc.removeSelectedText();

      // 换行 + 缩进（块内）
      QPlainTextEdit::keyPressEvent(event);
      if (indent > 0) insertPlainText(QString(indent, QLatin1Char(' ')));

      // 再换行 + 反缩进 + 闭字符
      int closeIndent = qMax(0, indent - FormatCode::kIndentSize);
      insertPlainText(QStringLiteral("\n") + QString(closeIndent, QLatin1Char(' ')) + nextChar);

      // 光标回到中间行末尾
      QTextCursor back = textCursor();
      back.movePosition(QTextCursor::Up, QTextCursor::MoveAnchor);
      back.movePosition(QTextCursor::EndOfLine, QTextCursor::MoveAnchor);
      setTextCursor(back);
    } else {
      // 普通 Enter 自动缩进
      QPlainTextEdit::keyPressEvent(event);
      if (indent > 0) insertPlainText(QString(indent, QLatin1Char(' ')));
    }
    return;
  }

  // ── 键入闭字符时自动反向缩进 ──
  if (event->text().length() == 1 && m_validationMode != NoValidation) {
    QChar ch = event->text()[0];
    if (FormatCode::isCloseChar(ch)) {
      QTextCursor tc = textCursor();
      tc.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
      QString beforeCursor = tc.selectedText();
      // 光标前只有空白 → 删除空白，在正确层级插入括号
      if (!beforeCursor.isEmpty() && beforeCursor.trimmed().isEmpty()) {
        int currentIndent = beforeCursor.length();
        int correctIndent = qMax(0, currentIndent - FormatCode::kIndentSize);
        tc.removeSelectedText();
        if (correctIndent > 0) insertPlainText(QString(correctIndent, QLatin1Char(' ')));
        insertPlainText(QString(ch));
        return;
      }
    }
  }

  // ── 普通按键 ──
  QPlainTextEdit::keyPressEvent(event);

  // ── Backspace 后刷新补全 ──
  if (m_completer && event->key() == Qt::Key_Backspace) {
    showCompleter();
  }

  // ── 键入非单词字符后自动关闭补全（Backspace / . 已单独处理） ──
  if (m_completer && m_completer->popup()->isVisible()) {
    if (event->key() != Qt::Key_Backspace && event->text() != QStringLiteral(".") &&
        (event->text().length() != 1 || !event->text()[0].isLetterOrNumber())) {
      m_completer->popup()->hide();
    }
  }

  // ── 键入 . 后弹出补全（对象方法访问） ──
  if (m_completer && event->text() == QStringLiteral(".")) {
    showCompleter();
  }

  // ── 键入空格后弹出补全（检测 new 上下文） ──
  if (m_completer && event->key() == Qt::Key_Space) {
    showCompleter();
  }

  // ── 键入单词字符后弹出补全 ──
  if (m_completer && event->text().length() == 1 && event->text()[0].isLetterOrNumber()) {
    showCompleter();
  }
}

// ──────────────────────────────────────────────────────────────
//  代码补全
// ──────────────────────────────────────────────────────────────

void CodeEditor::initCompleter(ValidationMode mode) {
  // 清理旧的补全器
  if (m_completer) {
    disconnect(m_completer, nullptr, this, nullptr);
    m_completer->deleteLater();
    m_completer = nullptr;
  }

  if (mode == NoValidation) return;

  m_completer = GuessCode::createCompleter(static_cast<int>(mode), this);
  if (!m_completer) return;

  m_completer->setWidget(this);
  connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated), this,
          &CodeEditor::insertCompletion);
}

void CodeEditor::insertCompletion(const QString &completion) {
  // 与 showCompleter() 保持一致：手动向左扫描单词前缀并删除
  QTextCursor tc = textCursor();
  int pos = tc.position();
  QString text = toPlainText();
  int start = pos;
  while (start > 0 && (text[start - 1].isLetterOrNumber() || text[start - 1] == '_')) --start;
  if (start < pos) {
    tc.setPosition(start);
    tc.setPosition(pos, QTextCursor::KeepAnchor);
    tc.removeSelectedText();
  }
  // 插入补全后的文本
  insertPlainText(completion);
  // 若补全内容以 () 结尾，光标移到括号中间
  if (completion.endsWith(QStringLiteral("()"))) {
    tc = textCursor();
    tc.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 1);
    setTextCursor(tc);
  }
  // 关闭补全弹窗
  if (m_completer) m_completer->popup()->hide();
}

/// @brief 弹出补全建议列表
static void showCompleterInternal(QCompleter *c) {
  auto *editor = static_cast<QPlainTextEdit *>(c->widget());
  QRect cr = editor->cursorRect();
  cr.setWidth(c->popup()->sizeHintForColumn(0) +
              c->popup()->verticalScrollBar()->sizeHint().width() + 20);
  c->complete(cr);
}

void CodeEditor::showCompleter() {
  if (!m_completer) return;

  // 手动从光标位置向左扫描，提取单词前缀
  // 不使用 QTextCursor::StartOfWord，因为它在某些标点后的位置行为不稳定
  int pos = textCursor().position();
  QString text = toPlainText();
  int start = pos;
  // 从光标前一个字符开始向左扫描单词字符（字母、数字、下划线）
  while (start > 0 && (text[start - 1].isLetterOrNumber() || text[start - 1] == '_')) --start;
  QString prefix = text.mid(start, pos - start);

  // ── 检测点上下文：光标前是 obj. 模式 ──
  bool isDotContext = false;
  QString objName;
  if (start > 0 && text[start - 1] == '.') {
    // 提取 . 前面的对象名
    int objStart = start - 2;
    while (objStart >= 0 && (text[objStart].isLetterOrNumber() || text[objStart] == '_'))
      --objStart;
    objName = text.mid(objStart + 1, start - 2 - objStart);
    isDotContext = !objName.isEmpty();
  }

  // ── 检测 new 上下文：光标前是 "new " 模式（中间可有空格） ──
  bool isNewContext = false;
  if (!isDotContext) {
    int ctxPos = start;
    // 跳过光标前的所有空白
    while (ctxPos > 0 && text[ctxPos - 1].isSpace()) --ctxPos;
    // 检查 "new" 关键字（前面必须是单词边界或行首）
    if (ctxPos >= 3 && text.mid(ctxPos - 3, 3) == QString::fromLatin1(AcKeyword::kNew)) {
      if (ctxPos == 3 || !(text[ctxPos - 4].isLetterOrNumber() || text[ctxPos - 4] == '_')) {
        isNewContext = true;
      }
    }
  }

  // ── 点上下文：只显示该对象类型的方法 ──
  QStringList words;
  if (isDotContext) {
    QHash<QString, QString> varTypes = GuessCode::extractVariableTypes(text);
    QHash<QString, QStringList> classMethods = GuessCode::extractClassMethods(text);
    QString typeName = varTypes.value(objName);
    if (!typeName.isEmpty()) words = classMethods.value(typeName);
    if (words.isEmpty()) {
      m_completer->popup()->hide();
      return;
    }
  } else {
    // new 上下文中允许空前缀（刚敲 "new " 时空格后无单词字符）
    if (prefix.isEmpty() && !isNewContext) {
      m_completer->popup()->hide();
      return;
    }

    // ── 合并静态词库 + 动态 let 变量 + 用户自定义函数/类 ──
    GuessCode::FileType ft = GuessCode::validationModeToFileType(m_validationMode);
    words = GuessCode::getAllCompletions(ft);
    int cursorLine = textCursor().blockNumber() + 1;  // 行号从 1 开始
    words << GuessCode::extractLetVariables(text, cursorLine);

    if (ft == GuessCode::AcFile) {
      // new 上下文：只提示类名（原生类 + 用户自定义类），名称带 ()
      if (isNewContext) {
        words.clear();
        for (const auto &cls : AcClass::kAll) words << cls + QStringLiteral("()");
        for (const auto &cls : GuessCode::extractUserClasses(text, cursorLine))
          words << cls + QStringLiteral("()");
      } else {
        words << GuessCode::extractUserFunctions(text, cursorLine);
        words << GuessCode::extractUserClasses(text, cursorLine);
      }
    }
  }

  words.removeDuplicates();

  // 更新补全模型
  auto *model = qobject_cast<QStringListModel *>(m_completer->model());
  if (model) {
    model->setStringList(words);
  } else {
    model = new QStringListModel(words, m_completer);
    m_completer->setModel(model);
  }

  // 设置补全前缀，过滤列表
  m_completer->setCompletionPrefix(prefix);

  if (m_completer->completionCount() > 0) {
    // 默认选中第一项
    m_completer->setCurrentRow(0);
    showCompleterInternal(m_completer);
  } else {
    m_completer->popup()->hide();
  }
}

// ──────────────────────────────────────────────────────────────
//  右键菜单 — 增加「格式化代码」
// ──────────────────────────────────────────────────────────────

void CodeEditor::contextMenuEvent(QContextMenuEvent *event) {
  // 创建标准右键菜单
  QMenu *menu = createStandardContextMenu();

  // 只在支持的验证模式下添加格式化功能（JSON / AC / TPL）
  if (m_validationMode != NoValidation) {
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
      mode = FormatCode::FormatJson;
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