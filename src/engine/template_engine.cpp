/**
 * @file template_engine.cpp
 * @brief 模板引擎实现
 *
 * TemplateEngine 的职责（精简后）：
 * 1. render()       — 对外入口：清空错误，调用 renderBlock
 * 2. renderBlock()  — 递归扫描 ${...}，交给 HandlerFactory 创建处理器
 * 3. resolvePath()  — 嵌套属性路径解析（共享工具，供 handler 调用）
 *
 * 算术表达式求值已移至 ExpressionHandler。
 */

#include "template_engine.h"

#include "function/fun_const.h"
#include "function/fun_mgr.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QString>

// ============================================================================
// 构造函数
// ============================================================================

TemplateEngine::TemplateEngine() : m_handlerFactory(*this) {}

// ============================================================================
// render — 对外渲染入口
// ============================================================================

QString TemplateEngine::render(const QString &tmpl,
                               const QJsonObject &data) const {
  m_lastError.clear();
  return renderBlock(tmpl, data);
}

// ============================================================================
// renderBlock — 递归扫描 ${...} 并交给处理器
// ============================================================================

QString TemplateEngine::renderBlock(const QString &block,
                                    const QJsonObject &context) const {
  QString result;
  int pos = 0;

  while (pos < block.length()) {
    int start = block.indexOf(QStringLiteral("${"), pos);
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

// ============================================================================
// resolvePath — 嵌套属性路径解析 + 通过 FunMgr 调用 C++ 函数
// ============================================================================

QJsonValue TemplateEngine::resolvePath(const QString &path,
                                       const QJsonObject &context) const {
  // 检查是否是函数调用格式: funcName(...)
  // 例如: str.toLowerCase(Hello) 或 file.read(C:/data.txt)
  int parenPos = path.indexOf(QStringLiteral("("));
  if (parenPos != -1 && path.endsWith(QStringLiteral(")"))) {
    // 拆分为 类名.函数名(参数)
    QString fullFunc = path.left(parenPos);
    QString argsStr = path.mid(parenPos + 1);
    argsStr.chop(1); // 去掉尾部的 )

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

    // 解析类名.函数名
    int dotPos = fullFunc.indexOf(QStringLiteral("."));
    if (dotPos == -1)
      return QJsonValue();

    QString className = fullFunc.left(dotPos).trimmed();
    QString funcName = fullFunc.mid(dotPos + 1).trimmed();

    // 将参数解析为 QJsonArray
    QJsonArray jsonArgs;
    for (const QString &raw : rawArgs) {
      // 尝试解析为 JSON，否则当作字符串
      QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
      if (doc.isNull() || doc.isObject()) {
        // 纯字符串
        jsonArgs.append(QJsonValue(raw));
      } else {
        jsonArgs.append(doc.array());
      }
    }

    // 通过 FunMgr 调用
    return FunMgr::ins().call(className, funcName, jsonArgs);
  }

  // 普通属性路径: user.name.email 或 modelName.toLowerCase
  QStringList parts = path.split(QStringLiteral("."));
  QJsonValue current(context);

  for (int i = 0; i < parts.size(); ++i) {
    const QString &part = parts[i];

    // 检查是否是 FunStr 字符串方法（如 toLowerCase、trim 等）
    // 此时当前值应作为输入，应用方法后得到新值
    if (FunConst::stringMethods().contains(part)) {
      if (!current.isString())
        return QJsonValue();
      QJsonArray arr = {current.toString()};
      return FunMgr::ins().call(QString::fromLatin1(FunConst::kClsStr), part,
                                arr);
    }

    if (!current.isObject())
      return QJsonValue();
    current = current.toObject().value(part);
  }

  return current;
}