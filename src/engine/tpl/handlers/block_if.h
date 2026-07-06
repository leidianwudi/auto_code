/**
 * @file block_if.h
 * @brief 条件块处理器 — 处理 ${if condition}...${else}...${/if} 语法
 */

#pragma once

#include "tpl_block.h"

class BlockIf : public TplBlock {
public:
  explicit BlockIf(const TplEngine &engine) : m_engine(engine) {}

  bool handle(const QString &block, int &pos, const QString &expr,
              const QJsonObject &context, QString &result) const override;

private:
  const TplEngine &m_engine;
};