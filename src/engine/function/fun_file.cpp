/**
 * @file fun_file.cpp
 * @brief 文件读写函数实现
 */

#include "fun_file.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QTextStream>

#include "../ac_language.h"
#include "fun_args.h"
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

accore::AcJsonValue FunFile::read(const accore::AcJsonValue &args) {
  // 参数校验：需要一个文件路径字符串参数
  if (!FunArgs::requireString(args, 0,
                              QStringLiteral("File::read() requires a file path argument")))
    return accore::AcJsonValue();

  const QString path = args.at(0).toString();

  // 打开文件，只读模式
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    FunMgr::setError(QStringLiteral("File::read() cannot open file: '%1'").arg(path));
    return accore::AcJsonValue();
  }

  // UTF-8 编码读取全部内容
  QTextStream in(&file);
  in.setEncoding(QStringConverter::Utf8);
  const QString content = in.readAll();
  file.close();

  return accore::AcJsonValue(content);
}

// ============================================================================
// write — 写文件（UTF-8）
// ============================================================================

accore::AcJsonValue FunFile::write(const accore::AcJsonValue &args) {
  // 参数校验：需要 path 和 content 两个字符串参数
  const QString writeErr =
      QStringLiteral("File::write() requires 2 arguments: file path and content");
  if (!FunArgs::requireCount(args, 2, writeErr) || !FunArgs::requireString(args, 0, writeErr) ||
      !FunArgs::requireString(args, 1, writeErr))
    return accore::AcJsonValue();

  const QString path = args.at(0).toString();
  const QString content = args.at(1).toString();

  QDir().mkpath(QFileInfo(path).absolutePath());

  // 打开文件，只写模式
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    FunMgr::setError(QStringLiteral("File::write() cannot open file for writing: '%1'").arg(path));
    return accore::AcJsonValue();
  }

  // UTF-8 编码写入，强制使用 Unix 换行符 (\n)
  QTextStream out(&file);
  out.setEncoding(QStringConverter::Utf8);
  out.setGenerateByteOrderMark(false);

  QString cleanContent = content;
  cleanContent.remove(QLatin1Char('\r'));

  out << cleanContent;
  file.close();

  return accore::AcJsonValue(true);
}
