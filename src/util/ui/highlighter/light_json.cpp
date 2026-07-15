#include "light_json.h"

#include "light_color.h"
#include "src/engine/ac_language.h"

// 构造函数：初始化所有高亮规则
// 规则按优先级从低到高排列，后面的规则会覆盖前面的规则格式
LightJson::LightJson(QTextDocument *parent) : QSyntaxHighlighter(parent) {
  using namespace LightColor;

  // ── 1. 注释规则（灰色斜体，与 AC 脚本高亮器风格一致） ──
  m_commentFormat.setForeground(comment);
  m_commentFormat.setFontItalic(true);

  // 单行注释：匹配 // 到行尾的所有字符，例：// 这是注释
  m_rules.append({QRegularExpression(QStringLiteral("//[^\n]*")), m_commentFormat});
  // 块注释（单行内）：匹配 /* ... */，多行块注释由 highlightBlock 状态机处理
  m_rules.append({QRegularExpression(QStringLiteral("/\\*.*?\\*/")), m_commentFormat});

  // ── 2. JSON 键名（蓝色加粗） ──
  // 匹配带引号的字符串后跟冒号和可选空格，例："name": 或 "age" :
  QTextCharFormat keyFormat;
  keyFormat.setForeground(keyword);
  keyFormat.setFontWeight(QFont::Bold);
  m_rules.append({QRegularExpression(QStringLiteral("\"[^\"]+\"\\s*:")), keyFormat});

  // ── 3. JSON 字符串值（绿色） ──
  // 匹配所有带引号的字符串（包括键名，会被键名规则覆盖）
  // 例："Hello World"、"/path/to/file"
  QTextCharFormat stringFormat;
  stringFormat.setForeground(variable);
  m_rules.append({QRegularExpression(QStringLiteral("\"[^\"]*\"")), stringFormat});

  // ── 4. 数字（橙色加粗） ──
  // 支持整数、浮点数、负数，例：42、3.14、-100
  QTextCharFormat numberFormat;
  numberFormat.setForeground(number);
  numberFormat.setFontWeight(QFont::Bold);
  m_rules.append({QRegularExpression(QStringLiteral("\\b-?\\d+(?:\\.\\d+)?\\b")), numberFormat});

  // ── 5. 布尔值（红色加粗） ──
  // 匹配 true 和 false 关键字
  QTextCharFormat boolFormat;
  boolFormat.setForeground(boolean_);
  boolFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b(?:") + QString::fromLatin1(AcKeyword::kTrue) +
                          QStringLiteral("|") + QString::fromLatin1(AcKeyword::kFalse) +
                          QStringLiteral(")\\b")),
       boolFormat});

  // ── 6. null 值（紫色加粗） ──
  // 匹配 null 关键字
  QTextCharFormat nullFormat;
  nullFormat.setForeground(builtin);
  nullFormat.setFontWeight(QFont::Bold);
  m_rules.append({QRegularExpression(QStringLiteral("\\bnull\\b")), nullFormat});
}

// 对单个文本块进行高亮处理
// 处理逻辑：
// 1. 检查是否处于块注释延续状态（previousBlockState == 1）
//    - 是：查找 */ 结束标记，找到则结束块注释并处理后续文本
//    - 未找到：整行继续作为块注释
// 2. 检查本行是否有新的块注释开始（/*）
//    - 有：先处理块注释前的普通文本，再处理块注释本身
//    - 块注释在本行结束：继续处理块注释后的文本
//    - 块注释跨行：设置状态码 1，下一行会进入步骤 1
// 3. 普通行：直接应用所有高亮规则
void LightJson::highlightBlock(const QString &text) {
  // ── 情况 1：上一行的块注释延续到本行 ──
  if (previousBlockState() == 1) {
    int endPos = text.indexOf(QLatin1String("*/"));
    if (endPos != -1) {
      // 块注释在本行结束：对 [0, endPos+2] 应用注释格式
      setFormat(0, endPos + 2, m_commentFormat);
      // 处理块注释之后的剩余文本（可能有正常 JSON 内容）
      highlightBlockHelper(text.mid(endPos + 2));
      setCurrentBlockState(0);  // 重置状态，不再处于块注释中
    } else {
      // 块注释继续到下一行：整行都是注释
      setFormat(0, text.length(), m_commentFormat);
      setCurrentBlockState(1);  // 保持状态，通知下一行仍在块注释中
    }
    return;
  }

  // ── 情况 2：本行有新的块注释开始 ──
  int blockStart = text.indexOf(QLatin1String("/*"));
  if (blockStart != -1) {
    // 先处理块注释开始之前的普通文本
    highlightBlockHelper(text.left(blockStart));

    // 查找块注释结束位置
    int blockEnd = text.indexOf(QLatin1String("*/"), blockStart + 2);
    if (blockEnd != -1) {
      // 块注释在本行结束：对 [blockStart, blockEnd+2] 应用注释格式
      setFormat(blockStart, blockEnd - blockStart + 2, m_commentFormat);
      // 处理块注释之后的剩余文本
      highlightBlockHelper(text.mid(blockEnd + 2));
    } else {
      // 块注释跨行：从 blockStart 到行尾都是注释
      setFormat(blockStart, text.length() - blockStart, m_commentFormat);
      setCurrentBlockState(1);  // 设置状态，下一行会进入情况 1
    }
    return;
  }

  // ── 情况 3：普通行，无块注释 ──
  highlightBlockHelper(text);
}

// 辅助函数：对文本应用所有高亮规则
// 遍历 m_rules 中的每条规则，使用正则表达式全局匹配
// 对每个匹配结果应用对应的文本格式
// 注意：此函数不处理块注释，调用前应确保文本不包含未闭合的 /*
void LightJson::highlightBlockHelper(const QString &text) {
  for (const HighlightRule &rule : std::as_const(m_rules)) {
    QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
    while (matchIterator.hasNext()) {
      QRegularExpressionMatch match = matchIterator.next();
      setFormat(match.capturedStart(), match.capturedLength(), rule.format);
    }
  }
}