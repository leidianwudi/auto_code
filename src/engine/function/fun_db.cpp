/**
 * @file fun_db.cpp
 * @brief 数据库函数实现 — 通过 MySQL C API 直连 MySQL
 *
 * 直接使用 libmysql.dll（捆绑在 exe 目录），无需 ODBC 或 Qt SQL 插件。
 * 支持多实例：每个 new DB({...}) 创建独立的 MySQL 连接。
 */

#include "fun_db.h"
#include "../ac_language.h"
#include "fun_mgr.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QUuid>

#include <mysql.h>

QHash<QString, MYSQL *> FunDb::s_connections;
QHash<QString, QJsonObject> FunDb::s_configs;

// init — 注册所有数据库函数到 FunMgr
void FunDb::init() {
  // 注册原生类 DB 的构造器和方法（名称来自 ac_language.h）
  FunMgr::ins().registerFuncs(
      QString::fromLatin1(AcDB::kClassName),
      {
          {QString::fromLatin1(AcRuntime::kConstructor), constructor},
          {QString::fromLatin1(AcDB::kTableSchema), tableSchema},
          {QString::fromLatin1(AcDB::kQuery), query},
          {QString::fromLatin1(AcDB::kDisconnect), disconnect},
      });
}

// cleanup — 关闭所有连接
void FunDb::cleanup() {
  for (auto it = s_connections.begin(); it != s_connections.end(); ++it) {
    if (it.value()) {
      mysql_close(it.value());
    }
  }
  s_connections.clear();
  s_configs.clear();
}

// getConnection — 根据实例对象获取连接
MYSQL *FunDb::getConnection(const QJsonObject &instance) {
  const QString connId = instance.value(QStringLiteral("connId")).toString();
  return s_connections.value(connId, nullptr);
}

// constructor — new DB({...}) 时调用
QJsonValue FunDb::constructor(const QJsonArray &args) {
  if (args.isEmpty() || !args[0].isObject())
    return QJsonValue(false);

  const QJsonObject cfg = args[0].toObject();
  const QString host = cfg.value(QStringLiteral("host")).toString();
  const int port = cfg.value(QStringLiteral("port")).toInt(3306);
  const QString user = cfg.value(QStringLiteral("user")).toString();
  const QString pass = cfg.value(QStringLiteral("password")).toString();
  const QString dbName = cfg.value(QStringLiteral("database")).toString();

  if (host.isEmpty() || user.isEmpty() || dbName.isEmpty())
    return QJsonValue(false);

  MYSQL *conn = mysql_init(nullptr);
  if (!conn)
    return QJsonValue(false);

  unsigned int timeout = 5;
  mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

  if (!mysql_real_connect(conn, host.toUtf8().constData(),
                          user.toUtf8().constData(), pass.toUtf8().constData(),
                          dbName.toUtf8().constData(), port, nullptr, 0)) {
    mysql_close(conn);
    return QJsonValue(false);
  }

  mysql_set_character_set(conn, "utf8mb4");

  const QString connId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  s_connections[connId] = conn;
  s_configs[connId] = cfg;

  QJsonObject result;
  result[QStringLiteral("connId")] = connId;
  result[QStringLiteral("connected")] = true;
  return QJsonValue(result);
}

// disconnect — 断开连接
QJsonValue FunDb::disconnect(const QJsonArray &args) {
  if (args.isEmpty() || !args[0].isObject())
    return QJsonValue(false);

  const QJsonObject instance = args[0].toObject();
  const QString connId = instance.value(QStringLiteral("connId")).toString();
  if (connId.isEmpty())
    return QJsonValue(false);

  if (s_connections.contains(connId)) {
    MYSQL *conn = s_connections[connId];
    if (conn)
      mysql_close(conn);
    s_connections.remove(connId);
    s_configs.remove(connId);
  }
  return QJsonValue(true);
}

// tableSchema — 获取表列信息
QJsonValue FunDb::tableSchema(const QJsonArray &args) {
  if (args.size() < 2 || !args[0].isObject() || !args[1].isObject())
    return QJsonValue();

  const QJsonObject instance = args[0].toObject();
  const QJsonObject params = args[1].toObject();

  MYSQL *conn = getConnection(instance);
  if (!conn)
    return QJsonValue();

  const QString dbName =
      s_configs.value(instance.value(QStringLiteral("connId")).toString())
          .value(QStringLiteral("database"))
          .toString();
  const QString table = params.value(QStringLiteral("table")).toString();
  if (table.isEmpty())
    return QJsonValue();

  const QString sql =
      QStringLiteral("SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, "
                     "       COLUMN_KEY, COLUMN_DEFAULT, EXTRA, COLUMN_COMMENT "
                     "FROM INFORMATION_SCHEMA.COLUMNS "
                     "WHERE TABLE_SCHEMA = '%1' AND TABLE_NAME = '%2' "
                     "ORDER BY ORDINAL_POSITION")
          .arg(dbName, table);

  QJsonArray columns;

  if (mysql_query(conn, sql.toUtf8().constData()) != 0)
    return QJsonValue();

  MYSQL_RES *result = mysql_store_result(conn);
  if (!result)
    return QJsonValue();

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result))) {
    QJsonObject col;

    col[QStringLiteral("name")] =
        row[0] ? QString::fromUtf8(row[0]) : QString();
    col[QStringLiteral("type")] =
        row[1] ? QString::fromUtf8(row[1]) : QString();
    col[QStringLiteral("nullable")] =
        row[2] && QString::fromUtf8(row[2]).toUpper() == QStringLiteral("YES");
    col[QStringLiteral("key")] = row[3] ? QString::fromUtf8(row[3]) : QString();
    col[QStringLiteral("default")] =
        row[4] ? QJsonValue(QString::fromUtf8(row[4])) : QJsonValue();
    col[QStringLiteral("extra")] =
        row[5] ? QString::fromUtf8(row[5]) : QString();
    col[QStringLiteral("comment")] =
        row[6] ? QString::fromUtf8(row[6]) : QString();

    columns.append(col);
  }

  mysql_free_result(result);
  return QJsonValue(columns);
}

// query — 执行自定义 SQL
QJsonValue FunDb::query(const QJsonArray &args) {
  if (args.size() < 2 || !args[0].isObject() || !args[1].isObject())
    return QJsonValue();

  const QJsonObject instance = args[0].toObject();
  const QJsonObject params = args[1].toObject();

  MYSQL *conn = getConnection(instance);
  if (!conn)
    return QJsonValue();

  const QString sql = params.value(QStringLiteral("sql")).toString();
  if (sql.isEmpty())
    return QJsonValue();

  if (mysql_query(conn, sql.toUtf8().constData()) != 0)
    return QJsonValue();

  MYSQL_RES *result = mysql_store_result(conn);
  if (!result) {
    return QJsonValue(QJsonArray{});
  }

  const unsigned int numFields = mysql_num_fields(result);
  MYSQL_FIELD *fields = mysql_fetch_fields(result);

  QJsonArray rows;
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result))) {
    QJsonObject rowObj;
    for (unsigned int i = 0; i < numFields; ++i) {
      const QString val = row[i] ? QString::fromUtf8(row[i]) : QString();
      rowObj[QString::fromUtf8(fields[i].name)] = QJsonValue(val);
    }
    rows.append(rowObj);
  }

  mysql_free_result(result);
  return QJsonValue(rows);
}