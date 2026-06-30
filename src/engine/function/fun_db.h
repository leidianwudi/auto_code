/**
 * @file fun_db.h
 * @brief 数据库函数 — 通过 MySQL C API 直连 MySQL 并查询表结构
 *
 * 直接使用 MySQL Connector/C 的 libmysql.dll，
 * 无需 Qt SQL 驱动插件，exe 打包 libmysql.dll 即可运行。
 *
 * 流程：FunDb → MySQL C API (libmysql.dll 捆绑在 exe 目录) → MySQL Server
 */

#pragma once

#include "fun_base.h"

/**
 * @class FunDb
 * @brief MySQL 数据库查询函数，继承 FunBase
 *
 * 用法示例：
 * @code
 *   FunLink 路由后调用 execute([{
 *       "host":     "127.0.0.1",
 *       "port":     3306,
 *       "user":     "root",
 *       "password": "123456",
 *       "database": "my_db",
 *       "table":    "users"
 *   }])
 * @endcode
 *
 * 参数说明（JSON 对象）：
 * | 字段       | 类型   | 必填 | 说明               |
 * |-----------|--------|------|--------------------|
 * | host      | string | 是   | MySQL 主机地址     |
 * | port      | int    | 否   | 端口号，默认 3306  |
 * | user      | string | 是   | 用户名             |
 * | password  | string | 是   | 密码               |
 * | database  | string | 是   | 数据库名           |
 * | table     | string | 是   | 表名               |
 *
 * 返回值（JSON 数组）示例：
 * @code
 *   [
 *       { "name": "id",   "type": "int",        "nullable": false, "key":
 * "PRI", "default": null,     "extra": "auto_increment", "comment": "主键" },
 *       { "name": "name", "type": "varchar(64)", "nullable": false, "key": "",
 * "default": "",       "extra": "",               "comment": "姓名" }, {
 * "name": "age",  "type": "int",         "nullable": true,  "key": "",
 * "default": "0",       "extra": "",               "comment": "年龄" }
 *   ]
 * @endcode
 *
 * @note 运行时依赖：exe 目录下必须存在 libmysql.dll（编译后自动复制）
 */
class FunDb : public FunBase {
public:
  QString name() const override { return QStringLiteral("db"); }

  /**
   * @brief 执行 MySQL 表结构查询
   * @param args [0] JSON 对象，包含 host/port/user/password/database/table
   * @return 表结构 JSON 数组；出错时返回 Null
   */
  QJsonValue execute(const QJsonArray &args) override;
};