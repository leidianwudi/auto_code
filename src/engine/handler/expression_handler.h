/**
 * @file expression_handler.h
 * @brief 表达式处理器 — 处理普通 ${expression} 表达式求值
 */

#pragma once

#include "block_handler.h"

class ExpressionHandler : public BlockHandler {
public:
  explicit ExpressionHandler(const TemplateEngine &engine) : m_engine(engine) {}

  bool handle(const QString &block, int &pos, const QString &expr,
              const QJsonObject &context, QString &result) const override;

private:
  double evalExpression(const QString &expr, const QJsonObject &context) const;
  double evalAddSub(const QString &expr, int &pos,
                    const QJsonObject &context) const;
  double evalMulDiv(const QString &expr, int &pos,
                    const QJsonObject &context) const;
  double evalPrimary(const QString &expr, int &pos,
                     const QJsonObject &context) const;

  const TemplateEngine &m_engine;
};