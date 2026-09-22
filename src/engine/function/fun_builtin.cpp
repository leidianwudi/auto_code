/**
 * @file fun_builtin.cpp
 * @brief 内置函数实现
 */

#include "fun_builtin.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "../ac_language.h"
#include "../ac_value_str.h"
#include "../tpl/tpl_engine.h"
#include "fun_args.h"
#include "fun_mgr.h"
#include "src/util/common/code_constants.h"
#include "src/util/common/util_json.h"

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
          {QString::fromLatin1(AcBuiltin::kScriptDir), scriptDir},
          {QString::fromLatin1(AcBuiltin::kMerge), merge},
          {QString::fromLatin1(AcBuiltin::kBasename), basename},
          {QString::fromLatin1(AcBuiltin::kFileName), fileName},
          {QString::fromLatin1(AcBuiltin::kFileExists), fileExists},
          {QString::fromLatin1(AcBuiltin::kListFiles), listFiles},
          {QString::fromLatin1(AcBuiltin::kFormatPath), formatPath},
          {QString::fromLatin1(AcBuiltin::kAssert), assertFn},
      });
}

// ============================================================================
// renderTpl — 渲染模板
// ============================================================================

accore::AcJsonValue FunBuiltin::renderTpl(const accore::AcJsonValue &args) {
  if (!FunArgs::requireCount(
          args, 2,
          QStringLiteral("renderTpl() requires 2 arguments: template path and data object")))
    return accore::AcJsonValue();

  QString tplPath = args.at(0).toString();
  QFileInfo tplInfo(tplPath);
  if (tplInfo.isRelative()) tplPath = s_ctx.scriptDir + QStringLiteral("/") + tplPath;

  QFile f(tplPath);
  if (!f.open(QIODevice::ReadOnly)) {
    FunMgr::setError(QStringLiteral("template not found: '%1'").arg(tplPath));
    return accore::AcJsonValue();
  }
  QString tplContent = QString::fromUtf8(f.readAll());

  TplEngine engine;
  if (s_ctx.logCallback) engine.setLogCallback(s_ctx.logCallback);

  if (!args.at(1).isObject()) {
    FunMgr::setError(QStringLiteral("renderTpl() second argument must be a data object"));
    return accore::AcJsonValue();
  }

  // TplEngine 已迁 accore：参数直达，零转换
  QString result = engine.render(tplContent, args.at(1));
  // 渲染错误必须传播到 FunMgr，否则脚本层只收到空串而看不到失败原因
  if (!engine.lastError().isEmpty()) {
    FunMgr::setError(engine.lastError());
    return accore::AcJsonValue();
  }
  return accore::AcJsonValue(result);
}

// ============================================================================
// readFile — 读文件（委托 FunFile::read）
// ============================================================================

accore::AcJsonValue FunBuiltin::readFile(const accore::AcJsonValue &args) {
  // 参数校验：需要文件路径
  if (!FunArgs::requireString(args, 0, QStringLiteral("readFile() requires a file path argument")))
    return accore::AcJsonValue();

  // 检查文件是否存在
  QString path = args.at(0).toString();
  if (!QFileInfo::exists(path)) {
    FunMgr::setError(QStringLiteral("file not found: '%1'").arg(path));
    return accore::AcJsonValue();
  }
  return FunMgr::ins().call(QString::fromLatin1(AcFile::kClassName),
                            QString::fromLatin1(AcFile::kRead), args);
}

// ============================================================================
// writeFile — 写文件（委托 FunFile::write）
// ============================================================================

accore::AcJsonValue FunBuiltin::writeFile(const accore::AcJsonValue &args) {
  if (!FunArgs::requireCount(
          args, 2, QStringLiteral("writeFile() requires 2 arguments: file path and content")) ||
      !FunArgs::requireString(
          args, 0, QStringLiteral("writeFile() requires 2 arguments: file path and content")))
    return accore::AcJsonValue();
  accore::AcJsonValue r = FunMgr::ins().call(QString::fromLatin1(AcFile::kClassName),
                                             QString::fromLatin1(AcFile::kWrite), args);
  if (r.toBool(false) && s_ctx.generatedFiles && args.size() > 0)
    s_ctx.generatedFiles->append(QDir::toNativeSeparators(args.at(0).toString()));
  return r;
}

// ============================================================================
// printLog / printError — 打印日志/错误
// ============================================================================

accore::AcJsonValue FunBuiltin::printLog(const accore::AcJsonValue &args) {
  if (args.size() == 0) return accore::AcJsonValue();

  QString text;
  for (const accore::AcJsonValue &v : args.items()) {
    if (v.isString()) {
      text += v.toString();
    } else {
      text += AcValueStr::toString(v);
    }
  }
#ifdef AC_DEBUG
  qDebug() << "[FunBuiltin::printLog] text:" << text << "hasCallback:" << (bool)s_ctx.logCallback
           << "currentLine:" << s_ctx.currentLine;
#endif
  if (s_ctx.logCallback) {
    if (s_ctx.currentLine > 0) {
      text = QStringLiteral("[%1] %2").arg(s_ctx.currentLine).arg(text);
    }
    s_ctx.logCallback(text, false);
  }
#ifdef AC_DEBUG
  else {
    qDebug() << "[FunBuiltin::printLog] WARNING: logCallback is null!";
  }
#endif

  return accore::AcJsonValue(true);
}

accore::AcJsonValue FunBuiltin::printError(const accore::AcJsonValue &args) {
  if (args.size() == 0) return accore::AcJsonValue();

  QString text;
  for (const accore::AcJsonValue &v : args.items()) {
    if (v.isString()) {
      text += v.toString();
    } else {
      text += AcValueStr::toString(v);
    }
  }
  if (s_ctx.logCallback) {
    if (s_ctx.currentLine > 0) {
      text = QStringLiteral("[%1] %2").arg(s_ctx.currentLine).arg(text);
    }
    s_ctx.logCallback(text, true);
  }

  return accore::AcJsonValue(true);
}

// ============================================================================
// getCheckedFiles — 获取 tree.config 中勾选的文件列表
// ============================================================================

accore::AcJsonValue FunBuiltin::getCheckedFiles(const accore::AcJsonValue &args) {
  QString treePath =
      s_ctx.rootDir.isEmpty()
          ? s_ctx.scriptDir + QString::fromUtf8(CodeConstants::Paths::kTreeConfigFile)
          : s_ctx.rootDir + QString::fromUtf8(CodeConstants::Paths::kTreeConfigFile);
  // 可选参数：基准路径，传入后只返回该路径下的文件
  QString basePath;
  if (args.size() > 0 && args.at(0).isString()) {
    basePath = QDir::cleanPath(args.at(0).toString());
  }

  accore::AcJsonValue result = accore::AcJsonValue::makeArray();
  QJsonDocument doc = UtilJson::loadFile(treePath);
  if (!doc.isNull()) {
    QJsonArray checked = doc.object().value(QStringLiteral("checked")).toArray();
    for (const QJsonValue &v : checked) {
      if (!v.isString()) continue;
      QString absPath = QDir::cleanPath(s_ctx.rootDir.isEmpty()
                                            ? s_ctx.scriptDir
                                            : s_ctx.rootDir + QStringLiteral("/") + v.toString());
      // 指定基准路径时，只保留该路径下的文件（避免处理其它项目的勾选文件）。
      // 统一用 '/' 规范化分隔符后比较，避免 Windows 下 cleanPath 返回 '\' 与
      // 字符串拼接的 '/' 不一致导致过滤失效。
      if (!basePath.isEmpty()) {
        QString normBase = basePath;
        normBase.replace(QLatin1Char('\\'), QLatin1Char('/'));
        if (!normBase.endsWith(QLatin1Char('/'))) normBase += QLatin1Char('/');
        // 基准目录本身的精确匹配形式（不带尾部分隔符）
        QString normBaseExact = normBase;
        normBaseExact.chop(1);
        QString normAbs = absPath;
        normAbs.replace(QLatin1Char('\\'), QLatin1Char('/'));
        if (normAbs != normBaseExact && !normAbs.startsWith(normBase)) {
          continue;
        }
      }
      result.append(absPath);
    }
  }
  return result;
}

// ============================================================================
// scriptDir — 获取当前 .ac 脚本所在目录
// ============================================================================

accore::AcJsonValue FunBuiltin::scriptDir(const accore::AcJsonValue & /*args*/) {
  return accore::AcJsonValue(s_ctx.scriptDir);
}

// ============================================================================
// merge — 合并两个 JSON 对象
// ============================================================================

accore::AcJsonValue FunBuiltin::merge(const accore::AcJsonValue &args) {
  if (!FunArgs::requireCount(
          args, 2, QStringLiteral("merge() requires 2 arguments: target object and source object")))
    return accore::AcJsonValue();

  accore::AcJsonValue result = args.at(0);
  const accore::AcJsonValue ob = args.at(1);
  for (const auto &m : ob.members()) result.set(m.key, m.value);

  return result;
}

// ============================================================================
// basename — 获取文件名（不含扩展名）
// ============================================================================

accore::AcJsonValue FunBuiltin::basename(const accore::AcJsonValue &args) {
  if (!FunArgs::requireString(args, 0, QStringLiteral("basename() requires a file path argument")))
    return accore::AcJsonValue();

  return accore::AcJsonValue(QFileInfo(args.at(0).toString()).completeBaseName());
}

// ============================================================================
// fileName — 获取文件名（含扩展名）
// ============================================================================

accore::AcJsonValue FunBuiltin::fileName(const accore::AcJsonValue &args) {
  if (!FunArgs::requireString(args, 0, QStringLiteral("fileName() requires a file path argument")))
    return accore::AcJsonValue();

  return accore::AcJsonValue(QFileInfo(args.at(0).toString()).fileName());
}

// ============================================================================
// fileExists — 判断文件/目录是否存在（脚本侧与模板侧同名函数语义一致）
// ============================================================================

accore::AcJsonValue FunBuiltin::fileExists(const accore::AcJsonValue &args) {
  if (!FunArgs::requireString(args, 0,
                              QStringLiteral("fileExists() requires a file path argument")))
    return accore::AcJsonValue();

  return accore::AcJsonValue(QFileInfo::exists(args.at(0).toString()));
}

// ============================================================================
// listFiles — 列出目录下匹配后缀的文件名数组（不递归，按名称排序）
// ============================================================================

accore::AcJsonValue FunBuiltin::listFiles(const accore::AcJsonValue &args) {
  if (!FunArgs::requireString(args, 0,
                              QStringLiteral("listFiles() requires a directory path argument")))
    return accore::AcJsonValue();

  const QString dirPath = args.at(0).toString();
  // 后缀可选：省略/为空返回全部文件
  QString suffix;
  if (args.size() > 1 && args.at(1).isString()) {
    suffix = args.at(1).toString();
  }

  QDir dir(dirPath);
  if (!dir.exists()) {
    FunMgr::setError(QStringLiteral("listFiles() directory not found: '%1'").arg(dirPath));
    return accore::AcJsonValue();
  }

  const QStringList names = dir.entryList(QDir::Files, QDir::Name);
  accore::AcJsonValue out = accore::AcJsonValue::makeArray();
  for (const QString &n : names) {
    if (suffix.isEmpty() || n.endsWith(suffix, Qt::CaseInsensitive)) {
      out.append(accore::AcJsonValue(n));
    }
  }
  return out;
}

// ============================================================================
// formatPath — 用 {key} 占位符从数据对象中取值替换，生成最终路径
// ============================================================================
//
// 参数：
//   args[0] - 路径模板字符串，如 "{basePath}/{name}.entity.ts"
//   args[1] - 数据对象，包含占位符对应的值
//
// 规则：
//   - {key}  从 data 中查找 key，替换为字符串值
//   - {{     转义为字面量 {
//   - }}     转义为字面量 }
//   - 占位符找不到对应键 → 报错并返回空串（避免文件写到错误位置）
//   - 路径分隔符统一为 /（避免 Windows \ 问题）
//   - 末尾自动通过 QDir::cleanPath 规整（去除冗余 ./、../）
//
// 示例：
//   formatPath("{base}/{name}.ts", {base:"D:/out", name:"user"})
//   → "D:/out/user.ts"
accore::AcJsonValue FunBuiltin::formatPath(const accore::AcJsonValue &args) {
  const QString fmtErr =
      QStringLiteral("formatPath() requires 2 arguments: pattern string and data object");
  if (!FunArgs::requireCount(args, 2, fmtErr) || !FunArgs::requireString(args, 0, fmtErr) ||
      !FunArgs::requireObject(args, 1, fmtErr))
    return accore::AcJsonValue();

  QString pattern = args.at(0).toString();
  const accore::AcJsonValue data = args.at(1);

  QString result;
  int i = 0;
  while (i < pattern.length()) {
    QChar ch = pattern[i];

    // 转义：{{ → {，}} → }
    if (ch == QLatin1Char('{') && i + 1 < pattern.length() && pattern[i + 1] == QLatin1Char('{')) {
      result += QLatin1Char('{');
      i += 2;
      continue;
    }
    if (ch == QLatin1Char('}') && i + 1 < pattern.length() && pattern[i + 1] == QLatin1Char('}')) {
      result += QLatin1Char('}');
      i += 2;
      continue;
    }

    // 占位符：{key}
    if (ch == QLatin1Char('{')) {
      int end = pattern.indexOf(QLatin1Char('}'), i + 1);
      if (end == -1) {
        FunMgr::setError(
            QStringLiteral("formatPath() unterminated placeholder at position %1").arg(i));
        return accore::AcJsonValue();
      }
      QString key = pattern.mid(i + 1, end - i - 1).trimmed();
      if (key.isEmpty()) {
        FunMgr::setError(QStringLiteral("formatPath() empty placeholder at position %1").arg(i));
        return accore::AcJsonValue();
      }
      if (!data.has(key)) {
        FunMgr::setError(
            QStringLiteral("formatPath() placeholder '%1' not found in data object").arg(key));
        return accore::AcJsonValue();
      }
      accore::AcJsonValue v = data.value(key);
      QString vs;
      if (v.isString()) {
        vs = v.toString();
      } else if (v.isNumber()) {
        vs = QString::number(v.toDouble());
      } else if (v.isBool()) {
        vs = v.toBool() ? QString::fromLatin1(AcKeyword::kTrue)
                        : QString::fromLatin1(AcKeyword::kFalse);
      } else {
        FunMgr::setError(
            QStringLiteral("formatPath() placeholder '%1' value must be string/number/bool")
                .arg(key));
        return accore::AcJsonValue();
      }
      result += vs;
      i = end + 1;
      continue;
    }

    result += ch;
    ++i;
  }

  // 路径分隔符统一为 /（Windows 兼容）
  result.replace(QLatin1Char('\\'), QLatin1Char('/'));
  // 规整路径（去除冗余 ./、解析 ../）
  result = QDir::cleanPath(result);

  return accore::AcJsonValue(result);
}

// ============================================================================
// assert — 断言函数
// ============================================================================

accore::AcJsonValue FunBuiltin::assertFn(const accore::AcJsonValue &args) {
  if (!FunArgs::requireCount(args, 1,
                             QStringLiteral("assert() requires at least 1 argument: condition")))
    return accore::AcJsonValue();

  bool condition = false;
  const accore::AcJsonValue &condVal = args.at(0);
  if (condVal.isBool()) {
    condition = condVal.toBool();
  } else if (condVal.isNumber()) {
    condition = condVal.toDouble() != 0;
  } else if (condVal.isString()) {
    condition = !condVal.toString().isEmpty();
  } else {
    condition = !condVal.isNull();
  }

  if (!condition) {
    QString message = args.size() >= 2 ? args.at(1).toString() : QStringLiteral("assertion failed");
    QString lineInfo =
        s_ctx.currentLine > 0 ? QStringLiteral(" at line %1").arg(s_ctx.currentLine) : QString();
    FunMgr::setError(QStringLiteral("Assertion failed: %1%2").arg(message, lineInfo));
  }

  return accore::AcJsonValue();
}
