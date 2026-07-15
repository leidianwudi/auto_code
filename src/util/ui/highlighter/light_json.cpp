#include "light_json.h"

#include "light_color.h"
#include "src/engine/ac_language.h"

// 构造函数：初始化所有高亮规则
// 注释规则单独存储在 m_commentFormat 中，由 highlightBlock 统一处理
// m_rules 只包含非注释的 JSON 语法元素规则（避免注释内的内容被错误着色）
LightJson::LightJson(QTextDocument *parent) : QSyntaxHighlighter(parent) {
  using namespace LightColor;

  // ── 注释格式（灰色斜体，与 AC 脚本高亮器风格一致） ──
  // 注释规则不放入 m_rules，而是由 highlightBlock 单独处理
  // 这样可以确保注释内的数字、关键字等不会被其他规则覆盖
  m_commentFormat.setForeground(comment);
  m_commentFormat.setFontItalic(true);

  // ── 1. JSON 键名（蓝色加粗） ──
  // 匹配带引号的字符串后跟冒号和可选空格，例："name": 或 "age" :
  QTextCharFormat keyFormat;
  keyFormat.setForeground(keyword);
  keyFormat.setFontWeight(QFont::Bold);
  m_rules.append({QRegularExpression(QStringLiteral("\"[^\"]+\"\\s*:")), keyFormat});

  // ── 2. JSON 字符串值（绿色） ──
  // 匹配所有带引号的字符串（包括键名，会被键名规则覆盖）
  // 例："Hello World"、"/path/to/file"
  QTextCharFormat stringFormat;
  stringFormat.setForeground(variable);
  m_rules.append({QRegularExpression(QStringLiteral("\"[^\"]*\"")), stringFormat});

  // ── 3. 数字（橙色加粗） ──
  // 支持整数、浮点数、负数，例：42、3.14、-100
  QTextCharFormat numberFormat;
  numberFormat.setForeground(number);
  numberFormat.setFontWeight(QFont::Bold);
  m_rules.append({QRegularExpression(QStringLiteral("\\b-?\\d+(?:\\.\\d+)?\\b")), numberFormat});

  // ── 4. 布尔值（红色加粗） ──
  // 匹配 true 和 false 关键字
  QTextCharFormat boolFormat;
  boolFormat.setForeground(boolean_);
  boolFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b(?:") + QString::fromLatin1(AcKeyword::kTrue) +
                          QStringLiteral("|") + QString::fromLatin1(AcKeyword::kFalse) +
                          QStringLiteral(")\\b")),
       boolFormat});

  // ── 5. null 值（紫色加粗） ──
  // 匹配 null 关键字
  QTextCharFormat nullFormat;
  nullFormat.setForeground(builtin);
  nullFormat.setFontWeight(QFont::Bold);
  m_rules.append({QRegularExpression(QStringLiteral("\\bnull\\b")), nullFormat});
}

// 对单个文本块进行高亮处理
// 处理逻辑（按优先级从高到低）：
// 1. 块注释延续（上一行的 /* 未闭合）：整行或部分是注释
// 2. 新块注释开始（本行有 /*）：分割成 [普通文本][块注释][普通文本]
// 3. 单行注释（// 到行尾）：分割成 [普通文本][单行注释]
// 4. 纯普通文本：直接应用所有非注释高亮规则
void LightJson::highlightBlock(const QString &text) {
  // ── 情况 1：上一行的块注释延续到本行 ──
  if (previousBlockState() == 1) {
    int endPos = text.indexOf(QLatin1String("*/"));
    if (endPos != -1) {
      setFormat(0, endPos + 2, m_commentFormat);
      highlightNonCommentText(text.mid(endPos + 2));
      setCurrentBlockState(0);
    } else {
      setFormat(0, text.length(), m_commentFormat);
      setCurrentBlockState(1);
    }
    return;
  }

  // ── 情况 2：本行有新的块注释开始（/*） ──
  int blockStart = text.indexOf(QLatin1String("/*"));
  if (blockStart != -1) {
    highlightNonCommentText(text.left(blockStart));

    int blockEnd = text.indexOf(QLatin1String("*/"), blockStart + 2);
    if (blockEnd != -1) {
      setFormat(blockStart, blockEnd - blockStart + 2, m_commentFormat);
      highlightNonCommentText(text.mid(blockEnd + 2));
    } else {
      setFormat(blockStart, text.length() - blockStart, m_commentFormat);
      setCurrentBlockState(1);
    }
    return;
  }

  // ── 情况 3：检查单行注释（//） ──
  int lineCommentStart = text.indexOf(QLatin1String("//"));
  if (lineCommentStart != -1) {
    // // 前面的内容按普通文本处理
    highlightNonCommentText(text.left(lineCommentStart));
    // // 到行尾的内容按注释处理
    setFormat(lineCommentStart, text.length() - lineCommentStart, m_commentFormat);
    return;
  }

  // ── 情况 4：纯普通文本，无任何注释 ──
  highlightNonCommentText(text);
}

// 辅助函数：对非注释文本应用所有高亮规则
// 只处理键名、字符串、数字、布尔值、null 等 JSON 语法元素
// 调用前应确保文本不包含任何注释内容
void LightJson::highlightNonCommentText(const QString &text) {
  for (const HighlightRule &rule : std::as_const(m_rules)) {
    QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
    while (matchIterator.hasNext()) {
      QRegularExpressionMatch match = matchIterator.next();
      setFormat(match.capturedStart(), match.capturedLength(), rule.format);
    }
  }
}