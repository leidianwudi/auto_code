/**
 * @file codeeditor.h
 * @brief 代码编辑器控件
 * 
 * 基于 QPlainTextEdit 的增强编辑器，提供：
 * - 行号显示
 * - 当前行高亮
 * - 括号匹配高亮
 */

#pragma once

#include <QPlainTextEdit>

class QPaintEvent;
class QResizeEvent;
class QWidget;

/**
 * @class CodeEditor
 * @brief 增强的代码编辑器控件
 */
class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit CodeEditor(QWidget *parent = nullptr);

    /**
     * @brief 行号区域绘制
     * @param event 绘制事件
     * @param area 行号区域矩形
     */
    void lineNumberAreaPaintEvent(QPaintEvent *event, const QRect &area);

    /**
     * @brief 计算行号区域宽度
     * @return 行号区域所需的宽度（像素）
     */
    int lineNumberAreaWidth() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    /**
     * @brief 更新行号区域宽度
     */
    void updateLineNumberAreaWidth(int newBlockCount);

    /**
     * @brief 高亮当前行并更新行号区域
     */
    void highlightCurrentLine();

    /**
     * @brief 更新行号区域
     */
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    /**
     * @brief 查找匹配的括号
     * @return 匹配括号的文本块和位置
     */
    void highlightMatchingBrackets();

    QWidget *m_lineNumberArea;  ///< 行号区域控件
};

/**
 * @class LineNumberArea
 * @brief 行号显示区域（内部辅助类）
 */
class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(CodeEditor *editor)
        : QWidget(editor), m_codeEditor(editor) {}

    QSize sizeHint() const override {
        return QSize(m_codeEditor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        QRect area = rect();
        m_codeEditor->lineNumberAreaPaintEvent(event, area);
    }

private:
    CodeEditor *m_codeEditor;
};
