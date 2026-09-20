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
 *   accore::AcJsonValue args = accore::AcJsonValue::makeArray();
 *   accore::AcJsonValue r = FunMgr::ins().call("db", "tableSchema", args);
 * @endcode
 */

#pragma once

#include <QHash>
#include <QMutex>
#include <QString>

#include "../ac_language.h"
#include "src/core/json/ac_json_value.h"

struct MYSQL;

/// 数据库工具类（全静态） — 支持多个 DB 实例
///
/// 通过 new DB({...}) 创建实例，每个实例独立持有一个 MySQL 连接。
class FunDb {
public:
  /**
   * @brief 注册所有数据库函数到 FunMgr
   *
   * 必须在首次 call("db", ...) 之前调用。
   */
  static void init();

  /**
   * @brief 关闭所有连接（程序退出时调用）
   */
  static void cleanup();

  // ── 实例构造/方法 ──

  /**
   * @brief 构造 DB 实例并连接数据库
   * args[0] JSON: { host, port?, user, password, database }
   * @return 返回连接实例对象：{ connId: uuid, connected: bool }
   */
  static accore::AcJsonValue constructor(const accore::AcJsonValue &args);

  /**
   * @brief 析构 DB 实例（引用计数归零时自动调用）
   *
   * 关闭 MySQL 连接并释放资源，与 disconnect() 逻辑相同。
   * thisObj: DB 实例对象
   */
  static accore::AcJsonValue destructor(const accore::AcJsonValue &thisObj,
                                        const accore::AcJsonValue &args);

  /**
   * @brief 断开数据库连接并释放资源
   * thisObj: DB 实例对象
   * @return 成功返回 true
   */
  static accore::AcJsonValue disconnect(const accore::AcJsonValue &thisObj,
                                        const accore::AcJsonValue &args);

  /**
   * @brief 获取指定表的列信息
   * thisObj: DB 实例对象
   * args[0] JSON: { table }
   * @return 列信息数组
   */
  static accore::AcJsonValue tableSchema(const accore::AcJsonValue &thisObj,
                                         const accore::AcJsonValue &args);

  /**
   * @brief 获取指定表的元信息（表注释、引擎等）
   * thisObj: DB 实例对象
   * args[0] JSON: { table }
   * @return { comment: "表注释", engine: "InnoDB" }
   */
  static accore::AcJsonValue tableInfo(const accore::AcJsonValue &thisObj,
                                       const accore::AcJsonValue &args);

  /**
   * @brief 执行自定义 SQL 查询
   * thisObj: DB 实例对象
   * args[0] JSON: { sql }
   * @return 查询结果数组
   */
  static accore::AcJsonValue query(const accore::AcJsonValue &thisObj,
                                   const accore::AcJsonValue &args);

private:
  /// 获取实例的连接（根据 this.obj 中的 connId）
  static MYSQL *getConnection(const accore::AcJsonValue &instance);
  /// 全局连接池：connId -> MYSQL*（受 s_connMutex 保护）
  static QHash<QString, MYSQL *> s_connections;
  /// 连接配置池：connId -> DbConfig（受 s_connMutex 保护）
  static QHash<QString, AcDB::DbConfig> s_configs;
  /// 连接池互斥锁：并行 worker 线程同时 new DB()/析构/查询时保护两张池表；
  /// 锁只保护池结构本身，单个 MYSQL* 连接仍不可跨线程并发使用
  /// （每个 DB 实例由创建它的 worker 线程独占使用）
  static QMutex s_connMutex;
};
