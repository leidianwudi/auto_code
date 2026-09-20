/**
 * @file ac_json_value.h
 * @brief accore 自有 JSON 值类型 — 对象键保序的运行时值模型
 *
 * 为什么不用 QJsonValue：QJsonObject 内部按键字母序存储，解析/构建完成的瞬间
 * 属性顺序即丢失（迭代只是暴露已丢的顺序）。本类型对象成员按插入序存储，
 * 供解释器 for-in、JSON.parse、生成脚本等顺序敏感场景使用。
 *
 * 设计要点：
 * - 命名与 QJsonValue 对齐（isObject/value/toString/toDouble/keys...），
 *   解释器与各层从 QJsonValue 迁移时以最小改动完成
 * - 写时复制（COW）：值拷贝廉价共享，写入时才分离 —— 与 QJsonValue 行为一致
 * - 容器插入深拷贝被插入值：结构上杜绝自引用环（与 QJson 深拷贝语义一致）
 * - 仅依赖 QtCore 数据类型（QString/QVector/QHash），核心层不依赖 QJson 与 GUI
 * - 递归结构经不完整类型间接层（ArrData/ObjData 定义在 .cpp）实现，
 *   析构/拷贝/移动在 .cpp 定义
 *
 * 顺序语义对照：
 * - 对象：键按插入序；对已存在键赋值 = 原位置更新（JS 语义）；remove 后重插 = 尾部追加
 * - 数组：元素按追加序
 */

#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>

namespace accore {

class AcJsonValue {
public:
  /// 值类型（Undefined 不存在：缺失即 Null，需要区分时用 has()）
  ///
  /// 运行时专用种类（Instance/ClassRef/FuncRef）由解释器内部使用：
  /// - Instance：类实例，类名与唯一 id 存于专用字段，属性存于对象存储 —— 用户的
  ///   属性空间里没有内部协议键，任意键名的用户数据都不会污染引擎
  /// - ClassRef / FuncRef：类引用与函数引用
  /// 三者对 isObject() 均返回 true（JS 语义：实例即对象），属性访问接口直接可用；
  /// 序列化时 Instance 输出属性集，ClassRef/FuncRef 输出 null（运行时引用不应持久化）
  enum class Type : char {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object,
    Instance,  ///< 类实例（运行时）
    ClassRef,  ///< 类引用（运行时）
    FuncRef    ///< 函数引用（运行时）
  };

  /// 数组元素列表（元素为完整 AcJsonValue；别名仅声明，使用点实例化）
  using Array = QVector<AcJsonValue>;
  /// 对象成员（键 + 值），按插入序排列；完整定义见类外
  struct Member;
  using Members = QVector<Member>;

  // ── 构造 / 析构 / 拷贝移动（递归类型：定义在 .cpp）──
  AcJsonValue();
  AcJsonValue(std::nullptr_t);
  AcJsonValue(bool b);
  AcJsonValue(int n);
  AcJsonValue(double n);
  AcJsonValue(const QString &s);
  AcJsonValue(const char *s);
  AcJsonValue(const AcJsonValue &other);
  AcJsonValue &operator=(const AcJsonValue &other);
  AcJsonValue(AcJsonValue &&other) noexcept;
  AcJsonValue &operator=(AcJsonValue &&other) noexcept;
  ~AcJsonValue();
  // ── 工厂 ──
  /// 空数组
  static AcJsonValue makeArray();
  /// 空对象
  static AcJsonValue makeObject();
  /// 类实例：属性集初始为空（set() 追加）；objId 由对象管理器生成后回填
  static AcJsonValue makeInstance(const QString &className);
  /// 以 obj 的属性为初始属性集创建实例（native 构造器结果包装用）
  static AcJsonValue instanceFrom(const AcJsonValue &obj, const QString &className,
                                  const QString &objId = QString());
  /// 类引用
  static AcJsonValue makeClassRef(const QString &className);
  /// 函数引用
  static AcJsonValue makeFuncRef(const QString &funcName);

  // ── 类型判断 ──
  Type type() const { return m_type; }
  bool isNull() const { return m_type == Type::Null; }
  bool isBool() const { return m_type == Type::Bool; }
  bool isNumber() const { return m_type == Type::Number; }
  /// 与 QJsonValue::isDouble 同义的别名（迁移兼容）
  bool isDouble() const { return m_type == Type::Number; }
  bool isString() const { return m_type == Type::String; }
  bool isArray() const { return m_type == Type::Array; }
  /// 对象判断（实例也是对象 —— JS 语义：实例即对象，属性访问接口对其可用）；
  /// 需要区分"纯数据对象"与"类实例"时用 isInstance()
  bool isObject() const { return m_type == Type::Object || m_type == Type::Instance; }
  bool isInstance() const { return m_type == Type::Instance; }
  bool isClassRef() const { return m_type == Type::ClassRef; }
  bool isFuncRef() const { return m_type == Type::FuncRef; }

  // ── 运行时种类访问 ──
  /// 实例/类引用的类名；其余类型返回空
  QString instanceClass() const { return isInstance() || isClassRef() ? m_str : QString(); }
  /// 实例唯一 id；非实例返回空
  QString instanceObjId() const { return isInstance() ? m_objId : QString(); }
  /// 函数引用的函数名；非函数引用返回空
  QString funcRefName() const { return isFuncRef() ? m_str : QString(); }
  /// 回填实例唯一 id（对象管理器生成 uuid 后调用）；非实例忽略
  void setInstanceObjId(const QString &objId) {
    if (isInstance()) m_objId = objId;
  }

  // ── 标量取值 ──
  bool toBool(bool def = false) const { return m_type == Type::Bool ? m_bool : def; }
  double toDouble(double def = 0) const { return m_type == Type::Number ? m_num : def; }
  /// 整数取值：仅当数值为整时返回，否则返回默认值（与 QJsonValue::toInt 语义一致）
  int toInt(int def = 0) const;
  /// 非字符串返回空串（与 Qt 6 QJsonValue::toString() 行为一致）
  QString toString() const { return m_type == Type::String ? m_str : QString(); }
  /// 非字符串返回默认值
  QString toString(const QString &def) const { return m_type == Type::String ? m_str : def; }

  // ── 通用 ──
  /// 数组元素数 / 对象成员数；标量返回 0
  int size() const;
  /// null、空数组、空对象返回 true
  bool isEmpty() const;

  // ── 数组 ──
  /// 越界返回 Null
  AcJsonValue at(int i) const;
  /// 数组元素（只读，按追加序）；非数组返回空
  const Array &items() const;
  /// 尾部追加（深拷贝被插入值）
  void append(const AcJsonValue &v);
  /// 覆盖指定位置元素（深拷贝）；越界忽略
  void replace(int i, const AcJsonValue &v);
  /// 尾部删除一个元素
  void removeLast();

  // ── 对象（键保序）──
  /// 键列表，插入序；非对象返回空
  QStringList keys() const;
  bool has(const QString &key) const;
  /// 缺失键返回 Null（与 QJsonValue::value 语义一致；区分"缺失"与"显式 null"用 has()）
  AcJsonValue value(const QString &key) const;
  /// 对象成员（只读，插入序）；非对象返回空
  const Members &members() const;
  /// 赋值：已存在的键在原位置更新（JS 语义），新键追加到尾部（深拷贝值）
  void set(const QString &key, const AcJsonValue &v);
  void remove(const QString &key);

  // ── 深拷贝 ──
  /// 递归深拷贝（容器插入时内部使用；外部一般不需要）
  AcJsonValue clone() const;

  // ── 边界互转（Qt 适配层）──
  /// 转为 QJsonValue（注意：QJsonObject 会丢失键序，仅供不关心顺序的 Qt 侧消费）
  QJsonValue toQJsonValue() const;
  /// 从 QJsonValue 转换（注意：QJsonObject 迭代已是字母序，输入侧顺序此前已丢失）
  static AcJsonValue fromQJsonValue(const QJsonValue &v);

  // ── 解析 / 序列化 ──
  /**
   * @brief 解析 JSON 文本（支持 JSON5 超集：注释、单引号字符串、无引号键、尾逗号）
   * @param text JSON/JSON5 文本
   * @param ok 输出参数：解析成功返回 true
   * @param error 错误信息输出（含位置描述），可为 nullptr
   * @return 解析结果；失败返回 Null
   */
  static AcJsonValue parse(const QString &text, bool *ok = nullptr, QString *error = nullptr);
  /**
   * @brief 序列化为 JSON 文本（数字按 JS 语义：整值不带小数点）
   * @param pretty true 时按 2 空格缩进格式化；false 为紧凑单行
   */
  QString serialize(bool pretty = false) const;

private:
  /// 数组存储（定义在 .cpp —— 值类型递归需要间接层）
  struct ArrData;
  /// 对象存储（定义在 .cpp）
  struct ObjData;

  Type m_type = Type::Null;
  bool m_bool = false;
  double m_num = 0.0;
  QString m_str;                   ///< String 值；Instance/ClassRef 的类名；FuncRef 的函数名
  QString m_objId;                 ///< Instance 的唯一 id（仅实例非空）
  std::shared_ptr<ArrData> m_arr;  // 共享 + 写时分离
  std::shared_ptr<ObjData> m_obj;  // 共享 + 写时分离

  void detachArr();
  void detachObj();
};

/// 对象成员：定义在类外（value 需要完整的 AcJsonValue）
struct AcJsonValue::Member {
  QString key;
  AcJsonValue value;
};

/// 数组边界互转：AcJsonValue 数组转 QJsonArray。
/// 供 FunMgr 等保持 Qt 签名的接口使用（原先在解释器两个编译单元里各自重复定义）。
inline QJsonArray toQJsonArray(const AcJsonValue &arr) {
  QJsonArray out;
  for (const AcJsonValue &v : arr.items()) out.append(v.toQJsonValue());
  return out;
}

}  // namespace accore
