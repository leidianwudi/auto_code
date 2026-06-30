/**
 * @file fun_db.cpp
 * @brief 数据库函数实现 — 通过 MySQL C API 直连 MySQL
 *
 * 直接使用 libmysql.dll（捆绑在 exe 目录），无需 ODBC 或 Qt SQL 插件。
 */

#include "fun_db.h"
#include "fun_const.h"
#include "fun_mgr.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <mysql.h>

MYSQL *FunDb::s_conn = nullptr;

// ============================================================================
// init — 注册所有数据库函数到 FunMgr
// ============================================================================

void FunDb::init() {
  FunMgr::ins().registerFuncs(
      QString::fromLatin1(FunConst::kClsDb),
      {
          {QString::fromLatin1(FunConst::kTableSchema), tableSchema},
          {QString::fromLatin1(FunConst::kQuery), query},
      });
}

// ============================================================================
// cleanup — 关闭持久连接
// ============================================================================

void FunDb::cleanup() {
  if (s_conn) {
    mysql_close(s_conn);
    s_conn = nullptr;
  }
}

// ============================================================================
// getConnection — 建立/获取持久连接
// ============================================================================

MYSQL *FunDb::getConnection(const QJsonObject &cfg) {
  // 已有持久连接则直接复用
  if (s_conn)
    return s_conn;

  const QString host = cfg.value(QStringLiteral("host")).toString();
  const int port = cfg.value(QStringLiteral("port")).toInt(3306);
  const QString user = cfg.value(QStringLiteral("user")).toString();
  const QString pass = cfg.value(QStringLiteral("password")).toString();
  const QString dbName = cfg.value(QStringLiteral("database")).toString();

  if (host.isEmpty() || user.isEmpty() || dbName.isEmpty())
    return nullptr;

  MYSQL *conn = mysql_init(nullptr);
  if (!conn)
    return nullptr;

  unsigned int timeout = 5;
  mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

  if (!mysql_real_connect(conn, host.toUtf8().constData(),
                          user.toUtf8().constData(), pass.toUtf8().constData(),
                          dbName.toUtf8().constData(), port, nullptr, 0)) {
    mysql_close(conn);
    return nullptr;
  }

  mysql_set_character_set(conn, "utf8mb4");
  s_conn = conn;
  return conn;
}

// ============================================================================
// tableSchema — 获取表列信息
// ============================================================================

QJsonValue FunDb::tableSchema(const QJsonArray &args) {
  if (args.isEmpty() || !args[0].isObject())
    return QJsonValue();

  const QJsonObject cfg = args[0].toObject();

  MYSQL *conn = getConnection(cfg);
  if (!conn)
    return QJsonValue();

  const QString dbName = cfg.value(QStringLiteral("database")).toString();
  const QString table = cfg.value(QStringLiteral("table")).toString();
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

// ============================================================================
// query — 执行自定义 SQL
// ============================================================================

QJsonValue FunDb::query(const QJsonArray &args) {
  if (args.isEmpty() || !args[0].isObject())
    return QJsonValue();

  const QJsonObject cfg = args[0].toObject();

  MYSQL *conn = getConnection(cfg);
  if (!conn)
    return QJsonValue();

  const QString sql = cfg.value(QStringLiteral("sql")).toString();
  if (sql.isEmpty())
    return QJsonValue();

  if (mysql_query(conn, sql.toUtf8().constData()) != 0)
    return QJsonValue();

  MYSQL_RES *result = mysql_store_result(conn);
  if (!result) {
    // 非查询语句（INSERT/UPDATE/DELETE）无结果集
    return QJsonValue(QJsonArray{});
  }

  // ── 读取列名 ──
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