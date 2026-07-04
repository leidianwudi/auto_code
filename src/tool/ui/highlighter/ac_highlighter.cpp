/**
 * @file ac_highlighter.cpp
 * @brief AC 脚本语法高亮器实现
 */

#include "ac_highlighter.h"

/**
 * @brief 构造函数
 * @param parent 父文档
 *
 * 初始化高亮规则，定义 AC 脚本元素的着色方案：
 * 1. 关键字（蓝色加粗）：main, for, in, if, else, call, return
 * 2. 内置函数（紫色加粗）：readJson, merge, basename, render, write, print,
 * getCheckedFiles
 * 3. 注释（灰色斜体）：// 单行注释
 * 4. 字符串（橙色）："" 和 '' 字面量
 * 5. 数字（橙色加粗）：整数、浮点数
 * 6. 布尔值（红色加粗）：true, false
 *
 * 注意：规则按顺序应用，前面的规则优先级更高。
 */
AcHighlighter::AcHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {
  // ── 1. 关键字（蓝色加粗） ──
  // 使用 \b 确保精确匹配单词边界
  QTextCharFormat keywordFormat;
  keywordFormat.setForeground(QColor("#0000FF")); // 蓝色
  keywordFormat.setFontWeight(QFont::Bold);
  m_rules.append({QRegularExpression(QStringLiteral(
                      "\\b(?:main|for|in|if|else|call|return)\\b")),
                  keywordFormat});

  // ── 2. 内置函数（紫色加粗） ──
  QTextCharFormat builtinFormat;
  builtinFormat.setForeground(QColor("#800080")); // 紫色
  builtinFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b(?:readJson|merge|basename|render|"
                                         "write|print|getCheckedFiles)\\b")),
       builtinFormat});

  // ── 3. 注释（灰色斜体） ──
  // 匹配 // 到行尾的文本
  QTextCharFormat commentFormat;
  commentFormat.setForeground(QColor("#808080")); // 灰色
  commentFormat.setFontItalic(true);
  m_rules.append(
      {QRegularExpression(QStringLiteral("//[^\n]*")), commentFormat});

  // ── 4. 字符串（橙色） ──
  // 匹配双引号或单引号包围的字符串
  QTextCharFormat stringFormat;
  stringFormat.setForeground(QColor("#FF8C00")); // 深橙色
  m_rules.append(
      {QRegularExpression(QStringLiteral("\"[^\"]*\"|'[^']*'")), stringFormat});

  // ── 5. 数字（橙色加粗） ──
  QTextCharFormat numberFormat;
  numberFormat.setForeground(QColor("#FF8C00")); // 深橙色
  numberFormat.setFontWeight(QFont::Bold);
  m_rules.append({QRegularExpression(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b")),
                  numberFormat});

  // ── 6. 布尔值（红色加粗） ──
  QTextCharFormat boolFormat;
  boolFormat.setForeground(QColor("#FF0000")); // 红色
  boolFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b(?:true|false)\\b")), boolFormat});
}

/**
 * @brief 对单个文本块进行高亮处理
 * @param text 当前行的文本内容
 *
 * 遍历所有高亮规则，对匹配的文本应用对应的格式
 */
void AcHighlighter::highlightBlock(const QString &text) {
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