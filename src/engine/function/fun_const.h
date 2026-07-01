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

/// 全部支持的类名、函数名与子命令常量
struct FunConst {
  // ── 类名 ──
  static constexpr const char *kClsStr = "str";   ///< 字符串处理类
  static constexpr const char *kClsDb = "db";     ///< 数据库查询类
  static constexpr const char *kClsFile = "file"; ///< 文件读写类

  // ── FunStr 子命令 ──
  static constexpr const char *kToLowerCase = "toLowerCase"; ///< 转小写
  static constexpr const char *kToUpperCase = "toUpperCase"; ///< 转大写
  static constexpr const char *kTrim = "trim";               ///< 去首尾空白
  static constexpr const char *kCapitalize = "capitalize";   ///< 首字母大写
  static constexpr const char *kSubstring = "substring";     ///< 子串
  static constexpr const char *kReplace = "replace";         ///< 替换

  /// 返回所有支持的字符串子命令列表
  static QStringList stringMethods();

  // ── FunDb 子命令 ──
  static constexpr const char *kTableSchema = "tableSchema"; ///< 获取表结构
  static constexpr const char *kQuery = "query";             ///< 执行 SQL
  static constexpr const char *kConnect = "connect";         ///< 连接数据库
  static constexpr const char *kDisconnect = "disconnect";   ///< 断开连接

  /// 返回所有支持的数据库子命令列表
  static QStringList dbMethods();

  // ── FunFile 子命令 ──
  static constexpr const char *kRead = "read";   ///< 读文件
  static constexpr const char *kWrite = "write"; ///< 写文件

  /// 返回所有支持的文件子命令列表
  static QStringList fileMethods();
};