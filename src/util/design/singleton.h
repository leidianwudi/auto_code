#ifndef SINGLETON_H
#define SINGLETON_H

// 单例模式
template <typename T> class Singleton {
public:
  Singleton() = default; // 默认构造函数（被 delete 拷贝构造抑制）

  static T &ins() {
    static T s_t;
    return s_t;
  }

  Singleton(const Singleton &) = delete;            // 禁用拷贝构造函数
  Singleton &operator=(const Singleton &) = delete; // 禁用赋值操作符
};

#endif // SINGLETON_H