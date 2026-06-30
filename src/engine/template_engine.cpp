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
// resolvePath — 嵌套属性路径解析 + 工厂函数调用
// ============================================================================

QJsonValue TemplateEngine::resolvePath(const QString &path,
                                       const QJsonObject &context) const {
  static const QStringList stringMethods = {
      QStringLiteral("toLowerCase"), QStringLiteral("toUpperCase"),
      QStringLiteral("trim"), QStringLiteral("capitalize")};

  QStringList parts = path.split(QStringLiteral("."));
  QJsonValue current(context);

  for (int i = 0; i < parts.size(); ++i) {
    const QString &part = parts[i];

    if (stringMethods.contains(part)) {
      if (!current.isString())
        return QJsonValue();

      QString str = current.toString();
      if (part == QStringLiteral("toLowerCase"))
        str = str.toLower();
      else if (part == QStringLiteral("toUpperCase"))
        str = str.toUpper();
      else if (part == QStringLiteral("trim"))
        str = str.trimmed();
      else if (part == QStringLiteral("capitalize"))
        if (!str.isEmpty())
          str = str[0].toUpper() + str.mid(1);

      current = QJsonValue(str);
      continue;
    }

    if (!current.isObject())
      return QJsonValue();
    current = current.toObject().value(part);
  }

  return current;
}