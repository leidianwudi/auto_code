/**
 * @file ac_json_parse.cpp
 * @brief accore JSON 解析器 — 递归下降，输出键保序的 AcJsonValue
 *
 * 语法：标准 JSON + JSON5 常用超集（解析时即接受，无需先归一化）：
 * - 行注释（双斜杠）与块注释（斜杠星号成对出现）
 * - 单引号字符串（与双引号等价）
 * - 对象键可不加引号（标识符：字母/_/$ 开头，字母/数字/_/$ 组成）
 * - 数组/对象尾逗号
 * - \uXXXX 转义按 Unicode 码点还原
 */

#include <cmath>

#include "ac_json_value.h"

namespace accore {
namespace {

/// 递归下降解析器（单遍扫描，字符串感知）
class Parser {
public:
  explicit Parser(const QString &text) : m_s(text) {}

  AcJsonValue run(bool *ok, QString *error) {
    AcJsonValue v;
    skipWs();
    if (!parseValue(v)) {
      if (ok) *ok = false;
      if (error) *error = m_err;
      return AcJsonValue();
    }
    skipWs();
    if (m_i < m_s.size()) {
      fail(QStringLiteral("JSON 文本末尾存在多余内容"));
      if (ok) *ok = false;
      if (error) *error = m_err;
      return AcJsonValue();
    }
    if (ok) *ok = true;
    return v;
  }

private:
  QChar peek() const { return m_i < m_s.size() ? m_s.at(m_i) : QChar(u'\0'); }

  int m_depth = 0;  ///< 当前嵌套深度（parseValue 递归计数）

  bool fail(const QString &msg) {
    if (m_err.isEmpty()) m_err = QStringLiteral("%1（位置 %2）").arg(msg).arg(m_i);
    return false;
  }

  /// 跳过空白与注释（// 行注释、/* */ 块注释）
  void skipWs() {
    while (m_i < m_s.size()) {
      const QChar c = m_s.at(m_i);
      if (c == u' ' || c == u'\t' || c == u'\n' || c == u'\r') {
        ++m_i;
        continue;
      }
      if (c == u'/' && m_i + 1 < m_s.size()) {
        const QChar n = m_s.at(m_i + 1);
        if (n == u'/') {
          m_i += 2;
          while (m_i < m_s.size() && m_s.at(m_i) != u'\n') ++m_i;
          continue;
        }
        if (n == u'*') {
          m_i += 2;
          while (m_i + 1 < m_s.size() && !(m_s.at(m_i) == u'*' && m_s.at(m_i + 1) == u'/')) {
            ++m_i;
          }
          m_i = qMin(m_i + 2, m_s.size());
          continue;
        }
      }
      break;
    }
  }

  bool parseValue(AcJsonValue &out) {
    // 深度防护：超深嵌套输入（如机器生成的 [[[[...]]]]）会打爆 C++ 栈，显式报错
    if (m_depth >= AcJsonValue::kMaxDepth) {
      return fail(QStringLiteral("JSON 嵌套过深（上限 %1 层）").arg(AcJsonValue::kMaxDepth));
    }
    ++m_depth;
    struct DepthPop {
      int &d;
      ~DepthPop() { --d; }
    } pop{m_depth};
    skipWs();
    const QChar c = peek();
    if (c == u'{') return parseObject(out);
    if (c == u'[') return parseArray(out);
    if (c == u'"' || c == u'\'') {
      QString s;
      if (!parseString(s)) return false;
      out = AcJsonValue(s);
      return true;
    }
    if (c == u'-' || c == u'+' || c.isDigit()) {
      double n = 0;
      if (!parseNumber(n)) return false;
      out = AcJsonValue(n);
      return true;
    }
    return parseLiteral(out);
  }

  bool parseObject(AcJsonValue &out) {
    out = AcJsonValue::makeObject();
    ++m_i;  // 跳过 '{'
    skipWs();
    if (peek() == u'}') {
      ++m_i;
      return true;
    }
    while (true) {
      skipWs();
      QString key;
      if (!parseKey(key)) return false;
      skipWs();
      if (peek() != u':') return fail(QStringLiteral("对象键后应为 ':'"));
      ++m_i;
      AcJsonValue v;
      if (!parseValue(v)) return false;
      out.set(key, v);
      skipWs();
      const QChar c = peek();
      if (c == u',') {
        ++m_i;
        skipWs();
        if (peek() == u'}') {  // 尾逗号（JSON5）
          ++m_i;
          return true;
        }
        continue;
      }
      if (c == u'}') {
        ++m_i;
        return true;
      }
      return fail(QStringLiteral("对象成员之间应为 ','"));
    }
  }

  bool parseArray(AcJsonValue &out) {
    out = AcJsonValue::makeArray();
    ++m_i;  // 跳过 '['
    skipWs();
    if (peek() == u']') {
      ++m_i;
      return true;
    }
    while (true) {
      AcJsonValue v;
      if (!parseValue(v)) return false;
      out.append(v);
      skipWs();
      const QChar c = peek();
      if (c == u',') {
        ++m_i;
        skipWs();
        if (peek() == u']') {  // 尾逗号（JSON5）
          ++m_i;
          return true;
        }
        continue;
      }
      if (c == u']') {
        ++m_i;
        return true;
      }
      return fail(QStringLiteral("数组元素之间应为 ','"));
    }
  }

  /// 解析字符串字面量（双引号/单引号），支持标准转义 + 反斜杠续行
  bool parseString(QString &out) {
    const QChar quote = peek();
    ++m_i;
    while (m_i < m_s.size()) {
      const QChar c = m_s.at(m_i);
      if (c == u'\\') {
        ++m_i;
        if (m_i >= m_s.size()) return fail(QStringLiteral("字符串转义未闭合"));
        const QChar e = m_s.at(m_i);
        switch (e.unicode()) {
          case u'"':
            out += u'"';
            ++m_i;
            break;
          case u'\'':
            out += u'\'';
            ++m_i;
            break;
          case u'\\':
            out += u'\\';
            ++m_i;
            break;
          case u'/':
            out += u'/';
            ++m_i;
            break;
          case u'b':
            out += u'\b';
            ++m_i;
            break;
          case u'f':
            out += u'\f';
            ++m_i;
            break;
          case u'n':
            out += u'\n';
            ++m_i;
            break;
          case u'r':
            out += u'\r';
            ++m_i;
            break;
          case u't':
            out += u'\t';
            ++m_i;
            break;
          case u'u': {
            if (m_i + 4 >= m_s.size()) return fail(QStringLiteral("\\u 转义不足 4 位"));
            bool hexOk = false;
            const uint cp = m_s.mid(m_i + 1, 4).toUInt(&hexOk, 16);
            if (!hexOk) return fail(QStringLiteral("\\u 转义含非法十六进制字符"));
            out += QChar(cp);
            m_i += 5;
            break;
          }
          case u'\n':
            ++m_i;
            break;  // 反斜杠续行（JSON5）
          default:
            out += e;
            ++m_i;
            break;  // 宽容：未知转义保留原字符
        }
        continue;
      }
      if (c == quote) {
        ++m_i;
        return true;
      }
      out += c;
      ++m_i;
    }
    return fail(QStringLiteral("字符串未闭合"));
  }

  /// 数字：JSON 语法（允许前导 +）
  bool parseNumber(double &out) {
    const int start = m_i;
    if (peek() == u'+' || peek() == u'-') ++m_i;
    while (m_i < m_s.size() &&
           (m_s.at(m_i).isDigit() || m_s.at(m_i) == u'.' || m_s.at(m_i) == u'e' ||
            m_s.at(m_i) == u'E' || m_s.at(m_i) == u'+' || m_s.at(m_i) == u'-')) {
      ++m_i;
    }
    bool numOk = false;
    const double n = m_s.mid(start, m_i - start).toDouble(&numOk);
    if (!numOk) return fail(QStringLiteral("非法数字"));
    out = n;
    return true;
  }

  /// true / false / null 字面量
  bool parseLiteral(AcJsonValue &out) {
    const QString rest = m_s.mid(m_i, 5);
    if (rest.startsWith(QStringLiteral("true"))) {
      m_i += 4;
      out = AcJsonValue(true);
      return true;
    }
    if (rest.startsWith(QStringLiteral("false"))) {
      m_i += 5;
      out = AcJsonValue(false);
      return true;
    }
    if (rest.startsWith(QStringLiteral("null"))) {
      m_i += 4;
      out = AcJsonValue();
      return true;
    }
    return fail(QStringLiteral("意外字符 '%1'").arg(peek()));
  }

  /// 对象键：双引号/单引号字符串或无引号标识符（JSON5）
  bool parseKey(QString &out) {
    const QChar c = peek();
    if (c == u'"' || c == u'\'') return parseString(out);
    if (!(c.isLetter() || c == u'_' || c == u'$')) {
      return fail(QStringLiteral("对象键应为字符串或标识符"));
    }
    while (m_i < m_s.size()) {
      const QChar k = m_s.at(m_i);
      if (k.isLetterOrNumber() || k == u'_' || k == u'$') {
        out += k;
        ++m_i;
        continue;
      }
      break;
    }
    return true;
  }

  const QString &m_s;
  int m_i = 0;
  QString m_err;
};

}  // namespace

AcJsonValue AcJsonValue::parse(const QString &text, bool *ok, QString *error) {
  Parser p(text);
  return p.run(ok, error);
}

}  // namespace accore
