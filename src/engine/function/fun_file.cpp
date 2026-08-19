/**
 * @file fun_file.cpp
 * @brief 文件读写函数实现
 */

#include "fun_file.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QString>
#include <QTextStream>

#include "../ac_language.h"
#include "fun_mgr.h"

void FunFile::init() {
  FunMgr::ins().registerFuncs(QString::fromLatin1(AcFile::kClassName),
                              {
                                  {QString::fromLatin1(AcFile::kRead), read},
                                  {QString::fromLatin1(AcFile::kWrite), write},
                              });
}

// ============================================================================
// read — 读文件（UTF-8）
// ============================================================================

QJsonValue FunFile::read(const QJsonArray &args) {
  // 原生类实例方法约定 args[0]=对象实例（见 evalClassMethod）；
  // 一级函数 readFile 委托调用时不带实例，args[0] 即为路径 → 自适应两种形态
  const int base = (!args.isEmpty() && args[0].isObject()) ? 1 : 0;
  // 参数校验：需要一个文件路径字符串参数
  if (args.size() <= base || !args[base].isString()) {
    FunMgr::setError(QStringLiteral("File::read() requires a file path argument"));
    return QJsonValue();
  }

  const QString path = args[base].toString();

  // 打开文件，只读模式
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    FunMgr::setError(QStringLiteral("File::read() cannot open file: '%1'").arg(path));
    return QJsonValue();
  }

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
  // 原生类实例方法约定 args[0]=对象实例（见 evalClassMethod）；
  // 一级函数 writeFile 委托调用时不带实例 → 自适应两种形态
  const int base = (!args.isEmpty() && args[0].isObject()) ? 1 : 0;
  // 参数校验：需要 path 和 content 两个字符串参数
  if (args.size() < base + 2 || !args[base].isString() || !args[base + 1].isString()) {
    FunMgr::setError(QStringLiteral("File::write() requires 2 arguments: file path and content"));
    return QJsonValue();
  }

  const QString path = args[base].toString();
  const QString content = args[base + 1].toString();

  QDir().mkpath(QFileInfo(path).absolutePath());

  // 打开文件，只写模式
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    FunMgr::setError(QStringLiteral("File::write() cannot open file for writing: '%1'").arg(path));
    return QJsonValue();
  }

  // UTF-8 编码写入，强制使用 Unix 换行符 (\n)
  QTextStream out(&file);
  out.setEncoding(QStringConverter::Utf8);
  out.setGenerateByteOrderMark(false);

  QString cleanContent = content;
  cleanContent.remove(QLatin1Char('\r'));

  out << cleanContent;
  file.close();

  return QJsonValue(true);
}