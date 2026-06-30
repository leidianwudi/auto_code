/**
 * @file fun_db.h
 * @brief 数据库函数 — 向 FunMgr 注册 MySQL 表结构 / 查询操作
 *
 * 通过 MySQL C API 直连 MySQL，使用 FunMgr::call("db", subCmd, args) 调用。
 * 支持持久连接管理：init() 建立连接，cleanup() 关闭连接。
 *
 * 用法示例：
 * @code
 *   FunDb::init();    // 注册函数 + 连接配置
 *   // 获取表结构
 *   QJsonValue r = FunMgr::ins().call("db", "tableSchema",
 *       QJsonArray{{{"host":"127.0.0.1","port":3306,"user":"root",
 *                     "password":"123456","database":"my_db",
 *                     "table":"users"}}});
 * @endcode
 */

#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <QString>

struct MYSQL;

/// 数据库工具类（全静态）
class FunDb {
public:
  /**
   * @brief 注册所有数据库函数到 FunMgr
   *
   * 必须在首次 call("db", ...) 之前调用。
   */
  static void init();

  /**
   * @brief 关闭持久连接（程序退出时调用）
   */
  static void cleanup();

  // ── 子命令 ──

  /**
   * @brief 获取指定表的列信息
   * args[0] JSON: { host, port?, user, password, database, table }
   * @return 列信息 JSON 数组
   */
  static QJsonValue tableSchema(const QJsonArray &args);

  /**
   * @brief 执行自定义 SQL 查询
   * args[0] JSON: { host, port?, user, password, database, sql }
   * @return 查询结果 JSON 数组
   */
  static QJsonValue query(const QJsonArray &args);

private:
  /// 建立/获取持久连接（内部调用）
  static MYSQL *getConnection(const QJsonObject &cfg);

  /// MySQL 持久连接（首次 getConnection 时建立）
  static MYSQL *s_conn;
};