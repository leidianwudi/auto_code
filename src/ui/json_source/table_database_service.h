/** @file table_database_service.h
 * @brief 后台数据库结构读取和建表服务；结果不持有界面或数据库句柄。
 */
#pragma once
#include "json_table_model.h"

/// 一次结构读取的快照；索引失败时保留旧索引，不将查询失败视作数据库删除。
struct DatabaseTableSnapshot {
  QVector<JsonTableColumn> columns;
  QVector<JsonTableIndex> indexes;
  QString comment;
  QString error;
  bool indexQueryOk = false;
};

/// MySQL DDL 可能部分完成，明确返回成功条数与错误，不承诺自动回滚。
struct DatabaseWriteResult {
  int completedCount = 0;
  QString error;
};

class TableDatabaseService {
public:
  /// 测试连接并在返回前释放句柄；成功返回空错误。
  static QString testConnection(const JsonTableConfig &config);

  /// 在调用线程内建立和释放连接；调用方应通过后台任务执行。
  static DatabaseTableSnapshot readTable(const JsonTableConfig &config, const QString &table);
  /// 顺序执行已确认的 DDL，首个失败后停止。
  static DatabaseWriteResult executeDdl(const JsonTableConfig &config, const QStringList &sqls);
};
