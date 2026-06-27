/**
 * @file jsonhighlighter.h
 * @brief JSON 语法高亮器
 * 
 * 为 JSON 编辑器提供语法高亮，包括：
 * - 键名（蓝色）: "key"
 * - 字符串值（绿色）: "value"
 * - 数字（橙色）: 123, 3.14
 * - 布尔值（红色）: true, false
 * - null（紫色）: null
 */

#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

/**
 * @class JsonHighlighter
 * @brief JSON 语法高亮器
 * 
 * 对 JSON 文本进行语法高亮着色，使 JSON 结构清晰易读。
 */
class JsonHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit JsonHighlighter(QTextDocument *parent = nullptr);

protected:
    /**
     * @brief 对单个文本块进行高亮处理
     * @param text 文本内容
     */
    void highlightBlock(const QString &text) override;

private:
    /**
     * @brief 高亮规则
     */
    struct HighlightRule {
        QRegularExpression pattern;  ///< 匹配模式
        QTextCharFormat format;      ///< 文本格式
    };

    QVector<HighlightRule> m_rules;  ///< 高亮规则列表
};
