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
  QString result;
  result.reserve(text.size());

  enum State { kNormal, kString, kLineComment, kBlockComment };
  State state = kNormal;

  for (int i = 0; i < text.size(); ++i) {
    const QChar ch = text[i];

    switch (state) {
      case kNormal:
        if (ch == QLatin1Char('"')) {
          result += ch;
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
          }
        } else {
          result += ch;
        }
        break;

      case kString:
        result += ch;
        if (ch == QLatin1Char('\\') && i + 1 < text.size()) {
          // 转义字符，原样保留下一个字符
          result += text[i + 1];
          ++i;
        } else if (ch == QLatin1Char('"')) {
          state = kNormal;
        }
        break;

      case kLineComment:
        if (ch == QLatin1Char('\n')) {
          result += ch;  // 保留换行符，维持行号
          state = kNormal;
        }
        break;

      case kBlockComment:
        if (ch == QLatin1Char('*') && i + 1 < text.size() && text[i + 1] == QLatin1Char('/')) {
          state = kNormal;
          ++i;
        } else if (ch == QLatin1Char('\n')) {
          result += ch;  // 保留换行符，维持行号
        }
        break;
    }
  }

  return result;
}

// fromJson — 解析 JSON 字符串（自动剥离注释）
QJsonDocument UtilJson::fromJson(const QString &text, QJsonParseError *error) {
  QString stripped = stripComments(text);
  return QJsonDocument::fromJson(stripped.toUtf8(), error);
}

// fromJson — 解析 JSON 字节数组（自动剥离注释）
QJsonDocument UtilJson::fromJson(const QByteArray &data, QJsonParseError *error) {
  QString text = QString::fromUtf8(data);
  QString stripped = stripComments(text);
  return QJsonDocument::fromJson(stripped.toUtf8(), error);
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