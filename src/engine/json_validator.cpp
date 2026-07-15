/**
 * @file json_validator.cpp
 * @brief JSON 语法验证器实现
 */

#include "json_validator.h"

#include <QJsonParseError>
#include <QTextBlock>

#include "src/util/common/util_json.h"

// 将 UTF-8 字节偏移量转换为 QString 字符索引
//
// Qt 的 error->offset 指向错误位置之后的字节，我们需要转换为指向错误字符本身
// 例：text = "a你b"（UTF-8 字节序列: 0x61, 0xE4, 0xBD, 0xA0, 0x62）
//   byteOffset=0 → charIdx=0 ('a')
//   byteOffset=1 → charIdx=1 ('你')
//   byteOffset=4 → charIdx=2 ('b')
static int byteOffsetToCharIndex(const QString &text, int byteOffset) {
  if (byteOffset <= 0) return 0;

  // Qt 的 offset 指向错误位置之后，减 1 指向错误字符本身
  int adjustedOffset = byteOffset - 1;
  if (adjustedOffset < 0) adjustedOffset = 0;

  int charIdx = 0;
  int bytePos = 0;
  while (charIdx < text.size()) {
    QChar c = text[charIdx];
    int bytes = (c.unicode() < 0x80) ? 1 : (c.unicode() < 0x800 ? 2 : 3);
    if (bytePos >= adjustedOffset) break;         // 已到达目标偏移
    if (bytePos + bytes > adjustedOffset) break;  // 目标偏移落在当前字符内
    bytePos += bytes;
    ++charIdx;
  }
  return charIdx;
}

QVector<ValidationResult> JsonValidator::validate(const QString &source) {
  QVector<ValidationResult> results;

  if (source.trimmed().isEmpty()) return results;

  QJsonParseError parseError;
  UtilJson::fromJson(source, &parseError);

  if (parseError.error == QJsonParseError::NoError) return results;

  // UtilJson::fromJson 已完成：
  //   1. 注释剥离 + 位置映射
  //   2. 尾随逗号修正（在剥离后的文本上搜索，不会被注释干扰）
  //   3. 将最终位置转为原始文本的 UTF-8 字节偏移量
  //
  // 这里只需：字节偏移 → 字符索引 → 行号/列号
  int charIdx = byteOffsetToCharIndex(source, static_cast<int>(parseError.offset));

  int line = 1;
  int col = 1;
  for (int i = 0; i < charIdx && i < source.size(); ++i) {
    if (source[i] == QLatin1Char('\n')) {
      ++line;
      col = 1;
    } else {
      ++col;
    }
  }

  results.append(ValidationResult(
      line, col, 1, QStringLiteral("JSON 解析错误: %1").arg(parseError.errorString())));

  return results;
}