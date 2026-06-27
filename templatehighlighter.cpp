/**
 * @file templatehighlighter.cpp
 * @brief 模板语法高亮器实现
 */

#include "templatehighlighter.h"

/**
 * @brief 构造函数
 * @param parent 父文档
 * 
 * 初始化高亮规则，定义各种模板元素的着色方案
 */
TemplateHighlighter::TemplateHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // 定义各种格式

    // 1. 模板控制标签（蓝色加粗）: ${each}, ${if}, ${else}, ${/each}, ${/if}
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor("#0000FF"));  // 蓝色
    keywordFormat.setFontWeight(QFont::Bold);
    m_rules.append({QRegularExpression(QStringLiteral("\\$\\{(?:each|if|else|/each|/if)\\b[^}]*\\}")), keywordFormat});

    // 2. 模板变量（绿色）: ${varName}
    QTextCharFormat variableFormat;
    variableFormat.setForeground(QColor("#008000"));  // 绿色
    m_rules.append({QRegularExpression(QStringLiteral("\\$\\{[^}]+\\}")), variableFormat});

    // 3. 算术运算符（红色加粗）: +, -, *, /
    QTextCharFormat operatorFormat;
    operatorFormat.setForeground(QColor("#FF0000"));  // 红色
    operatorFormat.setFontWeight(QFont::Bold);
    m_rules.append({QRegularExpression(QStringLiteral("[+\\-*/]")), operatorFormat});

    // 4. 注释（灰色斜体）: //
    QTextCharFormat commentFormat;
    commentFormat.setForeground(QColor("#808080"));  // 灰色
    commentFormat.setFontItalic(true);
    m_rules.append({QRegularExpression(QStringLiteral("//[^\n]*")), commentFormat});

    // 5. 字符串（橙色）: "..." 或 '...'
    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor("#FF8C00"));  // 深橙色
    m_rules.append({QRegularExpression(QStringLiteral("\"[^\"]*\"|'[^']*'")), stringFormat});
}

/**
 * @brief 对单个文本块进行高亮处理
 * @param text 当前行的文本内容
 * 
 * 遍历所有高亮规则，对匹配的文本应用对应的格式
 */
void TemplateHighlighter::highlightBlock(const QString &text)
{
    // 应用所有高亮规则
    for (const HighlightRule &rule : std::as_const(m_rules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}
