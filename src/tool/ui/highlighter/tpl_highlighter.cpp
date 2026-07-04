/**
 * @file tpl_highlighter.cpp
 * @brief 模板语法高亮器实现
 */

#include "tpl_highlighter.h"
#include "lighter_color.h"

/**
 * @brief 构造函数
 * @param parent 父文档
 *
 * 初始化高亮规则，定义模板元素的着色方案：
 * 1. 控制标签（蓝色加粗）：${each}, ${if}, ${else}, ${/each}, ${/if}
 * 2. 变量表达式（绿色）：${varName}, ${user.name}
 * 3. 算术运算符（红色加粗）：+ - * /
 * 4. 注释（灰色斜体）：// 单行注释
 * 5. 字符串（橙色）："" 和 '' 字面量
 * 6. 函数调用（紫色）：render(), write() 等函数调用
 *
 * 注意：规则按顺序应用，先匹配控制标签（更具体），再匹配变量表达式（通用），
 * 正则的顺序决定了优先级。
 */
TplHighlighter::TplHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {
  using namespace LighterColor;

  // ── 1. 模板控制标签（蓝色加粗） ──
  QTextCharFormat keywordFormat;
  keywordFormat.setForeground(keyword);
  keywordFormat.setFontWeight(QFont::Bold);
  m_rules.append({QRegularExpression(QStringLiteral(
                      "\\$\\{(?:each|if|else|/each|/if)\\b[^}]*\\}")),
                  keywordFormat});

  // ── 2. 模板变量（绿色） ──
  QTextCharFormat variableFormat;
  variableFormat.setForeground(variable);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\$\\{[^}]+\\}")), variableFormat});

  // ── 3. 算术运算符（红色加粗） ──
  QTextCharFormat operatorFormat;
  operatorFormat.setForeground(operator_);
  operatorFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("[+\\-*/]")), operatorFormat});

  // ── 4. 注释（灰色斜体） ──
  QTextCharFormat commentFormat;
  commentFormat.setForeground(comment);
  commentFormat.setFontItalic(true);
  m_rules.append(
      {QRegularExpression(QStringLiteral("//[^\n]*")), commentFormat});

  // ── 5. 字符串（橙色） ──
  QTextCharFormat stringFormat;
  stringFormat.setForeground(string_);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\"[^\"]*\"|'[^']*'")), stringFormat});

  // ── 6. 函数调用（紫色） ──
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
void TplHighlighter::highlightBlock(const QString &text) {
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