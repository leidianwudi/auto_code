/**
 * @file hand_if_block.h
 * @brief 条件块处理器 — 处理 ${if condition}...${else}...${/if} 语法
 */

#pragma once

#include "hand_block.h"

class HandIfBlock : public HandBlock {
public:
  explicit HandIfBlock(const TemplateEngine &engine) : m_engine(engine) {}

  bool handle(const QString &block, int &pos, const QString &expr,
              const QJsonObject &context, QString &result) const override;

private:
  const TemplateEngine &m_engine;
};