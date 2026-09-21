/**
 * @file ac_bytecode_cache.h
 * @brief 预编译缓存 — 字节码落盘与失效校验（阶段 3 / A1 加固）
 *
 * 位置：<脚本目录>/.ac_cache/<入口名>.<失效键前 4 字节十六进制>.acb
 * 失效键（invalidationHashFor）：
 *   入口文件内容 + 引擎缓存版本 + 直接/传递 import 文件（路径→内容，缺失显式封存）
 *   + builtin.d.ac 内容。任一变化即重编译——被 import 的文件改动不再是漏网之鱼。
 */

#pragma once

#include <QString>
#include <QStringList>

#include "ac_opcode.h"

/// @brief 字节码缓存门面
class AcBytecodeCache {
public:
  /// 引擎缓存常量：引擎字节码语义/格式变化时递增，强制全量失效
  static int engineCacheVersion() { return 3; }

  /// @brief 单个文件内容哈希（含版本；失败返回空串）
  static QString sourceHashFor(const QString &scriptFile);

  /// @brief 完整失效键：入口内容 + 版本 + import 文件（稳定排序去重）+ builtin.d.ac
  /// @param importFiles 解析后的 import 文件绝对路径（直接/传递；由调用方从 AST 收集）
  static QString invalidationHashFor(const QString &scriptFile, const QStringList &importFiles);

  /// @brief 缓存文件路径：<dir>/.ac_cache/<basename>.<失效键 8 位>.acb
  static QString cachePathFor(const QString &scriptFile, const QStringList &importFiles = {});

  /// @brief 命中则反序列化进 module 并返回 true（含格式/失效键校验）
  static bool tryLoad(const QString &scriptFile, const QStringList &importFiles,
                      AcModule &module);

  /// @brief 序列化并写入缓存目录（自动建目录；module.sourceHash 覆写为失效键）
  static bool trySave(const QString &scriptFile, const QStringList &importFiles,
                      const AcModule &module);
};