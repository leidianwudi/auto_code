/**
 * @file codeeditor.cpp
 * @brief 代码编辑器控件实现
 */

#include "codeeditor.h"
#include <QPainter>
#include <QTextBlock>
#include <QScrollBar>
#include <QMap>

/**
 * @brief 构造函数
 * @param parent 父控件
 * 
 * 初始化编辑器，设置等宽字体，创建行号区域，连接信号槽
 */
CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
    , m_lineNumberArea(new LineNumberArea(this))
{
    // 设置等宽字体
    QFont font;
    font.setFamily("Consolas");
    font.setFixedPitch(true);
    font.setPointSize(11);
    setFont(font);

    // 连接信号槽
    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &CodeEditor::highlightCurrentLine);

    // 初始化
    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

/**
 * @brief 计算行号区域宽度
 * @return 行号区域所需的像素宽度
 */
int CodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

/**
 * @brief 更新行号区域宽度
 * @param newBlockCount 新的文本块数量
 */
void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

/**
 * @brief 更新行号区域
 * @param rect 需要更新的矩形区域
 * @param dy 垂直滚动偏移量
 */
void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

/**
 * @brief 处理窗口大小改变事件
 * @param event 大小改变事件
 */
void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(),
                                        lineNumberAreaWidth(), cr.height()));
}

/**
 * @brief 高亮当前行并处理括号匹配
 * 
 * 统一处理当前行高亮和括号匹配高亮，
 * 将所有 ExtraSelection 合并后一次性设置。
 */
void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    // 当前行高亮
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        QColor lineColor = QColor(Qt::yellow).lighter(180);
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    // 括号匹配高亮
    QTextCursor cursor = textCursor();
    QString lineText = cursor.block().text();
    int posInBlock = cursor.positionInBlock();

    // 检查光标前一个字符（光标刚输入括号时的常见情况）
    int checkPos = posInBlock;
    if (checkPos >= lineText.length() && checkPos > 0)
        checkPos = posInBlock - 1;

    // 定义括号对
    static const QChar openBrackets[] = { '(', '{', '[' };
    static const QChar closeBrackets[] = { ')', '}', ']' };
    static const int numPairs = 3;

    auto isOpenBracket = [](QChar c) {
        return c == '(' || c == '{' || c == '[';
    };
    auto isCloseBracket = [](QChar c) {
        return c == ')' || c == '}' || c == ']';
    };
    auto findMatch = [](QChar open) -> QChar {
        if (open == '(') return ')';
        if (open == '{') return '}';
        if (open == '[') return ']';
        return {};
    };
    auto findOpen = [](QChar close) -> QChar {
        if (close == ')') return '(';
        if (close == '}') return '{';
        if (close == ']') return '[';
        return {};
    };

    if (checkPos >= 0 && checkPos < lineText.length()) {
        QChar ch = lineText[checkPos];

        int matchBlockPos = -1;  // 匹配括号在当前块内的位置

        if (isOpenBracket(ch)) {
            // 向前查找匹配的闭括号
            QChar matchCh = findMatch(ch);
            int depth = 0;
            for (int i = checkPos; i < lineText.length(); ++i) {
                if (lineText[i] == ch) depth++;
                else if (lineText[i] == matchCh) {
                    depth--;
                    if (depth == 0) { matchBlockPos = i; break; }
                }
            }
        } else if (isCloseBracket(ch)) {
            // 向后查找匹配的开括号
            QChar matchCh = findOpen(ch);
            int depth = 0;
            for (int i = checkPos; i >= 0; --i) {
                if (lineText[i] == ch) depth++;
                else if (lineText[i] == matchCh) {
                    depth--;
                    if (depth == 0) { matchBlockPos = i; break; }
                }
            }
        }

        if (matchBlockPos >= 0) {
            // 高亮两个括号
            QColor bracketColor = QColor(0, 200, 200);
            int blockPos = cursor.block().position();

            // 高亮当前括号
            QTextEdit::ExtraSelection sel1;
            sel1.format.setBackground(bracketColor);
            sel1.format.setForeground(Qt::white);
            sel1.format.setFontWeight(QFont::Bold);
            sel1.cursor = textCursor();
            sel1.cursor.setPosition(blockPos + checkPos);
            sel1.cursor.setPosition(blockPos + checkPos + 1, QTextCursor::KeepAnchor);
            extraSelections.append(sel1);

            // 高亮匹配括号
            QTextEdit::ExtraSelection sel2;
            sel2.format.setBackground(bracketColor);
            sel2.format.setForeground(Qt::white);
            sel2.format.setFontWeight(QFont::Bold);
            sel2.cursor = textCursor();
            sel2.cursor.setPosition(blockPos + matchBlockPos);
            sel2.cursor.setPosition(blockPos + matchBlockPos + 1, QTextCursor::KeepAnchor);
            extraSelections.append(sel2);
        }
    }

    setExtraSelections(extraSelections);
}

/**
 * @brief 查找并高亮匹配的括号（保留接口兼容）
 */
void CodeEditor::highlightMatchingBrackets()
{
    // 括号匹配已集成在 highlightCurrentLine 中
}

/**
 * @brief 绘制行号区域
 * @param event 绘制事件
 * @param area 行号区域矩形
 */
void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event, const QRect &area)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(area, QColor(Qt::lightGray).lighter(110));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= area.bottom()) {
        if (block.isVisible() && bottom >= area.top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);
            painter.drawText(0, top, m_lineNumberArea->width() - 4,
                           fontMetrics().height(), Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}
