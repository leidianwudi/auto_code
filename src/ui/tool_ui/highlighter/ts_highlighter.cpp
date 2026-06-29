/**
 * @file ts_highlighter.cpp
 * @brief TypeScript 语法高亮器实现
 */

#include "ts_highlighter.h"

/**
 * @brief 构造函数
 * @param parent 父文档
 *
 * 初始化 TypeScript 语法高亮规则。
 * 着色方案：
 * - 关键字（蓝色加粗）：export, class, const, function 等
 * - 内置类型（青色）：string, number, boolean, void 等
 * - 字符串（橙色）："" 和 '' 字面量
 * - 模板字符串（橙色）：`` 反引号字符串
 * - 注释（灰色斜体）：// 单行注释和多行注释
 * - 数字（紫色）：整数和浮点数
 * - 装饰器（洋红色）：@Component 等
 */
TypeScriptHighlighter::TypeScriptHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {
  // ── 关键字格式（蓝色加粗） ──
  QTextCharFormat keywordFormat;
  keywordFormat.setForeground(QColor("#0000FF"));
  keywordFormat.setFontWeight(QFont::Bold);

  // ── 类型格式（青色） ──
  QTextCharFormat typeFormat;
  typeFormat.setForeground(QColor("#267F99"));

  // ── 字符串格式（橙色） ──
  QTextCharFormat stringFormat;
  stringFormat.setForeground(QColor("#FF8C00"));

  // ── 注释格式（灰色斜体） ──
  QTextCharFormat commentFormat;
  commentFormat.setForeground(QColor("#808080"));
  commentFormat.setFontItalic(true);

  // ── 数字格式（紫色） ──
  QTextCharFormat numberFormat;
  numberFormat.setForeground(QColor("#800080"));

  // ── 装饰器格式（洋红色） ──
  QTextCharFormat decoratorFormat;
  decoratorFormat.setForeground(QColor("#FF00FF"));

  // TypeScript 关键字列表
  // 包含 ES6+ 关键字和 TypeScript 特有关键字
  QStringList keywords = {
      "export",    "import",  "from",       "class",    "interface", "type",
      "enum",      "extends", "implements", "abstract", "public",    "private",
      "protected", "static",  "readonly",   "async",    "await",     "function",
      "const",     "let",     "var",        "if",       "else",      "for",
      "while",     "do",      "switch",     "case",     "break",     "continue",
      "return",    "throw",   "try",        "catch",    "finally",   "new",
      "delete",    "typeof",  "instanceof", "in",       "of",        "this",
      "super"};

  // 为每个关键字注册正则规则（使用 \b 单词边界确保精确匹配）
  for (const QString &keyword : keywords) {
    m_rules.append({QRegularExpression(QStringLiteral("\\b") + keyword +
                                       QStringLiteral("\\b")),
                    keywordFormat});
  }

  // TypeScript 内置类型和工具类型列表
  // 包括基础类型和常用的 Utility Types
  QStringList types = {"string",      "number",
                       "boolean",     "void",
                       "null",        "undefined",
                       "never",       "any",
                       "unknown",     "object",
                       "symbol",      "bigint",
                       "Array",       "Promise",
                       "Record",      "Partial",
                       "Required",    "Readonly",
                       "Pick",        "Omit",
                       "Exclude",     "Extract",
                       "NonNullable", "ReturnType",
                       "Parameters",  "ConstructorParameters"};

  // 为每个类型注册正则规则
  for (const QString &type : types) {
    m_rules.append({QRegularExpression(QStringLiteral("\\b") + type +
                                       QStringLiteral("\\b")),
                    typeFormat});
  }

  // 字符串：单引号和双引号字符串
  m_rules.append({QRegularExpression("\"[^\"]*\"|'[^']*'"), stringFormat});

  // 模板字符串：反引号包围的字符串，支持 ${} 插值
  m_rules.append({QRegularExpression("`[^`]*`"), stringFormat});

  // 单行注释：// 到行尾
  m_rules.append({QRegularExpression("//[^\n]*"), commentFormat});

  // 数字：整数和浮点数
  m_rules.append({QRegularExpression("\\b\\d+(\\.\\d+)?\\b"), numberFormat});

  // 装饰器：@ 开头后跟标识符
  m_rules.append({QRegularExpression("@\\w+"), decoratorFormat});
}

/**
 * @brief 对单个文本块进行高亮处理
 * @param text 当前行的文本内容
 *
 * 处理流程：
 * 1. 遍历所有高亮规则，对匹配的文本应用对应的格式
 * 2. 处理多行注释（块注释），使用 previousBlockState/currentBlockState
 *    跨行传递注释状态（状态 1 = 在注释中）
 */
void TypeScriptHighlighter::highlightBlock(const QString &text) {
  // 应用所有高亮规则（单行匹配）
  for (const HighlightRule &rule : std::as_const(m_rules)) {
    QRegularExpressionMatchIterator matchIterator =
        rule.pattern.globalMatch(text);
    while (matchIterator.hasNext()) {
      QRegularExpressionMatch match = matchIterator.next();
      setFormat(match.capturedStart(), match.capturedLength(), rule.format);
    }
  }

  // 处理多行注释 /* ... */
  // 使用 QSyntaxHighlighter 的状态机制跨行传递注释状态
  setCurrentBlockState(0); // 默认不在注释中

  // 如果上一行以未闭合的 /* 结束，则从行首开始高亮
  int startIndex = 0;
  if (previousBlockState() != 1) {
    // 上一行不在注释中，本行从头查找 /*
    startIndex = text.indexOf("/*");
  }

  while (startIndex >= 0) {
    // 查找配对的 */ 结束标记
    int endIndex = text.indexOf("*/", startIndex);
    int commentLength;

    if (endIndex == -1) {
      // 未找到 */，注释延伸到下一行，设置跨行状态
      setCurrentBlockState(1);
      commentLength = text.length() - startIndex;
    } else {
      // 找到 */，注释在本行内结束
      commentLength = endIndex - startIndex + 2;
    }

    // 应用灰色斜体格式
    QTextCharFormat commentFormat;
    commentFormat.setForeground(QColor("#808080"));
    commentFormat.setFontItalic(true);
    setFormat(startIndex, commentLength, commentFormat);

    // 继续查找下一个 /*
    startIndex = text.indexOf("/*", startIndex + commentLength);
  }
}