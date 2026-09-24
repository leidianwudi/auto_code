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
      QString::fromLatin1(AcTypeName::kNumber),  QString::fromLatin1(AcTypeName::kInt),
      QString::fromLatin1(AcTypeName::kFloat),   QString::fromLatin1(AcTypeName::kDouble),
      QString::fromLatin1(AcTypeName::kString),  QString::fromLatin1(AcTypeName::kBool),
      QString::fromLatin1(AcTypeName::kBoolean), QString::fromLatin1(AcTypeName::kAny),
      QString::fromLatin1(AcTypeName::kVoid),    QString::fromLatin1(AcTypeName::kArray),
      QString::fromLatin1(AcTypeName::kObject)};
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
// 注释区域用单次左→右扫描收集：谁的注释起点更靠前谁生效——
// 行注释（//）先出现则吞掉到行尾（其中的 "/*" 不开启块注释），
// 块注释（/*）先出现则跨行延续，直至 "*/" 闭合或行尾（状态 1 传给下一行）。
// 修复：此前先扫块注释再扫行注释、互不感知，行注释内的 "/*"
// （如 "crud_nest/*.jsonglobalenum"）会误开块注释且无 "*/" 闭合时
// 状态级联到文件末尾，后续全部代码被渲染为注释斜体
void LightAc::highlightBlock(const QString &text) {
  setCurrentBlockState(0);

  struct CommentRegion {
    int start;
    int length;
    bool isBlock;  // true=块注释（m_blockCommentFormat），false=行注释（m_commentFormat）
  };
  QVector<CommentRegion> regions;

  int pos = 0;
  if (previousBlockState() == 1) {
    // 上一行存在未闭合块注释：从行首继续，先找闭合 "*/"
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
      // 块注释起点更靠前：找闭合；未闭合则延伸到行尾并保持跨行状态
      const int end = text.indexOf(QStringLiteral("*/"), blockPos + 2);
      if (end == -1) {
        setCurrentBlockState(1);
        regions.append({blockPos, static_cast<int>(text.length()) - blockPos, true});
        break;
      }
      regions.append({blockPos, end - blockPos + 2, true});
      pos = end + 2;
    } else {
      // 行注释起点更靠前（含块注释闭合后出现 // 的情形）：吞掉到行尾
      regions.append({linePos, static_cast<int>(text.length()) - linePos, false});
      break;
    }
  }

  // ── 在非注释区间应用高亮规则 ──
  int hlPos = 0;
  for (const auto &r : regions) {
    if (r.start > hlPos) {
      highlightNonCommentText(text.mid(hlPos, r.start - hlPos), hlPos);
    }
    hlPos = qMax(hlPos, r.start + r.length);
  }
  if (hlPos < text.length()) {
    highlightNonCommentText(text.mid(hlPos), hlPos);
  }

  // ── 最后强制设置注释格式（防止被变量/运算符规则覆盖） ──
  for (const auto &r : regions) {
    setFormat(r.start, r.length, r.isBlock ? m_blockCommentFormat : m_commentFormat);
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