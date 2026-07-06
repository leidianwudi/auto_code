/**
 * @file fun_json.h
 * @brief JSON 函数实现 — 读写 JSON 文件
 */

#pragma once

#include <QJsonArray>
#include <QJsonValue>

class FunJson {
public:
  /// 初始化：注册 JSON 函数到 FunMgr 的 builtin 伪类
  static void init();

  /// 读取 JSON 文件，返回 QJsonValue
  static QJsonValue readJson(const QJsonArray &args);
};