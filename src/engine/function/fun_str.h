/**
 * @file fun_str.h
 * @brief 字符串函数 — toLowerCase / toUpperCase / trim / capitalize
 */

#pragma once

#include "fun_base.h"
#include "fun_const.h"

/**
 * @class FunStr
 * @brief 字符串处理函数，继承 FunBase
 *
 * 用法示例：
 *   FunFactory 注册后调用 call("str", ["toLowerCase", "Hello World"])
 *
 * 支持的子命令：
 * - toLowerCase — 转换为小写
 * - toUpperCase — 转换为大写
 * - trim        — 去除首尾空白
 * - capitalize  — 首字母大写
 */
class FunStr : public FunBase {
public:
  QString name() const override { return QString::fromLatin1(FunConst::kStr); }

  /**
   * @brief 执行字符串操作
   * @param args [0] 子命令名称，[1] 输入字符串
   * @return 处理后的字符串；参数错误时返回 Null
   */
  QJsonValue execute(const QJsonArray &args) override;
};