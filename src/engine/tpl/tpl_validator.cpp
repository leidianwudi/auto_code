/**
 * @file tpl_validator.cpp
 * @brief 模板语法验证器实现
 */

#include "tpl_validator.h"

#include <QRegularExpression>
#include <QStack>
#include <QStringList>

#include "../ac_language.h"

// ── 常量 ──
namespace {
const QString kEachName = QString::fromLatin1(AcTemplate::kEachPrefix).trimmed();
const QString kIfName = QString::fromLatin1(AcTemplate::kIfPrefix).trimmed();
const QString kElseName = QString::fromLatin1(AcTemplate::kElse).mid(2, 4);
}  // namespace

QVector<ValidationResult> TplValidator::validate(const QString &source) {
  QVector<ValidationResult> results;

  if (source.isEmpty()) return results;

  checkBrackets(source, results);
  checkDollarBraces(source, results);
  checkTags(source, results);
  checkMethods(source, results);

  // 按行号排序
  std::sort(results.begin(), results.end(),
            [](const ValidationResult &a, const ValidationResult &b) {
              if (a.line != b.line) return a.line < b.line;
              return a.column < b.column;
            });

  return results;
}

// ═════════════════════════════════════════════════════════════════════════════
//  第 1 步：检查普通括号 () [] {} 匹配
// ═════════════════════════════════════════════════════════════════════════════

void TplValidator::checkBrackets(const QString &text, QVector<ValidationResult> &results) {
  struct BracketInfo {
    QChar ch;
    int pos;
  };
  QStack<BracketInfo> stack;
  int len = text.length();
  bool inString = false;
  bool inTemplateExpr = false;
  int templateExprDepth = 0;

  for (int i = 0; i < len; ++i) {
    QChar ch = text[i];

    // 跟踪 ${...} 表达式内部
    if (ch == '$' && i + 1 < len && text[i + 1] == '{') {
      inTemplateExpr = true;
      templateExprDepth++;
      i++;
      continue;
    }
    if (inTemplateExpr) {
      if (ch == '{') {
        templateExprDepth++;
      } else if (ch == '}') {
        templateExprDepth--;
        if (templateExprDepth <= 0) {
          inTemplateExpr = false;
          templateExprDepth = 0;
        }
      }
      continue;
    }

    // 字符串字面量
    if ((ch == '"' || ch == '\'') && (i == 0 || text[i - 1] != '\\')) {
      inString = !inString;
      continue;
    }
    if (inString) continue;

    // 单行注释
    if (ch == '/' && i + 1 < len && text[i + 1] == '/') {
      while (i < len && text[i] != '\n') ++i;
      continue;
    }

    // 栈匹配
    if (ch == '(' || ch == '[') {
      stack.push({ch, i});
    } else if (ch == ')' || ch == ']') {
      QChar expected = (ch == ')') ? '(' : '[';
      if (stack.isEmpty()) {
        results.append(ValidationResult::atLine(
            offsetToLine(text, i), QStringLiteral("多余的 '%1' 在位置 %2").arg(ch).arg(i + 1)));
      } else if (stack.top().ch != expected) {
        results.append(ValidationResult::atLine(
            offsetToLine(text, i), QStringLiteral("括号不匹配: '%1' 在位置 %2，期望 '%3'")
                                       .arg(ch)
                                       .arg(i + 1)
                                       .arg(expected)));
        while (!stack.isEmpty() && stack.top().ch != expected) {
          stack.pop();
        }
        if (!stack.isEmpty()) stack.pop();
      } else {
        stack.pop();
      }
    }
  }

  // 未闭合的开括号
  while (!stack.isEmpty()) {
    auto info = stack.pop();
    results.append(ValidationResult::atLine(
        offsetToLine(text, info.pos),
        QStringLiteral("未闭合的 '%1' 在位置 %2").arg(info.ch).arg(info.pos + 1)));
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  第 2 步：检查 ${...} 是否闭合
// ═════════════════════════════════════════════════════════════════════════════

void TplValidator::checkDollarBraces(const QString &text, QVector<ValidationResult> &results) {
  int len = text.length();
  for (int i = 0; i < len; ++i) {
    if (text[i] == '$' && i + 1 < len && text[i + 1] == '{') {
      int depth = 1;
      int start = i;
      i += 2;
      while (i < len && depth > 0) {
        if (text[i] == '{')
          depth++;
        else if (text[i] == '}')
          depth--;
        if (depth > 0) i++;
      }
      if (depth > 0) {
        results.append(ValidationResult::atLine(
            offsetToLine(text, start), QStringLiteral("未闭合的 ${ 在位置 %1").arg(start + 1)));
      }
    }
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  第 3 步：检查 ${each}/${if} 标签配对
// ═════════════════════════════════════════════════════════════════════════════

void TplValidator::checkTags(const QString &text, QVector<ValidationResult> &results) {
  static const QRegularExpression tagRegex(
      QStringLiteral("\\$\\{(") + kEachName + QStringLiteral("|") + kIfName + QStringLiteral("|") +
      kElseName + QStringLiteral("|/") + kEachName + QStringLiteral("|/") + kIfName +
      QStringLiteral(")\\b[^}]*\\}"));

  struct TagInfo {
    QString tag;
    int pos;
    int length;
  };
  QStack<TagInfo> tagStack;

  auto it = tagRegex.globalMatch(text);
  while (it.hasNext()) {
    auto match = it.next();
    QString tag = match.captured(1);
    int pos = match.capturedStart();
    int tagLen = match.capturedLength();

    if (tag == kEachName || tag == kIfName) {
      tagStack.push({tag, pos, tagLen});
    } else if (tag == QLatin1Char('/') + kEachName || tag == QLatin1Char('/') + kIfName) {
      QString openTag = tag.mid(1);
      if (tagStack.isEmpty()) {
        results.append(ValidationResult::atLine(
            offsetToLine(text, pos),
            QStringLiteral("多余的 ${/%1} 在位置 %2").arg(openTag).arg(pos + 1)));
      } else if (tagStack.top().tag != openTag) {
        results.append(ValidationResult::atLine(
            offsetToLine(text, pos), QStringLiteral("标签不匹配: ${/%1} 在位置 %2，期望 ${/%3}")
                                         .arg(openTag)
                                         .arg(pos + 1)
                                         .arg(tagStack.top().tag)));
        while (!tagStack.isEmpty() && tagStack.top().tag != openTag) {
          auto unmatched = tagStack.pop();
          results.append(ValidationResult::atLine(offsetToLine(text, unmatched.pos),
                                                  QStringLiteral("未闭合的 ${%1} 在位置 %2")
                                                      .arg(unmatched.tag)
                                                      .arg(unmatched.pos + 1)));
        }
        if (!tagStack.isEmpty()) tagStack.pop();
      } else {
        tagStack.pop();
      }
    }
  }

  while (!tagStack.isEmpty()) {
    auto info = tagStack.pop();
    results.append(ValidationResult::atLine(
        offsetToLine(text, info.pos),
        QStringLiteral("未闭合的 ${%1} 在位置 %2").arg(info.tag).arg(info.pos + 1)));
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  第 4 步：检查 ${...} 表达式中不支持的方法调用
// ═════════════════════════════════════════════════════════════════════════════

void TplValidator::checkMethods(const QString &text, QVector<ValidationResult> &results) {
  static const QStringList supportedMethods = {
      QString::fromLatin1(AcCallStr::kToLowerCase), QString::fromLatin1(AcCallStr::kToUpperCase),
      QString::fromLatin1(AcCallStr::kTrim), QString::fromLatin1(AcCallStr::kCapitalize)};

  static const QRegularExpression exprRegex(
      QStringLiteral("\\$\\{(?!(?:") + kEachName + QStringLiteral("|") + kIfName +
      QStringLiteral("|") + kElseName + QStringLiteral("|/") + kEachName + QStringLiteral("|/") +
      kIfName + QStringLiteral(")\\b)[^}]+\\}"));

  static const QRegularExpression identRegex(QStringLiteral(R"(^[a-zA-Z_][a-zA-Z0-9_]*$)"));

  auto looksLikeMethod = [](const QString &seg) -> bool {
    if (seg.isEmpty()) return false;
    if (!seg[0].isLetter() && seg[0] != '_') return true;
    for (const QChar &c : seg) {
      if (c.isUpper()) return true;
    }
    return false;
  };

  auto exprIt = exprRegex.globalMatch(text);
  while (exprIt.hasNext()) {
    auto exprMatch = exprIt.next();

    // 跳过注释中的表达式
    int lineStart = text.lastIndexOf(QLatin1Char('\n'), exprMatch.capturedStart());
    if (lineStart == -1) lineStart = 0;
    QString linePrefix = text.mid(lineStart, exprMatch.capturedStart() - lineStart);
    if (linePrefix.contains(QStringLiteral("//"))) continue;

    QString exprContent = exprMatch.captured().mid(2, exprMatch.capturedLength() - 3);
    int exprStart = exprMatch.capturedStart() + 2;

    QStringList segments = exprContent.split(QLatin1Char('.'));
    if (segments.size() < 2) continue;

    int segPos = exprStart;
    segPos += segments[0].length() + 1;

    for (int s = 1; s < segments.size(); ++s) {
      QString seg = segments[s].trimmed();

      if (looksLikeMethod(seg) && !supportedMethods.contains(seg)) {
        results.append(ValidationResult(
            offsetToLine(text, segPos), 0, seg.length(),
            QStringLiteral("不支持的方法 '%1'，支持: %2").arg(seg, supportedMethods.join(", "))));
      } else if (!identRegex.match(seg).hasMatch() && !supportedMethods.contains(seg)) {
        results.append(ValidationResult(offsetToLine(text, segPos), 0, seg.length(),
                                        QStringLiteral("无效的标识符 '%1'").arg(seg)));
      }

      if (s + 1 < segments.size()) segPos += seg.length() + 1;
    }
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  辅助函数
// ═════════════════════════════════════════════════════════════════════════════

int TplValidator::offsetToLine(const QString &text, int offset) const {
  if (offset <= 0) return 1;
  int line = 1;
  for (int i = 0; i < offset && i < text.length(); ++i) {
    if (text[i] == QLatin1Char('\n')) ++line;
  }
  return line;
}