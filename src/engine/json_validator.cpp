/**
 * @file json_validator.cpp
 * @brief JSON 语法验证器实现
 */

#include "json_validator.h"

#include <QJsonParseError>
#include <QTextBlock>

#include "src/tool/common/tool_json.h"

QVector<ValidationResult> JsonValidator::validate(const QString &source) {
  QVector<ValidationResult> results;

  if (source.trimmed().isEmpty()) return results;

  QJsonParseError parseError;
  ToolJson::fromJson(source, &parseError);

  if (parseError.error != QJsonParseError::NoError) {
    int offset = parseError.offset;

    // 将偏移量转换为行列号
    int line = 1;
    int col = 1;
    for (int i = 0; i < offset && i < source.length(); ++i) {
      if (source[i] == QLatin1Char('\n')) {
        ++line;
        col = 1;
      } else {
        ++col;
      }
    }

    // 标记错误位置（偏移量指向错误的下一位，标记前一个字符）
    int markPos = qMin(offset, source.length()) - 1;
    if (markPos < 0) markPos = 0;

    results.append(ValidationResult(
        line, col, 1, QStringLiteral("JSON 解析错误: %1").arg(parseError.errorString())));
  }

  return results;
}