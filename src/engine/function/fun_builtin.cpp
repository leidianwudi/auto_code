/**
 * @file fun_builtin.cpp
 * @brief 内置函数实现
 */

#include "fun_builtin.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>

#include "../ac_language.h"
#include "../tpl/tpl_engine.h"
#include "fun_mgr.h"
#include "src/util/common/tool_json.h"

// ============================================================================
// 上下文
// ============================================================================

BuiltinContext FunBuiltin::s_ctx;

void FunBuiltin::setContext(const BuiltinContext &ctx) { s_ctx = ctx; }

void FunBuiltin::setCurrentLine(int line) { s_ctx.currentLine = line; }

// ============================================================================
// init — 注册所有内置函数到 FunMgr 的 "builtin" 伪类
// ============================================================================

void FunBuiltin::init() {
  FunMgr::ins().registerFuncs(
      QString::fromLatin1(AcRuntime::kBuiltinClass),
      {
          {QString::fromLatin1(AcBuiltin::kRenderTpl), renderTpl},
          {QString::fromLatin1(AcBuiltin::kReadFile), readFile},
          {QString::fromLatin1(AcBuiltin::kWriteFile), writeFile},
          {QString::fromLatin1(AcBuiltin::kPrintLog), printLog},
          {QString::fromLatin1(AcBuiltin::kPrintError), printError},
          {QString::fromLatin1(AcBuiltin::kGetCheckedFiles), getCheckedFiles},
          {QString::fromLatin1(AcBuiltin::kMerge), merge},
          {QString::fromLatin1(AcBuiltin::kBasename), basename},
      });
}

// ============================================================================
// renderTpl — 渲染模板
// ============================================================================

QJsonValue FunBuiltin::renderTpl(const QJsonArray &args) {
  if (args.size() < 2) {
    FunMgr::setError(
        QStringLiteral("renderTpl() requires 2 arguments: template path and data object"));
    return QJsonValue();
  }

  QString tplPath = args[0].toString();
  QFileInfo tplInfo(tplPath);
  if (tplInfo.isRelative()) tplPath = s_ctx.scriptDir + QStringLiteral("/") + tplPath;

  QFile f(tplPath);
  if (!f.open(QIODevice::ReadOnly)) {
    FunMgr::setError(QStringLiteral("template not found: '%1'").arg(tplPath));
    return QJsonValue();
  }

  TplEngine engine;
  if (s_ctx.logCallback) engine.setLogCallback(s_ctx.logCallback);

  if (!args[1].isObject()) {
    FunMgr::setError(QStringLiteral("renderTpl() second argument must be a data object"));
    return QJsonValue();
  }

  return QJsonValue(engine.render(QString::fromUtf8(f.readAll()), args[1].toObject()));
}

// ============================================================================
// readFile — 读文件（委托 FunFile::read）
// ============================================================================

QJsonValue FunBuiltin::readFile(const QJsonArray &args) {
  // 参数校验：需要文件路径
  if (args.isEmpty() || !args[0].isString()) {
    FunMgr::setError(QStringLiteral("readFile() requires a file path argument"));
    return QJsonValue();
  }

  // 检查文件是否存在
  QString path = args[0].toString();
  if (!QFileInfo::exists(path)) {
    FunMgr::setError(QStringLiteral("file not found: '%1'").arg(path));
    return QJsonValue();
  }
  return FunMgr::ins().call(QString::fromLatin1(AcFile::kClassName),
                            QString::fromLatin1(AcFile::kRead), args);
}

// ============================================================================
// writeFile — 写文件（委托 FunFile::write）
// ============================================================================

QJsonValue FunBuiltin::writeFile(const QJsonArray &args) {
  if (args.size() < 2 || !args[0].isString()) {
    FunMgr::setError(QStringLiteral("writeFile() requires 2 arguments: file path and content"));
    return QJsonValue();
  }
  QJsonValue r = FunMgr::ins().call(QString::fromLatin1(AcFile::kClassName),
                                    QString::fromLatin1(AcFile::kWrite), args);
  if (r.toBool(false) && s_ctx.generatedFiles && !args.isEmpty())
    s_ctx.generatedFiles->append(QDir::toNativeSeparators(args[0].toString()));
  return r;
}

// ============================================================================
// printLog / printError — 打印日志/错误
// ============================================================================

QJsonValue FunBuiltin::printLog(const QJsonArray &args) {
  if (args.isEmpty()) return QJsonValue();

  QString text;
  for (const QJsonValue &v : args) {
    text += v.isString() ? v.toString() : QString::number(v.toDouble());
  }
  qDebug() << "[FunBuiltin::printLog] text:" << text << "hasCallback:" << (bool)s_ctx.logCallback
           << "currentLine:" << s_ctx.currentLine;
  if (s_ctx.logCallback) {
    if (s_ctx.currentLine > 0) {
      text = QStringLiteral("[%1] %2").arg(s_ctx.currentLine).arg(text);
    }
    s_ctx.logCallback(text, false);
  } else {
    qDebug() << "[FunBuiltin::printLog] WARNING: logCallback is null!";
  }

  return QJsonValue(true);
}

QJsonValue FunBuiltin::printError(const QJsonArray &args) {
  if (args.isEmpty()) return QJsonValue();

  QString text;
  for (const QJsonValue &v : args) {
    text += v.isString() ? v.toString() : QString::number(v.toDouble());
  }
  if (s_ctx.logCallback) {
    if (s_ctx.currentLine > 0) {
      text = QStringLiteral("[%1] %2").arg(s_ctx.currentLine).arg(text);
    }
    s_ctx.logCallback(text, true);
  }

  return QJsonValue(true);
}

// ============================================================================
// getCheckedFiles — 获取 tree.config 中勾选的文件列表
// ============================================================================

QJsonValue FunBuiltin::getCheckedFiles(const QJsonArray & /*args*/) {
  QString treePath = s_ctx.rootDir.isEmpty() ? s_ctx.scriptDir + QStringLiteral("/tree.config")
                                             : s_ctx.rootDir + QStringLiteral("/tree.config");
  QJsonArray result;
  QJsonDocument doc = ToolJson::loadFile(treePath);
  if (!doc.isNull()) {
    QJsonArray checked = doc.object().value(QStringLiteral("checked")).toArray();
    for (const QJsonValue &v : checked) {
      if (v.isString())
        result.append(QDir::cleanPath(s_ctx.rootDir.isEmpty()
                                          ? s_ctx.scriptDir
                                          : s_ctx.rootDir + QStringLiteral("/") + v.toString()));
    }
  }
  return result;
}

// ============================================================================
// merge — 合并两个 JSON 对象
// ============================================================================

QJsonValue FunBuiltin::merge(const QJsonArray &args) {
  if (args.size() < 2) {
    FunMgr::setError(
        QStringLiteral("merge() requires 2 arguments: target object and source object"));
    return QJsonValue();
  }

  QJsonObject result = args[0].toObject();
  QJsonObject ob = args[1].toObject();
  for (auto it = ob.begin(); it != ob.end(); ++it) result[it.key()] = it.value();

  return result;
}

// ============================================================================
// basename — 获取文件名（不含扩展名）
// ============================================================================

QJsonValue FunBuiltin::basename(const QJsonArray &args) {
  if (args.isEmpty() || !args[0].isString()) {
    FunMgr::setError(QStringLiteral("basename() requires a file path argument"));
    return QJsonValue();
  }

  return QJsonValue(QFileInfo(args[0].toString()).completeBaseName());
}