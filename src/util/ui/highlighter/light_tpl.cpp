/**
 * @file light_tpl.cpp
 * @brief 模板语法高亮器实现
 */

#include "light_tpl.h"

#include "light_color.h"
#include "src/engine/ac_language.h"

/**
 * @brief 构造函数
 * @param parent 父文档
 *
 * 初始化高亮规则，定义模板元素的着色方案：
 * 1. 控制标签（蓝色加粗）：${each}, ${if}, ${else}, ${/each}, ${/if}
 * 2. 模板注释（灰色斜体）：${# 注释内容}
 * 3. 变量表达式（绿色）：${varName}, ${user.name}
 * 4. 字符串（橙色）："" 和 '' 字面量
 * 5. 行注释（灰色斜体）：// 单行注释（内部的 ${...} 变量仍然显示绿色）
 * 6. 算术运算符（红色加粗）：+ - * /
 * 7. 函数调用（紫色）：render(), write() 等函数调用
 *
 * 注意：规则按顺序应用，先匹配控制标签（更具体），再匹配变量表达式（通用），
 * 正则的顺序决定了优先级。
 */
LightTpl::LightTpl(QTextDocument *parent) : QSyntaxHighlighter(parent) {
  using namespace LightColor;

  // ── 1. 模板控制标签（蓝色加粗） ──
  QTextCharFormat keywordFormat;
  keywordFormat.setForeground(keyword);
  keywordFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(
           QStringLiteral("\\$\\{(?:") + QString::fromLatin1(AcTemplate::kEachPrefix).trimmed() +
           QStringLiteral("|") + QString::fromLatin1(AcTemplate::kIfPrefix).trimmed() +
           QStringLiteral("|") + QString::fromLatin1(AcTemplate::kElse).mid(2, 4) +
           QStringLiteral("|/") + QString::fromLatin1(AcTemplate::kEachPrefix).trimmed() +
           QStringLiteral("|/") + QString::fromLatin1(AcTemplate::kIfPrefix).trimmed() +
           QStringLiteral(")\\b[^}]*\\}")),
       keywordFormat});

  // ── 2. 模板注释（灰色斜体） ──
  QTextCharFormat tplCommentFormat;
  tplCommentFormat.setForeground(comment);
  tplCommentFormat.setFontItalic(true);
  m_rules.append({QRegularExpression(QStringLiteral("\\$\\{#[^}]*\\}")), tplCommentFormat});

  // ── 3. 模板变量（绿色） ──
  QTextCharFormat variableFormat;
  variableFormat.setForeground(variable);
  m_rules.append({QRegularExpression(QStringLiteral("\\$\\{[^}]+\\}")), variableFormat});

  // ── 4. 字符串（橙色） ──
  QTextCharFormat stringFormat;
  stringFormat.setForeground(string_);
  m_rules.append({QRegularExpression(QStringLiteral("\"[^\"]*\"|'[^']*'")), stringFormat});

  // ── 5. 行注释定界符（灰色斜体） ──
  // .tpl 中 // 是输出文本（非模板注释），只把 // 本身标灰
  // 后续文字交给正常规则处理（如 ${var} 显示为变量绿色）
  QTextCharFormat commentFormat;
  commentFormat.setForeground(comment);
  commentFormat.setFontItalic(true);
  m_rules.append({QRegularExpression(QStringLiteral("//")), commentFormat});

  // ── 6. 算术运算符（红色加粗） ──
  QTextCharFormat operatorFormat;
  operatorFormat.setForeground(operator_);
  operatorFormat.setFontWeight(QFont::Bold);
  m_rules.append({QRegularExpression(QStringLiteral("[+\\-*/]")), operatorFormat});

  // ── 7. 函数调用（紫色） ──
  // 匹配任何标识符后跟括号，如 render()、write()、readJson() 等
  QTextCharFormat callFormat;
  callFormat.setForeground(call);
  m_rules.append({QRegularExpression(QStringLiteral("\\b\\w+(?=\\s*\\()")), callFormat});
}

/**
 * @brief 对单个文本块进行高亮处理
 * @param text 当前行的文本内容
 *
 * 按优先级遍历所有高亮规则，对匹配的文本应用对应的格式。
 * 使用位图标记已被高优先级规则格式化的位置，低优先级规则逐字符跳过这些位置。
 * 这样模板注释、变量、`//` 注释可以在同一行共存。
 */
void LightTpl::highlightBlock(const QString &text) {
  // 位图：标记已被高优先级规则格式化的字符位置
  QVector<bool> formatted(text.size(), false);

  // ── 预扫描：处理 ${# 注释（深度计数，跳过内部 ${...}） ──
  // 不能用正则 \\$\\{#[^}]*\\}，因为注释里可能包含 ${...} 示例
  QTextCharFormat commentFormat;
  commentFormat.setForeground(LightColor::comment);
  commentFormat.setFontItalic(true);

  int idx = 0;
  while (idx < text.length()) {
    int start = text.indexOf(QString::fromLatin1(AcTemplate::kExprOpen), idx);
    if (start == -1) break;
    if (start + 2 < text.length() && text[start + 2] == QChar('#')) {
      // 用深度计数找到正确的闭合 }
      int depth = 1;
      int cursor = start + 3;
      while (cursor < text.length()) {
        if (text.mid(cursor, 2) == QString::fromLatin1(AcTemplate::kExprOpen)) {
          depth++;
          cursor += 2;
        } else if (text[cursor] == QChar('}')) {
          depth--;
          if (depth == 0) break;
          cursor++;
        } else {
          cursor++;
        }
      }
      if (depth == 0) {
        // 整个 ${# ... } 标记为灰色斜体
        int end = cursor + 1;
        setFormat(start, end - start, commentFormat);
        for (int j = start; j < end && j < text.size(); ++j) formatted[j] = true;
        idx = end;
      } else {
        idx = start + 2;  // 未闭合，继续扫描
      }
    } else {
      idx = start + 2;  // 非注释，让后续规则处理
    }
  }

  // ── 预扫描2：处理 /* */ 输出注释定界符 ──
  // 在 .tpl 中 /* */ 是输出文本（非模板注释），只有 ${# } 才是模板注释。
  // 只把 /* 和 */ 定界符标灰斜体，中间内容默认色（${...} 由变量规则标绿）。
  // 但需标记中间位置防止运算符规则把 * / 标红。
  QTextCharFormat jsCommentDelimFormat;
  jsCommentDelimFormat.setForeground(LightColor::comment);
  jsCommentDelimFormat.setFontItalic(true);

  setCurrentBlockState(0);  // 默认不在注释中
  int jsCommentStart = -1;
  if (previousBlockState() != 1) {
    jsCommentStart = text.indexOf(QStringLiteral("/*"));
  }
  while (jsCommentStart >= 0) {
    int jsCommentEnd = text.indexOf(QStringLiteral("*/"), jsCommentStart);

    // 标灰 /* 定界符
    setFormat(jsCommentStart, 2, jsCommentDelimFormat);
    formatted[jsCommentStart] = true;
    if (jsCommentStart + 1 < text.size()) formatted[jsCommentStart + 1] = true;

    if (jsCommentEnd == -1) {
      // 未找到 */，注释延伸到下一行
      // 标记中间位置防止运算符匹配 * / 为红色（不设灰色格式）
      for (int j = jsCommentStart + 2; j < text.size(); ++j) formatted[j] = true;
      setCurrentBlockState(1);
      // 重新扫描模板变量，解除 formatted 让变量规则覆盖为绿色
      int varIdx = jsCommentStart + 2;
      while (varIdx < text.length()) {
        int varStart = text.indexOf(QString::fromLatin1(AcTemplate::kExprOpen), varIdx);
        if (varStart == -1) break;
        int varEnd = text.indexOf(QChar('}'), varStart + 2);
        if (varEnd == -1) {
          varIdx = varStart + 2;
          continue;
        }
        for (int j = varStart; j <= varEnd && j < text.size(); ++j) {
          formatted[j] = false;
        }
        varIdx = varEnd + 1;
      }
      break;
    }

    // 标灰 */ 定界符
    setFormat(jsCommentEnd, 2, jsCommentDelimFormat);
    formatted[jsCommentEnd] = true;
    if (jsCommentEnd + 1 < text.size()) formatted[jsCommentEnd + 1] = true;

    // 标记中间位置防止运算符匹配（不设灰色格式，保持默认色）
    for (int j = jsCommentStart + 2; j < jsCommentEnd && j < text.size(); ++j) {
      formatted[j] = true;
    }

    // 重新扫描中间区域，解除模板变量的 formatted 让变量规则覆盖为绿色
    int varIdx = jsCommentStart + 2;
    while (varIdx < jsCommentEnd && varIdx < text.length()) {
      int varStart = text.indexOf(QString::fromLatin1(AcTemplate::kExprOpen), varIdx);
      if (varStart == -1 || varStart >= jsCommentEnd) break;
      int varEnd = text.indexOf(QChar('}'), varStart + 2);
      if (varEnd == -1 || varEnd >= jsCommentEnd) {
        varIdx = varStart + 2;
        continue;
      }
      for (int j = varStart; j <= varEnd && j < text.size(); ++j) {
        formatted[j] = false;
      }
      varIdx = varEnd + 1;
    }

    jsCommentStart = text.indexOf(QStringLiteral("/*"), jsCommentEnd + 2);
  }

  // 如果上一行在 /* 注释中（previousBlockState == 1）且本行没找到新的 /*
  if (previousBlockState() == 1) {
    int jsCommentEnd = text.indexOf(QStringLiteral("*/"));
    if (jsCommentEnd == -1) {
      // 整行仍在注释中，标记位置防止运算符匹配（不设灰色格式）
      for (int j = 0; j < text.size(); ++j) formatted[j] = true;
      setCurrentBlockState(1);
      // 重新扫描模板变量，解除 formatted 让变量规则覆盖为绿色
      int varIdx = 0;
      while (varIdx < text.length()) {
        int varStart = text.indexOf(QString::fromLatin1(AcTemplate::kExprOpen), varIdx);
        if (varStart == -1) break;
        int varEnd = text.indexOf(QChar('}'), varStart + 2);
        if (varEnd == -1) {
          varIdx = varStart + 2;
          continue;
        }
        for (int j = varStart; j <= varEnd && j < text.size(); ++j) {
          formatted[j] = false;
        }
        varIdx = varEnd + 1;
      }
    } else {
      // 找到 */，标灰定界符，中间位置防运算符匹配（不设灰色格式）
      for (int j = 0; j < jsCommentEnd && j < text.size(); ++j) formatted[j] = true;
      setFormat(jsCommentEnd, 2, jsCommentDelimFormat);
      formatted[jsCommentEnd] = true;
      if (jsCommentEnd + 1 < text.size()) formatted[jsCommentEnd + 1] = true;

      // 重新扫描中间区域的模板变量
      int varIdx = 0;
      while (varIdx < jsCommentEnd && varIdx < text.length()) {
        int varStart = text.indexOf(QString::fromLatin1(AcTemplate::kExprOpen), varIdx);
        if (varStart == -1 || varStart >= jsCommentEnd) break;
        int varEnd = text.indexOf(QChar('}'), varStart + 2);
        if (varEnd == -1 || varEnd >= jsCommentEnd) {
          varIdx = varStart + 2;
          continue;
        }
        for (int j = varStart; j <= varEnd && j < text.size(); ++j) {
          formatted[j] = false;
        }
        varIdx = varEnd + 1;
      }
    }
  }

  // 按优先级顺序应用规则，后加入的规则优先级更低
  // （${# 注释已在上面预处理，这里的规则仅处理其他标签）
  for (const HighlightRule &rule : std::as_const(m_rules)) {
    QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
    while (matchIterator.hasNext()) {
      QRegularExpressionMatch match = matchIterator.next();
      int start = match.capturedStart();
      int end = start + match.capturedLength();

      // 逐字符处理：跳过已格式化的位置，只对未格式化的字符设置格式
      // 这样 `// ${var} 注释` 中 `//` 是灰色斜体，`${var}` 仍然是绿色
      int segStart = -1;
      for (int i = start; i < end && i < text.size(); ++i) {
        if (!formatted[i]) {
          if (segStart == -1) segStart = i;
        } else {
          if (segStart != -1) {
            setFormat(segStart, i - segStart, rule.format);
            for (int j = segStart; j < i; ++j) formatted[j] = true;
            segStart = -1;
          }
        }
      }
      if (segStart != -1) {
        setFormat(segStart, end - segStart, rule.format);
        for (int j = segStart; j < end && j < text.size(); ++j) formatted[j] = true;
      }
    }
  }
}