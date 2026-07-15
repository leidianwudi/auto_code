#include "light_ac.h"

#include "light_color.h"
#include "src/engine/ac_language.h"

// 构造函数：初始化所有高亮规则
// 注释格式单独存储，由 highlightBlock 统一处理
// m_rules 只包含非注释的 AC 语法元素规则（避免注释内的内容被错误着色）
LightAc::LightAc(QTextDocument *parent) : QSyntaxHighlighter(parent) {
  using namespace LightColor;

  // ── 注释格式（灰色斜体） ──
  // 不放入 m_rules，由 highlightBlock 单独处理
  m_commentFormat.setForeground(comment);
  m_commentFormat.setFontItalic(true);

  // ── 0. 变量（绿色） ──
  // 放在最前面作为兜底规则，后面更具体的规则会覆盖它
  QTextCharFormat variableFormat;
  variableFormat.setForeground(variable);
  m_rules.append({QRegularExpression(QStringLiteral("\\b[a-zA-Z_]\\w*\\b")), variableFormat});

  // ── 1. 关键字（蓝色加粗） ──
  QTextCharFormat keywordFormat;
  keywordFormat.setForeground(keyword);
  keywordFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b(?:") + AcKeyword::kAll.join(QStringLiteral("|")) +
                          QStringLiteral(")\\b")),
       keywordFormat});

  // ── 2. 内置函数（紫色加粗） ──
  QTextCharFormat builtinFormat;
  builtinFormat.setForeground(builtin);
  builtinFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b(?:") + AcBuiltin::kAll.join(QStringLiteral("|")) +
                          QStringLiteral(")\\b")),
       builtinFormat});

  // ── 3. 字符串（橙色） ──
  QTextCharFormat stringFormat;
  stringFormat.setForeground(string_);
  m_rules.append({QRegularExpression(QStringLiteral("\"[^\"]*\"|'[^']*'")), stringFormat});

  // ── 4. 数字（橙色加粗） ──
  QTextCharFormat numberFormat;
  numberFormat.setForeground(number);
  numberFormat.setFontWeight(QFont::Bold);
  m_rules.append({QRegularExpression(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b")), numberFormat});

  // ── 5. 布尔值（红色加粗） ──
  static const QStringList kBoolLiterals = {QString::fromLatin1(AcKeyword::kTrue),
                                            QString::fromLatin1(AcKeyword::kFalse)};
  QTextCharFormat boolFormat;
  boolFormat.setForeground(boolean_);
  boolFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b(?:") + kBoolLiterals.join(QStringLiteral("|")) +
                          QStringLiteral(")\\b")),
       boolFormat});

  // ── 6. 函数调用（紫色） ──
  // 匹配任何非关键字的标识符后跟括号
  QTextCharFormat callFormat;
  callFormat.setForeground(call);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b(?!(?:") + AcKeyword::kAll.join(QStringLiteral("|")) +
                          QStringLiteral(")\\b)\\w+(?=\\s*\\()")),
       callFormat});
}

// 对单个文本块进行高亮处理
// 先处理单行注释（// 到行尾），再对剩余文本应用非注释规则
// 这样确保注释内的关键字、数字、函数名等不会被错误着色
void LightAc::highlightBlock(const QString &text) {
  int lineCommentStart = text.indexOf(QLatin1String("//"));
  if (lineCommentStart != -1) {
    highlightNonCommentText(text.left(lineCommentStart));
    setFormat(lineCommentStart, text.length() - lineCommentStart, m_commentFormat);
  } else {
    highlightNonCommentText(text);
  }
}

// 辅助函数：对非注释文本应用所有高亮规则
void LightAc::highlightNonCommentText(const QString &text) {
  for (const HighlightRule &rule : std::as_const(m_rules)) {
    QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
    while (matchIterator.hasNext()) {
      QRegularExpressionMatch match = matchIterator.next();
      setFormat(match.capturedStart(), match.capturedLength(), rule.format);
    }
  }
}