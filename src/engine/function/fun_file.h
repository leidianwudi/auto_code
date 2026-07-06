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
 *   QJsonValue r = FunMgr::ins().call("File", "read",
 *       QJsonArray{"C:/data/config.json"});
 *   // 写文件
 *   FunMgr::ins().call("File", "write",
 *       QJsonArray{"C:/data/output.txt", "Hello World"});
 * @endcode
 */

#pragma once

#include <QJsonArray>
#include <QJsonValue>

/// 文件工具类（全静态）
class FunFile {
public:
  /// 注册所有文件函数到 FunMgr
  static void init();

  /// 读取文件内容（UTF-8），args: [filePath]
  static QJsonValue read(const QJsonArray &args);

  /// 写入文件内容（UTF-8），args: [filePath, content]
  static QJsonValue write(const QJsonArray &args);
};