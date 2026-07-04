/**
 * @file light_ac.cpp
 * @brief AC 脚本语法高亮器实现
 */

#include "light_ac.h"
#include "light_color.h"

/**
 * @brief 构造函数
 * @param parent 父文档
 *
 * 初始化高亮规则，定义 AC 脚本元素的着色方案：
 * 1. 变量（绿色）：普通标识符
 * 2. 关键字（蓝色加粗）：let, main, for, in, if, else, call, return, class,
 * function, new, this
 * 3. 内置函数（紫色加粗）：readJson, merge, basename, render, write, print,
 * getCheckedFiles
 * 4. 注释（灰色斜体）：// 单行注释
 * 5. 字符串（橙色）："" 和 '' 字面量
 * 6. 数字（橙色加粗）：整数、浮点数
 * 7. 布尔值（红色加粗）：true, false
 * 8. 函数调用（紫色）：任何非关键字的标识符后跟括号
 *
 * 注意：规则按顺序应用，前面的规则优先级更低，后面的规则会覆盖前面的。
 * 变量规则放在最前面，关键字/内置函数/布尔值/函数调用等更具体的规则在后面覆盖它。
 */
LightAc::LightAc(QTextDocument *parent) : QSyntaxHighlighter(parent) {
  using namespace LightColor;

  // ── 0. 变量（绿色） ──
  // 放在最前面作为兜底规则，后面更具体的规则（关键字、内置函数等）会覆盖它
  QTextCharFormat variableFormat;
  variableFormat.setForeground(variable);
  m_rules.append({QRegularExpression(QStringLiteral("\\b[a-zA-Z_]\\w*\\b")),
                  variableFormat});

  // ── 1. 关键字（蓝色加粗） ──
  QTextCharFormat keywordFormat;
  keywordFormat.setForeground(keyword);
  keywordFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b(?:let|main|for|in|if|else|call|"
                                         "return|class|function|new|this)\\b")),
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
  // 匹配任何非关键字的标识符后跟括号，如 render()、write()、readJson() 等
  // 使用负面前瞻排除关键字，避免 for(、if(、else( 等被误染成函数调用颜色
  QTextCharFormat callFormat;
  callFormat.setForeground(call);
  m_rules.append({QRegularExpression(QStringLiteral(
                      "\\b(?!(?:let|main|for|in|if|else|call|return|class|"
                      "function|new|this)\\b)\\w+(?=\\s*\\()")),
                  callFormat});
}

/**
 * @brief 对单个文本块进行高亮处理
 * @param text 当前行的文本内容
 *
 * 遍历所有高亮规则，对匹配的文本应用对应的格式
 */
void LightAc::highlightBlock(const QString &text) {
  for (const HighlightRule &rule : std::as_const(m_rules)) {
    QRegularExpressionMatchIterator matchIterator =
        rule.pattern.globalMatch(text);
    while (matchIterator.hasNext()) {
      QRegularExpressionMatch match = matchIterator.next();
      setFormat(match.capturedStart(), match.capturedLength(), rule.format);
    }
  }
}