/**
 * @file ac_json_value.cpp
 * @brief accore JSON 值类型实现 — 值模型 / 写时复制 / 深拷贝 / Qt 边界互转
 */

#include "ac_json_value.h"

#include <QJsonArray>
#include <QJsonObject>
#include <cmath>

namespace accore {

// ──────────────────────────────────────────────────────────────
//  递归存储结构（头文件中仅前置声明，此处 AcJsonValue 已完整）
// ──────────────────────────────────────────────────────────────

struct AcJsonValue::ArrData {
  Array items;
};

struct AcJsonValue::ObjData {
  Members members;            // 插入序
  QHash<QString, int> index;  // 键 → members 下标
};

// ──────────────────────────────────────────────────────────────
//  构造 / 析构 / 拷贝移动
// ──────────────────────────────────────────────────────────────

AcJsonValue::AcJsonValue() = default;
AcJsonValue::AcJsonValue(std::nullptr_t) {}
AcJsonValue::AcJsonValue(bool b) : m_type(Type::Bool), m_bool(b) {}
AcJsonValue::AcJsonValue(int n) : m_type(Type::Number), m_num(n) {}
AcJsonValue::AcJsonValue(double n) : m_type(Type::Number), m_num(n) {}
AcJsonValue::AcJsonValue(const QString &s) : m_type(Type::String), m_str(s) {}
AcJsonValue::AcJsonValue(const char *s) : m_type(Type::String), m_str(QString::fromUtf8(s)) {}

AcJsonValue::AcJsonValue(const AcJsonValue &other)
    : m_type(other.m_type),
      m_bool(other.m_bool),
      m_num(other.m_num),
      m_str(other.m_str),
      m_objId(other.m_objId),
      m_arr(other.m_arr),  // 共享（写入时分离）
      m_obj(other.m_obj) {}

AcJsonValue &AcJsonValue::operator=(const AcJsonValue &other) {
  if (this != &other) {
    m_type = other.m_type;
    m_bool = other.m_bool;
    m_num = other.m_num;
    m_str = other.m_str;
    m_objId = other.m_objId;
    m_arr = other.m_arr;
    m_obj = other.m_obj;
  }
  return *this;
}

AcJsonValue::AcJsonValue(AcJsonValue &&other) noexcept
    : m_type(other.m_type),
      m_bool(other.m_bool),
      m_num(other.m_num),
      m_str(std::move(other.m_str)),
      m_objId(std::move(other.m_objId)),
      m_arr(std::move(other.m_arr)),
      m_obj(std::move(other.m_obj)) {
  other.m_type = Type::Null;
}

AcJsonValue &AcJsonValue::operator=(AcJsonValue &&other) noexcept {
  if (this != &other) {
    m_type = other.m_type;
    m_bool = other.m_bool;
    m_num = other.m_num;
    m_str = std::move(other.m_str);
    m_objId = std::move(other.m_objId);
    m_arr = std::move(other.m_arr);
    m_obj = std::move(other.m_obj);
    other.m_type = Type::Null;
  }
  return *this;
}

AcJsonValue::~AcJsonValue() = default;

// ──────────────────────────────────────────────────────────────
//  工厂
// ──────────────────────────────────────────────────────────────

AcJsonValue AcJsonValue::makeArray() {
  AcJsonValue v;
  v.m_type = Type::Array;
  v.m_arr = std::make_shared<ArrData>();
  return v;
}

AcJsonValue AcJsonValue::makeObject() {
  AcJsonValue v;
  v.m_type = Type::Object;
  v.m_obj = std::make_shared<ObjData>();
  return v;
}

AcJsonValue AcJsonValue::makeInstance(const QString &className) {
  AcJsonValue v;
  v.m_type = Type::Instance;
  v.m_str = className;
  v.m_obj = std::make_shared<ObjData>();
  return v;
}

AcJsonValue AcJsonValue::instanceFrom(const AcJsonValue &obj, const QString &className,
                                      const QString &objId) {
  AcJsonValue v = makeInstance(className);
  v.m_objId = objId;
  if (obj.isObject()) {
    for (const Member &m : obj.members()) v.set(m.key, m.value);
  }
  return v;
}

AcJsonValue AcJsonValue::makeClassRef(const QString &className) {
  AcJsonValue v;
  v.m_type = Type::ClassRef;
  v.m_str = className;
  return v;
}

AcJsonValue AcJsonValue::makeFuncRef(const QString &funcName) {
  AcJsonValue v;
  v.m_type = Type::FuncRef;
  v.m_str = funcName;
  return v;
}

// ──────────────────────────────────────────────────────────────
//  通用
// ──────────────────────────────────────────────────────────────

int AcJsonValue::toInt(int def) const {
  if (m_type != Type::Number) return def;
  // 与 QJsonValue::toInt 语义一致：仅整值可转
  if (m_num != std::floor(m_num)) return def;
  return static_cast<int>(m_num);
}

int AcJsonValue::size() const {
  if (m_type == Type::Array) return m_arr ? int(m_arr->items.size()) : 0;
  if (m_type == Type::Object || m_type == Type::Instance)
    return m_obj ? int(m_obj->members.size()) : 0;
  return 0;
}

bool AcJsonValue::isEmpty() const {
  if (m_type == Type::Null) return true;
  if (m_type == Type::Array || m_type == Type::Object || m_type == Type::Instance)
    return size() == 0;
  return false;
}

// ──────────────────────────────────────────────────────────────
//  写时分离
// ──────────────────────────────────────────────────────────────

void AcJsonValue::detachArr() {
  if (!m_arr) {
    m_arr = std::make_shared<ArrData>();
    return;
  }
  if (m_arr.use_count() > 1) m_arr = std::make_shared<ArrData>(*m_arr);
}

void AcJsonValue::detachObj() {
  if (!m_obj) {
    m_obj = std::make_shared<ObjData>();
    return;
  }
  if (m_obj.use_count() > 1) m_obj = std::make_shared<ObjData>(*m_obj);
}

// ──────────────────────────────────────────────────────────────
//  数组
// ──────────────────────────────────────────────────────────────

AcJsonValue AcJsonValue::at(int i) const {
  if (m_type != Type::Array || !m_arr) return AcJsonValue();
  if (i < 0 || i >= int(m_arr->items.size())) return AcJsonValue();
  return m_arr->items.at(i);
}

const AcJsonValue::Array &AcJsonValue::items() const {
  static const Array kEmpty;
  return m_arr ? m_arr->items : kEmpty;
}

void AcJsonValue::append(const AcJsonValue &v) {
  if (m_type != Type::Array) return;
  detachArr();
  m_arr->items.append(v.clone());  // 深拷贝被插入值，杜绝自引用环
}

void AcJsonValue::replace(int i, const AcJsonValue &v) {
  if (m_type != Type::Array || !m_arr) return;
  if (i < 0 || i >= int(m_arr->items.size())) return;
  detachArr();
  m_arr->items[i] = v.clone();
}

void AcJsonValue::removeLast() {
  if (m_type != Type::Array || !m_arr || m_arr->items.isEmpty()) return;
  detachArr();
  m_arr->items.removeLast();
}

// ──────────────────────────────────────────────────────────────
//  对象（键保序）
// ──────────────────────────────────────────────────────────────

QStringList AcJsonValue::keys() const {
  QStringList ks;
  if ((m_type != Type::Object && m_type != Type::Instance) || !m_obj) return ks;
  ks.reserve(m_obj->members.size());
  for (const Member &m : m_obj->members) ks.append(m.key);
  return ks;
}

bool AcJsonValue::has(const QString &key) const {
  return (m_type == Type::Object || m_type == Type::Instance) && m_obj &&
         m_obj->index.contains(key);
}

AcJsonValue AcJsonValue::value(const QString &key) const {
  if ((m_type != Type::Object && m_type != Type::Instance) || !m_obj) return AcJsonValue();
  const auto it = m_obj->index.constFind(key);
  if (it == m_obj->index.constEnd()) return AcJsonValue();
  return m_obj->members.at(it.value()).value;
}

const AcJsonValue::Members &AcJsonValue::members() const {
  static const Members kEmpty;
  return m_obj ? m_obj->members : kEmpty;
}

void AcJsonValue::set(const QString &key, const AcJsonValue &v) {
  if (m_type != Type::Object && m_type != Type::Instance) return;
  detachObj();
  const auto it = m_obj->index.constFind(key);
  if (it != m_obj->index.constEnd()) {
    // 已存在的键：原位置更新（JS 语义）
    m_obj->members[it.value()].value = v.clone();
    return;
  }
  m_obj->index.insert(key, int(m_obj->members.size()));
  m_obj->members.append({key, v.clone()});
}

void AcJsonValue::remove(const QString &key) {
  if ((m_type != Type::Object && m_type != Type::Instance) || !m_obj) return;
  const auto it = m_obj->index.constFind(key);
  if (it == m_obj->index.constEnd()) return;
  detachObj();
  const int pos = it.value();
  m_obj->members.removeAt(pos);
  m_obj->index.remove(key);
  // 位移其后所有成员的下标索引
  for (int i = pos; i < m_obj->members.size(); ++i) {
    m_obj->index.insert(m_obj->members.at(i).key, i);
  }
}

// ──────────────────────────────────────────────────────────────
//  深拷贝
// ──────────────────────────────────────────────────────────────

AcJsonValue AcJsonValue::clone() const {
  AcJsonValue dst;
  dst.m_type = m_type;
  dst.m_bool = m_bool;
  dst.m_num = m_num;
  dst.m_str = m_str;
  dst.m_objId = m_objId;
  if (m_arr) {
    dst.m_arr = std::make_shared<ArrData>();
    dst.m_arr->items.reserve(m_arr->items.size());
    for (const AcJsonValue &e : m_arr->items) dst.m_arr->items.append(e.clone());
  }
  if (m_obj) {
    dst.m_obj = std::make_shared<ObjData>();
    dst.m_obj->members.reserve(m_obj->members.size());
    for (const Member &m : m_obj->members) {
      dst.m_obj->index.insert(m.key, int(dst.m_obj->members.size()));
      dst.m_obj->members.append({m.key, m.value.clone()});
    }
  }
  return dst;
}

// ──────────────────────────────────────────────────────────────
//  Qt 边界互转
// ──────────────────────────────────────────────────────────────

QJsonValue AcJsonValue::toQJsonValue() const {
  switch (m_type) {
    case Type::Null:
      return QJsonValue(QJsonValue::Null);
    case Type::Bool:
      return QJsonValue(m_bool);
    case Type::Number:
      return QJsonValue(m_num);
    case Type::String:
      return QJsonValue(m_str);
    case Type::Array: {
      QJsonArray arr;
      if (m_arr) {
        for (const AcJsonValue &e : m_arr->items) arr.append(e.toQJsonValue());
      }
      return QJsonValue(arr);
    }
    case Type::Object: {
      // 注意：QJsonObject 按键字母序存储，此处必然丢序（输出侧已知限制）
      QJsonObject obj;
      if (m_obj) {
        for (const Member &m : m_obj->members) obj.insert(m.key, m.value.toQJsonValue());
      }
      return QJsonValue(obj);
    }
    case Type::Instance: {
      // 实例按属性集序列化（类名/objId 是运行时元数据，不属于数据）
      QJsonObject obj;
      if (m_obj) {
        for (const Member &m : m_obj->members) obj.insert(m.key, m.value.toQJsonValue());
      }
      return QJsonValue(obj);
    }
    case Type::ClassRef:
    case Type::FuncRef:
      // 运行时引用不应持久化
      return QJsonValue(QJsonValue::Null);
  }
  return QJsonValue(QJsonValue::Null);
}

AcJsonValue AcJsonValue::fromQJsonValue(const QJsonValue &v) {
  switch (v.type()) {
    case QJsonValue::Null:
    case QJsonValue::Undefined:
      return AcJsonValue();
    case QJsonValue::Bool:
      return AcJsonValue(v.toBool());
    case QJsonValue::Double:
      return AcJsonValue(v.toDouble());
    case QJsonValue::String:
      return AcJsonValue(v.toString());
    case QJsonValue::Array: {
      AcJsonValue dst = makeArray();
      const QJsonArray arr = v.toArray();
      for (const QJsonValue &e : arr) dst.append(fromQJsonValue(e));
      return dst;
    }
    case QJsonValue::Object: {
      // 注意：QJsonObject 迭代已是字母序，输入侧顺序此前已丢失；需保序请走 parse()
      AcJsonValue dst = makeObject();
      const QJsonObject obj = v.toObject();
      for (auto it = obj.begin(); it != obj.end(); ++it) {
        dst.set(it.key(), fromQJsonValue(it.value()));
      }
      return dst;
    }
  }
  return AcJsonValue();
}

}  // namespace accore
