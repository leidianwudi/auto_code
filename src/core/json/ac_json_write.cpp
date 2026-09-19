/**
 * @file ac_json_write.cpp
 * @brief accore JSON 序列化器 — 紧凑单行 / 2 空格缩进格式化，对象键按插入序输出
 */

#include <cmath>

#include "ac_json_value.h"

namespace accore {
namespace {

/// 数字按 JS 语义输出：整值不带小数点；NaN/Inf 输出 null（与 JSON.stringify 一致）
QString formatNumber(double v) {
  if (std::isnan(v) || std::isinf(v)) return QStringLiteral("null");
  if (v == std::floor(v) && std::fabs(v) < 1e15) return QString::number(qlonglong(v));
  return QString::number(v, 'g', 15);
}

/// 字符串转义：标准 JSON（控制字符走 \uXXXX，非 ASCII 保持原样）
void writeString(QString &out, const QString &s) {
  out += u'"';
  for (const QChar c : s) {
    switch (c.unicode()) {
      case u'"':
        out += QStringLiteral("\\\"");
        break;
      case u'\\':
        out += QStringLiteral("\\\\");
        break;
      case u'\b':
        out += QStringLiteral("\\b");
        break;
      case u'\f':
        out += QStringLiteral("\\f");
        break;
      case u'\n':
        out += QStringLiteral("\\n");
        break;
      case u'\r':
        out += QStringLiteral("\\r");
        break;
      case u'\t':
        out += QStringLiteral("\\t");
        break;
      default:
        if (c.unicode() < 0x20) {
          out += QStringLiteral("\\u%1").arg(uint(c.unicode()), 4, 16, QChar(u'0'));
        } else {
          out += c;
        }
        break;
    }
  }
  out += u'"';
}

void writeIndent(QString &out, bool pretty, int depth) {
  if (!pretty) return;
  for (int i = 0; i < depth; ++i) out += QStringLiteral("  ");
}

void writeValue(QString &out, const AcJsonValue &v, bool pretty, int depth) {
  switch (v.type()) {
    case AcJsonValue::Type::Null:
      out += QStringLiteral("null");
      break;
    case AcJsonValue::Type::Bool:
      out += v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
      break;
    case AcJsonValue::Type::Number:
      out += formatNumber(v.toDouble());
      break;
    case AcJsonValue::Type::String:
      writeString(out, v.toString());
      break;
    case AcJsonValue::Type::Array: {
      const AcJsonValue::Array &items = v.items();
      if (items.isEmpty()) {
        out += QStringLiteral("[]");
        break;
      }
      out += u'[';
      for (int i = 0; i < items.size(); ++i) {
        if (i > 0) out += u',';
        if (pretty) {
          out += u'\n';
          writeIndent(out, true, depth + 1);
        }
        writeValue(out, items.at(i), pretty, depth + 1);
      }
      if (pretty) {
        out += u'\n';
        writeIndent(out, true, depth);
      }
      out += u']';
      break;
    }
    case AcJsonValue::Type::Object: {
      const AcJsonValue::Members &members = v.members();
      if (members.isEmpty()) {
        out += QStringLiteral("{}");
        break;
      }
      out += u'{';
      for (int i = 0; i < members.size(); ++i) {
        if (i > 0) out += u',';
        if (pretty) {
          out += u'\n';
          writeIndent(out, true, depth + 1);
        }
        writeString(out, members.at(i).key);
        out += pretty ? QStringLiteral(": ") : QStringLiteral(":");
        writeValue(out, members.at(i).value, pretty, depth + 1);
      }
      if (pretty) {
        out += u'\n';
        writeIndent(out, true, depth);
      }
      out += u'}';
      break;
    }
  }
}

}  // namespace

QString AcJsonValue::serialize(bool pretty) const {
  QString out;
  writeValue(out, *this, pretty, 0);
  return out;
}

}  // namespace accore
