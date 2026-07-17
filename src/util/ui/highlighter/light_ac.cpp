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

  m_blockCommentFormat.setForeground(comment);
  m_blockCommentFormat.setFontItalic(true);

  // ── 0. 变量（绿色） ──
  // 放在最前面作为兜底规则，后面更具体的规则会覆盖它
  QTextCharFormat variableFormat;
  variableFormat.setForeground(variable);
  m_rules.append({QRegularExpression(QStringLiteral("\\b[a-zA-Z_]\\w*\\b")), variableFormat});

  // ── 1. 关键字（蓝色加粗） ──
  // 使用 (?<!\.) 负向后顾，排除属性访问（如 col.default）中的关键字高亮
  QTextCharFormat keywordFormat;
  keywordFormat.setForeground(keyword);
  keywordFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("(?<![\\.\\w])\\b(?:") +
                          AcKeyword::kAll.join(QStringLiteral("|")) + QStringLiteral(")\\b")),
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
  m_rules.append({QRegularExpression(QStringLiteral("\"[^\"]*\"|'[^']*'|`[^`]*`")), stringFormat});

  // ── 4. 数字（橙色加粗） ──
  QTextCharFormat numberFormat;
  numberFormat.setForeground(number);
  numberFormat.setFontWeight(QFont::Bold);
  m_rules.append({QRegularExpression(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b")), numberFormat});

  // ── 5. 布尔值/null/undefined（红色加粗） ──
  static const QStringList kBoolLiterals = {
      QString::fromLatin1(AcKeyword::kTrue), QString::fromLatin1(AcKeyword::kFalse),
      QString::fromLatin1(AcKeyword::kNull), QString::fromLatin1(AcKeyword::kUndefined)};
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

  // ── 7. 运算符（青色加粗） ──
  QTextCharFormat opFormat;
  opFormat.setForeground(operator_);
  opFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\|\\||&&|!=|==|<=|>=|<|>|!|[+\\-*/]=?|\\?")), opFormat});
}

// 对单个文本块进行高亮处理
// 1. 先收集所有块注释 /* ... */ 的区域（支持跨行，使用状态机制）
// 2. 收集单行注释 // 的区域
// 3. 只在非注释区域应用其他高亮规则
// 4. 最后强制恢复注释格式（防止被运算符规则覆盖）
void LightAc::highlightBlock(const QString &text) {
  // ── 收集块注释区域 ──
  // 状态 1 = 在块注释中（上一行 /* 未闭合）
  setCurrentBlockState(0);

  struct CommentRegion {
    int start;
    int length;
  };
  QVector<CommentRegion> blockRegions;

  int startIndex = 0;
  if (previousBlockState() != 1) {
    startIndex = text.indexOf(QStringLiteral("/*"));
  } else {
    // 上一行未闭合的块注释，从行首开始
    int endIndex = text.indexOf(QStringLiteral("*/"));
    if (endIndex == -1) {
      setCurrentBlockState(1);
      blockRegions.append({0, static_cast<int>(text.length())});
    } else {
      blockRegions.append({0, endIndex + 2});
      startIndex = text.indexOf(QStringLiteral("/*"), endIndex + 2);
    }
  }

  while (startIndex >= 0) {
    int endIndex = text.indexOf(QStringLiteral("*/"), startIndex);
    int commentLength;

    if (endIndex == -1) {
      setCurrentBlockState(1);
      commentLength = static_cast<int>(text.length()) - startIndex;
    } else {
      commentLength = endIndex - startIndex + 2;
    }

    blockRegions.append({startIndex, commentLength});
    startIndex = text.indexOf(QStringLiteral("/*"), startIndex + commentLength);
  }

  // ── 收集单行注释区域（不在块注释内的 //） ──
  QVector<CommentRegion> lineRegions;
  int lineCommentStart = text.indexOf(QLatin1String("//"));
  while (lineCommentStart >= 0) {
    // 检查此位置是否在块注释内
    bool inBlock = false;
    for (const auto &r : blockRegions) {
      if (lineCommentStart >= r.start && lineCommentStart < r.start + r.length) {
        inBlock = true;
        break;
      }
    }
    if (!inBlock) {
      lineRegions.append({lineCommentStart, static_cast<int>(text.length()) - lineCommentStart});
      break;  // 只有一个单行注释区域（到行尾）
    }
    lineCommentStart = text.indexOf(QLatin1String("//"), lineCommentStart + 2);
  }

  // ── 在非注释区域应用高亮规则 ──
  // 构建非注释区间，逐段应用规则
  if (blockRegions.isEmpty() && lineRegions.isEmpty()) {
    highlightNonCommentText(text);
  } else {
    // 合并所有注释区域并排序
    QVector<CommentRegion> allRegions = blockRegions + lineRegions;
    std::sort(allRegions.begin(), allRegions.end(),
              [](const CommentRegion &a, const CommentRegion &b) { return a.start < b.start; });

    // 提取非注释区间
    int pos = 0;
    for (const auto &r : allRegions) {
      if (r.start > pos) {
        highlightNonCommentText(text.mid(pos, r.start - pos), pos);
      }
      pos = qMax(pos, r.start + r.length);
    }
    if (pos < text.length()) {
      highlightNonCommentText(text.mid(pos), pos);
    }
  }

  // ── 最后强制设置注释格式（防止被运算符规则覆盖） ──
  for (const auto &r : blockRegions) {
    setFormat(r.start, r.length, m_blockCommentFormat);
  }
  for (const auto &r : lineRegions) {
    setFormat(r.start, r.length, m_commentFormat);
  }
}

// 辅助函数：对非注释文本应用所有高亮规则
// offset 参数指定文本在原始行中的起始位置
void LightAc::highlightNonCommentText(const QString &text, int offset) {
  for (const HighlightRule &rule : std::as_const(m_rules)) {
    QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
    while (matchIterator.hasNext()) {
      QRegularExpressionMatch match = matchIterator.next();
      setFormat(match.capturedStart() + offset, match.capturedLength(), rule.format);
    }
  }
}