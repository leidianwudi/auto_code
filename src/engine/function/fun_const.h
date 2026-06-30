/**
 * @file fun_const.h
 * @brief 函数名称常量 — 集中管理所有支持的 C++ 函数名及子命令
 *
 * 所有需要使用函数名称的地方均通过 FunConst 引用，
 * 避免散落在各处的字符串字面量拼写错误和维护困难。
 */

#pragma once

#include <QString>
#include <QStringList>

/// 全部支持的函数名与子命令常量
struct FunConst {
  // ── 顶层函数名 ──
  static constexpr const char *kStr = "str"; ///< 字符串处理函数
  static constexpr const char *kDb = "db";   ///< 数据库查询函数

  // ── 字符串子命令（FunStr） ──
  static constexpr const char *kToLowerCase = "toLowerCase"; ///< 转小写
  static constexpr const char *kToUpperCase = "toUpperCase"; ///< 转大写
  static constexpr const char *kTrim = "trim";               ///< 去首尾空白
  static constexpr const char *kCapitalize = "capitalize";   ///< 首字母大写

  /// 返回所有支持的字符串子命令列表
  static QStringList stringMethods();
};