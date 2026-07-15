/**
 * @file format_code.cpp
 * @brief 代码自动排版格式化工具实现
 */

#include "format_code.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStringList>

#include "src/engine/ac_language.h"
#include "src/util/common/util_json.h"

// ──────────────────────────────────────────────────────────────
//  公共接口
// ──────────────────────────────────────────────────────────────

// 配对字符表
const FormatCode::CharPair FormatCode::kPairs[] = {
    {QLatin1Char('{'), QLatin1Char('}')},
    {QLatin1Char('['), QLatin1Char(']')},
    {QLatin1Char('('), QLatin1Char(')')},
    {QLatin1Char('"'), QLatin1Char('"')},
};

bool FormatCode::isOpenChar(QChar ch) {
  for (int i = 0; i < kPairCount; ++i) {
    if (kPairs[i].open == ch) return true;
  }
  return false;
}

QChar FormatCode::matchingCloseChar(QChar open) {
  for (int i = 0; i < kPairCount; ++i) {
    if (kPairs[i].open == open) return kPairs[i].close;
  }
  return QChar();
}

bool FormatCode::isCloseChar(QChar ch) {
  for (int i = 0; i < kPairCount; ++i) {
    if (kPairs[i].close == ch && kPairs[i].open != ch) return true;
  }
  return false;
}

bool FormatCode::shouldAutoPair(QChar open, QChar charBefore) {
  // 不支持配对的开字符 → 不配对
  if (!isOpenChar(open)) return false;

  // " 的特殊处理：前面是字母/数字时不配对（避免 typo 场景）
  if (open == QLatin1Char('"') && charBefore.isLetterOrNumber()) return false;

  return true;
}

QString FormatCode::format(const QString &input, FormatMode mode) {
  switch (mode) {
    case FormatJson:
      return formatJson(input);
    case FormatAc:
      return formatAc(input);
    case FormatTpl:
      return formatTpl(input);
  }
  return input;
}

// ──────────────────────────────────────────────────────────────
//  JSON 格式化（2 空格缩进）
// ──────────────────────────────────────────────────────────────

/**
 * @brief 递归序列化 QJsonValue 为格式化字符串
 */
static void serializeJsonValue(const QJsonValue &v, QStringList &lines, int indent, bool isLast) {
  const QString prefix(indent * FormatCode::kIndentSize, QLatin1Char(' '));

  switch (v.type()) {
    case QJsonValue::Object: {
      QJsonObject obj = v.toObject();
      if (obj.isEmpty()) {
        lines.last() += QStringLiteral("{}");
        return;
      }
      lines.last() += QLatin1Char('{');
      QStringList keys = obj.keys();
      for (int i = 0; i < keys.size(); ++i) {
        const QString &key = keys[i];
        lines.append(prefix + QStringLiteral("  \"") + key + QStringLiteral("\": "));
        serializeJsonValue(obj.value(key), lines, indent + 1, i == keys.size() - 1);
      }
      lines.append(prefix + QLatin1Char('}'));
      break;
    }
    case QJsonValue::Array: {
      QJsonArray arr = v.toArray();
      if (arr.isEmpty()) {
        lines.last() += QStringLiteral("[]");
        return;
      }
      lines.last() += QLatin1Char('[');
      for (int i = 0; i < arr.size(); ++i) {
        lines.append(prefix + QStringLiteral("  "));
        serializeJsonValue(arr[i], lines, indent + 1, i == arr.size() - 1);
      }
      lines.append(prefix + QLatin1Char(']'));
      break;
    }
    case QJsonValue::String:
      lines.last() += QLatin1Char('"') + v.toString() + QLatin1Char('"');
      if (!isLast) lines.last() += QLatin1Char(',');
      break;
    case QJsonValue::Double:
      // 避免输出科学计数法
      lines.last() += QString::number(v.toDouble(), 'f', 6);
      // 去掉末尾多余的 0
      if (lines.last().contains(QLatin1Char('.'))) {
        while (lines.last().endsWith(QLatin1Char('0'))) lines.last().chop(1);
        if (lines.last().endsWith(QLatin1Char('.'))) lines.last().chop(1);
      }
      if (!isLast) lines.last() += QLatin1Char(',');
      break;
    case QJsonValue::Bool:
      lines.last() += v.toBool() ? QString::fromLatin1(AcKeyword::kTrue)
                                 : QString::fromLatin1(AcKeyword::kFalse);
      if (!isLast) lines.last() += QLatin1Char(',');
      break;
    case QJsonValue::Null:
      lines.last() += QStringLiteral("null");
      if (!isLast) lines.last() += QLatin1Char(',');
      break;
    case QJsonValue::Undefined:
      break;
  }
}

// JsonCommentInfo — 记录一条注释信息
struct JsonCommentInfo {
  int line;       // 注释所在行号（0-based）
  int col;        // 注释起始列号（该行内的字符位置）
  QString text;   // 注释完整内容（包含 // 或 /* */）
  bool isInline;  // 是否为行内注释（同一行有代码内容）
};

// extractJsonComments — 从原始 JSON 文本中提取所有注释
// 返回按行号排序的注释列表
static QVector<JsonCommentInfo> extractJsonComments(const QString &input) {
  QVector<JsonCommentInfo> comments;
  QStringList srcLines = input.split(QLatin1Char('\n'));

  for (int lineIdx = 0; lineIdx < srcLines.size(); ++lineIdx) {
    const QString &line = srcLines[lineIdx];
    enum State { kNormal, kString, kLineComment, kBlockComment };
    State state = kNormal;
    int commentStart = -1;
    QString commentText;

    for (int i = 0; i < line.size(); ++i) {
      const QChar ch = line[i];

      switch (state) {
        case kNormal:
          if (ch == QLatin1Char('"')) {
            state = kString;
          } else if (ch == QLatin1Char('/') && i + 1 < line.size()) {
            const QChar next = line[i + 1];
            if (next == QLatin1Char('/')) {
              state = kLineComment;
              commentStart = i;
              commentText = line.mid(i);
              break;
            } else if (next == QLatin1Char('*')) {
              state = kBlockComment;
              commentStart = i;
              commentText += ch;
              ++i;
            }
          }
          break;

        case kString:
          if (ch == QLatin1Char('\\') && i + 1 < line.size()) {
            ++i;
          } else if (ch == QLatin1Char('"')) {
            state = kNormal;
          }
          break;

        case kBlockComment:
          commentText += ch;
          if (ch == QLatin1Char('*') && i + 1 < line.size() && line[i + 1] == QLatin1Char('/')) {
            commentText += QLatin1Char('/');
            ++i;
            state = kNormal;
          }
          break;

        default:
          break;
      }
    }

    if (!commentText.isEmpty() && commentStart >= 0) {
      JsonCommentInfo info;
      info.line = lineIdx;
      info.col = commentStart;
      info.isInline = (commentStart > 0 && line.left(commentStart).trimmed().isEmpty() == false);

      // 处理跨行块注释
      if (state == kBlockComment) {
        info.text = commentText + QLatin1Char('\n');
        for (int j = lineIdx + 1; j < srcLines.size(); ++j) {
          const QString &blockLine = srcLines[j];
          int endPos = blockLine.indexOf(QLatin1String("*/"));
          if (endPos != -1) {
            info.text += blockLine.left(endPos + 2);
            break;
          } else {
            info.text += blockLine + QLatin1Char('\n');
          }
        }
      } else {
        info.text = commentText;
      }

      comments.append(info);
    }
  }

  return comments;
}

QString FormatCode::formatJson(const QString &input) {
  QJsonParseError err;
  QJsonDocument doc = UtilJson::fromJson(input, &err);
  if (err.error != QJsonParseError::NoError) return input;

  QVector<JsonCommentInfo> allComments = extractJsonComments(input);

  // 分离行内注释和独立注释
  QVector<JsonCommentInfo> inlineComments;      // 行内注释（与代码同行）
  QVector<JsonCommentInfo> standaloneComments;  // 独立注释行

  for (const JsonCommentInfo &cmt : allComments) {
    if (cmt.isInline) {
      inlineComments.append(cmt);
    } else {
      standaloneComments.append(cmt);
    }
  }

  QStringList lines;
  lines.append(QString());
  serializeJsonValue(doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array()), lines, 0,
                     true);

  while (!lines.isEmpty() && lines.first().trimmed().isEmpty()) lines.removeFirst();
  while (!lines.isEmpty() && lines.last().trimmed().isEmpty()) lines.removeLast();

  // 将行内注释分散到对应的值行（"key": value 格式的行）末尾
  QStringList resultLines;
  int inlineIdx = 0;

  for (int i = 0; i < lines.size(); ++i) {
    QString line = lines[i];

    // 只对包含键值对的行（有引号且不是纯括号行）追加行内注释
    bool isValueLine = line.contains(QLatin1Char('"')) &&
                       !line.trimmed().startsWith(QLatin1Char('{')) &&
                       !line.trimmed().startsWith(QLatin1Char('[')) &&
                       !line.trimmed().startsWith(QLatin1Char('}')) &&
                       !line.trimmed().startsWith(QLatin1Char(']'));

    if (isValueLine && inlineIdx < inlineComments.size()) {
      line += QLatin1Char(' ') + inlineComments[inlineIdx].text.trimmed();
      ++inlineIdx;
    }

    resultLines.append(line);

    // 在当前行后插入独立注释（如果有）
    // 简单策略：把独立注释均匀分布在输出中或集中放在末尾
  }

  // 追加剩余未分配的行内注释
  while (inlineIdx < inlineComments.size()) {
    resultLines.append(inlineComments[inlineIdx].text.trimmed());
    ++inlineIdx;
  }

  // 追加所有独立注释到末尾
  for (const JsonCommentInfo &cmt : standaloneComments) {
    resultLines.append(cmt.text.trimmed());
  }

  return resultLines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

// ──────────────────────────────────────────────────────────────
//  AC 脚本格式化（基于 {} 缩进）
// ──────────────────────────────────────────────────────────────

/**
 * @brief 判断一行是否只包含注释（去除前导空格后以 // 开头）
 */
static bool isCommentLine(const QString &line) {
  return line.trimmed().startsWith(QStringLiteral("//"));
}

QString FormatCode::formatAc(const QString &input) {
  QStringList srcLines = input.split(QLatin1Char('\n'));
  QStringList out;
  int indent = 0;

  for (int i = 0; i < srcLines.size(); ++i) {
    QString raw = srcLines[i];
    QString trimmed = raw.trimmed();

    // 保留空行
    if (trimmed.isEmpty()) {
      out.append(QString());
      continue;
    }

    // 注释行：保持原始缩进（去除前导空格后按当前缩进重新缩进）
    if (isCommentLine(raw)) {
      QString prefix(indent * kIndentSize, QLatin1Char(' '));
      out.append(prefix + trimmed);
      continue;
    }

    // 计算此行开头的 } 或 ] 数量（用于提前减少缩进）
    int closeBraces = 0;
    int pos = 0;
    while (pos < trimmed.size() &&
           (trimmed[pos] == QLatin1Char('}') || trimmed[pos] == QLatin1Char(']'))) {
      ++closeBraces;
      ++pos;
    }

    // 如果此行以 } 或 ] 开头，先减少缩进
    int effectiveIndent = indent;
    if (closeBraces > 0) effectiveIndent = qMax(0, indent - closeBraces);

    // 构建缩进前缀
    QString prefix(effectiveIndent * kIndentSize, QLatin1Char(' '));
    out.append(prefix + trimmed);

    // 更新缩进：计算此行中的 { } [ ] 数量
    int openCount = 0;
    int closeCount = 0;
    for (const QChar &ch : trimmed) {
      if (ch == QLatin1Char('{') || ch == QLatin1Char('['))
        ++openCount;
      else if (ch == QLatin1Char('}') || ch == QLatin1Char(']'))
        ++closeCount;
    }

    indent += openCount - closeCount;
    if (indent < 0) indent = 0;
  }

  return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

// ──────────────────────────────────────────────────────────────
//  TPL 模板格式化（基于 ${each}/${if} 缩进）
// ──────────────────────────────────────────────────────────────

QString FormatCode::formatTpl(const QString &input) {
  QStringList srcLines = input.split(QLatin1Char('\n'));
  QStringList out;
  int indent = 0;

  // 正则：检测模板控制标签
  // 开标签：${each ...} 或 ${if ...}
  // 闭标签：${/each} 或 ${/if}
  // else 标签：${else}
  static const QRegularExpression openTagRe(
      QStringLiteral("\\$\\{(") + QString::fromLatin1(AcTemplate::kEachPrefix).trimmed() +
      QStringLiteral("\\b|") + QString::fromLatin1(AcTemplate::kIfPrefix).trimmed() +
      QStringLiteral("\\b)"));
  static const QRegularExpression closeTagRe(
      QStringLiteral("\\$\\{/(") + QString::fromLatin1(AcTemplate::kEachPrefix).trimmed() +
      QStringLiteral("|") + QString::fromLatin1(AcTemplate::kIfPrefix).trimmed() +
      QStringLiteral(")\\}"));
  static const QRegularExpression elseTagRe(QString::fromLatin1(AcTemplate::kElse).chopped(1) +
                                            QStringLiteral("\\b"));

  for (int i = 0; i < srcLines.size(); ++i) {
    QString raw = srcLines[i];
    QString trimmed = raw.trimmed();

    // 保留空行
    if (trimmed.isEmpty()) {
      out.append(QString());
      continue;
    }

    // 检查是否包含闭标签（${/each} 或 ${/if}）
    // 如果行以闭标签开头，先减少缩进
    if (closeTagRe.match(trimmed).hasMatch()) {
      // 检查是否以闭标签开头
      int firstClose =
          trimmed.indexOf(QString::fromLatin1(AcTemplate::kExprOpen) + QLatin1Char('/'));
      if (firstClose == 0) {
        indent = qMax(0, indent - 1);
      }
    }

    // 构建缩进前缀
    QString prefix(indent * kIndentSize, QLatin1Char(' '));
    out.append(prefix + trimmed);

    // 更新缩进：检查开标签
    auto openMatch = openTagRe.match(trimmed);
    if (openMatch.hasMatch()) {
      // 如果行以开标签开头，增加缩进
      int firstOpen = trimmed.indexOf(QString::fromLatin1(AcTemplate::kExprOpen));
      // 只处理行首或紧跟在闭标签前的开标签
      if (firstOpen >= 0) {
        ++indent;
      }
    }
  }

  return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}