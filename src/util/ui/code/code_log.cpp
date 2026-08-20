/**
 * @file code_log.cpp
 * @brief 日志输出控件实现
 */

#include "code_log.h"

#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QKeySequence>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
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

  QTextBlock lastBlock = doc->begin();
  while (lastBlock.next().isValid()) lastBlock = lastBlock.next();

  // 用户光标状态：滚动条在底部、无选区、光标位于末块时才自动跟随新日志滚动，
  // 否则保留用户的选区与滚动位置（避免流式输出不断破坏正在进行的复制选择）
  const bool atBottom = verticalScrollBar()->value() >= verticalScrollBar()->maximum() - 1;
  const bool follow = atBottom && !textCursor().hasSelection() &&
                      textCursor().block() == lastBlock;

  // 如果文档末尾有空的尾随段落（QPlainTextEdit 默认行为），回退到前一个块
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

  // 仅在"跟随"状态下抢占光标并滚动到底（保护用户选区/滚动位置）
  if (follow) {
    setTextCursor(cursor);
    ensureCursorVisible();
  }

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

void CodeLog::contextMenuEvent(QContextMenuEvent *event) {
  QMenu menu(this);

  // 复制选中内容（有选区时可用，Ctrl+C 为 Qt 原生快捷键）
  QAction *copySelAct = menu.addAction(QStringLiteral("复制选中内容"));
  copySelAct->setShortcut(QKeySequence::Copy);
  copySelAct->setEnabled(textCursor().hasSelection());

  // 复制当前行（右键点击位置所在行，不依赖选区）
  QAction *copyLineAct = menu.addAction(QStringLiteral("复制当前行"));

  // 复制全部（行号区域独立绘制，不进入复制的文本）
  QAction *copyAllAct = menu.addAction(QStringLiteral("复制全部"));
  copyAllAct->setEnabled(!document()->isEmpty());

  menu.addSeparator();
  QAction *selectAllAct = menu.addAction(QStringLiteral("全选"));
  selectAllAct->setShortcut(QKeySequence::SelectAll);

  menu.addSeparator();
  QAction *clearAct = menu.addAction(QStringLiteral("清空输出"));

  QAction *chosen = menu.exec(event->globalPos());
  if (!chosen) return;

  if (chosen == copySelAct) {
    copy();
  } else if (chosen == copyLineAct) {
    // 取右键点击处所在行（临时光标，不移动光标、不破坏已有选区）
    QTextCursor lineCursor = cursorForPosition(event->pos());
    QGuiApplication::clipboard()->setText(lineCursor.block().text());
  } else if (chosen == copyAllAct) {
    QGuiApplication::clipboard()->setText(toPlainText());
  } else if (chosen == selectAllAct) {
    selectAll();
  } else if (chosen == clearAct) {
    clearLog();
  }
}