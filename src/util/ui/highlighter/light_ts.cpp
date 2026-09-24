/**
 * @file light_ts.cpp
 * @brief TypeScript 语法高亮器实现
 */

#include "light_ts.h"

#include "light_color.h"

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
LightTs::LightTs(QTextDocument *parent) : QSyntaxHighlighter(parent) { buildRules(); }

// 重建所有高亮规则（构造与主题刷新共用）
void LightTs::buildRules() {
  using namespace LightColor;
  m_rules.clear();

  // ── 关键字格式（蓝色加粗） ──
  QTextCharFormat keywordFormat;
  keywordFormat.setForeground(keyword());
  keywordFormat.setFontWeight(QFont::Bold);

  // ── 类型格式（青色） ──
  QTextCharFormat typeFormat;
  typeFormat.setForeground(type());

  // ── 字符串格式（橙色） ──
  QTextCharFormat stringFormat;
  stringFormat.setForeground(string_());

  // ── 注释格式（灰色斜体） ──
  QTextCharFormat commentFormat;
  commentFormat.setForeground(comment());
  commentFormat.setFontItalic(true);

  // ── 数字格式（紫色） ──
  QTextCharFormat numberFormat;
  numberFormat.setForeground(builtin());

  // ── 装饰器格式（洋红色） ──
  QTextCharFormat decoratorFormat;
  decoratorFormat.setForeground(decorator());

  // TypeScript 关键字列表
  // 包含 ES6+ 关键字和 TypeScript 特有关键字
  QStringList keywords = {"export",    "import",  "from",       "class",    "interface", "type",
                          "enum",      "extends", "implements", "abstract", "public",    "private",
                          "protected", "static",  "readonly",   "async",    "await",     "function",
                          "const",     "let",     "var",        "if",       "else",      "for",
                          "while",     "do",      "switch",     "case",     "break",     "continue",
                          "return",    "throw",   "try",        "catch",    "finally",   "new",
                          "delete",    "typeof",  "instanceof", "in",       "of",        "this",
                          "super"};

  // 为每个关键字注册正则规则（使用 \b 单词边界确保精确匹配）
  for (const QString &keyword : keywords) {
    m_rules.append({QRegularExpression(QStringLiteral("\\b") + keyword + QStringLiteral("\\b")),
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
    m_rules.append(
        {QRegularExpression(QStringLiteral("\\b") + type + QStringLiteral("\\b")), typeFormat});
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

// 重新从 SettingStore 读取颜色并刷新高亮
void LightTs::reloadColors() {
  buildRules();
  rehighlight();
}

/**
 * @brief 对单个文本块进行高亮处理
 * @param text 当前行的文本内容
 *
 * 处理流程：
 * 1. 单次左→右扫描收集注释区域（行注释与块注释先到先得，
 *    块注释用 previousBlockState/currentBlockState 跨行传递，状态 1 = 在注释中）
 * 2. 仅在非注释区间应用语法规则
 * 3. 最后统一施加注释格式（覆盖区间内其它规则着色）
 */
void LightTs::highlightBlock(const QString &text) {
  // ── 注释区域收集（单次左→右扫描，块/行注释按出现顺序处理）──
  // 修复：此前块注释扫描不感知行注释，行注释内的 "/*"（如 "see /a/*.md"）
  // 会误开块注释并在无 "*/" 闭合时级联到文件末尾（后续代码全部渲染为注释）
  setCurrentBlockState(0);

  struct CommentRegion {
    int start;
    int length;
    bool isBlock;
  };
  QVector<CommentRegion> regions;

  int pos = 0;
  if (previousBlockState() == 1) {
    const int end = text.indexOf(QStringLiteral("*/"));
    if (end == -1) {
      setCurrentBlockState(1);
      regions.append({0, static_cast<int>(text.length()), true});
    } else {
      regions.append({0, end + 2, true});
      pos = end + 2;
    }
  }
  while (pos < text.length()) {
    const int linePos = text.indexOf(QLatin1String("//"), pos);
    const int blockPos = text.indexOf(QStringLiteral("/*"), pos);
    if (linePos == -1 && blockPos == -1) break;
    if (blockPos != -1 && (linePos == -1 || blockPos < linePos)) {
      const int end = text.indexOf(QStringLiteral("*/"), blockPos + 2);
      if (end == -1) {
        setCurrentBlockState(1);
        regions.append({blockPos, static_cast<int>(text.length()) - blockPos, true});
        break;
      }
      regions.append({blockPos, end - blockPos + 2, true});
      pos = end + 2;
    } else {
      regions.append({linePos, static_cast<int>(text.length()) - linePos, false});
      break;
    }
  }

  // ── 非注释区间应用语法规则（此前整行应用，规则色会污染注释区间再被覆盖）──
  int hlPos = 0;
  for (const auto &r : regions) {
    if (r.start > hlPos) {
      for (const HighlightRule &rule : std::as_const(m_rules)) {
        QRegularExpressionMatchIterator matchIterator =
            rule.pattern.globalMatch(text, hlPos);
        while (matchIterator.hasNext()) {
          QRegularExpressionMatch match = matchIterator.next();
          if (match.capturedStart() >= r.start) break;  // 只处理本非注释区间内
          setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
      }
    }
    hlPos = qMax(hlPos, r.start + r.length);
  }
  if (hlPos < text.length()) {
    for (const HighlightRule &rule : std::as_const(m_rules)) {
      QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text, hlPos);
      while (matchIterator.hasNext()) {
        QRegularExpressionMatch match = matchIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), rule.format);
      }
    }
  }

  // ── 注释格式最后统一施加（覆盖区间内其它规则着色）──
  QTextCharFormat commentFormat;
  commentFormat.setForeground(LightColor::comment());
  commentFormat.setFontItalic(true);
  for (const auto &r : regions) {
    setFormat(r.start, r.length, commentFormat);
  }
}