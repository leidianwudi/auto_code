/**
 * @file fun_mgr.h
 * @brief 函数管理器（单例） — 统一注册/调用脚本可访问的 C++ 函数
 *
 * 外部通过 FunMgr::ins().call(类名, 函数名, 参数) 调用已注册的任意函数。
 * 内置函数（renderTpl、write 等）注册在伪类 "builtin"
 * 下，调用方式与普通类一致。
 *
 * 架构：
 * - FunBuiltin / FunStr / FunDb / FunFile / FunJson 等类在初始化时调用
 *   FunMgr::ins().registerFuncs() 将所有支持的函数指针注册到 FunMgr 中
 * - 每个函数使用 std::function<QJsonValue(const QJsonArray&)> 签名
 *
 * 用法示例：
 * @code
 *   FunMgr::init();  // 注册所有内置函数
 *   QJsonValue r = FunMgr::ins().call("str", "toLowerCase",
 * QJsonArray{"Hello"}); QJsonValue v = FunMgr::ins().call("builtin",
 * "readJson", QJsonArray{"data.json"});
 * @endcode
 */

#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <functional>
#include <map>

#include "src/util/design/singleton.h"

/**
 * @class FunMgr
 * @brief 函数管理器（单例，继承 Singleton<FunMgr>）
 *
 * 线程安全说明：当前为单线程设计，未加锁。
 */
class FunMgr : public Singleton<FunMgr> {
public:
  FunMgr() = default;
  ~FunMgr();

  /// 函数指针类型：接收 QJsonArray 参数，返回 QJsonValue
  using FunPtr = std::function<QJsonValue(const QJsonArray &)>;
  /// 无参函数指针类型：不接收参数，返回 QJsonValue
  using FunPtrVoid = std::function<QJsonValue()>;
  /// 实例方法指针类型：显式接收对象实例（this）与实参，返回 QJsonValue
  /// 由 registerFuncsWithThis() 注册，用于需要访问对象状态的类方法（如 DB 实例）
  using FunPtrThis = std::function<QJsonValue(const QJsonValue &thisObj, const QJsonArray &args)>;

  /**
   * @brief
   * 全局初始化：注册所有内置函数（FunBuiltin、FunStr、FunDb、FunFile、FunJson）
   *
   * 应用启动时调用一次。
   */
  static void init();

  /**
   * @brief 全局清理：释放资源（如关闭 DB 连接）
   *
   * 应用退出时调用一次。
   */
  static void cleanup();

  /**
   * @brief 注册一个类的所有函数
   * @param className 类名（如 "builtin"、"str"、"DB"、"file"）
   * @param funcs     函数名 → 函数指针 映射表
   */
  void registerFuncs(const QString &className, const std::map<QString, FunPtr> &funcs);

  /**
   * @brief 注册一个类的所有无参函数（自动包装为 FunPtr）
   * @param className 类名
   * @param funcs     函数名 → 无参函数指针 映射表
   */
  void registerFuncs(const QString &className, const std::map<QString, FunPtrVoid> &funcs);

  /**
   * @brief 注册一个类的实例方法（显式接收对象实例 thisObj）
   *
   * 与方法参数约定不同：实例方法第一个参数为对象实例，后续才是实参。
   * 由 new 实例化类的实例方法（如 DB 的 tableSchema/query）使用，
   * 签名在编译期强制显式声明"接收实例"，避免 args[0] 约定混乱。
   *
   * @param className 类名
   * @param funcs     函数名 → 实例方法指针 映射表
   */
  void registerFuncsWithThis(const QString &className, const std::map<QString, FunPtrThis> &funcs);

  /**
   * @brief 调用已注册的类成员函数（不携带对象实例，thisObj 为 Null）
   *
   * 适用于静态类函数与 call("类","方法",...) 系统调用。
   * @param className 类名（如 "builtin"、"str"、"DB"）
   * @param funcName  函数名
   * @param args      参数数组
   * @return 执行结果；未注册时返回 Null
   */
  QJsonValue call(const QString &className, const QString &funcName, const QJsonArray &args);

  /**
   * @brief 调用已注册的类实例方法（显式携带对象实例）
   *
   * 适用于 new 实例化类的实例方法调用，thisObj 为对象实例本身。
   * 静态类函数调用时传 QJsonValue() 即可。
   * @param className 类名
   * @param funcName  函数名
   * @param thisObj   对象实例（this）
   * @param args      实参数组
   * @return 执行结果；未注册时返回 Null
   */
  QJsonValue call(const QString &className, const QString &funcName, const QJsonValue &thisObj,
                  const QJsonArray &args);

  /**
   * @brief 检查某类是否已注册
   */
  bool contains(const QString &className) const;

  /**
   * @brief 检查某类的某函数是否已注册
   */
  bool contains(const QString &className, const QString &funcName) const;

  /**
   * @brief 设置函数执行错误（由各函数实现调用）
   */
  static void setError(const QString &msg);

  /**
   * @brief 获取并清除最后一次函数执行错误
   */
  static QString takeError();

private:
  /// 二级映射：className → (funcName → FunPtrThis)
  /// 纯参数方法注册时自动包装为忽略 this 的实例方法签名，保证调用路径统一
  std::map<QString, std::map<QString, FunPtrThis>> m_registry;

  /// 最后一次函数执行错误（单线程，每次执行前清空）
  static QString s_lastError;
};