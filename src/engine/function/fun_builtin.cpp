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
#include "../ac_value_str.h"
#include "../tpl/tpl_engine.h"
#include "fun_mgr.h"
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
          {QString::fromLatin1(AcBuiltin::kMerge), merge},
          {QString::fromLatin1(AcBuiltin::kBasename), basename},
          {QString::fromLatin1(AcBuiltin::kFormatPath), formatPath},
          {QString::fromLatin1(AcBuiltin::kAssert), assertFn},
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

  return QJsonValue(true);
}

QJsonValue FunBuiltin::printError(const QJsonArray &args) {
  if (args.isEmpty()) return QJsonValue();

  QString text;
  for (const QJsonValue &v : args) {
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

  return QJsonValue(true);
}

// ============================================================================
// getCheckedFiles — 获取 tree.config 中勾选的文件列表
// ============================================================================

QJsonValue FunBuiltin::getCheckedFiles(const QJsonArray & /*args*/) {
  QString treePath = s_ctx.rootDir.isEmpty() ? s_ctx.scriptDir + QStringLiteral("/tree.config")
                                             : s_ctx.rootDir + QStringLiteral("/tree.config");
  QJsonArray result;
  QJsonDocument doc = UtilJson::loadFile(treePath);
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
QJsonValue FunBuiltin::formatPath(const QJsonArray &args) {
  if (args.size() < 2 || !args[0].isString() || !args[1].isObject()) {
    FunMgr::setError(
        QStringLiteral("formatPath() requires 2 arguments: pattern string and data object"));
    return QJsonValue();
  }

  QString pattern = args[0].toString();
  QJsonObject data = args[1].toObject();

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
        return QJsonValue();
      }
      QString key = pattern.mid(i + 1, end - i - 1).trimmed();
      if (key.isEmpty()) {
        FunMgr::setError(QStringLiteral("formatPath() empty placeholder at position %1").arg(i));
        return QJsonValue();
      }
      if (!data.contains(key)) {
        FunMgr::setError(
            QStringLiteral("formatPath() placeholder '%1' not found in data object").arg(key));
        return QJsonValue();
      }
      QJsonValue v = data.value(key);
      QString vs;
      if (v.isString()) {
        vs = v.toString();
      } else if (v.isDouble()) {
        vs = QString::number(v.toDouble());
      } else if (v.isBool()) {
        vs = v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
      } else {
        FunMgr::setError(
            QStringLiteral("formatPath() placeholder '%1' value must be string/number/bool")
                .arg(key));
        return QJsonValue();
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

  return QJsonValue(result);
}

// ============================================================================
// assert — 断言函数
// ============================================================================

QJsonValue FunBuiltin::assertFn(const QJsonArray &args) {
  if (args.size() < 1) {
    FunMgr::setError(QStringLiteral("assert() requires at least 1 argument: condition"));
    return QJsonValue();
  }

  bool condition = false;
  const QJsonValue &condVal = args[0];
  if (condVal.isBool()) {
    condition = condVal.toBool();
  } else if (condVal.isDouble()) {
    condition = condVal.toDouble() != 0;
  } else if (condVal.isString()) {
    condition = !condVal.toString().isEmpty();
  } else {
    condition = !condVal.isNull() && !condVal.isUndefined();
  }

  if (!condition) {
    QString message = args.size() >= 2 ? args[1].toString() : QStringLiteral("assertion failed");
    QString lineInfo =
        s_ctx.currentLine > 0 ? QStringLiteral(" at line %1").arg(s_ctx.currentLine) : QString();
    FunMgr::setError(QStringLiteral("Assertion failed: %1%2").arg(message, lineInfo));
  }

  return QJsonValue();
}