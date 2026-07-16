/**
 * @file util_json.cpp
 * @brief JSON 解析工具类实现（支持 JSON5 → 标准 JSON 转换）
 */

#include "util_json.h"

#include <QFile>

// ──────────────────────────────────────────────────────────────
//  json5ToJson — JSON5 → 标准 JSON 转换
//
//  状态机识别 8 种状态，遇到 JSON5 扩展语法时在输出中转换为标准 JSON。
//  同时可选记录位置映射（offsetMap[newIdx] = origIdx），用于错误位置回指。
// ──────────────────────────────────────────────────────────────
QString UtilJson::json5ToJson(const QString &text, QVector<int> *offsetMap) {
  QString result;
  result.reserve(text.size() + text.size() / 10);  // 预留 10% 余量
  if (offsetMap) {
    offsetMap->clear();
    offsetMap->reserve(text.size());
  }

  // —— 8 种状态 ——
  enum State {
    kNormal,        // 正常文本（值/key 之间）
    kKey,           // 正在识别无引号 key（收集标识符）
    kStringDq,      // 双引号字符串内部
    kStringSq,      // 单引号字符串内部（JSON5）
    kLineComment,   // 行注释
    kBlockComment,  // 块注释
    kNumber,        // 正在识别数字（可能是十六进制/前导小数点）
    kKeyword        // 正在识别关键字（true/false/null/Infinity/NaN）
  };
  State state = kNormal;

  // 辅助小函数：追加一个字符（同时记录位置映射）
  auto appendChar = [&](QChar ch, int origIdx) {
    result += ch;
    if (offsetMap) offsetMap->append(origIdx);
  };

  // 辅助：追加一个字符串（每个字符记录同一个 origIdx 或不记录，简单起见不精细映射）
  auto appendStr = [&](const QString &s, int origIdx) {
    for (QChar c : s) {
      result += c;
      if (offsetMap) offsetMap->append(origIdx);
    }
  };

  // —— 暂存缓冲区 ——
  QString keyBuf;       // 正在收集的无引号 key 字符
  QString numBuf;       // 正在收集的数字
  QString kwBuf;        // 正在收集的关键字
  int keyStartIdx = 0;  // key 起始位置
  int numStartIdx = 0;  // 数字起始位置
  int kwStartIdx = 0;   // 关键字起始位置

  // —— 尾随逗号检测：记录最近一个逗号在 result 中的位置 ——
  // 思路：遇到逗号不立即写入，而是"暂存"，等下一个有效字符出现时才决定是否写入逗号
  struct PendingComma {
    bool has = false;
    int origIdx = 0;
  };
  PendingComma pendingComma;

  // 辅助：把暂存的逗号真正写入（如果有）
  auto flushComma = [&]() {
    if (pendingComma.has) {
      result += QLatin1Char(',');
      if (offsetMap) offsetMap->append(pendingComma.origIdx);
      pendingComma.has = false;
    }
  };

  for (int i = 0; i < text.size(); ++i) {
    QChar ch = text[i];

    // ============================================================
    //  行注释处理
    // ============================================================
    if (state == kLineComment) {
      if (ch == QLatin1Char('\n')) {
        state = kNormal;  // 保留换行以维持行号
        appendChar(ch, i);
      }
      continue;
    }

    // ============================================================
    //  块注释处理
    // ============================================================
    if (state == kBlockComment) {
      if (ch == QLatin1Char('*') && i + 1 < text.size() && text[i + 1] == QLatin1Char('/')) {
        state = kNormal;
        ++i;
      } else if (ch == QLatin1Char('\n')) {
        appendChar(ch, i);  // 保留换行
      }
      continue;
    }

    // ============================================================
    //  双引号字符串（JSON 原生，JSON5 兼容）
    // ============================================================
    if (state == kStringDq) {
      // JSON5 允许字符串跨行：反斜杠后跟换行 → 跳过两者
      if (ch == QLatin1Char('\\') && i + 1 < text.size()) {
        QChar next = text[i + 1];
        if (next == QLatin1Char('\n') || next == QLatin1Char('\r')) {
          // 跳过 \ 和换行符（JSON5 字符串续行）
          if (next == QLatin1Char('\r') && i + 2 < text.size() &&
              text[i + 2] == QLatin1Char('\n')) {
            i += 2;  // \r\n
          } else {
            i += 1;  // \n 或 \r
          }
          continue;
        }
        // 普通转义，原样输出
        appendChar(ch, i);
        appendChar(next, i + 1);
        ++i;
        continue;
      }
      if (ch == QLatin1Char('"')) {
        state = kNormal;
        appendChar(ch, i);
        continue;
      }
      appendChar(ch, i);
      continue;
    }

    // ============================================================
    //  单引号字符串（JSON5 扩展 → 需要转成双引号）
    // ============================================================
    if (state == kStringSq) {
      if (ch == QLatin1Char('\\') && i + 1 < text.size()) {
        QChar next = text[i + 1];
        // 字符串续行
        if (next == QLatin1Char('\n') || next == QLatin1Char('\r')) {
          if (next == QLatin1Char('\r') && i + 2 < text.size() &&
              text[i + 2] == QLatin1Char('\n')) {
            i += 2;
          } else {
            i += 1;
          }
          continue;
        }
        // 特殊：如果单引号字符串里有双引号，转义它
        if (next == QLatin1Char('"')) {
          appendChar(QLatin1Char('\\'), i);
          appendChar(next, i + 1);
          ++i;
          continue;
        }
        // 特殊：JSON5 允许 \0 转义，但标准 JSON 不支持 → 转为 \u0000
        if (next == QLatin1Char('0') && (i + 2 >= text.size() || !text[i + 2].isDigit())) {
          appendStr(QStringLiteral("\\u0000"), i);
          ++i;
          continue;
        }
        // 普通转义：原样保留（\n、\t、\' 等通用转义都是合法的）
        appendChar(QLatin1Char('\\'), i);
        appendChar(next, i + 1);
        ++i;
        continue;
      }
      // 字符串结束
      if (ch == QLatin1Char('\'')) {
        state = kNormal;
        appendChar(QLatin1Char('"'), i);  // 以双引号结尾
        continue;
      }
      // 内容里的双引号 → 转义
      if (ch == QLatin1Char('"')) {
        appendChar(QLatin1Char('\\'), i);
        appendChar(ch, i);
        continue;
      }
      appendChar(ch, i);
      continue;
    }

    // ============================================================
    //  正在收集"无引号 key"
    // ============================================================
    if (state == kKey) {
      // JSON5 标识符 key：首字符为字母/$/_，后续可为字母/数字/$/_
      if (ch.isLetterOrNumber() || ch == QLatin1Char('$') || ch == QLatin1Char('_')) {
        keyBuf += ch;
        continue;
      }
      // 结束收集：先写入引号包裹的 key
      flushComma();
      appendChar(QLatin1Char('"'), keyStartIdx);
      for (int k = 0; k < keyBuf.size(); ++k) {
        appendChar(keyBuf[k], keyStartIdx + k);
      }
      appendChar(QLatin1Char('"'), keyStartIdx + keyBuf.size() - 1);
      keyBuf.clear();
      state = kNormal;
      // 回到当前字符的正常处理（不 continue，直接 fall-through 到 kNormal）
    }

    // ============================================================
    //  正在识别数字
    // ============================================================
    if (state == kNumber) {
      bool isHex = numBuf.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive) ||
                   numBuf.startsWith(QStringLiteral("-0x"), Qt::CaseInsensitive);

      // 继续收集合法数字字符
      bool keepCollecting = false;
      if (isHex) {
        // 十六进制：只接受 0-9 a-f A-F
        if (ch.isDigit() || (ch.unicode() >= 'a' && ch.unicode() <= 'f') ||
            (ch.unicode() >= 'A' && ch.unicode() <= 'F')) {
          numBuf += ch;
          keepCollecting = true;
        }
      } else {
        // 十进制/小数：数字、小数点、e/E、正负号
        if (ch.isDigit() || ch == QLatin1Char('.') || ch == QLatin1Char('e') ||
            ch == QLatin1Char('E') || ch == QLatin1Char('+') || ch == QLatin1Char('-')) {
          numBuf += ch;
          keepCollecting = true;
        }
      }

      if (keepCollecting) continue;

      // —— 数字结束 ——
      flushComma();
      QString finalNum;
      if (isHex) {
        // 十六进制 → 十进制
        bool ok = false;
        QString hexPart = numBuf;
        bool neg = false;
        if (hexPart.startsWith(QLatin1Char('-'))) {
          neg = true;
          hexPart = hexPart.mid(1);
        }
        // hexPart 形如 "0xFF"
        QString hexDigits = hexPart.mid(2);
        qlonglong val = hexDigits.toLongLong(&ok, 16);
        if (ok) {
          if (neg) val = -val;
          finalNum = QString::number(val);
        } else {
          finalNum = "0";  // 兜底
        }
      } else {
        // 十进制：可能是 "5."、".5"、正常数字
        // ".5" → "0.5"
        if (numBuf.startsWith(QLatin1Char('.'))) {
          finalNum = QStringLiteral("0") + numBuf;
        }
        // "5." → "5"
        else if (numBuf.endsWith(QLatin1Char('.'))) {
          finalNum = numBuf.left(numBuf.size() - 1);
        }
        // 正常数字：检查并输出
        else {
          bool ok = false;
          double v = numBuf.toDouble(&ok);
          if (ok) {
            finalNum = QString::number(v, 'f', 20);
            // 去掉尾随 0
            if (finalNum.contains(QLatin1Char('.'))) {
              while (finalNum.endsWith(QLatin1Char('0'))) finalNum.chop(1);
              if (finalNum.endsWith(QLatin1Char('.'))) finalNum.chop(1);
            }
          } else {
            finalNum = numBuf;
          }
        }
      }
      appendStr(finalNum, numStartIdx);
      numBuf.clear();
      state = kNormal;
      // 当前字符交给正常状态处理
    }

    // ============================================================
    //  正在识别关键字（true/false/null/Infinity/NaN）
    // ============================================================
    if (state == kKeyword) {
      if (ch.isLetterOrNumber() || ch == QLatin1Char('_')) {
        kwBuf += ch;
        continue;
      }
      // 关键字结束 → 识别并处理
      flushComma();
      if (kwBuf == QStringLiteral("Infinity") || kwBuf == QStringLiteral("+Infinity")) {
        appendStr(QStringLiteral("\"Infinity\""), kwStartIdx);
      } else if (kwBuf == QStringLiteral("-Infinity")) {
        appendStr(QStringLiteral("\"-Infinity\""), kwStartIdx);
      } else if (kwBuf == QStringLiteral("NaN") || kwBuf == QStringLiteral("+NaN")) {
        appendStr(QStringLiteral("\"NaN\""), kwStartIdx);
      } else if (kwBuf == QStringLiteral("-NaN")) {
        appendStr(QStringLiteral("\"-NaN\""), kwStartIdx);
      } else {
        // true/false/null 或未知词 → 原样输出（交给 QJsonDocument 去校验）
        appendStr(kwBuf, kwStartIdx);
      }
      kwBuf.clear();
      state = kNormal;
      // 当前字符交给正常状态处理
    }

    // ============================================================
    //  kNormal — 正常文本状态
    // ============================================================

    // 跳过空白字符（空白不影响逗号判断，但还是保留换行便于阅读）
    if (ch.isSpace() && ch != QLatin1Char('\n')) {
      continue;
    }

    // 保留换行符
    if (ch == QLatin1Char('\n')) {
      appendChar(ch, i);
      continue;
    }

    // 注释起始
    if (ch == QLatin1Char('/') && i + 1 < text.size()) {
      QChar next = text[i + 1];
      if (next == QLatin1Char('/')) {
        state = kLineComment;
        ++i;
        continue;
      }
      if (next == QLatin1Char('*')) {
        state = kBlockComment;
        i += 2;
        continue;
      }
    }

    // 字符串起始
    if (ch == QLatin1Char('"')) {
      flushComma();
      state = kStringDq;
      appendChar(ch, i);
      continue;
    }
    if (ch == QLatin1Char('\'')) {
      flushComma();
      state = kStringSq;
      appendChar(QLatin1Char('"'), i);  // 改为双引号起始
      continue;
    }

    // 对象/数组起始：先刷新 pending 的逗号（前一个值后面的逗号），然后清空尾随逗号状态
    if (ch == QLatin1Char('{') || ch == QLatin1Char('[')) {
      flushComma();
      pendingComma.has = false;
      appendChar(ch, i);
      continue;
    }

    // 对象/数组结束：先丢弃暂存逗号（正是这里实现了尾随逗号删除）
    if (ch == QLatin1Char('}') || ch == QLatin1Char(']')) {
      pendingComma.has = false;
      appendChar(ch, i);
      continue;
    }

    // 逗号：暂存，不立即写（等到下一个有效字符决定是否写入）
    if (ch == QLatin1Char(',')) {
      pendingComma.has = true;
      pendingComma.origIdx = i;
      continue;
    }

    // 冒号：key-value 分隔符（直接写）
    if (ch == QLatin1Char(':')) {
      appendChar(ch, i);
      continue;
    }

    // —— 数字起始识别 ——
    //  可能形式：
    //    123、-123、+123（+JSON5不支持，忽略）
    //    0xFF、-0xFF
    //    .5（前导小数点）
    //    5.（后导小数点，在数字状态中处理）
    if (ch.isDigit()) {
      flushComma();
      state = kNumber;
      numBuf = ch;
      numStartIdx = i;
      continue;
    }
    if (ch == QLatin1Char('-') && i + 1 < text.size()) {
      QChar next = text[i + 1];
      if (next.isDigit() || next == QLatin1Char('.')) {
        flushComma();
        state = kNumber;
        numBuf = ch;
        numStartIdx = i;
        continue;
      }
    }
    // 前导小数点：.5
    if (ch == QLatin1Char('.') && i + 1 < text.size() && text[i + 1].isDigit()) {
      flushComma();
      state = kNumber;
      numBuf = ch;
      numStartIdx = i;
      continue;
    }

    // —— Key 识别（优先级高于关键字）——
    //  检测：前一个有效 JSON 结构字符是 { 或 , → 这是 key
    //  向前扫描，跳过所有空白、注释内容、字符串内容等
    auto findPrevSignificantChar = [&]() -> QChar {
      int m = i - 1;
      while (m >= 0) {
        QChar mc = text[m];
        if (mc.isSpace()) {
          --m;
          continue;
        }
        // 只在遇到对象起始或属性分隔符时停止
        if (mc == QLatin1Char('{') || mc == QLatin1Char(',')) return mc;
        // 其他所有内容（注释、字符串、数字、关键字等）全部跳过
        --m;
      }
      return QChar();
    };

    bool isKeyContext = false;
    if (ch.isLetter() || ch == QLatin1Char('$') || ch == QLatin1Char('_')) {
      QChar prev = findPrevSignificantChar();
      if (prev == QLatin1Char('{') || prev == QLatin1Char(',') || prev.isNull()) {
        // 再验证：后面跟着冒号
        int m = i + 1;
        while (m < text.size() && (text[m].isLetterOrNumber() || text[m] == QLatin1Char('$') ||
                                   text[m] == QLatin1Char('_')))
          ++m;
        while (m < text.size() && text[m].isSpace()) ++m;
        if (m < text.size() && text[m] == QLatin1Char(':')) {
          isKeyContext = true;
        }
      }
    }

    // —— 如果是 key 上下文：加双引号输出
    if (isKeyContext) {
      flushComma();  // key 之前可能有暂存的逗号
      appendChar(QLatin1Char('"'), i);
      // 收集标识符字符
      int j = i;
      while (j < text.size()) {
        QChar kc = text[j];
        if (kc.isLetterOrNumber() || kc == QLatin1Char('$') || kc == QLatin1Char('_')) {
          appendChar(kc, j);
          ++j;
        } else {
          break;
        }
      }
      appendChar(QLatin1Char('"'), j - 1);
      i = j - 1;  // j 位置交给下次循环处理
      continue;
    }

    // —— 关键字起始识别 ——
    //  I → Infinity / N → NaN / t → true / f → false / n → null
    //  注意：只有"值"的位置才会到这里（key 已被上面的 isKeyContext 处理）
    if (ch.isLetter()) {
      flushComma();
      state = kKeyword;
      kwBuf = ch;
      kwStartIdx = i;
      continue;
    }

    // 其他字符：原样输出（理论上只可能是标准 JSON 的特殊字符，例如 - 符号等）
    flushComma();
    appendChar(ch, i);
  }

  // —— 文本结束后的收尾处理 ——
  if (state == kKey && !keyBuf.isEmpty()) {
    flushComma();
    appendChar(QLatin1Char('"'), keyStartIdx);
    for (int k = 0; k < keyBuf.size(); ++k) appendChar(keyBuf[k], keyStartIdx + k);
    appendChar(QLatin1Char('"'), keyStartIdx + keyBuf.size() - 1);
  } else if (state == kNumber && !numBuf.isEmpty()) {
    flushComma();
    QString finalNum;
    bool isHex = numBuf.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive) ||
                 numBuf.startsWith(QStringLiteral("-0x"), Qt::CaseInsensitive);
    if (isHex) {
      bool ok = false;
      QString hexPart = numBuf;
      bool neg = false;
      if (hexPart.startsWith(QLatin1Char('-'))) {
        neg = true;
        hexPart = hexPart.mid(1);
      }
      QString hexDigits = hexPart.mid(2);
      qlonglong val = hexDigits.toLongLong(&ok, 16);
      if (ok) {
        if (neg) val = -val;
        finalNum = QString::number(val);
      } else
        finalNum = "0";
    } else {
      if (numBuf.startsWith(QLatin1Char('.')))
        finalNum = QStringLiteral("0") + numBuf;
      else if (numBuf.endsWith(QLatin1Char('.')))
        finalNum = numBuf.left(numBuf.size() - 1);
      else {
        bool ok = false;
        double v = numBuf.toDouble(&ok);
        if (ok) {
          finalNum = QString::number(v, 'f', 20);
          if (finalNum.contains(QLatin1Char('.'))) {
            while (finalNum.endsWith(QLatin1Char('0'))) finalNum.chop(1);
            if (finalNum.endsWith(QLatin1Char('.'))) finalNum.chop(1);
          }
        } else
          finalNum = numBuf;
      }
    }
    appendStr(finalNum, numStartIdx);
  } else if (state == kKeyword && !kwBuf.isEmpty()) {
    flushComma();
    if (kwBuf == QStringLiteral("Infinity") || kwBuf == QStringLiteral("+Infinity"))
      appendStr(QStringLiteral("\"Infinity\""), kwStartIdx);
    else if (kwBuf == QStringLiteral("-Infinity"))
      appendStr(QStringLiteral("\"-Infinity\""), kwStartIdx);
    else if (kwBuf == QStringLiteral("NaN") || kwBuf == QStringLiteral("+NaN"))
      appendStr(QStringLiteral("\"NaN\""), kwStartIdx);
    else if (kwBuf == QStringLiteral("-NaN"))
      appendStr(QStringLiteral("\"-NaN\""), kwStartIdx);
    else
      appendStr(kwBuf, kwStartIdx);
  }

  return result;
}

// stripComments — 剥离 JSON 文本中的注释（兼容旧接口，内部转发到 json5ToJson）
QString UtilJson::stripComments(const QString &text) { return json5ToJson(text, nullptr); }

// stripCommentsWithMap — 剥离 JSON 文本中的注释并记录位置映射（内部转发到 json5ToJson）
//
// offsetMap[strippedIndex] = originalIndex，映射剥离后文本中每个字符对应原始文本的位置。
QString UtilJson::stripCommentsWithMap(const QString &text, QVector<int> &offsetMap) {
  return json5ToJson(text, &offsetMap);
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