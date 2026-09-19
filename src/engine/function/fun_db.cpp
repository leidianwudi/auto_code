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
#include <QString>
#include <QUuid>

#include "../ac_language.h"
#include "fun_mgr.h"

QHash<QString, MYSQL *> FunDb::s_connections;
QHash<QString, AcDB::DbConfig> FunDb::s_configs;

// escapeSqlLiteral — 把字符串转义为可安全嵌入 SQL 单引号字面量的形式
// 使用连接的字符集做转义（含引号/反斜杠），防止表名/库名中的特殊字符
// 破坏 SQL 结构（注入或查询失败）。仅在已建立连接后调用。
static QString escapeSqlLiteral(MYSQL *conn, const QString &value) {
  const QByteArray utf8 = value.toUtf8();
  QByteArray buf(utf8.size() * 2 + 1, '\0');
  const unsigned long n =
      mysql_real_escape_string_quote(conn, buf.data(), utf8.constData(), utf8.size(), '\'');
  return QString::fromUtf8(buf.data(), static_cast<int>(n));
}

// init — 注册所有数据库函数到 FunMgr
void FunDb::init() {
  // 注册原生类 DB 的构造器（纯参数，new DB({...}) 时调用）
  FunMgr::ins().registerFuncs(QString::fromLatin1(AcDB::kClassName),
                              {{QString::fromLatin1(AcRuntime::kConstructor), constructor}});
  // 实例方法：显式接收 DB 实例对象（thisObj），编译期声明"接收实例"约定
  FunMgr::ins().registerFuncsWithThis(QString::fromLatin1(AcDB::kClassName),
                                      {
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
MYSQL *FunDb::getConnection(const accore::AcJsonValue &instance) {
  const QString connId = instance.value(QString::fromLatin1(AcDB::kConnId)).toString();
  return s_connections.value(connId, nullptr);
}

// constructor — new DB({...}) 时调用
accore::AcJsonValue FunDb::constructor(const accore::AcJsonValue &args) {
  if (args.size() == 0 || !args.at(0).isObject()) {
    FunMgr::setError(
        QStringLiteral("DB() requires a config object with host, user, password, database"));
    return accore::AcJsonValue(false);
  }

  const AcDB::DbConfig cfg = AcDB::DbConfig::fromJson(args.at(0));

  if (!cfg.isValid()) {
    FunMgr::setError(
        QStringLiteral("DB() config is invalid: host, user, password, database are required"));
    return accore::AcJsonValue(false);
  }

  MYSQL *conn = mysql_init(nullptr);
  if (!conn) {
    FunMgr::setError(QStringLiteral("DB() failed to initialize MySQL connection"));
    return accore::AcJsonValue(false);
  }

  unsigned int timeout = 5;
  mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

  if (!mysql_real_connect(conn, cfg.host.toUtf8().constData(), cfg.user.toUtf8().constData(),
                          cfg.password.toUtf8().constData(), cfg.database.toUtf8().constData(),
                          cfg.port, nullptr, 0)) {
    QString err = QString::fromUtf8(mysql_error(conn));
    mysql_close(conn);
    FunMgr::setError(QStringLiteral("DB() connection failed: %1").arg(err));
    return accore::AcJsonValue(false);
  }

  mysql_set_character_set(conn, "utf8mb4");

  const QString connId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  s_connections[connId] = conn;
  s_configs[connId] = cfg;

  accore::AcJsonValue result = accore::AcJsonValue::makeObject();
  result.set(QString::fromLatin1(AcDB::kConnId), accore::AcJsonValue(connId));
  result.set(QString::fromLatin1(AcDB::kConnected), accore::AcJsonValue(true));
  return result;
}

// destructor — 引用计数归零时自动调用，关闭 MySQL 连接
accore::AcJsonValue FunDb::destructor(const accore::AcJsonValue &thisObj,
                                      const accore::AcJsonValue &) {
  if (!thisObj.isObject()) return accore::AcJsonValue(false);
  const QString connId = thisObj.value(QString::fromLatin1(AcDB::kConnId)).toString();
  if (connId.isEmpty()) return accore::AcJsonValue(false);
  if (s_connections.contains(connId)) {
    MYSQL *conn = s_connections[connId];
    if (conn) mysql_close(conn);
    s_connections.remove(connId);
    s_configs.remove(connId);
  }
  return accore::AcJsonValue(true);
}

// disconnect — 断开连接
accore::AcJsonValue FunDb::disconnect(const accore::AcJsonValue &thisObj,
                                      const accore::AcJsonValue &) {
  if (!thisObj.isObject()) {
    FunMgr::setError(QStringLiteral("DB::disconnect() requires a DB instance object"));
    return accore::AcJsonValue();
  }

  const QString connId = thisObj.value(QString::fromLatin1(AcDB::kConnId)).toString();
  if (connId.isEmpty()) {
    FunMgr::setError(QStringLiteral("DB::disconnect() invalid DB instance"));
    return accore::AcJsonValue();
  }

  if (s_connections.contains(connId)) {
    MYSQL *conn = s_connections[connId];
    if (conn) mysql_close(conn);
    s_connections.remove(connId);
    s_configs.remove(connId);
  }
  return accore::AcJsonValue(true);
}

// tableSchema — 获取表列信息
accore::AcJsonValue FunDb::tableSchema(const accore::AcJsonValue &thisObj,
                                       const accore::AcJsonValue &args) {
  if (!thisObj.isObject() || args.size() == 0 || !args.at(0).isObject()) {
    FunMgr::setError(QStringLiteral("DB::tableSchema() requires a DB instance and params object"));
    return accore::AcJsonValue();
  }

  const accore::AcJsonValue params = args.at(0);

  MYSQL *conn = getConnection(thisObj);
  if (!conn) {
    FunMgr::setError(QStringLiteral("DB::tableSchema() not connected, call new DB() first"));
    return accore::AcJsonValue();
  }

  const QString connId = thisObj.value(QString::fromLatin1(AcDB::kConnId)).toString();
  const AcDB::DbConfig cfg = s_configs.value(connId);
  const QString table = params.value(QString::fromLatin1(AcDB::kTable)).toString();
  if (table.isEmpty()) {
    FunMgr::setError(QStringLiteral("DB::tableSchema() requires 'table' in params"));
    return accore::AcJsonValue();
  }

  const QString sql = QStringLiteral(
                          "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, "
                          "       COLUMN_KEY, COLUMN_DEFAULT, EXTRA, COLUMN_COMMENT "
                          "FROM INFORMATION_SCHEMA.COLUMNS "
                          "WHERE TABLE_SCHEMA = '%1' AND TABLE_NAME = '%2' "
                          "ORDER BY ORDINAL_POSITION")
                          .arg(escapeSqlLiteral(conn, cfg.database), escapeSqlLiteral(conn, table));

  accore::AcJsonValue columns = accore::AcJsonValue::makeArray();

  if (mysql_query(conn, sql.toUtf8().constData()) != 0) {
    FunMgr::setError(QStringLiteral("DB::tableSchema() query failed: %1")
                         .arg(QString::fromUtf8(mysql_error(conn))));
    return accore::AcJsonValue();
  }

  MYSQL_RES *result = mysql_store_result(conn);
  if (!result) {
    FunMgr::setError(QStringLiteral("DB::tableSchema() no result from query"));
    return accore::AcJsonValue();
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result))) {
    accore::AcJsonValue col = accore::AcJsonValue::makeObject();

    col.set(QString::fromLatin1(AcDB::kColName),
            accore::AcJsonValue(row[0] ? QString::fromUtf8(row[0]) : QString()));
    col.set(QString::fromLatin1(AcDB::kColType),
            accore::AcJsonValue(row[1] ? QString::fromUtf8(row[1]) : QString()));
    col.set(QString::fromLatin1(AcDB::kColNullable),
            accore::AcJsonValue(row[2] &&
                                QString::fromUtf8(row[2]).toUpper() == QStringLiteral("YES")));
    col.set(QString::fromLatin1(AcDB::kColKey),
            accore::AcJsonValue(row[3] ? QString::fromUtf8(row[3]) : QString()));
    col.set(QString::fromLatin1(AcDB::kColDefault),
            row[4] ? accore::AcJsonValue(QString::fromUtf8(row[4])) : accore::AcJsonValue());
    col.set(QString::fromLatin1(AcDB::kColExtra),
            accore::AcJsonValue(row[5] ? QString::fromUtf8(row[5]) : QString()));
    col.set(QString::fromLatin1(AcDB::kColComment),
            accore::AcJsonValue(row[6] ? QString::fromUtf8(row[6]) : QString()));

    columns.append(col);
  }

  mysql_free_result(result);
  return columns;
}

// tableInfo — 获取表元信息（表注释、引擎等）
accore::AcJsonValue FunDb::tableInfo(const accore::AcJsonValue &thisObj,
                                     const accore::AcJsonValue &args) {
  if (!thisObj.isObject() || args.size() == 0 || !args.at(0).isObject()) {
    FunMgr::setError(QStringLiteral("DB::tableInfo() requires a DB instance and params object"));
    return accore::AcJsonValue();
  }

  const accore::AcJsonValue params = args.at(0);

  MYSQL *conn = getConnection(thisObj);
  if (!conn) {
    FunMgr::setError(QStringLiteral("DB::tableInfo() not connected, call new DB() first"));
    return accore::AcJsonValue();
  }

  const QString connId = thisObj.value(QString::fromLatin1(AcDB::kConnId)).toString();
  const AcDB::DbConfig cfg = s_configs.value(connId);
  const QString table = params.value(QString::fromLatin1(AcDB::kTable)).toString();
  if (table.isEmpty()) {
    FunMgr::setError(QStringLiteral("DB::tableInfo() requires 'table' in params"));
    return accore::AcJsonValue();
  }

  const QString sql = QStringLiteral(
                          "SELECT TABLE_COMMENT, ENGINE "
                          "FROM INFORMATION_SCHEMA.TABLES "
                          "WHERE TABLE_SCHEMA = '%1' AND TABLE_NAME = '%2'")
                          .arg(escapeSqlLiteral(conn, cfg.database), escapeSqlLiteral(conn, table));

  if (mysql_query(conn, sql.toUtf8().constData()) != 0) {
    FunMgr::setError(QStringLiteral("DB::tableInfo() query failed: %1")
                         .arg(QString::fromUtf8(mysql_error(conn))));
    return accore::AcJsonValue();
  }

  MYSQL_RES *result = mysql_store_result(conn);
  if (!result) {
    FunMgr::setError(QStringLiteral("DB::tableInfo() no result from query"));
    return accore::AcJsonValue();
  }

  accore::AcJsonValue info = accore::AcJsonValue::makeObject();
  MYSQL_ROW row = mysql_fetch_row(result);
  if (row) {
    info.set(QString::fromLatin1(AcDB::kTblComment),
             accore::AcJsonValue(row[0] ? QString::fromUtf8(row[0]) : QString()));
    info.set(QString::fromLatin1(AcDB::kTblEngine),
             accore::AcJsonValue(row[1] ? QString::fromUtf8(row[1]) : QString()));
  } else {
    info.set(QString::fromLatin1(AcDB::kTblComment), accore::AcJsonValue());
    info.set(QString::fromLatin1(AcDB::kTblEngine), accore::AcJsonValue());
  }

  mysql_free_result(result);
  return info;
}

// query — 执行自定义 SQL
accore::AcJsonValue FunDb::query(const accore::AcJsonValue &thisObj,
                                 const accore::AcJsonValue &args) {
  if (!thisObj.isObject() || args.size() == 0 || !args.at(0).isObject()) {
    FunMgr::setError(QStringLiteral("DB::query() requires a DB instance and params object"));
    return accore::AcJsonValue();
  }

  const accore::AcJsonValue params = args.at(0);

  MYSQL *conn = getConnection(thisObj);
  if (!conn) {
    FunMgr::setError(QStringLiteral("DB::query() not connected, call new DB() first"));
    return accore::AcJsonValue();
  }

  const QString sql = params.value(QString::fromLatin1(AcDB::kSql)).toString();
  if (sql.isEmpty()) {
    FunMgr::setError(QStringLiteral("DB::query() requires 'sql' in params"));
    return accore::AcJsonValue();
  }

  if (mysql_query(conn, sql.toUtf8().constData()) != 0) {
    FunMgr::setError(
        QStringLiteral("DB::query() failed: %1").arg(QString::fromUtf8(mysql_error(conn))));
    return accore::AcJsonValue();
  }

  MYSQL_RES *result = mysql_store_result(conn);
  if (!result) {
    return accore::AcJsonValue::makeArray();
  }

  const unsigned int numFields = mysql_num_fields(result);
  MYSQL_FIELD *fields = mysql_fetch_fields(result);

  accore::AcJsonValue rows = accore::AcJsonValue::makeArray();
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result))) {
    accore::AcJsonValue rowObj = accore::AcJsonValue::makeObject();
    for (unsigned int i = 0; i < numFields; ++i) {
      const QString val = row[i] ? QString::fromUtf8(row[i]) : QString();
      rowObj.set(QString::fromUtf8(fields[i].name), accore::AcJsonValue(val));
    }
    rows.append(rowObj);
  }

  mysql_free_result(result);
  return rows;
}
