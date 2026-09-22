/**
 * @file fun_json.h
 * @brief JSON 函数 — 向 FunMgr 注册 JSON 工具
 */

#pragma once

#include "src/core/json/ac_json_value.h"

/// JSON 工具类（全静态）
class FunJson {
public:
  /// 注册所有 JSON 函数到 FunMgr（builtin 伪类）
  static void init();

  /// 读取 JSON 文件，args: [filePath]
  static accore::AcJsonValue readJson(const accore::AcJsonValue &args);

  /// 把对象/数组序列化为紧凑 JSON 字符串，args: [value]
  static accore::AcJsonValue toJsonString(const accore::AcJsonValue &args);
};
