/**
 * @file tshighlighter.cpp
 * @brief TypeScript 语法高亮器实现
 */

#include "tshighlighter.h"

/**
 * @brief 构造函数
 * @param parent 父文档
 * 
 * 初始化 TypeScript 语法高亮规则
 */
TypeScriptHighlighter::TypeScriptHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // 关键字格式（蓝色加粗）
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor("#0000FF"));
    keywordFormat.setFontWeight(QFont::Bold);

    // 类型格式（绿色）
    QTextCharFormat typeFormat;
    typeFormat.setForeground(QColor("#267F99"));

    // 字符串格式（橙色）
    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor("#FF8C00"));

    // 注释格式（灰色斜体）
    QTextCharFormat commentFormat;
    commentFormat.setForeground(QColor("#808080"));
    commentFormat.setFontItalic(true);

    // 数字格式（紫色）
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor("#800080"));

    // 装饰器格式（洋红色）
    QTextCharFormat decoratorFormat;
    decoratorFormat.setForeground(QColor("#FF00FF"));

    // TypeScript 关键字
    QStringList keywords = {
        "export", "import", "from", "class", "interface", "type", "enum",
        "extends", "implements", "abstract", "public", "private", "protected",
        "static", "readonly", "async", "await", "function", "const", "let",
        "var", "if", "else", "for", "while", "do", "switch", "case", "break",
        "continue", "return", "throw", "try", "catch", "finally", "new",
        "delete", "typeof", "instanceof", "in", "of", "this", "super"
    };

    // 添加关键字规则
    for (const QString &keyword : keywords) {
        m_rules.append({QRegularExpression(QStringLiteral("\\b") + keyword + QStringLiteral("\\b")), keywordFormat});
    }

    // TypeScript 内置类型
    QStringList types = {
        "string", "number", "boolean", "void", "null", "undefined", "never",
        "any", "unknown", "object", "symbol", "bigint", "Array", "Promise",
        "Record", "Partial", "Required", "Readonly", "Pick", "Omit", "Exclude",
        "Extract", "NonNullable", "ReturnType", "Parameters", "ConstructorParameters"
    };

    // 添加类型规则
    for (const QString &type : types) {
        m_rules.append({QRegularExpression(QStringLiteral("\\b") + type + QStringLiteral("\\b")), typeFormat});
    }

    // 字符串（单引号和双引号）
    m_rules.append({QRegularExpression("\"[^\"]*\"|'[^']*'"), stringFormat});

    // 模板字符串
    m_rules.append({QRegularExpression("`[^`]*`"), stringFormat});

    // 单行注释
    m_rules.append({QRegularExpression("//[^\n]*"), commentFormat});

    // 数字（整数和浮点数）
    m_rules.append({QRegularExpression("\\b\\d+(\\.\\d+)?\\b"), numberFormat});

    // 装饰器
    m_rules.append({QRegularExpression("@\\w+"), decoratorFormat});
}

/**
 * @brief 对单个文本块进行高亮处理
 * @param text 当前行的文本内容
 * 
 * 遍历所有高亮规则，对匹配的文本应用对应的格式
 */
void TypeScriptHighlighter::highlightBlock(const QString &text)
{
    // 应用所有高亮规则
    for (const HighlightRule &rule : std::as_const(m_rules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // 处理多行注释 /* ... */
    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1) {
        startIndex = text.indexOf("/*");
    }

    while (startIndex >= 0) {
        int endIndex = text.indexOf("*/", startIndex);
        int commentLength;

        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + 2;
        }

        QTextCharFormat commentFormat;
        commentFormat.setForeground(QColor("#808080"));
        commentFormat.setFontItalic(true);
        setFormat(startIndex, commentLength, commentFormat);

        startIndex = text.indexOf("/*", startIndex + commentLength);
    }
}
