/**
 * @file ac_highlighter.cpp
 * @brief AC 脚本语法高亮器实现
 */

#include "ac_highlighter.h"
#include "lighter_color.h"

/**
 * @brief 构造函数
 * @param parent 父文档
 *
 * 初始化高亮规则，定义 AC 脚本元素的着色方案：
 * 1. 关键字（蓝色加粗）：let, main, for, in, if, else, call, return
 * 2. 内置函数（紫色加粗）：readJson, merge, basename, render, write, print,
 * getCheckedFiles
 * 3. 注释（灰色斜体）：// 单行注释
 * 4. 字符串（橙色）："" 和 '' 字面量
 * 5. 数字（橙色加粗）：整数、浮点数
 * 6. 布尔值（红色加粗）：true, false
 * 7. 函数调用（紫色）：任何标识符后跟括号
 *
 * 注意：规则按顺序应用，前面的规则优先级更高。
 */
AcHighlighter::AcHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {
  using namespace LighterColor;

  // ── 1. 关键字（蓝色加粗） ──
  QTextCharFormat keywordFormat;
  keywordFormat.setForeground(keyword);
  keywordFormat.setFontWeight(QFont::Bold);
  m_rules.append({QRegularExpression(QStringLiteral(
                      "\\b(?:let|main|for|in|if|else|call|return)\\b")),
                  keywordFormat});

  // ── 2. 内置函数（紫色加粗） ──
  QTextCharFormat builtinFormat;
  builtinFormat.setForeground(builtin);
  builtinFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b(?:readJson|merge|basename|render|"
                                         "write|print|getCheckedFiles)\\b")),
       builtinFormat});

  // ── 3. 注释（灰色斜体） ──
  QTextCharFormat commentFormat;
  commentFormat.setForeground(comment);
  commentFormat.setFontItalic(true);
  m_rules.append(
      {QRegularExpression(QStringLiteral("//[^\n]*")), commentFormat});

  // ── 4. 字符串（橙色） ──
  QTextCharFormat stringFormat;
  stringFormat.setForeground(string_);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\"[^\"]*\"|'[^']*'")), stringFormat});

  // ── 5. 数字（橙色加粗） ──
  QTextCharFormat numberFormat;
  numberFormat.setForeground(number);
  numberFormat.setFontWeight(QFont::Bold);
  m_rules.append({QRegularExpression(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b")),
                  numberFormat});

  // ── 6. 布尔值（红色加粗） ──
  QTextCharFormat boolFormat;
  boolFormat.setForeground(boolean_);
  boolFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b(?:true|false)\\b")), boolFormat});

  // ── 7. 函数调用（紫色） ──
  // 匹配任何标识符后跟括号，如 render()、write()、readJson() 等
  QTextCharFormat callFormat;
  callFormat.setForeground(call);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b\\w+(?=\\s*\\()")), callFormat});
}

/**
 * @brief 对单个文本块进行高亮处理
 * @param text 当前行的文本内容
 *
 * 遍历所有高亮规则，对匹配的文本应用对应的格式
 */
void AcHighlighter::highlightBlock(const QString &text) {
  for (const HighlightRule &rule : std::as_const(m_rules)) {
    QRegularExpressionMatchIterator matchIterator =
        rule.pattern.globalMatch(text);
    while (matchIterator.hasNext()) {
      QRegularExpressionMatch match = matchIterator.next();
      setFormat(match.capturedStart(), match.capturedLength(), rule.format);
    }
  }
}