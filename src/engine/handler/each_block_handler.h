/**
 * @file each_block_handler.h
 * @brief 循环块处理器 — 处理 ${each items}...${/each} 语法
 */

#pragma once

#include "block_handler.h"

class EachBlockHandler : public BlockHandler {
public:
  explicit EachBlockHandler(const TemplateEngine &engine) : m_engine(engine) {}

  bool handle(const QString &block, int &pos, const QString &expr,
              const QJsonObject &context, QString &result) const override;

private:
  const TemplateEngine &m_engine;
};