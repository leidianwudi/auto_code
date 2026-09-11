/**
 * @file schema_validator.cpp
 * @brief SchemaValidator 实现
 */

#include "schema_validator.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSet>

#include "src/util/common/util_json.h"

// ═════════════════════════════════════════════════════════════════════════════
//  Schema JSON 键名常量（文件内部使用）
// ═════════════════════════════════════════════════════════════════════════════

namespace {
inline constexpr const char *kSchemaRoot = "root";
inline constexpr const char *kSchemaDefinitions = "definitions";
inline constexpr const char *kSchemaProperties = "properties";
inline constexpr const char *kSchemaRequired = "required";
inline constexpr const char *kSchemaAdditionalProperties = "additionalProperties";
inline constexpr const char *kSchemaType = "type";
inline constexpr const char *kSchemaItems = "items";
inline constexpr const char *kSchemaClass = "class";
inline constexpr const char *kSchemaEnum = "enum";
inline constexpr const char *kSchemaDescription = "description";

inline constexpr const char *kTypeInt = "int";
inline constexpr const char *kTypeString = "string";
inline constexpr const char *kTypeDouble = "double";
inline constexpr const char *kTypeBool = "bool";
inline constexpr const char *kTypeArray = "array";
inline constexpr const char *kTypeObject = "object";

/// 元数据键（$schema/$id 等以 $ 开头的键不参与校验）
inline bool isMetaKey(const QString &key) { return key.startsWith(QLatin1Char('$')); }

/// 递归扫描标准 JSON 文本，记录每个对象节点的键声明顺序（对象路径 → 键序）。
/// QJsonObject/QMap 都按键排序，schema 的声明顺序只能从文本本身获得；
/// 文本先经 UtilJson::json5ToJson 归一化，故此处只需处理标准 JSON 语法。
class KeyOrderScanner {
public:
  explicit KeyOrderScanner(const QString &text) : m_text(text) {}

  QHash<QString, QStringList> scan() {
    QHash<QString, QStringList> out;
    m_i = 0;
    skipWs();
    parseValue(QString(), out);
    return out;
  }

private:
  QChar peek() const { return m_i < m_text.size() ? m_text.at(m_i) : QChar(); }

  void skipWs() {
    while (m_i < m_text.size() && m_text.at(m_i).isSpace()) ++m_i;
  }

  /// 解析字符串字面量（含转义还原）；out 为空时仅跳过
  void parseString(QString *out) {
    ++m_i;  // 开引号
    while (m_i < m_text.size()) {
      const QChar c = m_text.at(m_i);
      if (c == QLatin1Char('"')) {
        ++m_i;
        return;
      }
      if (c == QLatin1Char('\\')) {
        ++m_i;
        if (m_i >= m_text.size()) return;
        const QChar e = m_text.at(m_i);
        if (out) {
          switch (e.unicode()) {
            case '"':
              out->append(QLatin1Char('"'));
              break;
            case '\\':
              out->append(QLatin1Char('\\'));
              break;
            case '/':
              out->append(QLatin1Char('/'));
              break;
            case 'b':
              out->append(QLatin1Char('\b'));
              break;
            case 'f':
              out->append(QLatin1Char('\f'));
              break;
            case 'n':
              out->append(QLatin1Char('\n'));
              break;
            case 'r':
              out->append(QLatin1Char('\r'));
              break;
            case 't':
              out->append(QLatin1Char('\t'));
              break;
            case 'u': {
              if (m_i + 4 < m_text.size()) {
                bool ok = false;
                const uint cp = m_text.mid(m_i + 1, 4).toUInt(&ok, 16);
                if (ok) out->append(QChar(cp));
                m_i += 4;
              }
              break;
            }
            default:
              out->append(e);
              break;
          }
        }
        if (e == QLatin1Char('u'))
          ++m_i;  // u 后 4 位已在上面消费（失败也跳过避免死循环）
        else
          ++m_i;
        continue;
      }
      if (out) out->append(c);
      ++m_i;
    }
  }

  /// 跳过标量（数字/true/false/null）
  void skipScalar() {
    while (m_i < m_text.size()) {
      const QChar c = m_text.at(m_i);
      if (c.isSpace() || c == QLatin1Char(',') || c == QLatin1Char('}') || c == QLatin1Char(']'))
        break;
      ++m_i;
    }
  }

  void parseValue(const QString &path, QHash<QString, QStringList> &out) {
    skipWs();
    if (m_i >= m_text.size()) return;
    const QChar c = peek();
    if (c == QLatin1Char('{')) {
      parseObject(path, out);
    } else if (c == QLatin1Char('[')) {
      ++m_i;
      int idx = 0;
      while (m_i < m_text.size()) {
        skipWs();
        if (peek() == QLatin1Char(']')) {
          ++m_i;
          return;
        }
        parseValue(path + QLatin1Char('.') + QString::number(idx), out);
        ++idx;
        skipWs();
        if (peek() == QLatin1Char(',')) ++m_i;
      }
    } else if (c == QLatin1Char('"')) {
      parseString(nullptr);
    } else {
      skipScalar();
    }
  }

  void parseObject(const QString &path, QHash<QString, QStringList> &out) {
    ++m_i;  // '{'
    QStringList keys;
    while (m_i < m_text.size()) {
      skipWs();
      if (peek() == QLatin1Char('}')) {
        ++m_i;
        break;
      }
      QString key;
      parseString(&key);
      skipWs();
      if (peek() == QLatin1Char(':')) ++m_i;
      const QString child = path.isEmpty() ? key : path + QLatin1Char('.') + key;
      if (!key.isEmpty()) keys.append(key);
      parseValue(child, out);
      skipWs();
      if (peek() == QLatin1Char(',')) {
        ++m_i;
        continue;
      }
      if (peek() == QLatin1Char('}')) {
        ++m_i;
        break;
      }
      break;  // 容错：异常文本直接结束
    }
    out.insert(path, keys);
  }

  const QString &m_text;
  int m_i = 0;
};
}  // namespace

// load — 加载 schema 定义文件（支持新格式 root/definitions 与旧格式类名）
bool SchemaValidator::load(const QString &filePath) {
  // 读取原文并归一化（JSON5 → 标准 JSON，保持声明顺序）：
  // 值用 QJsonDocument 解析；键声明顺序从同一份归一化文本单独扫描
  // （QJsonObject 按键字母排序，声明序在解析时就丢了）。
  const QString raw = UtilJson::readTextFile(filePath);
  if (raw.isEmpty()) return false;
  const QString normalized = UtilJson::json5ToJson(raw);
  QJsonParseError err;
  QJsonDocument doc = UtilJson::fromJson(normalized, &err);
  if (err.error != QJsonParseError::NoError || doc.isNull()) return false;
  if (!doc.isObject()) return false;

  QJsonObject root = doc.object();
  m_classes.clear();
  m_rootClass.clear();
  m_keyOrders = KeyOrderScanner(normalized).scan();

  // ── 新格式：{ root: "X", definitions: { ... } } ──
  if (root.contains(QString::fromLatin1(kSchemaRoot)) &&
      root.contains(QString::fromLatin1(kSchemaDefinitions))) {
    m_rootClass = root.value(QString::fromLatin1(kSchemaRoot)).toString();
    QJsonObject defs = root.value(QString::fromLatin1(kSchemaDefinitions)).toObject();
    for (auto it = defs.begin(); it != defs.end(); ++it) {
      ClassDef def;
      parseClassDef(it.value().toObject(), def);
      applyDeclarationOrder(QStringLiteral("definitions.") + it.key(), def);
      m_classes[it.key()] = def;
    }
    if (m_rootClass.isEmpty() && !m_classes.isEmpty()) m_rootClass = m_classes.begin().key();
    return true;
  }

  // ── 旧格式：根对象 key 为类名 ──
  for (auto it = root.begin(); it != root.end(); ++it) {
    ClassDef def;
    parseClassDef(it.value().toObject(), def);
    applyDeclarationOrder(it.key(), def);
    m_classes[it.key()] = def;
  }
  if (!m_classes.isEmpty()) m_rootClass = m_classes.begin().key();
  return true;
}

// applyDeclarationOrder — 按声明顺序重排类属性表
void SchemaValidator::applyDeclarationOrder(const QString &classPath, ClassDef &def) const {
  const QStringList order = m_keyOrders.value(classPath + QStringLiteral(".properties"));
  if (order.isEmpty()) return;
  QVector<QPair<QString, PropertyDef>> ordered;
  ordered.reserve(def.properties.size());
  QSet<QString> placed;
  // 1) 先按声明序取出已声明的属性
  for (const QString &k : order) {
    for (const auto &kv : def.properties) {
      if (kv.first == k) {
        ordered.append(kv);
        placed.insert(k);
        break;
      }
    }
  }
  // 2) 声明序里缺失的键（异常 schema/重复键）按原序补尾，避免丢属性
  for (const auto &kv : def.properties) {
    if (!placed.contains(kv.first)) ordered.append(kv);
  }
  def.properties = ordered;
}

// 解析单个类的定义（properties / required / additionalProperties）
void SchemaValidator::parseClassDef(const QJsonObject &obj, ClassDef &def) const {
  QJsonObject props = obj.value(QString::fromLatin1(kSchemaProperties)).toObject();
  for (auto pit = props.begin(); pit != props.end(); ++pit) {
    PropertyDef pd;
    QJsonObject pdef = pit.value().toObject();
    pd.type = pdef.value(QString::fromLatin1(kSchemaType)).toString();
    pd.items = pdef.value(QString::fromLatin1(kSchemaItems)).toString();
    pd.className = pdef.value(QString::fromLatin1(kSchemaClass)).toString();
    pd.description = pdef.value(QString::fromLatin1(kSchemaDescription)).toString();

    QJsonArray en = pdef.value(QString::fromLatin1(kSchemaEnum)).toArray();
    for (const QJsonValue &v : en) pd.enumValues.append(v.toString());
    def.properties.append(qMakePair(pit.key(), pd));
  }

  QJsonArray req = obj.value(QString::fromLatin1(kSchemaRequired)).toArray();
  for (const QJsonValue &v : req) def.required.append(v.toString());

  if (obj.contains(QString::fromLatin1(kSchemaAdditionalProperties))) {
    QJsonObject ap = obj.value(QString::fromLatin1(kSchemaAdditionalProperties)).toObject();
    def.hasAdditionalProp = true;
    def.additionalProp.type = ap.value(QString::fromLatin1(kSchemaType)).toString();
    QJsonArray aen = ap.value(QString::fromLatin1(kSchemaEnum)).toArray();
    for (const QJsonValue &v : aen) def.additionalProp.enumValues.append(v.toString());
  }
}

// 结构访问 — 供可视化表单渲染器使用
bool SchemaValidator::classInfo(const QString &className, SchemaClassInfo *out) const {
  auto it = m_classes.find(className);
  if (it == m_classes.end() || !out) return false;
  out->required = it->required;
  out->properties.clear();
  // 类级 additionalProperties（任意键对象）约束
  out->hasAdditionalProps = it->hasAdditionalProp;
  if (it->hasAdditionalProp) {
    auto ap = std::make_shared<SchemaPropInfo>();
    ap->type = it->additionalProp.type;
    ap->enumValues = it->additionalProp.enumValues;
    out->additionalProp = ap;
  }
  for (auto pit = it->properties.begin(); pit != it->properties.end(); ++pit) {
    SchemaPropInfo pi;
    const PropertyDef &pd = pit->second;
    pi.type = pd.type;
    pi.items = pd.items;
    pi.className = pd.className;
    pi.enumValues = pd.enumValues;
    pi.description = pd.description;
    out->properties.append(qMakePair(pit->first, pi));
  }
  return true;
}

bool SchemaValidator::isRequiredProperty(const SchemaClassInfo &info, const QString &name) {
  return info.required.contains(name);
}

// validate — 校验入口（旧接口，返回第一个错误）
QString SchemaValidator::validate(const QString &className, const QJsonObject &data) const {
  auto it = m_classes.find(className);
  if (it == m_classes.end()) return QStringLiteral("Schema class '%1' not defined").arg(className);

  QVector<QString> errors;
  validateObject(it.value(), data, className, &errors);
  return errors.isEmpty() ? QString() : errors.first();
}

// validateDocument — 校验整个文档（返回所有错误）
QVector<QString> SchemaValidator::validateDocument(const QJsonObject &data) const {
  QVector<QString> errors;
  if (m_rootClass.isEmpty()) {
    errors.append(QStringLiteral("Schema root not defined"));
    return errors;
  }
  auto it = m_classes.find(m_rootClass);
  if (it == m_classes.end()) {
    errors.append(QStringLiteral("Schema root class '%1' not defined").arg(m_rootClass));
    return errors;
  }
  validateObject(it.value(), data, m_rootClass, &errors);
  return errors;
}

// validateObject — 递归校验对象
void SchemaValidator::validateObject(const ClassDef &def, const QJsonObject &obj,
                                     const QString &path, QVector<QString> *errors) const {
  auto addErr = [&](const QString &msg) { errors->append(msg); };

  // 检查未定义的属性（跳过 $ 开头的元数据键）
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    const QString &key = it.key();
    if (isMetaKey(key)) continue;
    if (!propertyOf(def, key) && !def.hasAdditionalProp) {
      addErr(QStringLiteral("'%1.%2' is not a valid property").arg(path, key));
      continue;
    }
  }

  // 检查必填属性
  for (const QString &req : def.required) {
    if (!obj.contains(req)) {
      addErr(QStringLiteral("'%1.%2' is required").arg(path, req));
    }
  }

  // 检查每个属性的类型（按声明顺序）
  for (const auto &kv : def.properties) {
    const QString &propName = kv.first;
    const PropertyDef &pd = kv.second;
    if (!obj.contains(propName)) continue;  // 可选属性，跳过

    QJsonValue val = obj.value(propName);
    QString childPath = path + QStringLiteral(".") + propName;
    validateValue(pd, val, childPath, errors);
  }

  // additionalProperties：任意键，值按该类型约束校验
  if (def.hasAdditionalProp) {
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      const QString &key = it.key();
      if (propertyOf(def, key) || isMetaKey(key)) continue;
      QString childPath = path + QStringLiteral(".") + key;
      validateValue(def.additionalProp, it.value(), childPath, errors);
    }
  }
}

// validateValue — 按属性类型校验单个值
void SchemaValidator::validateValue(const PropertyDef &pd, const QJsonValue &val,
                                    const QString &childPath, QVector<QString> *errors) const {
  const QString &type = pd.type;
  if (type == QString::fromLatin1(kTypeInt)) {
    if (!val.isDouble()) errors->append(QStringLiteral("'%1' should be int").arg(childPath));
  } else if (type == QString::fromLatin1(kTypeString)) {
    if (!val.isString()) {
      errors->append(QStringLiteral("'%1' should be string").arg(childPath));
    } else if (!pd.enumValues.isEmpty()) {
      const QString s = val.toString();
      if (!pd.enumValues.contains(s)) {
        errors->append(QStringLiteral("'%1' should be one of: %2")
                           .arg(childPath, pd.enumValues.join(QStringLiteral(", "))));
      }
    }
  } else if (type == QString::fromLatin1(kTypeDouble)) {
    if (!val.isDouble()) errors->append(QStringLiteral("'%1' should be double").arg(childPath));
  } else if (type == QString::fromLatin1(kTypeBool)) {
    if (!val.isBool()) errors->append(QStringLiteral("'%1' should be bool").arg(childPath));
  } else if (type == QString::fromLatin1(kTypeArray)) {
    if (!val.isArray()) {
      errors->append(QStringLiteral("'%1' should be array").arg(childPath));
      return;
    }
    QJsonArray arr = val.toArray();
    if (pd.items.isEmpty()) return;
    // 数组元素为基本类型
    if (isPrimitiveType(pd.items)) {
      for (int i = 0; i < arr.size(); ++i) {
        PropertyDef elem;
        elem.type = pd.items;
        validateValue(elem, arr[i], childPath + QStringLiteral("[%1]").arg(i), errors);
      }
      return;
    }
    // 数组元素为指定类，逐个校验
    auto itemIt = m_classes.find(pd.items);
    if (itemIt == m_classes.end()) {
      errors->append(QStringLiteral("'%1' item class '%2' not defined").arg(childPath, pd.items));
      return;
    }
    for (int i = 0; i < arr.size(); ++i) {
      if (!arr[i].isObject()) {
        errors->append(QStringLiteral("'%1[%2]' should be object").arg(childPath).arg(i));
        continue;
      }
      validateObject(itemIt.value(), arr[i].toObject(), childPath + QStringLiteral("[%1]").arg(i),
                     errors);
    }
  } else if (type == QString::fromLatin1(kTypeObject)) {
    if (!val.isObject()) {
      errors->append(QStringLiteral("'%1' should be object").arg(childPath));
      return;
    }
    if (!pd.className.isEmpty()) {
      auto clsIt = m_classes.find(pd.className);
      if (clsIt == m_classes.end()) {
        errors->append(QStringLiteral("'%1' class '%2' not defined").arg(childPath, pd.className));
        return;
      }
      validateObject(clsIt.value(), val.toObject(), childPath, errors);
    }
  }
}

// propertyOf — 在类中查找属性定义（线性查找，属性表按声明序保存）
const SchemaValidator::PropertyDef *SchemaValidator::propertyOf(const ClassDef &def,
                                                                const QString &name) {
  for (const auto &kv : def.properties) {
    if (kv.first == name) return &kv.second;
  }
  return nullptr;
}

// isPrimitiveType — 判断是否为基本类型名
bool SchemaValidator::isPrimitiveType(const QString &type) {
  return type == QString::fromLatin1(kTypeInt) || type == QString::fromLatin1(kTypeString) ||
         type == QString::fromLatin1(kTypeDouble) || type == QString::fromLatin1(kTypeBool);
}

// classNames — 所有已注册类名
QStringList SchemaValidator::classNames() const { return m_classes.keys(); }

// hasClass — 判断类是否已注册
bool SchemaValidator::hasClass(const QString &className) const {
  return m_classes.contains(className);
}

// ═════════════════════════════════════════════════════════════════════════════
//  智能提示（completions）
// ═════════════════════════════════════════════════════════════════════════════

namespace {

/// 光标上下文：描述光标当前所在的 JSON 结构位置
struct CursorCtx {
  // 从根到当前对象的帧（对象/数组），帧的 key 为它在父对象中的属性名
  struct Frame {
    bool isObject;
    QString key;
  };
  QVector<Frame> stack;
  bool afterColon = false;  // 当前是否处于值位置（冒号之后）
  QString lastKey;          // 当前最近一次出现的键
  QString typedKey;         // 正在输入的键（部分）
  QString typedValue;       // 正在输入的值（部分）
};

/**
 * @brief 扫描文本到 pos，得到光标处的 JSON 结构上下文
 * 支持 JSON5 语法：注释、单引号、无引号键、尾随逗号。
 */
CursorCtx findCursorCtx(const QString &text, int pos) {
  CursorCtx ctx;
  bool inString = false;
  QChar quote;
  bool escaped = false;
  bool afterColon = false;
  QString lastKey;
  QString keyAcc;
  QString valueAcc;

  pos = qBound(0, pos, text.size());
  int i = 0;
  while (i < pos) {
    const QChar c = text[i];
    if (inString) {
      if (escaped) {
        // 转义字符：键名一般不含转义，简单保留转义后的字符
        escaped = false;
        if (!afterColon) keyAcc += c;
      } else if (c == QLatin1Char('\\')) {
        escaped = true;
      } else if (c == quote) {
        inString = false;  // 字符串结束（内容已累积到 keyAcc/valueAcc）
      } else if (!afterColon) {
        // 键位置的字符串内容（带引号 key）累积到 keyAcc，供 ':' 处生成 lastKey
        keyAcc += c;
      }
      ++i;
      continue;
    }
    if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
      inString = true;
      quote = c;
      ++i;
      continue;
    }
    // 行注释
    if (c == QLatin1Char('/') && i + 1 < pos && text[i + 1] == QLatin1Char('/')) {
      while (i < pos && text[i] != QLatin1Char('\n')) ++i;
      continue;
    }
    // 块注释
    if (c == QLatin1Char('/') && i + 1 < pos && text[i + 1] == QLatin1Char('*')) {
      i += 2;
      while (i + 1 < pos && !(text[i] == QLatin1Char('*') && text[i + 1] == QLatin1Char('/'))) ++i;
      i += 2;
      continue;
    }
    if (c == QLatin1Char('{')) {
      ctx.stack.push_back({true, lastKey.trimmed()});
      afterColon = false;
      lastKey.clear();
      keyAcc.clear();
      valueAcc.clear();
      ++i;
      continue;
    }
    if (c == QLatin1Char('}')) {
      if (!ctx.stack.isEmpty()) ctx.stack.pop_back();
      afterColon = false;
      lastKey.clear();
      keyAcc.clear();
      valueAcc.clear();
      ++i;
      continue;
    }
    if (c == QLatin1Char('[')) {
      ctx.stack.push_back({false, lastKey.trimmed()});
      afterColon = false;
      lastKey.clear();
      keyAcc.clear();
      valueAcc.clear();
      ++i;
      continue;
    }
    if (c == QLatin1Char(']')) {
      if (!ctx.stack.isEmpty()) ctx.stack.pop_back();
      afterColon = false;
      lastKey.clear();
      keyAcc.clear();
      valueAcc.clear();
      ++i;
      continue;
    }
    if (c == QLatin1Char(':')) {
      afterColon = true;
      lastKey = keyAcc.trimmed();
      keyAcc.clear();
      valueAcc.clear();
      ++i;
      continue;
    }
    if (c == QLatin1Char(',')) {
      afterColon = false;
      lastKey.clear();
      keyAcc.clear();
      valueAcc.clear();
      ++i;
      continue;
    }
    // 普通字符：累积到键或值
    if (afterColon) {
      valueAcc += c;
    } else {
      keyAcc += c;
    }
    ++i;
  }

  ctx.afterColon = afterColon;
  ctx.lastKey = lastKey;
  ctx.typedKey = keyAcc.trimmed();
  ctx.typedValue = valueAcc.trimmed();
  return ctx;
}

/// 提取 text 中 pos 处的完整属性键（支持引号包裹的键），非键字符位置返回空串
QString keyTokenAt(const QString &text, int pos) {
  if (text.isEmpty()) return QString();
  pos = qBound(0, pos, text.size());
  auto isKeyChar = [](QChar c) {
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('$') ||
           c == QLatin1Char('-');
  };
  int start = pos, end = pos;
  while (start > 0 && isKeyChar(text[start - 1])) --start;
  while (end < text.size() && isKeyChar(text[end])) ++end;
  if (start == end) return QString();
  return text.mid(start, end - start);
}

}  // namespace

// completions — 根据光标位置返回 schema 智能提示
QStringList SchemaValidator::completions(const QString &text, int pos) const {
  QStringList result;
  if (m_rootClass.isEmpty()) return result;

  CursorCtx ctx = findCursorCtx(text, pos);
  if (ctx.stack.isEmpty()) return result;  // 未进入任何对象

  // ── 解析当前光标所在对象对应的类 ──
  QString currentClass = m_rootClass;
  for (int si = 0; si < ctx.stack.size(); ++si) {
    const CursorCtx::Frame &f = ctx.stack[si];
    if (f.key.isEmpty()) continue;  // 根或匿名数组元素：类不变
    auto it = m_classes.find(currentClass);
    if (it == m_classes.end()) return result;
    const PropertyDef *pd = propertyOf(it.value(), f.key);
    if (!pd) return result;
    if (pd->type == QString::fromLatin1(kTypeObject)) {
      currentClass = pd->className;
    } else if (pd->type == QString::fromLatin1(kTypeArray)) {
      currentClass = isPrimitiveType(pd->items) ? QString() : pd->items;
    } else {
      return result;  // 标量，无进一步对象
    }
  }

  auto it = m_classes.find(currentClass);
  if (it == m_classes.end()) return result;
  const ClassDef &def = it.value();

  // 栈顶是数组 → 光标在数组元素位置，暂不提示
  if (!ctx.stack.isEmpty() && !ctx.stack.last().isObject) return result;

  // ── 值位置：提示枚举值 ──
  if (ctx.afterColon) {
    const PropertyDef *pd = propertyOf(def, ctx.lastKey);
    if (pd && !pd->enumValues.isEmpty()) {
      for (const QString &v : pd->enumValues) result.append(v);
    }
    return result;
  }

  // ── 键位置：提示属性名（按声明顺序） ──
  for (const auto &kv : def.properties) {
    result.append(kv.first);
  }
  return result;
}

// ═════════════════════════════════════════════════════════════════════════════
//  属性定位（悬停提示 / Ctrl+点击跳转）
// ═════════════════════════════════════════════════════════════════════════════

// resolveProperty — 解析 JSON 路径到叶子属性定义
bool SchemaValidator::resolveProperty(const QString &jsonPath, QString *ownerClass,
                                      const PropertyDef **out) const {
  if (jsonPath.isEmpty() || m_rootClass.isEmpty()) return false;

  QStringList segs = jsonPath.split(QLatin1Char('.'));
  QString currentClass = m_rootClass;
  const PropertyDef *leaf = nullptr;
  QString leafOwner;

  for (const QString &seg : segs) {
    if (seg.isEmpty()) continue;

    // 是否为数组索引段（全数字）
    bool isIndex = true;
    for (const QChar &c : seg) {
      if (!c.isDigit()) {
        isIndex = false;
        break;
      }
    }

    auto it = m_classes.find(currentClass);
    if (it == m_classes.end()) return false;
    const ClassDef &def = it.value();

    if (isIndex) {
      // 数组索引：属性类不变（当前类应为数组元素类 items），跳过
      continue;
    }

    const PropertyDef *pd = propertyOf(def, seg);
    if (!pd) return false;
    leaf = pd;
    leafOwner = currentClass;

    // 进入下一级类
    if (pd->type == QString::fromLatin1(kTypeObject)) {
      currentClass = pd->className;
    } else if (pd->type == QString::fromLatin1(kTypeArray)) {
      currentClass = isPrimitiveType(pd->items) ? QString() : pd->items;
    } else {
      currentClass = QString();
    }
  }

  if (!leaf) return false;
  if (ownerClass) *ownerClass = leafOwner;
  if (out) *out = leaf;
  return true;
}

// propertyDescription — 根据 JSON 路径返回属性说明文本（供悬停提示）
QString SchemaValidator::propertyDescription(const QString &jsonPath) const {
  QString ownerClass;
  const PropertyDef *pd = nullptr;
  if (!resolveProperty(jsonPath, &ownerClass, &pd)) return QString();

  QStringList lines;
  const QString leafName = jsonPath.section(QLatin1Char('.'), -1);

  // 类型
  QString typeStr = pd->type;
  if (pd->type == QString::fromLatin1(kTypeArray)) {
    typeStr = QStringLiteral("array<%1>").arg(pd->items);
  } else if (pd->type == QString::fromLatin1(kTypeObject)) {
    typeStr = QStringLiteral("object<%1>").arg(pd->className);
  }
  lines << QStringLiteral("类型: ") + typeStr;

  // 必填
  auto cit = m_classes.find(ownerClass);
  if (cit != m_classes.end() && cit.value().required.contains(leafName)) {
    lines << QStringLiteral("必填: 是");
  }

  // 枚举
  if (!pd->enumValues.isEmpty()) {
    lines << QStringLiteral("枚举: ") + pd->enumValues.join(QStringLiteral(", "));
  }

  // 描述
  if (!pd->description.isEmpty()) {
    lines << pd->description;
  }

  return lines.join(QLatin1Char('\n'));
}

// propertyContext — 返回属性所在类名与属性名（供在 schema 文件中定位）
bool SchemaValidator::propertyContext(const QString &jsonPath, QString *className,
                                      QString *propName) const {
  QString owner;
  const PropertyDef *pd = nullptr;
  if (!resolveProperty(jsonPath, &owner, &pd)) return false;
  if (className) *className = owner;
  if (propName) *propName = jsonPath.section(QLatin1Char('.'), -1);
  return true;
}

// propertyPathAt — 根据光标位置返回该处属性的 JSON 路径
QString SchemaValidator::propertyPathAt(const QString &text, int pos) const {
  if (m_rootClass.isEmpty()) return QString();

  CursorCtx ctx = findCursorCtx(text, pos);
  if (ctx.afterColon) return QString();  // 值位置，非属性键

  QString key = keyTokenAt(text, pos);
  if (key.isEmpty()) return QString();

  // 构建路径：非空帧键 + 当前键
  QStringList segs;
  for (const auto &f : ctx.stack) {
    if (!f.key.isEmpty()) segs.push_back(f.key);
  }
  segs.push_back(key);
  return segs.join(QLatin1Char('.'));
}