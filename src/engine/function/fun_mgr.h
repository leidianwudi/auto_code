/**
 * @file fun_mgr.h
 * @brief 函数管理器（单例） — 统一注册/调用脚本可访问的 C++ 函数
 *
 * 外部通过 FunMgr::ins().call(类名, 函数名, 参数) 调用已注册的任意函数。
 *
 * 架构：
 * - FunStr / FunDb / FunFile 等类在初始化时调用 FunMgr::ins().registerFuncs()
 *   将所有支持的函数指针注册到 FunMgr 中
 * - 每个函数使用 std::function<QJsonValue(const QJsonArray&)> 签名
 *
 * 用法示例：
 * @code
 *   FunStr::init();  // 注册 str::toLowerCase, str::toUpperCase 等
 *   FunDb::init();   // 注册 db::tableSchema
 *
 *   QJsonValue r = FunMgr::ins().call("str", "toLowerCase",
 *                                      QJsonArray{"Hello"});
 * @endcode
 */

#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <functional>
#include <map>

/**
 * @class FunMgr
 * @brief 函数管理器（单例）
 *
 * 线程安全说明：当前为单线程设计，未加锁。
 */
class FunMgr {
public:
  /// 函数指针类型：接收 QJsonArray 参数，返回 QJsonValue
  using FunPtr = std::function<QJsonValue(const QJsonArray &)>;

  /**
   * @brief 单例访问
   */
  static FunMgr &ins();

  /**
   * @brief 全局初始化：注册所有内置函数（FunStr、FunDb、FunFile）
   *
   * 应用启动时调用一次，替代逐个调用 FunStr::init() / FunDb::init()。
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
   * @param className 类名（如 "str"、"db"、"file"）
   * @param funcs     函数名 → 函数指针 映射表
   */
  void registerFuncs(const QString &className,
                     const std::map<QString, FunPtr> &funcs);

  /**
   * @brief 调用已注册的函数
   * @param className 类名
   * @param funcName  函数名
   * @param args      参数数组
   * @return 执行结果；未注册时返回 Null
   */
  QJsonValue call(const QString &className, const QString &funcName,
                  const QJsonArray &args);

  /**
   * @brief 检查某类是否已注册
   */
  bool contains(const QString &className) const;

private:
  FunMgr() = default;
  ~FunMgr();
  FunMgr(const FunMgr &) = delete;
  FunMgr &operator=(const FunMgr &) = delete;

  /// 二级映射：className → (funcName → FunPtr)
  std::map<QString, std::map<QString, FunPtr>> m_registry;
};