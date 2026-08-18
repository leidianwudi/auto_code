/**
 * @file code_log.cpp
 * @brief 日志输出控件实现
 */

#include "code_log.h"

#include <QFontDatabase>
#include <QPainter>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>

#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/setting_store.h"

// 自定义字符格式属性：标记错误文本（用于主题切换时重建颜色）
enum { PropertyIsError = QTextFormat::UserProperty + 1 };

// ════════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════════

CodeLog::CodeLog(QWidget *parent) : QPlainTextEdit(parent) {
  setReadOnly(true);
  setLineWrapMode(QPlainTextEdit::NoWrap);  // 长行不自动换行，使用水平滚动条
  setMaximumBlockCount(5000);               // 限制行数，防止内存溢出
  document()->setDocumentMargin(0);

  // 字体跟随「代码字体」设置（大小 + 字体族）
  applyFontFromSetting();

  // 使用 AuiStyle 统一样式表（背景/文字色随主题）
  setStyleSheet(AuiStyle::logPanelStyleSheet());

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

  // 字体大小 / 字体族变化时即时刷新
  connect(&SettingStore::ins(), &SettingStore::fontsChanged, this, &CodeLog::reloadStyle);

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
    // 错误文本：标记属性 + 当前主题的错误红色
    msgFmt.setProperty(PropertyIsError, true);
    msgFmt.setForeground(AuiStyle::errorTextColor());
  }
  // 普通日志不设前景色，沿用样式表文字色（主题切换时自动跟随）

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

void CodeLog::reloadStyle() {
  // 字体（大小/字体族）随设置更新
  applyFontFromSetting();
  // 重新应用样式表（背景/文字色随当前主题重建）
  setStyleSheet(AuiStyle::logPanelStyleSheet());

  // 重建已有文本的字符颜色：错误红随当前主题刷新；普通文本沿用样式表文字色（不设前景色）
  QTextDocument *doc = document();
  QTextCursor cur(doc);
  QTextBlock block = doc->begin();
  while (block.isValid()) {
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
      const QTextFragment frag = it.fragment();
      if (!frag.isValid()) continue;
      if (frag.charFormat().property(PropertyIsError).toBool()) {
        QTextCharFormat nf = frag.charFormat();
        nf.setForeground(AuiStyle::errorTextColor());
        cur.setPosition(frag.position());
        cur.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
        cur.setCharFormat(nf);
      }
    }
    block = block.next();
  }

  viewport()->update();
  m_lineNumberArea->update();
}

// ════════════════════════════════════════════════════════════════
//  字体设置
// ════════════════════════════════════════════════════════════════

void CodeLog::applyFontFromSetting() {
  QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  // 字体族：跟随「代码字体」设置（未设置时用系统等宽字体）
  const QString fam = SettingStore::ins().fontFamily(QStringLiteral("font.code"));
  if (!fam.isEmpty()) font.setFamily(fam);
  font.setPointSize(SettingStore::ins().fontSize(QStringLiteral("font.code")));
  setFont(font);
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