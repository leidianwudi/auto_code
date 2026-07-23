/**
 * @file fun_db.cpp
 * @brief 数据库函数实现 — 通过 MySQL C API 直连 MySQL
 *
 * 直接使用 libmysql.dll（捆绑在 exe 目录），无需 ODBC 或 Qt SQL 插件。
 * 支持多实例：每个 new DB({...}) 创建独立的 MySQL 连接。
 */

#include "fun_db.h"

#include <mysql.h>

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QUuid>

#include "../ac_language.h"
#include "fun_mgr.h"

QHash<QString, MYSQL *> FunDb::s_connections;
QHash<QString, AcDB::DbConfig> FunDb::s_configs;

// init — 注册所有数据库函数到 FunMgr
void FunDb::init() {
  // 注册原生类 DB 的构造器、析构器和方法（名称来自 ac_language.h）
  FunMgr::ins().registerFuncs(QString::fromLatin1(AcDB::kClassName),
                              {
                                  {QString::fromLatin1(AcRuntime::kConstructor), constructor},
                                  {QString::fromLatin1(AcRuntime::kDestructor), destructor},
                                  {QString::fromLatin1(AcDB::kTableSchema), tableSchema},
                                  {QString::fromLatin1(AcDB::kTableInfo), tableInfo},
                                  {QString::fromLatin1(AcDB::kQuery), query},
                                  {QString::fromLatin1(AcDB::kDisconnect), disconnect},
                                  {QString::fromLatin1(AcKeyword::kDispose), disconnect},
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
  const QString connId = instance.value(QString::fromLatin1(AcDB::kConnId)).toString();
  return s_connections.value(connId, nullptr);
}

// constructor — new DB({...}) 时调用
QJsonValue FunDb::constructor(const QJsonArray &args) {
  if (args.isEmpty() || !args[0].isObject()) {
    FunMgr::setError(
        QStringLiteral("DB() requires a config object with host, user, password, database"));
    return QJsonValue(false);
  }

  const AcDB::DbConfig cfg = AcDB::DbConfig::fromJson(args[0].toObject());

  if (!cfg.isValid()) {
    FunMgr::setError(
        QStringLiteral("DB() config is invalid: host, user, password, database are required"));
    return QJsonValue(false);
  }

  MYSQL *conn = mysql_init(nullptr);
  if (!conn) {
    FunMgr::setError(QStringLiteral("DB() failed to initialize MySQL connection"));
    return QJsonValue(false);
  }

  unsigned int timeout = 5;
  mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

  if (!mysql_real_connect(conn, cfg.host.toUtf8().constData(), cfg.user.toUtf8().constData(),
                          cfg.password.toUtf8().constData(), cfg.database.toUtf8().constData(),
                          cfg.port, nullptr, 0)) {
    QString err = QString::fromUtf8(mysql_error(conn));
    mysql_close(conn);
    FunMgr::setError(QStringLiteral("DB() connection failed: %1").arg(err));
    return QJsonValue(false);
  }

  mysql_set_character_set(conn, "utf8mb4");

  const QString connId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  s_connections[connId] = conn;
  s_configs[connId] = cfg;

  QJsonObject result;
  result[QString::fromLatin1(AcDB::kConnId)] = connId;
  result[QString::fromLatin1(AcDB::kConnected)] = true;
  return QJsonValue(result);
}

// destructor — 引用计数归零时自动调用，关闭 MySQL 连接
QJsonValue FunDb::destructor(const QJsonArray &args) {
  if (args.isEmpty() || !args[0].isObject()) return QJsonValue(false);
  const QJsonObject instance = args[0].toObject();
  const QString connId = instance.value(QString::fromLatin1(AcDB::kConnId)).toString();
  if (connId.isEmpty()) return QJsonValue(false);
  if (s_connections.contains(connId)) {
    MYSQL *conn = s_connections[connId];
    if (conn) mysql_close(conn);
    s_connections.remove(connId);
    s_configs.remove(connId);
  }
  return QJsonValue(true);
}

// disconnect — 断开连接
QJsonValue FunDb::disconnect(const QJsonArray &args) {
  if (args.isEmpty() || !args[0].isObject()) {
    FunMgr::setError(QStringLiteral("DB::disconnect() requires a DB instance object"));
    return QJsonValue(false);
  }

  const QJsonObject instance = args[0].toObject();
  const QString connId = instance.value(QString::fromLatin1(AcDB::kConnId)).toString();
  if (connId.isEmpty()) {
    FunMgr::setError(QStringLiteral("DB::disconnect() invalid DB instance"));
    return QJsonValue(false);
  }

  if (s_connections.contains(connId)) {
    MYSQL *conn = s_connections[connId];
    if (conn) mysql_close(conn);
    s_connections.remove(connId);
    s_configs.remove(connId);
  }
  return QJsonValue(true);
}

// tableSchema — 获取表列信息
QJsonValue FunDb::tableSchema(const QJsonArray &args) {
  if (args.size() < 2 || !args[0].isObject() || !args[1].isObject()) {
    FunMgr::setError(QStringLiteral("DB::tableSchema() requires a DB instance and params object"));
    return QJsonValue();
  }

  const QJsonObject instance = args[0].toObject();
  const QJsonObject params = args[1].toObject();

  MYSQL *conn = getConnection(instance);
  if (!conn) {
    FunMgr::setError(QStringLiteral("DB::tableSchema() not connected, call new DB() first"));
    return QJsonValue();
  }

  const QString connId = instance.value(QString::fromLatin1(AcDB::kConnId)).toString();
  const AcDB::DbConfig cfg = s_configs.value(connId);
  const QString table = params.value(QString::fromLatin1(AcDB::kTable)).toString();
  if (table.isEmpty()) {
    FunMgr::setError(QStringLiteral("DB::tableSchema() requires 'table' in params"));
    return QJsonValue();
  }

  const QString sql = QStringLiteral(
                          "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, "
                          "       COLUMN_KEY, COLUMN_DEFAULT, EXTRA, COLUMN_COMMENT "
                          "FROM INFORMATION_SCHEMA.COLUMNS "
                          "WHERE TABLE_SCHEMA = '%1' AND TABLE_NAME = '%2' "
                          "ORDER BY ORDINAL_POSITION")
                          .arg(cfg.database, table);

  QJsonArray columns;

  if (mysql_query(conn, sql.toUtf8().constData()) != 0) {
    FunMgr::setError(QStringLiteral("DB::tableSchema() query failed: %1")
                         .arg(QString::fromUtf8(mysql_error(conn))));
    return QJsonValue();
  }

  MYSQL_RES *result = mysql_store_result(conn);
  if (!result) {
    FunMgr::setError(QStringLiteral("DB::tableSchema() no result from query"));
    return QJsonValue();
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result))) {
    QJsonObject col;

    col[QString::fromLatin1(AcDB::kColName)] = row[0] ? QString::fromUtf8(row[0]) : QString();
    col[QString::fromLatin1(AcDB::kColType)] = row[1] ? QString::fromUtf8(row[1]) : QString();
    col[QString::fromLatin1(AcDB::kColNullable)] =
        row[2] && QString::fromUtf8(row[2]).toUpper() == QStringLiteral("YES");
    col[QString::fromLatin1(AcDB::kColKey)] = row[3] ? QString::fromUtf8(row[3]) : QString();
    col[QString::fromLatin1(AcDB::kColDefault)] =
        row[4] ? QJsonValue(QString::fromUtf8(row[4])) : QJsonValue();
    col[QString::fromLatin1(AcDB::kColExtra)] = row[5] ? QString::fromUtf8(row[5]) : QString();
    col[QString::fromLatin1(AcDB::kColComment)] = row[6] ? QString::fromUtf8(row[6]) : QString();

    columns.append(col);
  }

  mysql_free_result(result);
  return QJsonValue(columns);
}

// tableInfo — 获取表元信息（表注释、引擎等）
QJsonValue FunDb::tableInfo(const QJsonArray &args) {
  if (args.size() < 2 || !args[0].isObject() || !args[1].isObject()) {
    FunMgr::setError(QStringLiteral("DB::tableInfo() requires a DB instance and params object"));
    return QJsonValue();
  }

  const QJsonObject instance = args[0].toObject();
  const QJsonObject params = args[1].toObject();

  MYSQL *conn = getConnection(instance);
  if (!conn) {
    FunMgr::setError(QStringLiteral("DB::tableInfo() not connected, call new DB() first"));
    return QJsonValue();
  }

  const QString connId = instance.value(QString::fromLatin1(AcDB::kConnId)).toString();
  const AcDB::DbConfig cfg = s_configs.value(connId);
  const QString table = params.value(QString::fromLatin1(AcDB::kTable)).toString();
  if (table.isEmpty()) {
    FunMgr::setError(QStringLiteral("DB::tableInfo() requires 'table' in params"));
    return QJsonValue();
  }

  const QString sql = QStringLiteral(
                          "SELECT TABLE_COMMENT, ENGINE "
                          "FROM INFORMATION_SCHEMA.TABLES "
                          "WHERE TABLE_SCHEMA = '%1' AND TABLE_NAME = '%2'")
                          .arg(cfg.database, table);

  if (mysql_query(conn, sql.toUtf8().constData()) != 0) {
    FunMgr::setError(QStringLiteral("DB::tableInfo() query failed: %1")
                         .arg(QString::fromUtf8(mysql_error(conn))));
    return QJsonValue();
  }

  MYSQL_RES *result = mysql_store_result(conn);
  if (!result) {
    FunMgr::setError(QStringLiteral("DB::tableInfo() no result from query"));
    return QJsonValue();
  }

  QJsonObject info;
  MYSQL_ROW row = mysql_fetch_row(result);
  if (row) {
    info[QString::fromLatin1(AcDB::kTblComment)] = row[0] ? QString::fromUtf8(row[0]) : QString();
    info[QString::fromLatin1(AcDB::kTblEngine)] = row[1] ? QString::fromUtf8(row[1]) : QString();
  } else {
    info[QString::fromLatin1(AcDB::kTblComment)] = QString();
    info[QString::fromLatin1(AcDB::kTblEngine)] = QString();
  }

  mysql_free_result(result);
  return QJsonValue(info);
}

// query — 执行自定义 SQL
QJsonValue FunDb::query(const QJsonArray &args) {
  if (args.size() < 2 || !args[0].isObject() || !args[1].isObject()) {
    FunMgr::setError(QStringLiteral("DB::query() requires a DB instance and params object"));
    return QJsonValue();
  }

  const QJsonObject instance = args[0].toObject();
  const QJsonObject params = args[1].toObject();

  MYSQL *conn = getConnection(instance);
  if (!conn) {
    FunMgr::setError(QStringLiteral("DB::query() not connected, call new DB() first"));
    return QJsonValue();
  }

  const QString sql = params.value(QString::fromLatin1(AcDB::kSql)).toString();
  if (sql.isEmpty()) {
    FunMgr::setError(QStringLiteral("DB::query() requires 'sql' in params"));
    return QJsonValue();
  }

  if (mysql_query(conn, sql.toUtf8().constData()) != 0) {
    FunMgr::setError(
        QStringLiteral("DB::query() failed: %1").arg(QString::fromUtf8(mysql_error(conn))));
    return QJsonValue();
  }

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