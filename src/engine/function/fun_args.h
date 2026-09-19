/**
 * @file fun_args.h
 * @brief 内置函数参数校验 helper — 统一参数检查与 FunMgr 错误通道
 *
 * 内置函数的参数校验原先在各实现里手写 if + setError + return，
 * 模式重复（全库约 16 处）。本头文件收敛为可组合的 inline 函数：
 * 校验失败时设置 FunMgr 错误并返回 false，调用方直接 return accore::AcJsonValue()。
 * 错误文案由调用方传入，保持各函数原有文案不变。
 *
 * 参数为 accore::AcJsonValue 数组值（与 FunMgr::call 的 accore 签名一致）。
 */
#pragma once

#include <QString>

#include "fun_mgr.h"
#include "src/core/json/ac_json_value.h"

namespace FunArgs {

/// 参数个数至少 count 个，否则设置错误并返回 false
inline bool requireCount(const accore::AcJsonValue &args, int count, const QString &error) {
  if (args.size() >= count) return true;
  FunMgr::setError(error);
  return false;
}

/// 第 index 个参数为字符串，否则设置错误并返回 false
inline bool requireString(const accore::AcJsonValue &args, int index, const QString &error) {
  if (index < args.size() && args.at(index).isString()) return true;
  FunMgr::setError(error);
  return false;
}

/// 第 index 个参数为对象，否则设置错误并返回 false
inline bool requireObject(const accore::AcJsonValue &args, int index, const QString &error) {
  if (index < args.size() && args.at(index).isObject()) return true;
  FunMgr::setError(error);
  return false;
}

/// 第 index 个参数为数值，否则设置错误并返回 false
inline bool requireNumber(const accore::AcJsonValue &args, int index, const QString &error) {
  if (index < args.size() && args.at(index).isNumber()) return true;
  FunMgr::setError(error);
  return false;
}

/// 第 index 个参数为数组，否则设置错误并返回 false
inline bool requireArray(const accore::AcJsonValue &args, int index, const QString &error) {
  if (index < args.size() && args.at(index).isArray()) return true;
  FunMgr::setError(error);
  return false;
}

}  // namespace FunArgs
