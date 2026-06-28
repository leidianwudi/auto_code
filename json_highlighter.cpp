/**
 * @file json_highlighter.cpp
 * @brief JSON 语法高亮器实现
 */

#include "json_highlighter.h"

/**
 * @brief 构造函数
 * @param parent 父文档
 *
 * 初始化高亮规则，定义 JSON 元素的着色方案
 */
JsonHighlighter::JsonHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {
  // 定义各种格式

  // 1. JSON 键名（蓝色加粗）: "key":
  QTextCharFormat keyFormat;
  keyFormat.setForeground(QColor("#0000FF")); // 蓝色
  keyFormat.setFontWeight(QFont::Bold);
  // 匹配冒号前的字符串
  m_rules.append(
      {QRegularExpression(QStringLiteral("\"[^\"]+\"\\s*:")), keyFormat});

  // 2. JSON 字符串值（绿色）: "value"
  QTextCharFormat stringFormat;
  stringFormat.setForeground(QColor("#008000")); // 绿色
  m_rules.append(
      {QRegularExpression(QStringLiteral("\"[^\"]*\"")), stringFormat});

  // 3. 数字（橙色）: 123, 3.14, -42
  QTextCharFormat numberFormat;
  numberFormat.setForeground(QColor("#FF8C00")); // 深橙色
  numberFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b-?\\d+(?:\\.\\d+)?\\b")),
       numberFormat});

  // 4. 布尔值（红色加粗）: true, false
  QTextCharFormat boolFormat;
  boolFormat.setForeground(QColor("#FF0000")); // 红色
  boolFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b(?:true|false)\\b")), boolFormat});

  // 5. null（紫色加粗）: null
  QTextCharFormat nullFormat;
  nullFormat.setForeground(QColor("#800080")); // 紫色
  nullFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\bnull\\b")), nullFormat});
}

/**
 * @brief 对单个文本块进行高亮处理
 * @param text 当前行的文本内容
 *
 * 遍历所有高亮规则，对匹配的文本应用对应的格式
 */
void JsonHighlighter::highlightBlock(const QString &text) {
  // 应用所有高亮规则
  for (const HighlightRule &rule : std::as_const(m_rules)) {
    QRegularExpressionMatchIterator matchIterator =
        rule.pattern.globalMatch(text);
    while (matchIterator.hasNext()) {
      QRegularExpressionMatch match = matchIterator.next();
      setFormat(match.capturedStart(), match.capturedLength(), rule.format);
    }
  }
}