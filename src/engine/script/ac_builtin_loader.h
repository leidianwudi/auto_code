/**
 * @file ac_builtin_loader.h
 * @brief builtin.d.ac 声明文件加载器与原生类注册
 *
 * 统一管理 C++ 原生函数声明文件（builtin.d.ac）的查找与加载，
 * 以及 AcClass::kAll 原生类的注册，消除 ac_validator / ac_executor /
 * ac_interpreter 三处的重复代码。
 */

#pragma once

#include <QHash>
#include <QString>

#include "ac_type.h"

/// @brief builtin.d.ac 加载器（静态工具类）
class AcBuiltinLoader {
public:
  /// @brief 查找 builtin.d.ac 文件路径
  /// @param scriptPath 当前脚本路径（用于推导搜索目录）
  /// @return 找到的 builtin.d.ac 绝对路径，未找到返回空串
  static QString findBuiltinFile(const QString &scriptPath);

  /// @brief 加载 builtin.d.ac 声明文件，收集其中的类和函数定义
  /// @param scriptPath 当前脚本路径
  /// @param classes [in/out] 收集到的类定义会插入此表（不覆盖已有项）
  /// @param functions [in/out] 收集到的函数定义会插入此表（不覆盖已有项）
  /// @return 是否成功加载
  static bool loadDeclarations(const QString &scriptPath,
                               QHash<QString, ClassDef> &classes,
                               QHash<QString, MethodDef> &functions);

  /// @brief 注册 C++ 原生类（AcClass::kAll）到类表
  /// @param classes [in/out] 原生类定义会插入此表（不覆盖已有项）
  static void registerNativeClasses(QHash<QString, ClassDef> &classes);

  /// @brief 一站式加载：builtin.d.ac + 原生类注册
  /// @param scriptPath 当前脚本路径
  /// @param classes [in/out] 类定义表
  /// @param functions [in/out] 函数定义表
  static void loadAll(const QString &scriptPath,
                      QHash<QString, ClassDef> &classes,
                      QHash<QString, MethodDef> &functions);
};
