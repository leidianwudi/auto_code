/**
 * @file workspace_diag.cpp
 * @brief 工作区全量诊断实现
 */

#include "workspace_diag.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QTextStream>

#include "src/engine/json_validator.h"
#include "src/engine/schema_validator.h"
#include "src/engine/script/ac_validator.h"
#include "src/engine/tpl/tpl_validator.h"
#include "src/util/common/code_constants.h"
#include "src/util/common/util_json.h"

namespace {

/// 是否为可验证文件类型
bool isVerifiableFile(const QString &path) {
  return path.endsWith(QStringLiteral(".ac"), Qt::CaseInsensitive) ||
         path.endsWith(QStringLiteral(".tpl"), Qt::CaseInsensitive) ||
         path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive) ||
         path.endsWith(QStringLiteral(".jsonvue"), Qt::CaseInsensitive);
}

/// 从错误消息中提取出错路径（与 CodeEditor 中一致）
QString extractSchemaPath(const QString &msg) {
  int s = msg.indexOf(QLatin1Char('\''));
  int e = msg.indexOf(QLatin1Char('\''), s + 1);
  if (s < 0 || e < 0) return QString();
  return msg.mid(s + 1, e - s - 1);
}

/// 在 JSON5 文本中查找指定键所在的行号；找不到返回 1（与 CodeEditor 中一致）
int findSchemaKeyLine(const QString &text, const QString &key) {
  if (key.isEmpty()) return 1;
  QRegularExpression re(QStringLiteral("(['\"]?)%1\\1\\s*:").arg(QRegularExpression::escape(key)));
  auto m = re.match(text);
  if (!m.hasMatch()) return 1;
  int matchPos = m.capturedStart();
  int line = 1;
  for (int i = 0; i < matchPos && i < text.size(); ++i) {
    if (text[i] == QLatin1Char('\n')) ++line;
  }
  return line;
}

/// 解析 $schema 路径（与 CodeEditor 中一致）：
/// 以 / 开头 → 基于项目根目录（PROJECT_SOURCE_DIR/file）；相对路径 → 基于文件所在目录
QString resolveSchemaPath(const QString &filePath, const QString &schemaRef) {
  QString schemaPath = schemaRef;
  if (schemaRef.startsWith(QLatin1Char('/'))) {
    schemaPath = QStringLiteral(PROJECT_SOURCE_DIR) +
                 QString::fromUtf8(CodeConstants::Paths::kFileDirName) + schemaRef;
  } else if (QFileInfo(schemaRef).isRelative()) {
    QFileInfo fi(filePath);
    schemaPath = fi.absolutePath() + QLatin1Char('/') + schemaRef;
  }
  return QDir::cleanPath(schemaPath);
}

/// 读取文件内容（UTF-8）
QString readFileText(const QString &filePath) {
  QFile f(filePath);
  if (!f.open(QIODevice::ReadOnly)) return QString();
  QTextStream ts(&f);
  return ts.readAll();
}

/// 验证单个 JSON/JSONVue 文件：语法 + $schema 结构校验
QVector<ValidationResult> validateJsonFile(const QString &filePath, const QString &source) {
  QVector<ValidationResult> results;

  // 1. 语法校验
  JsonValidator validator;
  results = validator.validate(source);

  // 2. 语法通过后，按 $schema 做结构校验
  QJsonParseError perr;
  QJsonDocument doc = UtilJson::fromJson(source, &perr);
  if (perr.error != QJsonParseError::NoError || !doc.isObject()) return results;

  const QJsonObject obj = doc.object();
  QString schemaRef = obj.value(QStringLiteral("$schema")).toString();
  if (schemaRef.isEmpty()) return results;

  SchemaValidator schema;
  if (!schema.load(resolveSchemaPath(filePath, schemaRef))) return results;
  if (!schema.hasRoot()) return results;

  const QVector<QString> errs = schema.validateDocument(obj);
  for (const QString &e : errs) {
    QString prop = extractSchemaPath(e).section(QLatin1Char('.'), -1);
    int line = findSchemaKeyLine(source, prop);
    results.append(ValidationResult(line, 1, 1, QStringLiteral("Schema: %1").arg(e)));
  }
  return results;
}

}  // namespace

QStringList collectWorkspaceFiles(const QString &rootDir) {
  QStringList out;
  QDirIterator it(rootDir, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString p = it.next();
    if (isVerifiableFile(p)) out.append(QDir::cleanPath(p));
  }
  return out;
}

QVector<WorkspaceFileDiag> scanWorkspaceDiagnostics(const QStringList &filePaths) {
  QVector<WorkspaceFileDiag> all;
  all.reserve(filePaths.size());
  for (const QString &fp : filePaths) {
    WorkspaceFileDiag item;
    item.filePath = fp;
    const QString source = readFileText(fp);
    if (source.isEmpty()) {
      // 空文件/读取失败：不产出诊断（空文件本就没有错误）
      all.append(item);
      continue;
    }

    if (fp.endsWith(QStringLiteral(".ac"), Qt::CaseInsensitive)) {
      AcValidator v;
      v.setFilePath(fp);  // 用于解析 import 相对路径
      item.issues = v.validate(source);
    } else if (fp.endsWith(QStringLiteral(".tpl"), Qt::CaseInsensitive)) {
      TplValidator v;
      item.issues = v.validate(source);
    } else if (fp.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive) ||
               fp.endsWith(QStringLiteral(".jsonvue"), Qt::CaseInsensitive)) {
      item.issues = validateJsonFile(fp, source);
    }

    all.append(item);
  }
  return all;
}
