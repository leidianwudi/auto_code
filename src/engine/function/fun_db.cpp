/**
 * @file fun_db.cpp
 * @brief 数据库函数实现 — 通过 MySQL C API 直连 MySQL
 *
 * 直接使用 libmysql.dll（捆绑在 exe 目录），无需 ODBC 或 Qt SQL 插件。
 */

#include "fun_db.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>


#include <mysql.h>

QJsonValue FunDb::execute(const QJsonArray &args) {
  if (args.isEmpty() || !args[0].isObject())
    return QJsonValue();

  const QJsonObject cfg = args[0].toObject();

  const QString host = cfg.value(QStringLiteral("host")).toString();
  const int port = cfg.value(QStringLiteral("port")).toInt(3306);
  const QString user = cfg.value(QStringLiteral("user")).toString();
  const QString pass = cfg.value(QStringLiteral("password")).toString();
  const QString dbName = cfg.value(QStringLiteral("database")).toString();
  const QString table = cfg.value(QStringLiteral("table")).toString();

  if (host.isEmpty() || user.isEmpty() || dbName.isEmpty() || table.isEmpty())
    return QJsonValue();

  // ── 连接 MySQL ──
  MYSQL *conn = mysql_init(nullptr);
  if (!conn)
    return QJsonValue();

  // 设置连接超时 5 秒
  unsigned int timeout = 5;
  mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

  if (!mysql_real_connect(conn, host.toUtf8().constData(),
                          user.toUtf8().constData(), pass.toUtf8().constData(),
                          dbName.toUtf8().constData(), port, nullptr, 0)) {
    mysql_close(conn);
    return QJsonValue();
  }

  // ── 设置字符集 ──
  mysql_set_character_set(conn, "utf8mb4");

  // ── 查询 INFORMATION_SCHEMA.COLUMNS ──
  const QString sql =
      QStringLiteral("SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, "
                     "       COLUMN_KEY, COLUMN_DEFAULT, EXTRA, COLUMN_COMMENT "
                     "FROM INFORMATION_SCHEMA.COLUMNS "
                     "WHERE TABLE_SCHEMA = '%1' AND TABLE_NAME = '%2' "
                     "ORDER BY ORDINAL_POSITION")
          .arg(dbName, table);

  QJsonArray columns;

  if (mysql_query(conn, sql.toUtf8().constData()) != 0) {
    mysql_close(conn);
    return QJsonValue();
  }

  MYSQL_RES *result = mysql_store_result(conn);
  if (!result) {
    mysql_close(conn);
    return QJsonValue();
  }

  // ── 遍历结果集 ──
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result))) {
    QJsonObject col;
    unsigned long *lengths = mysql_fetch_lengths(result);

    // COLUMN_NAME
    col[QStringLiteral("name")] =
        row[0] ? QString::fromUtf8(row[0]) : QString();

    // COLUMN_TYPE
    col[QStringLiteral("type")] =
        row[1] ? QString::fromUtf8(row[1]) : QString();

    // IS_NULLABLE
    col[QStringLiteral("nullable")] =
        row[2] && QString::fromUtf8(row[2]).toUpper() == QStringLiteral("YES");

    // COLUMN_KEY
    col[QStringLiteral("key")] = row[3] ? QString::fromUtf8(row[3]) : QString();

    // COLUMN_DEFAULT
    if (row[4]) {
      col[QStringLiteral("default")] = QJsonValue(QString::fromUtf8(row[4]));
    } else {
      col[QStringLiteral("default")] = QJsonValue();
    }

    // EXTRA
    col[QStringLiteral("extra")] =
        row[5] ? QString::fromUtf8(row[5]) : QString();

    // COLUMN_COMMENT
    col[QStringLiteral("comment")] =
        row[6] ? QString::fromUtf8(row[6]) : QString();

    columns.append(col);
  }

  // ── 清理 ──
  mysql_free_result(result);
  mysql_close(conn);

  return QJsonValue(columns);
}