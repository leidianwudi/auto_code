/**
 * @file fun_file.h
 * @brief 文件读写函数 — 向 FunMgr 注册 File 类
 *
 * 通过 new File() 创建实例后调用：
 * - read  — 读文件   args: [filePath]
 * - write — 写文件   args: [filePath, content]
 *
 * 用法示例：
 * @code
 *   FunFile::init();  // 启动时注册
 *   // 读文件
 *   accore::AcJsonValue args = accore::AcJsonValue::makeArray();
 *   args.append("C:/data/config.json");
 *   accore::AcJsonValue r = FunMgr::ins().call("File", "read", args);
 * @endcode
 */

#pragma once

#include "src/core/json/ac_json_value.h"

/// 文件工具类（全静态）
class FunFile {
public:
  /// 注册所有文件函数到 FunMgr
  static void init();

  /// 读取文件内容（UTF-8），args: [filePath]
  static accore::AcJsonValue read(const accore::AcJsonValue &args);

  /// 写入文件内容（UTF-8），args: [filePath, content]
  static accore::AcJsonValue write(const accore::AcJsonValue &args);
};
