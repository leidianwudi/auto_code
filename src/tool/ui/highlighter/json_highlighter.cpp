/**
 * @file json_highlighter.cpp
 * @brief JSON 语法高亮器实现
 */

#include "json_highlighter.h"
#include "lighter_color.h"

/**
 * @brief 构造函数
 * @param parent 父文档
 *
 * 初始化高亮规则，定义 JSON 元素的着色方案：
 * 1. 键名（蓝色加粗）：冒号前的带引号字符串
 * 2. 字符串值（绿色）：所有带引号字符串
 * 3. 数字（橙色加粗）：整数、浮点数、负数
 * 4. 布尔值（红色加粗）：true, false
 * 5. null（紫色加粗）：null
 *
 * 注意：键名使用前瞻匹配冒号，字符串值使用通用匹配，两者可能重叠。
 * 由于规则按顺序应用，键名（更具体）放在前面，字符串值（通用）放在后面。
 */
JsonHighlighter::JsonHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {
  using namespace LighterColor;

  // ── 1. JSON 键名（蓝色加粗） ──
  QTextCharFormat keyFormat;
  keyFormat.setForeground(keyword);
  keyFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\"[^\"]+\"\\s*:")), keyFormat});

  // ── 2. JSON 字符串值（绿色） ──
  QTextCharFormat stringFormat;
  stringFormat.setForeground(variable);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\"[^\"]*\"")), stringFormat});

  // ── 3. 数字（橙色加粗） ──
  QTextCharFormat numberFormat;
  numberFormat.setForeground(number);
  numberFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b-?\\d+(?:\\.\\d+)?\\b")),
       numberFormat});

  // ── 4. 布尔值（红色加粗） ──
  QTextCharFormat boolFormat;
  boolFormat.setForeground(boolean_);
  boolFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b(?:true|false)\\b")), boolFormat});

  // ── 5. null（紫色加粗） ──
  QTextCharFormat nullFormat;
  nullFormat.setForeground(builtin);
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