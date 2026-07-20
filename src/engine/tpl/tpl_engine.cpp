/**
 * @file tpl_engine.cpp
 * @brief 模板引擎实现
 *
 * TplEngine 的职责（精简后）：
 * 1. render()       — 对外入口：Lexer → Parser → Renderer
 * 2. resolvePath()  — 嵌套属性路径解析（共享工具，供 Renderer 调用）
 */

#include "tpl_engine.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "../ac_language.h"
#include "../function/fun_mgr.h"
#include "../schema_validator.h"
#include "tpl_lexer.h"
#include "tpl_parser.h"
#include "tpl_renderer.h"

// 静态成员定义

const SchemaValidator *TplEngine::sm_validator = nullptr;
QString TplEngine::sm_schemaClass;

// setSchema — 设置 Schema 校验
void TplEngine::setSchema(const QString &className, const SchemaValidator *validator) {
  sm_schemaClass = className;
  sm_validator = validator;
}

// clearSchema — 清除 Schema 校验
void TplEngine::clearSchema() {
  sm_schemaClass.clear();
  sm_validator = nullptr;
}

// render — 对外渲染入口
//
// 三阶段 AST 方案：
//   1. Lexer   — 把模板字符串切成 Token 流
//   2. Parser  — 把 Token 流组装成 AST 树
//   3. Renderer — 遍历 AST 树，输出最终字符串
//
// 空行控制规则：
//   - 块标签（${if}/${each}/${else} 等）独占一行时，剔除该行的缩进和换行符
//   - 块标签行内出现时，保留所有空白字符
//   - 不做任何"智能空行压缩"，模板里几个 \n 就输出几个 \n
QString TplEngine::render(const QString &tmpl, const QJsonObject &data) const {
  m_lastError.clear();

  if (sm_validator && !sm_schemaClass.isEmpty()) {
    QString err = sm_validator->validate(sm_schemaClass, data);
    if (!err.isEmpty()) {
      m_lastError = err;
      return {};
    }
  }

  // 去除 \r（统一行尾格式）
  QString cleanTmpl = tmpl;
  cleanTmpl.remove(QLatin1Char('\r'));

  // 阶段 1：词法分析
  QString lexError;
  QList<TplLexer::Token> tokens = TplLexer::tokenize(cleanTmpl, lexError);
  if (!lexError.isEmpty()) {
    m_lastError = lexError;
    return {};
  }

  // 阶段 2：语法分析
  QString parseError;
  QList<QSharedPointer<TplAst::AstNode>> ast = TplParser::parse(tokens, parseError);
  if (!parseError.isEmpty()) {
    m_lastError = parseError;
    return {};
  }

  // 阶段 3：渲染
  QString result = TplRenderer::render(ast, data, *this);

  return result;
}

// resolvePath — 路径解析入口（函数调用 / 变量路径分发）

QJsonValue TplEngine::resolvePath(const QString &path, const QJsonObject &context) const {
  int parenPos = path.indexOf(QLatin1Char('('));
  if (parenPos != -1 && path.endsWith(QLatin1Char(')'))) {
    return resolveFuncCall(path, context);
  }
  return resolveVarPath(path, context);
}

// resolveFuncCall — 函数调用解析（类名.函数名(参数)）

QJsonValue TplEngine::resolveFuncCall(const QString &path, const QJsonObject &context) const {
  int parenPos = path.indexOf(QLatin1Char('('));
  QString fullFunc = path.left(parenPos);
  QString argsStr = path.mid(parenPos + 1);
  argsStr.chop(1);  // 去掉尾部的 )

  // 解析参数：逗号分隔，去除首尾空白
  QStringList rawArgs;
  if (!argsStr.isEmpty()) {
    int start = 0;
    for (int i = 0; i < argsStr.length(); ++i) {
      if (argsStr[i] == QLatin1Char(',')) {
        rawArgs.append(argsStr.mid(start, i - start).trimmed());
        start = i + 1;
      }
    }
    rawArgs.append(argsStr.mid(start).trimmed());
  }

  // 解析参数：递归调用 resolvePath 处理每个参数
  QJsonArray evalArgs;
  for (const QString &raw : rawArgs) {
    bool ok = false;
    double num = raw.toDouble(&ok);
    if (ok) {
      evalArgs.append(num);
    } else if ((raw.startsWith(QStringLiteral("\"")) && raw.endsWith(QStringLiteral("\""))) ||
               (raw.startsWith(QStringLiteral("'")) && raw.endsWith(QStringLiteral("'")))) {
      evalArgs.append(raw.mid(1, raw.length() - 2));
    } else {
      evalArgs.append(resolvePath(raw, context));
    }
  }

  // 解析函数名：类名.函数名
  int dotPos = fullFunc.indexOf(QLatin1Char('.'));
  if (dotPos != -1) {
    QString clsName = fullFunc.left(dotPos);
    QString funcName = fullFunc.mid(dotPos + 1);
    return FunMgr::ins().call(clsName, funcName, evalArgs);
  }

  // 无类名前缀：尝试作为一级内置函数（注册在 "builtin" 伪类下）
  if (FunMgr::ins().contains(QString::fromLatin1(AcRuntime::kBuiltinClass), fullFunc)) {
    return FunMgr::ins().call(QString::fromLatin1(AcRuntime::kBuiltinClass), fullFunc, evalArgs);
  }

  m_lastError = QStringLiteral("invalid function call format: %1").arg(path);
  return QJsonValue();
}

// resolveVarPath — 嵌套属性路径解析（按点号逐层查找）

QJsonValue TplEngine::resolveVarPath(const QString &path, const QJsonObject &context) const {
  QStringList parts = path.split(QStringLiteral("."));
  QJsonValue value = QJsonValue(context);

  for (const QString &part : parts) {
    if (value.isObject()) {
      value = value.toObject().value(part);
    } else if (value.isArray()) {
      bool ok = false;
      int idx = part.toInt(&ok);
      if (ok) {
        QJsonArray arr = value.toArray();
        if (idx >= 0 && idx < arr.size())
          value = arr[idx];
        else
          return QJsonValue();
      } else {
        return QJsonValue();
      }
    } else {
      return QJsonValue();
    }
  }

  return value;
}
