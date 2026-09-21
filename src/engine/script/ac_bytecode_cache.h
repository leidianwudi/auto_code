/**
 * @file ac_bytecode_cache.h
 * @brief 预编译缓存 — 字节码落盘与失效校验（阶段 3）
 *
 * 位置：<脚本目录>/.ac_cache/<入口名>.<源哈希前 4 字节十六进制>.acb
 * 失效：缓存头校验 格式版本 + 引擎缓存常量 + 源文件内容哈希。任一不匹配即视为失效。
 */

#pragma once

#include <QString>

#include "ac_opcode.h"

/// @brief 字节码缓存门面
class AcBytecodeCache {
public:
  /// 引擎缓存常量：引擎字节码语义变化时递增，强制全量失效
  static int engineCacheVersion() { return 2; }

  /// @brief 源文件内容哈希（失败返回空串）
  static QString sourceHashFor(const QString &scriptFile);

  /// @brief 缓存文件路径：<dir>/.ac_cache/<basename>.<hash4>.acb
  static QString cachePathFor(const QString &scriptFile);

  /// @brief 命中则反序列化进 module 并返回 true（含格式/哈希/版本校验）
  static bool tryLoad(const QString &scriptFile, AcModule &module);

  /// @brief 序列化并写入缓存目录（自动建目录；module.sourceHash 将被覆写为内容哈希）
  static bool trySave(const QString &scriptFile, const AcModule &module);
};