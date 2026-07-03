/**
 * @file fun_file.cpp
 * @brief 文件读写函数实现
 */

#include "fun_file.h"
#include "fun_const.h"
#include "fun_mgr.h"

#include <QFile>
#include <QJsonArray>
#include <QString>
#include <QTextStream>

void FunFile::init() {
  FunMgr::ins().registerFuncs(
      QString::fromLatin1(FunConst::kClsFile),
      {
          {QString::fromLatin1(FunConst::kRead), read},
          {QString::fromLatin1(FunConst::kWrite), write},
      });
}

// ============================================================================
// read — 读文件（UTF-8）
// ============================================================================

QJsonValue FunFile::read(const QJsonArray &args) {
  // 参数校验：需要一个文件路径字符串参数
  if (args.isEmpty() || !args[0].isString())
    return QJsonValue();

  const QString path = args[0].toString();

  // 打开文件，只读模式
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return QJsonValue();

  // UTF-8 编码读取全部内容
  QTextStream in(&file);
  in.setEncoding(QStringConverter::Utf8);
  const QString content = in.readAll();
  file.close();

  return QJsonValue(content);
}

// ============================================================================
// write — 写文件（UTF-8）
// ============================================================================

QJsonValue FunFile::write(const QJsonArray &args) {
  // 参数校验：需要 path 和 content 两个字符串参数
  if (args.size() < 2 || !args[0].isString() || !args[1].isString())
    return QJsonValue();

  const QString path = args[0].toString();
  const QString content = args[1].toString();

  // 打开文件，只写模式
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return QJsonValue();

  // UTF-8 编码写入
  QTextStream out(&file);
  out.setEncoding(QStringConverter::Utf8);
  out << content;
  file.close();

  return QJsonValue(true);
}