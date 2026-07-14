/**
 * @file tpl_engine.cpp
 * @brief 模板引擎实现
 *
 * TplEngine 的职责（精简后）：
 * 1. render()       — 对外入口：清空错误，调用 renderBlock
 * 2. renderBlock()  — 递归扫描 ${...}，交给 TplFactory 创建处理器
 * 3. resolvePath()  — 嵌套属性路径解析（共享工具，供 handler 调用）
 *
 * 算术表达式求值已移至 BlockExpression。
 */

#include "tpl_engine.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QString>

#include "../ac_language.h"
#include "../function/fun_mgr.h"
#include "../schema_validator.h"

// 静态成员定义

const SchemaValidator *TplEngine::sm_validator = nullptr;
QString TplEngine::sm_schemaClass;

TplEngine::TplEngine() : m_handlerFactory(*this) {}

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
QString TplEngine::render(const QString &tmpl, const QJsonObject &data) const {
  m_lastError.clear();

  if (sm_validator && !sm_schemaClass.isEmpty()) {
    QString err = sm_validator->validate(sm_schemaClass, data);
    if (!err.isEmpty()) {
      m_lastError = err;
      return {};
    }
  }

  return renderBlock(tmpl, data);
}

// renderBlock — 递归扫描 ${...} 并交给处理器

QString TplEngine::renderBlock(const QString &block, const QJsonObject &context) const {
  QString result;
  int pos = 0;

  while (pos < block.length()) {
    int start = block.indexOf(QString::fromLatin1(AcTemplate::kExprOpen), pos);
    if (start == -1) {
      result += block.mid(pos);
      break;
    }

    result += block.mid(pos, start - pos);

    int end = block.indexOf(QStringLiteral("}"), start + 2);
    if (end == -1) {
      m_lastError = QStringLiteral("Unclosed ${ at position %1").arg(start);
      return {};
    }

    QString expr = block.mid(start + 2, end - start - 2).trimmed();
    pos = end + 1;

    auto handler = m_handlerFactory.createHandler(expr);
    if (!handler->handle(block, pos, expr, context, result)) {
      return {};
    }
  }

  return result;
}

// resolvePath — 嵌套属性路径解析 + 通过 FunMgr 调用 C++ 函数

QJsonValue TplEngine::resolvePath(const QString &path, const QJsonObject &context) const {
  // 检查是否是函数调用格式: funcName(...)
  // 例如: str.toLowerCase(Hello) 或 file.read(C:/data.txt)
  int parenPos = path.indexOf(QStringLiteral("("));
  if (parenPos != -1 && path.endsWith(QStringLiteral(")"))) {
    // 拆分为 类名.函数名(参数)
    QString fullFunc = path.left(parenPos);
    QString argsStr = path.mid(parenPos + 1);
    argsStr.chop(1);  // 去掉尾部的 )

    // 解析参数：逗号分隔，去除首尾空白
    QStringList rawArgs;
    if (!argsStr.isEmpty()) {
      // 简单逗号分割（不考虑转义逗号）
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
      // 尝试作为数字解析
      bool ok = false;
      double num = raw.toDouble(&ok);
      if (ok) {
        evalArgs.append(num);
      } else if ((raw.startsWith(QStringLiteral("\"")) && raw.endsWith(QStringLiteral("\""))) ||
                 (raw.startsWith(QStringLiteral("'")) && raw.endsWith(QStringLiteral("'")))) {
        // 字符串字面量
        evalArgs.append(raw.mid(1, raw.length() - 2));
      } else {
        // 变量路径
        evalArgs.append(resolvePath(raw, context));
      }
    }

    // 解析函数名：类名.函数名
    int dotPos = fullFunc.indexOf(QStringLiteral("."));
    if (dotPos != -1) {
      QString clsName = fullFunc.left(dotPos);
      QString funcName = fullFunc.mid(dotPos + 1);
      return FunMgr::ins().call(clsName, funcName, evalArgs);
    }

    m_lastError = QStringLiteral("invalid function call format: %1").arg(path);
    return QJsonValue();
  }

  // ── 变量路径解析：按点号分割逐层查找 ──
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