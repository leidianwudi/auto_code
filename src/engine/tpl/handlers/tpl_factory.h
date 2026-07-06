/**
 * @file tpl_factory.h
 * @brief 模板块处理器 — 工厂类
 *
 * TplFactory 根据表达式前缀自动选择并创建对应的 TplBlock 处理器：
 * - "each " 开头 => BlockEach
 * - "if "   开头 => BlockIf
 * - 其他         => BlockExpression
 */

#pragma once

#include <QString>
#include <memory>

#include "tpl_block.h"

class TplEngine;

/**
 * @class TplFactory
 * @brief 处理器工厂
 *
 * 根据表达式前缀自动选择并创建合适的处理器。
 */
class TplFactory {
public:
  explicit TplFactory(const TplEngine &engine) : m_engine(engine) {}

  /**
   * @brief 根据表达式创建对应的处理器
   * @param expr ${...} 内的表达式内容（已 trim）
   * @return 处理器实例的 unique_ptr
   */
  std::unique_ptr<TplBlock> createHandler(const QString &expr) const;

private:
  /**
   * @enum BlockType
   * @brief 模板块类型枚举
   *
   * 根据表达式前缀匹配到对应的块类型，
   * 用于 createHandler 内部分发创建对应的 TplBlock 实例。
   */
  enum class BlockType {
    Each,      // ${each items}...${/each} 循环块
    If,        // ${if cond}...${else}...${/if} 条件块
    Expression // ${expression} 普通表达式（兜底）
  };

  /**
   * @brief 根据表达式前缀检测块类型
   */
  static BlockType detectType(const QString &expr);

  const TplEngine &m_engine;
};