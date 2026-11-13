#include "table_database_service.h"

#include "src/engine/function/fun_db.h"
#include "src/engine/function/fun_mgr.h"
#include "table_schema_service.h"
using TableSchema::quote;
namespace {
bool connectDb(const JsonTableConfig &config, accore::AcJsonValue *instance, QString *error) {
  accore::AcJsonValue cfg = accore::AcJsonValue::makeObject();
  cfg.set(QString::fromLatin1(AcDB::kHost), accore::AcJsonValue(config.dbHost));
  cfg.set(QString::fromLatin1(AcDB::kPort), accore::AcJsonValue(config.dbPort));
  cfg.set(QString::fromLatin1(AcDB::kUser), accore::AcJsonValue(config.dbUser));
  cfg.set(QString::fromLatin1(AcDB::kPassword), accore::AcJsonValue(config.dbPassword));
  cfg.set(QString::fromLatin1(AcDB::kDatabase), accore::AcJsonValue(config.dbDatabase));
  accore::AcJsonValue args = accore::AcJsonValue::makeArray();
  args.append(cfg);
  *instance = FunDb::constructor(args);
  if (!instance->isObject() ||
      !instance->value(QString::fromLatin1(AcDB::kConnected)).toBool(false)) {
    if (error) *error = FunMgr::takeError();
    return false;
  }
  return true;
}
/// 数据库连接只在本次任务的线程/作用域中存活，所有提前返回都自动释放。
struct DbSession {
  accore::AcJsonValue instance;
  DbSession(const JsonTableConfig &config, QString *error) {
    FunMgr::init();
    FunMgr::takeError();
    if (!connectDb(config, &instance, error) && error && error->isEmpty())
      *error = QStringLiteral("数据库连接失败");
  }
  ~DbSession() { FunDb::destructor(instance, accore::AcJsonValue::makeArray()); }
};
}  // namespace

DatabaseTableSnapshot TableDatabaseService::readTable(const JsonTableConfig &config,
                                                      const QString &table) {
  accore::AcJsonValue inst;
  DatabaseTableSnapshot result;
  DbSession session(config, &result.error);
  inst = session.instance;
  if (!result.error.isEmpty()) return result;

  // tableSchema / tableInfo 共用 {table} 参数
  accore::AcJsonValue args = accore::AcJsonValue::makeArray();
  accore::AcJsonValue params = accore::AcJsonValue::makeObject();
  params.set(QString::fromLatin1(AcDB::kTable), accore::AcJsonValue(table));
  args.append(params);

  const accore::AcJsonValue schema = FunDb::tableSchema(inst, args);
  if (!schema.isArray()) {
    result.error = FunMgr::takeError();
    if (result.error.isEmpty()) result.error = QStringLiteral("读取表结构失败（表可能不存在）");
    return result;
  }

  // 数据库列 → JsonTableColumn（纯物理属性）
  QVector<JsonTableColumn> dbColumns;
  for (const auto &v : schema.items()) {
    JsonTableColumn col;
    col.name = v.value(QString::fromLatin1(AcDB::kColName)).toString();
    // COLUMN_TYPE 形如 "bigint unsigned"：拆出 unsigned 标记后存纯类型
    QString type = v.value(QString::fromLatin1(AcDB::kColType)).toString().trimmed();
    if (type.endsWith(QStringLiteral("unsigned"), Qt::CaseInsensitive)) {
      col.isUnsigned = true;
      type.chop(QStringLiteral("unsigned").size());
      type = type.trimmed();
    }
    col.mysqlType = type;
    col.nullable = v.value(QString::fromLatin1(AcDB::kColNullable)).toBool(false);
    col.isPrimary = v.value(QString::fromLatin1(AcDB::kColKey)).toString() == QStringLiteral("PRI");
    col.isAutoInc = v.value(QString::fromLatin1(AcDB::kColExtra))
                        .toString()
                        .contains(QStringLiteral("auto_increment"));
    // default 兼容数字字面量（Qt6 的 toString 不转换数值，需显式处理）
    const accore::AcJsonValue dv = v.value(QString::fromLatin1(AcDB::kColDefault));
    if (dv.isString()) {
      col.defaultValue = dv.toString();
    } else if (dv.isDouble()) {
      col.defaultValue = QString::number(dv.toDouble());
    }
    col.comment = v.value(QString::fromLatin1(AcDB::kColComment)).toString();
    dbColumns.append(col);
  }

  FunMgr::takeError();

  // 表注释
  QString dbComment;
  const accore::AcJsonValue info = FunDb::tableInfo(inst, args);
  if (info.isObject()) {
    dbComment = info.value(QString::fromLatin1(AcDB::kTblComment)).toString();
  }

  FunMgr::takeError();

  // 索引（INFORMATION_SCHEMA.STATISTICS；PRIMARY 由主键列维护，跳过）
  QVector<JsonTableIndex> dbIndexes;
  bool indexQueryOk = false;
  {
    accore::AcJsonValue idxArgs = accore::AcJsonValue::makeArray();
    accore::AcJsonValue idxParams = accore::AcJsonValue::makeObject();
    idxParams.set(QString::fromLatin1(AcDB::kSql),
                  accore::AcJsonValue(
                      QStringLiteral("SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME "
                                     "FROM INFORMATION_SCHEMA.STATISTICS "
                                     "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = %1 "
                                     "ORDER BY INDEX_NAME, SEQ_IN_INDEX")
                          .arg(quote(table))));
    idxArgs.append(idxParams);
    const accore::AcJsonValue idxRows = FunDb::query(inst, idxArgs);
    indexQueryOk = idxRows.isArray();
    if (indexQueryOk) {
      QHash<QString, int> indexPositions;
      for (const auto &row : idxRows.items()) {
        const QString name = row.value(QStringLiteral("INDEX_NAME")).toString();
        if (name.isEmpty() || name.compare(QStringLiteral("PRIMARY"), Qt::CaseInsensitive) == 0) {
          continue;
        }
        const QString column = row.value(QStringLiteral("COLUMN_NAME")).toString();
        if (column.isEmpty()) continue;  // 函数索引等无列名条目
        const QString key = name.toLower();
        int position = indexPositions.value(key, -1);
        if (position < 0) {
          position = dbIndexes.size();
          indexPositions.insert(key, position);
          JsonTableIndex index;
          index.name = name;
          index.unique = row.value(QStringLiteral("NON_UNIQUE")).toString() == QStringLiteral("0");
          dbIndexes.append(index);
        }
        dbIndexes[position].cols << column;
      }
    }
  }

  FunMgr::takeError();
  result.columns = dbColumns;
  result.indexes = dbIndexes;
  result.comment = dbComment;
  result.indexQueryOk = indexQueryOk;
  return result;
}

DatabaseWriteResult TableDatabaseService::executeDdl(const JsonTableConfig &config,
                                                     const QStringList &sqls) {
  DatabaseWriteResult result;
  DbSession session(config, &result.error);
  if (!result.error.isEmpty()) return result;
  for (const QString &sql : sqls) {
    auto args = accore::AcJsonValue::makeArray();
    auto params = accore::AcJsonValue::makeObject();
    params.set(QString::fromLatin1(AcDB::kSql), accore::AcJsonValue(sql));
    args.append(params);
    const auto response = FunDb::exec(session.instance, args);
    if (!response.isDouble()) {
      result.error = FunMgr::takeError();
      if (result.error.isEmpty()) result.error = QStringLiteral("执行 DDL 失败");
      return result;
    }
    ++result.completedCount;
  }
  return result;
}

QString TableDatabaseService::testConnection(const JsonTableConfig &config) {
  QString error;
  DbSession session(config, &error);
  return error;
}
