/**
 * @file singleton.h
 * @brief 通用单例基类（CRTP + Meyers 单例）
 *
 * 通过 CRTP 继承获得线程安全的全局唯一实例：
 *
 *   class Foo : public Singleton<Foo> { ... };
 *   Foo &f = Foo::ins();
 *
 * 说明：
 * - 使用 C++11 静态局部变量初始化，线程安全
 * - 自动禁用拷贝构造与赋值
 * - 派生类构造函数需可被 Singleton 访问（设为 public，
 *   或添加 friend class Singleton<派生类>）
 */

#pragma once

/**
 * @class Singleton
 * @brief 单例基类（CRTP）
 * @tparam T 派生类类型
 */
template <typename T>
class Singleton {
public:
  Singleton(const Singleton &) = delete;
  Singleton &operator=(const Singleton &) = delete;

  /// 全局唯一实例（首次调用时构造，线程安全）
  static T &ins() {
    static T instance;
    return instance;
  }

protected:
  Singleton() = default;
  ~Singleton() = default;
};
