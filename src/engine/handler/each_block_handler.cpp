/**
 * @file each_block_handler.cpp
 * @brief 循环块处理器实现
 */

#include "each_block_handler.h"
#include "../template_engine.h"

bool EachBlockHandler::handle(const QString &block, int &pos,
                              const QString &expr, const QJsonObject &context,
                              QString &result) const {
  QString varName = expr.mid(5).trimmed();

  int eachStart = pos;
  int eachEnd = block.indexOf(QStringLiteral("${/each}"), pos);
  if (eachEnd == -1) {
    m_engine.setError(QStringLiteral("Unclosed ${each %1}").arg(varName));
    return false;
  }

  QString body = block.mid(eachStart, eachEnd - eachStart);
  pos = eachEnd + 8;

  QJsonValue val = m_engine.resolvePath(varName, context);
  if (!val.isArray())
    return true;

  QJsonArray arr = val.toArray();

  for (int i = 0; i < arr.size(); ++i) {
    QJsonObject itemContext = context;
    QJsonValue item = arr[i];

    if (item.isObject()) {
      QJsonObject obj = item.toObject();
      for (auto it = obj.begin(); it != obj.end(); ++it)
        itemContext.insert(it.key(), it.value());
      itemContext.insert(QStringLiteral("."), QJsonValue());
    } else {
      itemContext.insert(QStringLiteral("."), item);
    }

    result += m_engine.renderBlock(body, itemContext);
  }
  return true;
}