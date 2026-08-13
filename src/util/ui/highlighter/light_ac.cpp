#include "light_ac.h"

#include "light_color.h"
#include "src/engine/ac_language.h"

// 构造函数：初始化所有高亮规则
// 注释格式单独存储，由 highlightBlock 统一处理
// m_rules 只包含非注释的 AC 语法元素规则（避免注释内的内容被错误着色）
LightAc::LightAc(QTextDocument *parent) : QSyntaxHighlighter(parent) { buildRules(); }

// 重建所有高亮规则（构造与主题刷新共用）
void LightAc::buildRules() {
  using namespace LightColor;
  m_rules.clear();

  // ── 注释格式（灰色斜体） ──
  // 不放入 m_rules，由 highlightBlock 单独处理
  m_commentFormat.setForeground(comment());
  m_commentFormat.setFontItalic(true);

  m_blockCommentFormat.setForeground(commentBlock());
  m_blockCommentFormat.setFontItalic(true);

  // ── 0. 变量（浅蓝/深蓝色，VSCode 变量色） ──
  // 放在最前面作为兜底规则，后面更具体的规则会覆盖它。
  // 使用 hl.variable 颜色（浅色=#001080 深蓝，深色=#9CDCFE 浅蓝），与关键字蓝色有区分。
  QTextCharFormat variableFormat;
  variableFormat.setForeground(variable());
  m_rules.append({QRegularExpression(QStringLiteral("\\b[a-zA-Z_]\\w*\\b")), variableFormat});

  // ── 1. 关键字（蓝色加粗） ──
  // 使用 (?<!\.) 负向后顾，排除属性访问（如 col.default）中的关键字高亮
  QTextCharFormat keywordFormat;
  keywordFormat.setForeground(keyword());
  keywordFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("(?<![\\.\\w])\\b(?:") +
                          AcKeyword::kAll.join(QStringLiteral("|")) + QStringLiteral(")\\b")),
       keywordFormat});

  // ── 2. 内置函数（紫色加粗） ──
  QTextCharFormat builtinFormat;
  builtinFormat.setForeground(builtin());
  builtinFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b(?:") + AcBuiltin::kAll.join(QStringLiteral("|")) +
                          QStringLiteral(")\\b")),
       builtinFormat});

  // ── 3. 字符串（红/橙） ──
  QTextCharFormat stringFormat;
  stringFormat.setForeground(string_());
  m_rules.append({QRegularExpression(QStringLiteral("\"[^\"]*\"|'[^']*'")), stringFormat});
  // 模板字符串（反引号，用模板字符串色）
  QTextCharFormat tplStringFormat;
  tplStringFormat.setForeground(stringTemplate());
  m_rules.append({QRegularExpression(QStringLiteral("`[^`]*`")), tplStringFormat});

  // ── 4. 数字（绿） ──
  QTextCharFormat numberFormat;
  numberFormat.setForeground(number());
  numberFormat.setFontWeight(QFont::Bold);
  m_rules.append({QRegularExpression(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b")), numberFormat});

  // ── 5. 布尔值 true/false（蓝/青） ──
  static const QStringList kBoolLiterals = {QString::fromLatin1(AcKeyword::kTrue),
                                            QString::fromLatin1(AcKeyword::kFalse)};
  QTextCharFormat boolFormat;
  boolFormat.setForeground(boolean_());
  boolFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b(?:") + kBoolLiterals.join(QStringLiteral("|")) +
                          QStringLiteral(")\\b")),
       boolFormat});

  // ── 5b. 空值 null/undefined（紫） ──
  static const QStringList kNullLiterals = {QString::fromLatin1(AcKeyword::kNull),
                                            QString::fromLatin1(AcKeyword::kUndefined)};
  QTextCharFormat nullFormat;
  nullFormat.setForeground(null_());
  nullFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b(?:") + kNullLiterals.join(QStringLiteral("|")) +
                          QStringLiteral(")\\b")),
       nullFormat});

  // ── 5c. 内置变量 this/self/super（蓝） ──
  QTextCharFormat specialFormat;
  specialFormat.setForeground(special());
  specialFormat.setFontWeight(QFont::Bold);
  m_rules.append({QRegularExpression(QStringLiteral("\\b(?:this|super)\\b")), specialFormat});

  // ── 6. 函数调用（黄色） ──
  // 匹配非关键字、非内置函数的标识符后跟括号
  // 排除关键字和内置函数，避免覆盖它们的颜色
  const QStringList excludedFromCall = AcKeyword::kAll + AcBuiltin::kAll;
  QTextCharFormat callFormat;
  callFormat.setForeground(call());
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\b(?!(?:") + excludedFromCall.join(QStringLiteral("|")) +
                          QStringLiteral(")\\b)\\w+(?=\\s*\\()")),
       callFormat});

  // ── 7. 运算符（青色加粗） ──
  QTextCharFormat opFormat;
  opFormat.setForeground(operator_());
  opFormat.setFontWeight(QFont::Bold);
  m_rules.append(
      {QRegularExpression(QStringLiteral("\\|\\||&&|!=|==|<=|>=|<|>|!|[+\\-*/]=?|\\?")), opFormat});

  // ── 8. 内建类型名（青色，VSCode 类型色） ──
  // String / Number / Int / Float / Double / Bool / Boolean / Any / Void / Array / Object
  // 严格区分大小写；(?<![\.\w]) 排除属性访问（如 obj.String）
  static const QStringList kTypeNames = {
      QStringLiteral("Number"),  QStringLiteral("Int"),    QStringLiteral("Float"),
      QStringLiteral("Double"),  QStringLiteral("String"), QStringLiteral("Bool"),
      QStringLiteral("Boolean"), QStringLiteral("Any"),    QStringLiteral("Void"),
      QStringLiteral("Array"),   QStringLiteral("Object")};
  QTextCharFormat typeFormat;
  typeFormat.setForeground(type());
  m_rules.append({QRegularExpression(QStringLiteral("(?<![\\.\\w])(?:") +
                                     kTypeNames.join(QStringLiteral("|")) + QStringLiteral(")\\b")),
                  typeFormat});

  // ── 9. 类 / 接口 / 枚举 声明名（青绿色，VSCode 类名色） ──
  // class Foo / interface Bar / enum Baz → 名字用类名色
  QTextCharFormat classNameFormat;
  classNameFormat.setForeground(className());
  m_rules.append(
      {QRegularExpression(QStringLiteral("(?<=\\b(?:class|interface|enum)\\s)[A-Za-z_]\\w*")),
       classNameFormat});

  // ── 10. 函数声明名（黄色，VSCode 函数声明色） ──
  // function foo(...) → foo 用函数声明色（放在 call 规则之后覆盖）
  QTextCharFormat funcNameFormat;
  funcNameFormat.setForeground(funcDecl());
  m_rules.append(
      {QRegularExpression(QStringLiteral("(?<=\\bfunction\\s)[A-Za-z_]\\w*")), funcNameFormat});

  // ── 11. new 实例化类名（青色） ──
  // new DB(...) / new File() → 类名用类型色
  QTextCharFormat newClassFormat;
  newClassFormat.setForeground(type());
  m_rules.append(
      {QRegularExpression(QStringLiteral("(?<=\\bnew\\s)(?:") +
                          AcClass::kAll.join(QStringLiteral("|")) + QStringLiteral(")\\b")),
       newClassFormat});

  // ── 12. 导入模块名（红/橙） ──
  // import "module" / import { a } from "module" / from "module"
  QTextCharFormat importFormat;
  importFormat.setForeground(import_());
  m_rules.append(
      {QRegularExpression(QStringLiteral("(?<=\\b(?:import|from)\\s+)(?:\"[^\"]*\"|'[^']*'|"
                                         "\\S+)")),
       importFormat});

  // ── 13. 标点符号（深灰/浅灰） ──
  // 括号、分号、逗号、冒号、点号等（排除运算符与数字/标识符）
  QTextCharFormat punctFormat;
  punctFormat.setForeground(punctuation());
  m_rules.append({QRegularExpression(QStringLiteral("[(){}\\[\\];,:.]")), punctFormat});
}

// 重新从 SettingStore 读取颜色并刷新高亮
void LightAc::reloadColors() {
  buildRules();
  rehighlight();
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