/**
 * @file fun_str.h
 * @brief 字符串函数 — 向 FunMgr 注册字符串操作
 *
 * 支持的子命令（通过 FunMgr::call("str", subCmd, args) 调用）：
 * - toLowerCase — 转小写    args: [inputStr]
 * - toUpperCase — 转大写    args: [inputStr]
 * - trim        — 去首尾空白 args: [inputStr]
 * - capitalize  — 首字母大写 args: [inputStr]
 * - substring   — 截子串    args: [inputStr, start, length?]
 * - replace     — 替换      args: [inputStr, oldStr, newStr]
 *
 * 用法示例：
 * @code
 *   FunStr::init();  // 启动时注册
 *   FunMgr::ins().call("str", "toLowerCase", QJsonArray{"Hello"});
 * @endcode
 */

#pragma once

#include <QJsonArray>
#include <QJsonValue>

class FunMgr;

/// 字符串工具类（全静态）
class FunStr {
public:
  /// 注册所有字符串函数到 FunMgr
  static void init();

  // ── 具体操作（公开，也可直接调用） ──
  static QJsonValue toLowerCase(const QJsonArray &args);
  static QJsonValue toUpperCase(const QJsonArray &args);
  static QJsonValue trim(const QJsonArray &args);
  static QJsonValue capitalize(const QJsonArray &args);
  static QJsonValue substring(const QJsonArray &args);
  static QJsonValue replace(const QJsonArray &args);
};