/**
 * @file util_json.cpp
 * @brief JSON 解析工具类实现
 */

#include "util_json.h"

#include <QFile>

// stripComments — 剥离 JSON 文本中的注释
//
// 使用状态机遍历字符，正确区分普通文本、字符串、行注释、块注释四种状态：
//   - kNormal:      普通文本，检测 "//" "/*" 和字符串起始 '"'
//   - kString:      字符串内部，忽略注释符号，处理转义字符 '\'
//   - kLineComment: 行注释，直到换行符结束
//   - kBlockComment:块注释，直到 "*/" 结束
//
// 保留换行符以维持行号一致，便于解析错误定位。
QString UtilJson::stripComments(const QString &text) {
  QVector<int> dummy;
  return stripCommentsWithMap(text, dummy);
}

// stripCommentsWithMap — 剥离 JSON 文本中的注释并记录位置映射
//
// offsetMap[strippedIndex] = originalIndex，映射剥离后文本中每个字符对应原始文本的位置。
// 解析后获得的 offset 可直接通过 offsetMap[offset] 转换回原始文本中的位置。
QString UtilJson::stripCommentsWithMap(const QString &text, QVector<int> &offsetMap) {
  QString result;
  result.reserve(text.size());
  offsetMap.clear();
  offsetMap.reserve(text.size());

  enum State { kNormal, kString, kLineComment, kBlockComment };
  State state = kNormal;

  for (int i = 0; i < text.size(); ++i) {
    const QChar ch = text[i];

    switch (state) {
      case kNormal:
        if (ch == QLatin1Char('"')) {
          result += ch;
          offsetMap.append(i);
          state = kString;
        } else if (ch == QLatin1Char('/') && i + 1 < text.size()) {
          const QChar next = text[i + 1];
          if (next == QLatin1Char('/')) {
            state = kLineComment;
            ++i;
          } else if (next == QLatin1Char('*')) {
            state = kBlockComment;
            ++i;
          } else {
            result += ch;
            offsetMap.append(i);
          }
        } else {
          result += ch;
          offsetMap.append(i);
        }
        break;

      case kString:
        result += ch;
        offsetMap.append(i);
        if (ch == QLatin1Char('\\') && i + 1 < text.size()) {
          // 转义字符，原样保留下一个字符
          result += text[i + 1];
          offsetMap.append(i + 1);
          ++i;
        } else if (ch == QLatin1Char('"')) {
          state = kNormal;
        }
        break;

      case kLineComment:
        if (ch == QLatin1Char('\n')) {
          result += ch;  // 保留换行符，维持行号
          offsetMap.append(i);
          state = kNormal;
        }
        break;

      case kBlockComment:
        if (ch == QLatin1Char('*') && i + 1 < text.size() && text[i + 1] == QLatin1Char('/')) {
          state = kNormal;
          ++i;
        } else if (ch == QLatin1Char('\n')) {
          result += ch;  // 保留换行符，维持行号
          offsetMap.append(i);
        }
        break;
    }
  }

  return result;
}

// fromJson — 解析 JSON 字符串（自动剥离注释），错误位置已映射回原始文本
//
// 位置修正流程：
//   1. 剥离注释得到 stripped 文本，并记录每个字符到原始文本的位置映射
//   2. 调用 Qt 官方解析器，得到的 offset 是 stripped 文本的 UTF-8 字节偏移量
//   3. 在 stripped 文本上做尾随逗号修正（从错误位置向前搜索逗号）
//   4. 通过 offsetMap 将 stripped 字符索引映射回原始文本字符索引
//   5. 将原始文本字符索引转为 UTF-8 字节偏移量，写回 error->offset
QJsonDocument UtilJson::fromJson(const QString &text, QJsonParseError *error) {
  QVector<int> offsetMap;
  QString stripped = stripCommentsWithMap(text, offsetMap);
  QJsonDocument doc = QJsonDocument::fromJson(stripped.toUtf8(), error);

  if (!error || error->error == QJsonParseError::NoError) return doc;

  // ── Step 1: 将 Qt 返回的字节偏移量（stripped 文本中）转为字符索引
  int strippedByteOffset = static_cast<int>(error->offset);
  int strippedCharIdx = 0;
  int byteCount = 0;

  // 特殊处理：如果偏移量指向文本末尾，直接设置为最后一个字符位置
  if (strippedByteOffset <= 0) {
    strippedCharIdx = 0;
  } else if (strippedByteOffset >= stripped.toUtf8().size()) {
    strippedCharIdx = stripped.size() - 1;
    if (strippedCharIdx < 0) strippedCharIdx = 0;
  } else {
    while (strippedCharIdx < stripped.size()) {
      QChar c = stripped[strippedCharIdx];
      int bytes = (c.unicode() < 0x80) ? 1 : (c.unicode() < 0x800 ? 2 : 3);
      if (byteCount + bytes > strippedByteOffset) break;
      byteCount += bytes;
      ++strippedCharIdx;
    }
  }

  // ── Step 2: 在 stripped 文本上做尾随逗号修正
  if (error->error == QJsonParseError::MissingObject ||
      error->error == QJsonParseError::UnterminatedArray ||
      error->error == QJsonParseError::MissingValueSeparator) {
    for (int i = strippedCharIdx - 1; i >= 0; --i) {
      QChar ch = stripped[i];
      if (ch == QLatin1Char(',')) {
        strippedCharIdx = i;
        break;
      }
      if (ch != QLatin1Char(' ') && ch != QLatin1Char('\t') && ch != QLatin1Char('\n') &&
          ch != QLatin1Char('\r')) {
        break;  // 遇到非空白字符，不是尾随逗号
      }
    }
  }

  // ── Step 3: 通过 offsetMap 映射回原始文本字符索引
  int origCharIdx = 0;
  if (!offsetMap.isEmpty()) {
    // 如果搜索到了逗号（strippedCharIdx 被修正过）
    // 直接使用 offsetMap 映射
    if (strippedCharIdx >= 0 && strippedCharIdx < offsetMap.size()) {
      origCharIdx = offsetMap[strippedCharIdx];
    } else if (!stripped.isEmpty()) {
      // 如果索引越界，使用最后一个有效映射
      // 但如果是尾随逗号场景，应该已经找到逗号了
      origCharIdx = offsetMap.last();
    }
  }

  // 如果经过尾随逗号修正后仍未找到有效位置，使用文本末尾位置
  if (origCharIdx <= 0 || origCharIdx >= text.size()) {
    origCharIdx = text.size() - 1;
    if (origCharIdx < 0) origCharIdx = 0;
  }

  // ── Step 4: 将原始文本字符索引转为 UTF-8 字节偏移量
  int origByteOffset = 0;
  // 使用 <= 确保包含逗号字符本身的字节数，得到逗号之后的字节偏移量
  // 这与 Qt 返回的 error->offset 格式一致（指向错误位置之后）
  for (int i = 0; i <= origCharIdx && i < text.size(); ++i) {
    QChar c = text[i];
    origByteOffset += (c.unicode() < 0x80) ? 1 : (c.unicode() < 0x800 ? 2 : 3);
  }

  error->offset = origByteOffset;
  return doc;
}

// fromJson — 解析 JSON 字节数组（自动剥离注释）
QJsonDocument UtilJson::fromJson(const QByteArray &data, QJsonParseError *error) {
  QString text = QString::fromUtf8(data);
  return fromJson(text, error);
}

// loadFile — 从文件加载并解析 JSON（自动剥离注释）
QJsonDocument UtilJson::loadFile(const QString &filePath, QJsonParseError *error) {
  QFile f(filePath);
  if (!f.open(QIODevice::ReadOnly)) {
    if (error) error->error = QJsonParseError::NoError;
    return QJsonDocument();
  }
  QByteArray data = f.readAll();
  f.close();
  return fromJson(data, error);
}