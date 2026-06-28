/**
 * @file if_block_handler.h
 * @brief 条件块处理器 — 处理 ${if condition}...${else}...${/if} 语法
 */

#pragma once

#include "block_handler.h"

class IfBlockHandler : public BlockHandler {
public:
  explicit IfBlockHandler(const TemplateEngine &engine) : m_engine(engine) {}

  bool handle(const QString &block, int &pos, const QString &expr,
              const QJsonObject &context, QString &result) const override;

private:
  const TemplateEngine &m_engine;
};