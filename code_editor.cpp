/**
 * @file code_editor.cpp
 * @brief 代码编辑器控件实现
 */

#include "code_editor.h"
#include <QJsonDocument>
#include <QMap>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStack>
#include <QTextBlock>
#include <QToolTip>

// ──────────────────────────────────────────────────────────────
//  构造 / 基本接口
// ──────────────────────────────────────────────────────────────

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent), m_lineNumberArea(new LineNumberArea(this)) {
  QFont font;
  font.setFamily("Consolas");
  font.setFixedPitch(true);
  font.setPointSize(11);
  setFont(font);

  // 行号 + 当前行高亮
  connect(this, &QPlainTextEdit::blockCountChanged, this,
          &CodeEditor::updateLineNumberAreaWidth);
  connect(this, &QPlainTextEdit::updateRequest, this,
          &CodeEditor::updateLineNumberArea);
  connect(this, &QPlainTextEdit::cursorPositionChanged, this,
          [this]() { highlightCurrentLine(); });

  // 防抖定时器
  m_validationTimer.setSingleShot(true);
  m_validationTimer.setInterval(500); // 500ms 防抖
  connect(&m_validationTimer, &QTimer::timeout, this,
          &CodeEditor::performValidation);

  // 文本变化时触发验证
  connect(this, &QPlainTextEdit::textChanged, this,
          &CodeEditor::scheduleValidation);

  updateLineNumberAreaWidth(0);
  highlightCurrentLine();
}

void CodeEditor::setValidationMode(ValidationMode mode) {
  m_validationMode = mode;
  performValidation(); // 立即做一次验证
}

// ──────────────────────────────────────────────────────────────
//  行号区域
// ──────────────────────────────────────────────────────────────

int CodeEditor::lineNumberAreaWidth() const {
  int digits = 1;
  int max = qMax(1, blockCount());
  while (max >= 10) {
    max /= 10;
    ++digits;
  }
  return 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void CodeEditor::updateLineNumberAreaWidth(int) {
  setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
  if (dy)
    m_lineNumberArea->scroll(0, dy);
  else
    m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(),
                             rect.height());

  if (rect.contains(viewport()->rect()))
    updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *event) {
  QPlainTextEdit::resizeEvent(event);
  QRect cr = contentsRect();
  m_lineNumberArea->setGeometry(
      QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event,
                                          const QRect &area) {
  QPainter painter(m_lineNumberArea);
  painter.fillRect(area, QColor(Qt::lightGray).lighter(110));

  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top =
      qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + qRound(blockBoundingRect(block).height());

  while (block.isValid() && top <= area.bottom()) {
    if (block.isVisible() && bottom >= area.top()) {
      QString number = QString::number(blockNumber + 1);
      // 如果该行有错误，用红色显示行号
      if (m_errorLines.contains(blockNumber)) {
        painter.setPen(Qt::red);
      } else {
        painter.setPen(Qt::black);
      }
      painter.drawText(0, top, m_lineNumberArea->width() - 4,
                       fontMetrics().height(), Qt::AlignRight, number);
    }
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

  // 1. 当前行高亮
  if (!isReadOnly()) {
    QTextEdit::ExtraSelection sel;
    sel.format.setBackground(QColor(Qt::yellow).lighter(180));
    sel.format.setProperty(QTextFormat::FullWidthSelection, true);
    sel.cursor = textCursor();
    sel.cursor.clearSelection();
    selections.append(sel);
  }

  // 2. 括号匹配高亮
  QTextCursor cursor = textCursor();
  QString lineText = cursor.block().text();
  int posInBlock = cursor.positionInBlock();

  int checkPos = posInBlock;
  if (checkPos >= lineText.length() && checkPos > 0)
    checkPos = posInBlock - 1;

  auto isOpen = [](QChar c) { return c == '(' || c == '{' || c == '['; };
  auto isClose = [](QChar c) { return c == ')' || c == '}' || c == ']'; };
  auto findMatch = [](QChar open) -> QChar {
    if (open == '(')
      return ')';
    if (open == '{')
      return '}';
    if (open == '[')
      return ']';
    return {};
  };
  auto findOpen = [](QChar close) -> QChar {
    if (close == ')')
      return '(';
    if (close == '}')
      return '{';
    if (close == ']')
      return '[';
    return {};
  };

  if (checkPos >= 0 && checkPos < lineText.length()) {
    QChar ch = lineText[checkPos];
    int matchBlockPos = -1;

    if (isOpen(ch)) {
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

    if (matchBlockPos >= 0) {
      QColor bracketColor(0, 200, 200);
      int blockPos = cursor.block().position();

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
      makeSel(checkPos);
      makeSel(matchBlockPos);
    }
  }

  // 3. 错误标记（由验证持久化保存）
  selections.append(m_errorSelections);

  setExtraSelections(selections);
}

// ──────────────────────────────────────────────────────────────
//  验证调度
// ──────────────────────────────────────────────────────────────

void CodeEditor::scheduleValidation() {
  if (m_validationMode == NoValidation)
    return;
  m_validationTimer.start();
}

void CodeEditor::performValidation() {
  if (m_validationMode == NoValidation)
    return;

  // 清除旧的错误标记
  m_errorSelections.clear();
  m_errorLines.clear();

  QStringList errors;
  if (m_validationMode == JsonValidation) {
    errors = validateJson();
  } else if (m_validationMode == TemplateValidation) {
    errors = validateTemplate();
  }

  if (errors.isEmpty()) {
    emit validationMessage(QString());
  } else {
    emit validationMessage(errors.first());
  }

  // 刷新 ExtraSelections（重新绘制行高亮 + 括号匹配 + 错误标记）
  refreshExtraSelections();

  // 刷新行号区域（错误行号显示红色）
  m_lineNumberArea->update();
}

// ──────────────────────────────────────────────────────────────
//  JSON 验证
// ──────────────────────────────────────────────────────────────

QStringList CodeEditor::validateJson() {
  QStringList errors;
  QString text = toPlainText();

  if (text.trimmed().isEmpty()) {
    return errors;
  }

  QJsonParseError parseError;
  QJsonDocument::fromJson(text.toUtf8(), &parseError);

  if (parseError.error != QJsonParseError::NoError) {
    int offset = parseError.offset;
    // 将偏移量转换为 block/position
    QTextBlock block = document()->begin();
    int blockStart = 0;
    while (block.isValid() && blockStart + block.length() <= offset) {
      blockStart += block.length();
      block = block.next();
    }

    int line = block.isValid() ? block.blockNumber() + 1 : 1;
    int col = block.isValid() ? offset - blockStart + 1 : 1;

    QString msg = QStringLiteral("行 %1, 列 %2: %3")
                      .arg(line)
                      .arg(col)
                      .arg(parseError.errorString());
    errors << msg;

    // 标记错误位置（红色波浪下划线）
    int markPos = qMin(offset, text.length() - 1);
    if (markPos < 0)
      markPos = 0;
    applyErrorUnderline(markPos, 1, msg, m_errorSelections);
  }

  return errors;
}

// ──────────────────────────────────────────────────────────────
//  模板验证（括号 + 标签匹配）
// ──────────────────────────────────────────────────────────────

QStringList CodeEditor::validateTemplate() {
  QStringList errors;
  QString text = toPlainText();
  int len = text.length();

  if (text.isEmpty())
    return errors;

  // ── 1. 检查普通括号 () [] {} 匹配 ──
  // 排除字符串和 ${...} 内部的括号
  struct BracketInfo {
    QChar ch;
    int pos;
  };
  QStack<BracketInfo> stack;
  bool inString = false;
  bool inTemplateExpr = false;
  int templateExprDepth = 0;

  for (int i = 0; i < len; ++i) {
    QChar ch = text[i];

    // 跟踪 ${...} 表达式内部
    if (ch == '$' && i + 1 < len && text[i + 1] == '{') {
      inTemplateExpr = true;
      templateExprDepth++;
      i++; // 跳过 '{'
      continue;
    }
    if (inTemplateExpr) {
      if (ch == '{') {
        templateExprDepth++;
      } else if (ch == '}') {
        templateExprDepth--;
        if (templateExprDepth <= 0) {
          inTemplateExpr = false;
          templateExprDepth = 0;
        }
      }
      continue; // 跳过 ${...} 内部的括号
    }

    // 跟踪字符串内部
    if ((ch == '"' || ch == '\'') && (i == 0 || text[i - 1] != '\\')) {
      inString = !inString;
      continue;
    }
    if (inString)
      continue;

    // 跳过注释
    if (ch == '/' && i + 1 < len && text[i + 1] == '/') {
      // 跳到行尾
      while (i < len && text[i] != '\n')
        ++i;
      continue;
    }

    if (ch == '(' || ch == '[') {
      stack.push({ch, i});
    } else if (ch == ')' || ch == ']') {
      QChar expected = (ch == ')') ? '(' : '[';
      if (stack.isEmpty()) {
        QString msg =
            QStringLiteral("多余的 '%1' 在位置 %2").arg(ch).arg(i + 1);
        errors << msg;
        applyErrorUnderline(i, 1, msg, m_errorSelections);
      } else if (stack.top().ch != expected) {
        QString msg = QStringLiteral("括号不匹配: '%1' 在位置 %2，期望 '%3'")
                          .arg(ch)
                          .arg(i + 1)
                          .arg(expected);
        errors << msg;
        applyErrorUnderline(i, 1, msg, m_errorSelections);
        // 弹出直到找到匹配的
        while (!stack.isEmpty() && stack.top().ch != expected) {
          stack.pop();
        }
        if (!stack.isEmpty())
          stack.pop();
      } else {
        stack.pop();
      }
    }
  }

  // 未闭合的开括号
  while (!stack.isEmpty()) {
    auto info = stack.pop();
    QString msg = QStringLiteral("未闭合的 '%1' 在位置 %2")
                      .arg(info.ch)
                      .arg(info.pos + 1);
    errors << msg;
    applyErrorUnderline(info.pos, 1, msg, m_errorSelections);
  }

  // ── 2. 检查 ${...} 未闭合 ──
  for (int i = 0; i < len; ++i) {
    if (text[i] == '$' && i + 1 < len && text[i + 1] == '{') {
      int depth = 1;
      int start = i;
      i += 2;
      while (i < len && depth > 0) {
        if (text[i] == '{')
          depth++;
        else if (text[i] == '}')
          depth--;
        if (depth > 0)
          i++;
      }
      if (depth > 0) {
        QString msg = QStringLiteral("未闭合的 ${ 在位置 %1").arg(start + 1);
        errors << msg;
        applyErrorUnderline(start, 2, msg, m_errorSelections);
      }
    }
  }

  // ── 3. 检查 ${each}...${/each} 和 ${if}...${/if} 匹配 ──
  static const QRegularExpression tagRegex(
      QStringLiteral(R"(\$\{(each|if|else|/each|/if)\b[^}]*\})"));

  struct TagInfo {
    QString tag;
    int pos;
    int length;
  };
  QStack<TagInfo> tagStack;

  auto it = tagRegex.globalMatch(text);
  while (it.hasNext()) {
    auto match = it.next();
    QString tag = match.captured(1);
    int pos = match.capturedStart();
    int tagLen = match.capturedLength();

    if (tag == "each" || tag == "if") {
      tagStack.push({tag, pos, tagLen});
    } else if (tag == "/each" || tag == "/if") {
      QString openTag = tag.mid(1); // "each" or "if"
      if (tagStack.isEmpty()) {
        QString msg =
            QStringLiteral("多余的 ${/%1} 在位置 %2").arg(openTag).arg(pos + 1);
        errors << msg;
        applyErrorUnderline(pos, tagLen, msg, m_errorSelections);
      } else if (tagStack.top().tag != openTag) {
        QString msg =
            QStringLiteral("标签不匹配: ${/%1} 在位置 %2，期望 ${/%3}")
                .arg(openTag)
                .arg(pos + 1)
                .arg(tagStack.top().tag);
        errors << msg;
        applyErrorUnderline(pos, tagLen, msg, m_errorSelections);
        // 尝试恢复：弹出直到找到匹配
        while (!tagStack.isEmpty() && tagStack.top().tag != openTag) {
          auto unmatched = tagStack.pop();
          QString unMsg = QStringLiteral("未闭合的 ${%1} 在位置 %2")
                              .arg(unmatched.tag)
                              .arg(unmatched.pos + 1);
          errors << unMsg;
          applyErrorUnderline(unmatched.pos, unmatched.length, unMsg,
                              m_errorSelections);
        }
        if (!tagStack.isEmpty())
          tagStack.pop();
      } else {
        tagStack.pop();
      }
    }
    // ${else} 不压栈也不弹出
  }

  // 未闭合的标签
  while (!tagStack.isEmpty()) {
    auto info = tagStack.pop();
    QString msg = QStringLiteral("未闭合的 ${%1} 在位置 %2")
                      .arg(info.tag)
                      .arg(info.pos + 1);
    errors << msg;
    applyErrorUnderline(info.pos, info.length, msg, m_errorSelections);
  }

  // ── 4. 检查 ${...} 表达式中不支持的方法调用 ──
  static const QStringList supportedMethods = {
      QStringLiteral("toLowerCase"), QStringLiteral("toUpperCase"),
      QStringLiteral("trim"), QStringLiteral("capitalize")};
  // 匹配 ${...} 中的普通表达式（排除 each/if/else//each//if 标签）
  static const QRegularExpression exprRegex(
      QStringLiteral(R"(\$\{(?!(?:each|if|else|/each|/if)\b)[^}]+\})"));
  // 合法标识符：字母或下划线开头，只含字母、数字、下划线
  static const QRegularExpression identRegex(
      QStringLiteral(R"(^[a-zA-Z_][a-zA-Z0-9_]*$)"));
  // 判断段是否含有方法调用特征（含大写字母，或以数字/非字母开头）
  auto looksLikeMethod = [](const QString &seg) -> bool {
    if (seg.isEmpty())
      return false;
    // 以数字或非字母开头 → 不合法
    if (!seg[0].isLetter() && seg[0] != '_')
      return true;
    // 含大写字母 → 驼峰方法风格
    for (const QChar &c : seg) {
      if (c.isUpper())
        return true;
    }
    return false;
  };

  auto exprIt = exprRegex.globalMatch(text);
  while (exprIt.hasNext()) {
    auto exprMatch = exprIt.next();

    // 跳过注释中的表达式
    int lineStart =
        text.lastIndexOf(QLatin1Char('\n'), exprMatch.capturedStart());
    if (lineStart == -1)
      lineStart = 0;
    QString linePrefix =
        text.mid(lineStart, exprMatch.capturedStart() - lineStart);
    if (linePrefix.contains(QStringLiteral("//")))
      continue;

    // 去掉 ${ 和 } 取表达式内容
    QString exprContent =
        exprMatch.captured().mid(2, exprMatch.capturedLength() - 3);
    int exprStart =
        exprMatch.capturedStart() + 2; // 表达式内容在文本中的起始位置

    // 按点号分割路径段
    QStringList segments = exprContent.split(QLatin1Char('.'));
    if (segments.size() < 2)
      continue; // 没有点号，跳过

    // 第一段是变量名，后续段可能是属性或方法
    int segPos = exprStart;
    segPos += segments[0].length() + 1; // 跳过第一段和点号

    for (int s = 1; s < segments.size(); ++s) {
      QString seg = segments[s].trimmed();

      if (looksLikeMethod(seg) && !supportedMethods.contains(seg)) {
        // 方法名风格且不在支持列表
        QString msg =
            QStringLiteral("不支持的方法 '%1'，支持: %2")
                .arg(seg, supportedMethods.join(QStringLiteral(", ")));
        errors << msg;
        applyErrorUnderline(segPos, seg.length(), msg, m_errorSelections);
      } else if (!identRegex.match(seg).hasMatch() &&
                 !supportedMethods.contains(seg)) {
        // 不是合法标识符也不是已知方法
        QString msg = QStringLiteral("无效的标识符 '%1'").arg(seg);
        errors << msg;
        applyErrorUnderline(segPos, seg.length(), msg, m_errorSelections);
      }

      // 跳到下一个段
      if (s + 1 < segments.size())
        segPos += seg.length() + 1;
    }
  }

  return errors;
}

// ──────────────────────────────────────────────────────────────
//  错误标记辅助
// ──────────────────────────────────────────────────────────────

void CodeEditor::applyErrorUnderline(
    int from, int length, const QString &tooltip,
    QList<QTextEdit::ExtraSelection> &selections) {
  QTextEdit::ExtraSelection sel;
  sel.format.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
  sel.format.setUnderlineColor(Qt::red);
  sel.format.setToolTip(tooltip);
  sel.cursor = textCursor();
  sel.cursor.setPosition(qMin(from, document()->characterCount() - 1));
  sel.cursor.setPosition(qMin(from + length, document()->characterCount()),
                         QTextCursor::KeepAnchor);
  selections.append(sel);

  // 记录错误行号，用于行号区域红色显示
  QTextBlock block =
      document()->findBlock(qMin(from, document()->characterCount() - 1));
  if (block.isValid()) {
    m_errorLines.insert(block.blockNumber());
  }
}