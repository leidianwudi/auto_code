/**
 * @file fun_builtin.cpp
 * @brief 内置函数实现
 */

#include "fun_builtin.h"
#include "../ac_language.h"
#include "../tpl/tpl_engine.h"
#include "fun_mgr.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

// ============================================================================
// 上下文
// ============================================================================

BuiltinContext FunBuiltin::s_ctx;

void FunBuiltin::setContext(const BuiltinContext &ctx) { s_ctx = ctx; }

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
          {QString::fromLatin1(AcBuiltin::kPrint), print},
          {QString::fromLatin1(AcBuiltin::kGetCheckedFiles), getCheckedFiles},
          {QString::fromLatin1(AcBuiltin::kMerge), merge},
          {QString::fromLatin1(AcBuiltin::kBasename), basename},
      });
}

// ============================================================================
// renderTpl — 渲染模板
// ============================================================================

QJsonValue FunBuiltin::renderTpl(const QJsonArray &args) {
  if (args.size() < 2)
    return QJsonValue();

  QString tplPath = args[0].toString();
  QFileInfo tplInfo(tplPath);
  if (tplInfo.isRelative())
    tplPath = s_ctx.scriptDir + QStringLiteral("/") + tplPath;

  QFile f(tplPath);
  if (!f.open(QIODevice::ReadOnly))
    return QJsonValue();

  TplEngine engine;
  if (s_ctx.logCallback)
    engine.setLogCallback(s_ctx.logCallback);

  if (!args[1].isObject())
    return QJsonValue();

  return QJsonValue(
      engine.render(QString::fromUtf8(f.readAll()), args[1].toObject()));
}

// ============================================================================
// readFile — 读文件（委托 FunFile::read）
// ============================================================================

QJsonValue FunBuiltin::readFile(const QJsonArray &args) {
  return FunMgr::ins().call(QString::fromLatin1(AcFile::kClassName), QString::fromLatin1(AcFile::kRead),
                            args);
}

// ============================================================================
// writeFile — 写文件（委托 FunFile::write）
// ============================================================================

QJsonValue FunBuiltin::writeFile(const QJsonArray &args) {
  QJsonValue r =
      FunMgr::ins().call(QString::fromLatin1(AcFile::kClassName), QString::fromLatin1(AcFile::kWrite), args);
  if (r.toBool(false) && s_ctx.generatedFiles && !args.isEmpty())
    s_ctx.generatedFiles->append(QDir::toNativeSeparators(args[0].toString()));
  return r;
}

// ============================================================================
// print — 打印日志
// ============================================================================

QJsonValue FunBuiltin::print(const QJsonArray &args) {
  if (args.isEmpty())
    return QJsonValue();

  QString text = args[0].toString();
  if (s_ctx.logCallback)
    s_ctx.logCallback(text, false);

  return QJsonValue(true);
}

// ============================================================================
// getCheckedFiles — 获取 tree.config 中勾选的文件列表
// ============================================================================

QJsonValue FunBuiltin::getCheckedFiles(const QJsonArray & /*args*/) {
  QString treePath = s_ctx.rootDir.isEmpty()
                         ? s_ctx.scriptDir + QStringLiteral("/tree.config")
                         : s_ctx.rootDir + QStringLiteral("/tree.config");
  QFile f(treePath);
  QJsonArray result;
  if (f.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    QJsonArray checked =
        doc.object().value(QStringLiteral("checked")).toArray();
    for (const QJsonValue &v : checked) {
      if (v.isString())
        result.append(QDir::cleanPath(
            s_ctx.rootDir.isEmpty()
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
  if (args.size() < 2)
    return QJsonValue();

  QJsonObject result = args[0].toObject();
  QJsonObject ob = args[1].toObject();
  for (auto it = ob.begin(); it != ob.end(); ++it)
    result[it.key()] = it.value();

  return result;
}

// ============================================================================
// basename — 获取文件名（不含扩展名）
// ============================================================================

QJsonValue FunBuiltin::basename(const QJsonArray &args) {
  if (args.isEmpty())
    return QJsonValue();

  return QJsonValue(QFileInfo(args[0].toString()).completeBaseName());
}