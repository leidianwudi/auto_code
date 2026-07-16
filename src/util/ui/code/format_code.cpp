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
//  类型声明（供 JSON 格式化内部使用）
// ──────────────────────────────────────────────────────────────

// OrderedKeyMap — 记录每个对象的 key 顺序
// key: 对象路径（如 "0" 表示顶层对象，"0.2" 表示第3个key的子对象）
// value: 该对象的 key 按原始出现顺序排列的列表
using OrderedKeyMap = QMap<QString, QStringList>;

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
      return formatJson(input, false);
    case FormatJson5:
      return formatJson(input, true);
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
 * @brief 递归序列化 QJsonValue 为格式化字符串（支持 key 顺序）
 * @param orderedKeys 每个对象的 key 顺序（路径 -> key 列表）
 * @param path 当前对象在 JSON 树中的路径（如 "0" 表示顶层，"0.2.1" 表示更深层）
 * @param json5Style 是否使用 JSON5 风格（无引号 key、尾随逗号）
 */
static void serializeJsonValue(const QJsonValue &v, QStringList &lines, int indent, bool isLast,
                               const OrderedKeyMap &orderedKeys, const QString &path,
                               bool json5Style = false) {
  const QString prefix(indent * FormatCode::kIndentSize, QLatin1Char(' '));

  switch (v.type()) {
    case QJsonValue::Object: {
      QJsonObject obj = v.toObject();
      if (obj.isEmpty()) {
        lines.last() += QStringLiteral("{}");
        return;
      }
      lines.last() += QLatin1Char('{');

      // 优先使用 orderedKeys 中记录的原始顺序；如无记录则退化为字母顺序
      QStringList keys;
      if (orderedKeys.contains(path)) {
        const QStringList &ordered = orderedKeys[path];
        // 使用原始顺序，并补充可能遗漏的 key
        for (const QString &k : ordered) {
          if (obj.contains(k)) keys.append(k);
        }
        // 添加未在原始顺序中记录的 key（理论上不应发生）
        for (const QString &k : obj.keys()) {
          if (!keys.contains(k)) keys.append(k);
        }
      } else {
        keys = obj.keys();
      }

      for (int i = 0; i < keys.size(); ++i) {
        const QString &key = keys[i];
        if (json5Style) {
          // JSON5 风格：key 不加引号
          lines.append(prefix + QStringLiteral("  ") + key + QStringLiteral(": "));
        } else {
          lines.append(prefix + QStringLiteral("  \"") + key + QStringLiteral("\": "));
        }
        // 子路径：path + "." + index（例如 "0" -> "0.0" 表示第0个key的值）
        QString childPath = path + QStringLiteral(".") + QString::number(i);
        serializeJsonValue(obj.value(key), lines, indent + 1, i == keys.size() - 1, orderedKeys,
                           childPath, json5Style);
      }
      // JSON5 风格：对象结束行也添加尾随逗号
      lines.append(prefix + QLatin1Char('}'));
      if (json5Style || !isLast) lines.last() += QLatin1Char(',');
      break;
    }
    case QJsonValue::Array: {
      QJsonArray arr = v.toArray();
      if (arr.isEmpty()) {
        lines.last() += QStringLiteral("[]");
        if (!isLast) lines.last() += QLatin1Char(',');
        return;
      }
      lines.last() += QLatin1Char('[');
      for (int i = 0; i < arr.size(); ++i) {
        lines.append(prefix + QStringLiteral("  "));
        // 子路径：path + ".arr" + index（数组元素的路径独立编码）
        QString childPath = path + QStringLiteral(".arr") + QString::number(i);
        serializeJsonValue(arr[i], lines, indent + 1, i == arr.size() - 1, orderedKeys, childPath,
                           json5Style);
      }
      // JSON5 风格：数组结束行也添加尾随逗号
      lines.append(prefix + QLatin1Char(']'));
      if (json5Style || !isLast) lines.last() += QLatin1Char(',');
      break;
    }
    case QJsonValue::String: {
      // 按 JSON 规范转义字符串中的特殊字符
      QString escaped = v.toString();
      escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));  // \ -> \\
      escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));    // " -> \"
      escaped.replace(QLatin1Char('\n'), QStringLiteral("\\n"));   // 换行 -> \n
      escaped.replace(QLatin1Char('\r'), QStringLiteral("\\r"));   // 回车 -> \r
      escaped.replace(QLatin1Char('\t'), QStringLiteral("\\t"));   // 制表符 -> \t
      escaped.replace(QLatin1Char('\b'), QStringLiteral("\\b"));   // 退格 -> \b
      escaped.replace(QLatin1Char('\f'), QStringLiteral("\\f"));   // 换页 -> \f
      if (json5Style) {
        // JSON5 风格：使用单引号
        lines.last() += QLatin1Char('\'') + escaped + QLatin1Char('\'');
      } else {
        lines.last() += QLatin1Char('"') + escaped + QLatin1Char('"');
      }
      // JSON5 风格：所有值后都加尾随逗号
      if (json5Style || !isLast) lines.last() += QLatin1Char(',');
      break;
    }
    case QJsonValue::Double: {
      // 避免输出科学计数法
      lines.last() += QString::number(v.toDouble(), 'f', 6);
      // 去掉末尾多余的 0
      if (lines.last().contains(QLatin1Char('.'))) {
        while (lines.last().endsWith(QLatin1Char('0'))) lines.last().chop(1);
        if (lines.last().endsWith(QLatin1Char('.'))) lines.last().chop(1);
      }
      // JSON5 风格：所有值后都加尾随逗号
      if (json5Style || !isLast) lines.last() += QLatin1Char(',');
      break;
    }
    case QJsonValue::Bool: {
      lines.last() += v.toBool() ? QString::fromLatin1(AcKeyword::kTrue)
                                 : QString::fromLatin1(AcKeyword::kFalse);
      // JSON5 风格：所有值后都加尾随逗号
      if (json5Style || !isLast) lines.last() += QLatin1Char(',');
      break;
    }
    case QJsonValue::Null: {
      lines.last() += QStringLiteral("null");
      // JSON5 风格：所有值后都加尾随逗号
      if (json5Style || !isLast) lines.last() += QLatin1Char(',');
      break;
    }
    case QJsonValue::Undefined:
      break;
  }
}

/**
 * @brief 从原始 JSON 文本中提取所有对象的 key 顺序
 *
 * 采用字符级状态机扫描输入文本，遇到 `{` 进入对象，扫描其中的 "key": 模式
 * 按出现顺序记录 keys。用路径编码区分不同层级的对象：
 *   - "0" 表示顶层对象
 *   - "0.2" 表示顶层对象第 3 个 key 的值是对象
 *   - "0.2.arr1.0" 表示数组中的对象
 *
 * 同时跳过注释、字符串内容，保证解析准确。
 */
static OrderedKeyMap extractOrderedKeys(const QString &input) {
  OrderedKeyMap result;

  // 状态
  enum State { kNormal, kString, kLineComment, kBlockComment };
  State state = kNormal;

  // 栈：记录正在扫描的容器信息
  struct StackFrame {
    QChar type;    // 'O' 或 'A'
    QString path;  // 容器路径编码
    int childIdx;  // 下一个子元素的索引
  };
  QVector<StackFrame> stack;

  int topLevelIdx = 0;

  for (int i = 0; i < input.size(); ++i) {
    QChar ch = input[i];

    // ── 字符串状态处理 ──
    if (state == kString) {
      if (ch == QLatin1Char('\\') && i + 1 < input.size()) {
        ++i;
        continue;
      }
      if (ch == QLatin1Char('"')) state = kNormal;
      continue;
    }

    // ── 行注释处理 ──
    if (state == kLineComment) {
      if (ch == QLatin1Char('\n')) state = kNormal;
      continue;
    }

    // ── 块注释处理 ──
    if (state == kBlockComment) {
      if (ch == QLatin1Char('*') && i + 1 < input.size() && input[i + 1] == QLatin1Char('/')) {
        state = kNormal;
        ++i;
      }
      continue;
    }

    // ── 正常状态处理 ──
    // key 识别：三种形式
    //   1. 双引号 key: "key": value
    //   2. 单引号 key: 'key': value   (JSON5)
    //   3. 无引号 key: key: value      (JSON5)
    //
    // 统一流程：先检测 key 起始，收集 key 内容，检测后面的冒号

    // 辅助：检测 i 位置是否是对象的 key（即前面有效字符是 { 或 ,）
    auto isObjectKeyContext = [&]() {
      if (stack.isEmpty() || stack.last().type != QLatin1Char('O')) return false;
      // 向前扫描，跳过所有非结构字符（空白、注释内容等），只寻找 { 或 ,
      int m = i - 1;
      while (m >= 0) {
        QChar mc = input[m];
        if (mc.isSpace()) {
          --m;
          continue;
        }
        // 只在遇到对象起始或属性分隔符时停止
        if (mc == QLatin1Char('{') || mc == QLatin1Char(',')) return true;
        // 其他所有内容（注释、字符串、数字、关键字等）全部跳过
        --m;
      }
      return false;
    };

    // 双引号 key
    if (ch == QLatin1Char('"') && isObjectKeyContext()) {
      int j = i + 1;
      QString keyStr;
      while (j < input.size()) {
        QChar kc = input[j];
        if (kc == QLatin1Char('\\') && j + 1 < input.size()) {
          keyStr += kc;
          keyStr += input[j + 1];
          j += 2;
          continue;
        }
        if (kc == QLatin1Char('"')) {
          int check = j + 1;
          while (check < input.size() && input[check].isSpace()) ++check;
          if (check < input.size() && input[check] == QLatin1Char(':')) {
            QString curPath = stack.last().path;
            if (!result.contains(curPath)) result[curPath] = QStringList();
            result[curPath].append(keyStr);
            stack.last().childIdx++;
          }
          break;
        }
        keyStr += kc;
        ++j;
      }
      i = j;
      continue;
    }

    // 单引号 key (JSON5)
    if (ch == QLatin1Char('\'') && isObjectKeyContext()) {
      int j = i + 1;
      QString keyStr;
      while (j < input.size()) {
        QChar kc = input[j];
        if (kc == QLatin1Char('\\') && j + 1 < input.size()) {
          keyStr += kc;
          keyStr += input[j + 1];
          j += 2;
          continue;
        }
        if (kc == QLatin1Char('\'')) {
          int check = j + 1;
          while (check < input.size() && input[check].isSpace()) ++check;
          if (check < input.size() && input[check] == QLatin1Char(':')) {
            QString curPath = stack.last().path;
            if (!result.contains(curPath)) result[curPath] = QStringList();
            result[curPath].append(keyStr);
            stack.last().childIdx++;
          }
          break;
        }
        keyStr += kc;
        ++j;
      }
      i = j;
      continue;
    }

    // 无引号 key (JSON5): 字母/$/_ 开头的标识符
    if ((ch.isLetter() || ch == QLatin1Char('$') || ch == QLatin1Char('_')) &&
        isObjectKeyContext()) {
      int j = i;
      QString keyStr;
      while (j < input.size()) {
        QChar kc = input[j];
        if (kc.isLetterOrNumber() || kc == QLatin1Char('$') || kc == QLatin1Char('_')) {
          keyStr += kc;
          ++j;
          continue;
        }
        break;
      }
      int check = j;
      while (check < input.size() && input[check].isSpace()) ++check;
      if (check < input.size() && input[check] == QLatin1Char(':') && !keyStr.isEmpty()) {
        QString curPath = stack.last().path;
        if (!result.contains(curPath)) result[curPath] = QStringList();
        result[curPath].append(keyStr);
        stack.last().childIdx++;
      }
      i = j - 1;
      continue;
    }

    // 保留原来的双引号字符串检测（值的双引号字符串不需要处理 key，跳过）
    if (ch == QLatin1Char('"')) {
      // 不是 key 上下文的双引号（值的字符串）
      int j = i + 1;
      while (j < input.size()) {
        if (input[j] == QLatin1Char('\\') && j + 1 < input.size()) {
          j += 2;
          continue;
        }
        if (input[j] == QLatin1Char('"')) break;
        ++j;
      }
      i = j;
      continue;
    }

    if (ch == QLatin1Char('/') && i + 1 < input.size() && input[i + 1] == QLatin1Char('/')) {
      state = kLineComment;
      ++i;
      continue;
    }
    if (ch == QLatin1Char('/') && i + 1 < input.size() && input[i + 1] == QLatin1Char('*')) {
      state = kBlockComment;
      i += 2;
      continue;
    }

    // ★ 对象开始
    // 父类型是对象 → 用 childIdx - 1（key 索引）→ parent.path + "." + idx
    // 父类型是数组 → 用 childIdx（数组元素索引）→ parent.path + ".arr" + idx
    if (ch == QLatin1Char('{')) {
      QString path;
      if (stack.isEmpty()) {
        path = QString::number(topLevelIdx++);
      } else if (stack.last().type == QLatin1Char('O')) {
        int idx =
            stack.last().childIdx - 1;  // 记录 key 后 childIdx 已递增，减 1 才是 key 的原始索引
        if (idx < 0) idx = 0;
        path = stack.last().path + QStringLiteral(".") + QString::number(idx);
      } else {
        path = stack.last().path + QStringLiteral(".arr") + QString::number(stack.last().childIdx);
      }
      stack.push_back({QLatin1Char('O'), path, 0});
      continue;
    }

    if (ch == QLatin1Char('}')) {
      if (!stack.isEmpty() && stack.last().type == QLatin1Char('O')) stack.pop_back();
      continue;
    }

    // ★ 数组开始
    // 父类型是对象 → 用 childIdx - 1（key 索引）→ parent.path + ".arr" + idx
    // 父类型是数组 → 用 childIdx（数组元素索引）→ parent.path + ".arr" + idx
    if (ch == QLatin1Char('[')) {
      QString path;
      if (stack.isEmpty()) {
        path = QString::number(topLevelIdx++);
      } else if (stack.last().type == QLatin1Char('O')) {
        int idx = stack.last().childIdx - 1;
        if (idx < 0) idx = 0;
        path = stack.last().path + QStringLiteral(".arr") + QString::number(idx);
      } else {
        path = stack.last().path + QStringLiteral(".arr") + QString::number(stack.last().childIdx);
      }
      stack.push_back({QLatin1Char('A'), path, 0});
      continue;
    }

    if (ch == QLatin1Char(']')) {
      if (!stack.isEmpty() && stack.last().type == QLatin1Char('A')) stack.pop_back();
      continue;
    }

    // ★ 数组元素分隔符 → 在数组中遇到逗号时递增 childIdx
    if (ch == QLatin1Char(',')) {
      if (!stack.isEmpty() && stack.last().type == QLatin1Char('A')) stack.last().childIdx++;
      continue;
    }
  }

  return result;
}

// JsonCommentInfo — 记录一条注释信息
struct JsonCommentInfo {
  int line;           // 注释所在行号（0-based）
  int col;            // 注释起始列号（该行内的字符位置）
  QString text;       // 注释完整内容（包含 // 或 /* */）
  bool isInline;      // 是否为行内注释（同一行有代码内容）
  QString anchorKey;  // 锚点 key：注释后面紧跟的 JSON key（用于位置匹配）
  bool isTopLevel;    // 是否位于顶层结构（{ 之后的第一个注释）
};

// 从输入文本中，从指定行开始向后查找第一个 JSON key 名称
// 返回找到的 key 名称（不含引号），如果找不到返回空字符串
static QString findNextKeyAfter(const QStringList &srcLines, int startLine) {
  enum State { kNormal, kString, kLineComment, kBlockComment };
  State state = kNormal;

  for (int lineIdx = startLine; lineIdx < srcLines.size(); ++lineIdx) {
    const QString &line = srcLines[lineIdx];

    for (int i = 0; i < line.size(); ++i) {
      QChar ch = line[i];

      // 处理字符串状态
      if (state == kString) {
        if (ch == QLatin1Char('\\') && i + 1 < line.size()) {
          ++i;
          continue;
        }
        if (ch == QLatin1Char('"')) {
          state = kNormal;
        }
        continue;
      }

      // 处理行注释
      if (state == kLineComment) {
        continue;
      }

      // 处理块注释
      if (state == kBlockComment) {
        if (ch == QLatin1Char('*') && i + 1 < line.size() && line[i + 1] == QLatin1Char('/')) {
          state = kNormal;
          ++i;
        }
        continue;
      }

      // kNormal 状态
      if (ch == QLatin1Char('"')) {
        // 双引号 key: "key":
        int j = i + 1;
        QString key;
        while (j < line.size()) {
          QChar kc = line[j];
          if (kc == QLatin1Char('\\') && j + 1 < line.size()) {
            key += kc;
            key += line[j + 1];
            j += 2;
            continue;
          }
          if (kc == QLatin1Char('"')) {
            int checkPos = j + 1;
            while (checkPos < line.size() && line[checkPos].isSpace()) ++checkPos;
            if (checkPos < line.size() && line[checkPos] == QLatin1Char(':')) {
              return key;
            }
            break;
          }
          key += kc;
          ++j;
        }
        i = j;
      } else if (ch == QLatin1Char('\'')) {
        // 单引号 key (JSON5): 'key':
        int j = i + 1;
        QString key;
        while (j < line.size()) {
          QChar kc = line[j];
          if (kc == QLatin1Char('\\') && j + 1 < line.size()) {
            key += kc;
            key += line[j + 1];
            j += 2;
            continue;
          }
          if (kc == QLatin1Char('\'')) {
            int checkPos = j + 1;
            while (checkPos < line.size() && line[checkPos].isSpace()) ++checkPos;
            if (checkPos < line.size() && line[checkPos] == QLatin1Char(':')) {
              return key;
            }
            break;
          }
          key += kc;
          ++j;
        }
        i = j;
      } else if (ch.isLetter() || ch == QLatin1Char('$') || ch == QLatin1Char('_')) {
        // 无引号 key (JSON5): key:
        int j = i;
        QString key;
        while (j < line.size()) {
          QChar kc = line[j];
          if (kc.isLetterOrNumber() || kc == QLatin1Char('$') || kc == QLatin1Char('_')) {
            key += kc;
            ++j;
          } else {
            break;
          }
        }
        int checkPos = j;
        while (checkPos < line.size() && line[checkPos].isSpace()) ++checkPos;
        if (checkPos < line.size() && line[checkPos] == QLatin1Char(':') && !key.isEmpty()) {
          return key;
        }
        i = j - 1;
      } else if (ch == QLatin1Char('/') && i + 1 < line.size()) {
        if (line[i + 1] == QLatin1Char('/')) {
          state = kLineComment;
          break;
        } else if (line[i + 1] == QLatin1Char('*')) {
          state = kBlockComment;
          ++i;
        }
      }
    }

    // 行结束，重置行注释状态
    if (state == kLineComment) state = kNormal;
  }

  return QString();
}

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
            info.line = j;
            break;
          } else {
            info.text += blockLine + QLatin1Char('\n');
          }
        }
      } else {
        info.text = commentText;
      }

      // 查找注释后面的 JSON key（用于锚点匹配）
      // 对于独立注释行：查找下一行之后的第一个 key
      // 对于行内注释：查找同一行之后或下一行的第一个 key
      int searchStartLine = lineIdx;
      if (!info.isInline && info.text.startsWith("//")) {
        searchStartLine = lineIdx + 1;
      }
      info.anchorKey = findNextKeyAfter(srcLines, searchStartLine);

      // 检查是否为顶层注释（紧跟在 { 或 [ 之后）
      info.isTopLevel = false;
      for (int j = lineIdx - 1; j >= 0; --j) {
        QString trimmed = srcLines[j].trimmed();
        if (!trimmed.isEmpty()) {
          if (trimmed == "{" || trimmed == "[" || trimmed.endsWith("{") || trimmed.endsWith("[")) {
            info.isTopLevel = true;
          }
          break;
        }
      }

      comments.append(info);
    }
  }

  return comments;
}

// 从序列化行中提取 key 名称（如 "modelName": "xxx" -> "modelName"）
// 如果该行不是 key 行，返回空字符串
static QString extractKeyFromLine(const QString &line) {
  QString trimmed = line.trimmed();
  if (trimmed.isEmpty()) return QString();

  // 双引号 key: "key":
  if (trimmed.startsWith(QLatin1Char('"'))) {
    int i = 1;
    QString key;
    while (i < trimmed.size()) {
      QChar ch = trimmed[i];
      if (ch == QLatin1Char('\\') && i + 1 < trimmed.size()) {
        key += ch;
        key += trimmed[i + 1];
        i += 2;
        continue;
      }
      if (ch == QLatin1Char('"')) {
        int checkPos = i + 1;
        while (checkPos < trimmed.size() && trimmed[checkPos].isSpace()) ++checkPos;
        if (checkPos < trimmed.size() && trimmed[checkPos] == QLatin1Char(':')) {
          return key;
        }
        return QString();
      }
      key += ch;
      ++i;
    }
  }

  // JSON5 无引号 key: key:
  QChar first = trimmed[0];
  if (first.isLetter() || first == QLatin1Char('$') || first == QLatin1Char('_')) {
    int i = 0;
    QString key;
    while (i < trimmed.size()) {
      QChar ch = trimmed[i];
      if (ch.isLetterOrNumber() || ch == QLatin1Char('$') || ch == QLatin1Char('_')) {
        key += ch;
        ++i;
      } else {
        break;
      }
    }
    int checkPos = i;
    while (checkPos < trimmed.size() && trimmed[checkPos].isSpace()) ++checkPos;
    if (checkPos < trimmed.size() && trimmed[checkPos] == QLatin1Char(':') && !key.isEmpty()) {
      return key;
    }
  }

  return QString();
}

// 从序列化行中提取当前缩进（前导空格数）
static int extractIndent(const QString &line) {
  int indent = 0;
  while (indent < line.size() && line[indent] == QLatin1Char(' ')) ++indent;
  return indent;
}

QString FormatCode::formatJson(const QString &input, bool json5Style) {
  QJsonParseError err;
  QJsonDocument doc = UtilJson::fromJson(input, &err);
  if (err.error != QJsonParseError::NoError) return input;

  // 1. 从原始输入提取每个对象的 key 原始顺序
  OrderedKeyMap orderedKeys = extractOrderedKeys(input);

  QVector<JsonCommentInfo> allComments = extractJsonComments(input);

  QStringList lines;
  lines.append(QString());
  // 2. 使用原始 key 顺序序列化 JSON；顶层对象路径为 "0"
  serializeJsonValue(doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array()), lines, 0,
                     true, orderedKeys, QStringLiteral("0"), json5Style);

  while (!lines.isEmpty() && lines.first().trimmed().isEmpty()) lines.removeFirst();
  while (!lines.isEmpty() && lines.last().trimmed().isEmpty()) lines.removeLast();

  // 标记哪些注释已经被使用
  QVector<bool> usedComments(allComments.size(), false);

  QStringList resultLines;

  for (int i = 0; i < lines.size(); ++i) {
    QString line = lines[i];
    int lineIndent = extractIndent(line);

    // 步骤 1：在当前行之前插入匹配的独立注释
    // 提取当前行的 key
    QString currentKey = extractKeyFromLine(line);

    if (!currentKey.isEmpty()) {
      // 查找所有 anchorKey == currentKey 且未使用的独立注释
      for (int c = 0; c < allComments.size(); ++c) {
        if (usedComments[c]) continue;
        const JsonCommentInfo &cmt = allComments[c];
        if (!cmt.isInline && !cmt.anchorKey.isEmpty() && cmt.anchorKey == currentKey) {
          // 插入注释行，使用当前行的缩进
          QString prefix(lineIndent, QLatin1Char(' '));
          QString commentBody = cmt.text.trimmed();
          // 支持多行块注释
          QStringList commentLines = commentBody.split(QLatin1Char('\n'));
          for (const QString &cl : commentLines) {
            if (cl.trimmed().isEmpty()) {
              resultLines.append(QString());
            } else {
              resultLines.append(prefix + cl.trimmed());
            }
          }
          usedComments[c] = true;
        }
      }
    }

    // 步骤 2：检查当前行是否有行内注释
    // 策略：查找 isInline 注释，其 anchorKey 与当前行 key 匹配（如果有）
    // 或者：如果当前行是 key 行，按顺序匹配未使用的行内注释
    if (!currentKey.isEmpty()) {
      for (int c = 0; c < allComments.size(); ++c) {
        if (usedComments[c]) continue;
        const JsonCommentInfo &cmt = allComments[c];
        if (cmt.isInline && cmt.anchorKey == currentKey) {
          line += QLatin1Char(' ') + cmt.text.trimmed();
          usedComments[c] = true;
          break;
        }
      }
    }

    resultLines.append(line);
  }

  // 步骤 3：将剩余未匹配的注释放到文件末尾（保持原始顺序）
  bool hasTrailing = false;
  for (int c = 0; c < allComments.size(); ++c) {
    if (!usedComments[c]) {
      if (!hasTrailing) {
        resultLines.append(QString());  // 添加空行分隔
        hasTrailing = true;
      }
      QString commentBody = allComments[c].text.trimmed();
      QStringList commentLines = commentBody.split(QLatin1Char('\n'));
      for (const QString &cl : commentLines) {
        resultLines.append(cl.trimmed());
      }
    }
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