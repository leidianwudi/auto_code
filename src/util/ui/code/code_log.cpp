/**
 * @file code_log.cpp
 * @brief 日志输出控件实现
 */

#include "code_log.h"

#include <QPainter>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>

#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════════

CodeLog::CodeLog(QWidget *parent) : QPlainTextEdit(parent) {
  setReadOnly(true);
  setMaximumBlockCount(5000);  // 限制行数，防止内存溢出
  document()->setDocumentMargin(0);

  // 使用 AuiStyle 统一样式表
  setStyleSheet(AuiStyle::logPanelStyleSheet());
  // setFont(AuiStyle::createLogFont());  // 字体大小 10pt，等宽，列间距归零

  // 行间隔为 0 — 只使用字体本身高度，无额外间距
  QTextBlockFormat blockFmt = AuiStyle::createLogBlockFormat(font());
  QTextCursor cur(document());
  cur.movePosition(QTextCursor::Start);
  cur.setBlockFormat(blockFmt);

  // 行号区域
  m_lineNumberArea = new LineNumberArea(this);
  connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeLog::updateLineNumberAreaWidth);
  connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect &rect, int dy) {
    if (dy)
      m_lineNumberArea->scroll(0, dy);
    else
      m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
  });

  updateLineNumberAreaWidth(0);
}

// ════════════════════════════════════════════════════════════════
//  公开接口
// ════════════════════════════════════════════════════════════════

void CodeLog::append(const QString &text, bool isError) {
  QTextDocument *doc = document();

  // 去掉文本末尾的换行符，避免产生多余空行
  QString cleanText = text;
  while (cleanText.endsWith('\n')) cleanText.chop(1);

  QTextCharFormat msgFmt;
  if (isError) {
    msgFmt.setForeground(AuiStyle::errorTextColor());
  } else {
    msgFmt.setForeground(AuiStyle::textColor());
  }

  QTextCursor cursor(doc);
  cursor.movePosition(QTextCursor::End);

  // 如果文档末尾有空的尾随段落（QPlainTextEdit 默认行为），回退到前一个块
  QTextBlock lastBlock = doc->begin();
  while (lastBlock.next().isValid()) lastBlock = lastBlock.next();
  if (lastBlock.text().isEmpty() && doc->blockCount() > 1) {
    cursor.movePosition(QTextCursor::PreviousBlock);
    cursor.movePosition(QTextCursor::EndOfBlock);
  }

  // 非首条日志时，先插入新块再追加（同时指定行块格式）
  if (cursor.position() > 0) {
    QTextBlockFormat lineBlockFmt = AuiStyle::createLogBlockFormat(font());
    cursor.insertBlock(lineBlockFmt, QTextCharFormat());
  }

  cursor.insertText(cleanText, msgFmt);

  setTextCursor(cursor);
  ensureCursorVisible();

  ++m_lineNumber;
  m_lineNumberArea->update();
}

void CodeLog::clearLog() {
  clear();
  m_lineNumber = 0;
  updateLineNumberAreaWidth(0);
}

// ════════════════════════════════════════════════════════════════
//  行号区域
// ════════════════════════════════════════════════════════════════

int CodeLog::lineNumberAreaWidth() const {
  int digits = 1;
  int max = qMax(1, m_lineNumber);
  while (max >= 10) {
    max /= 10;
    ++digits;
  }
  return 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits + 4;
}

void CodeLog::updateLineNumberAreaWidth(int) { setViewportMargins(lineNumberAreaWidth(), 0, 0, 0); }

void CodeLog::lineNumberAreaPaintEvent(QPaintEvent *event, const QRect &area) {
  QPainter painter(m_lineNumberArea);
  painter.fillRect(area, AuiStyle::lineNumberBackground());

  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + qRound(blockBoundingRect(block).height());

  painter.setFont(font());

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      int lineNum = blockNumber + 1;
      if (lineNum <= m_lineNumber) {
        painter.setPen(AuiStyle::textColor());
        painter.drawText(0, top, area.width() - 4, painter.fontMetrics().height(), Qt::AlignRight,
                         QString::number(lineNum));
      }
    }
    block = block.next();
    top = bottom;
    bottom = top + qRound(blockBoundingRect(block).height());
    ++blockNumber;
  }
}

// ════════════════════════════════════════════════════════════════
//  事件重写
// ════════════════════════════════════════════════════════════════

void CodeLog::resizeEvent(QResizeEvent *event) {
  QPlainTextEdit::resizeEvent(event);
  QRect cr = contentsRect();
  m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}