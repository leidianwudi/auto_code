/**
 * @file tpl_block.h
 * @brief 模板块处理器 — 基类接口与共享辅助函数
 *
 * 定义：
 * - TplBlock 抽象基类：所有处理器的公共接口
 * - findMatchingClose()：查找匹配闭合标签的共享工具函数
 *
 * 具体处理器定义在各自的头文件中：
 * - BlockEach        -> block_each.h
 * - BlockIf          -> block_if.h
 * - BlockExpression  -> block_expression.h
 */

#pragma once

#include <QJsonObject>
#include <QString>

class TplEngine;

/**
 * @class TplBlock
 * @brief 模板块处理器抽象基类
 *
 * 所有表达式处理器的公共接口。
 * 子类实现 handle 方法，完成特定类型的表达式解析和渲染。
 */
class TplBlock {
public:
  virtual ~TplBlock() = default;

  /**
   * @brief 查找匹配的闭合标签（支持嵌套）
   *
   * 从 startPos 开始扫描 block，计数嵌套的 openTag 和 closeTag，
   * 返回深度归零时的位置（即匹配的闭合标签位置）。
   *
   * @param block      完整的模板字符串
   * @param startPos   开始搜索的位置
   * @param openPrefix 开放标签前缀（如 "if "、"each "）
   * @param closeTag   闭合标签（如 "${/if}"、"${/each}"）
   * @return 匹配的闭合标签位置，未找到返回 -1
   */
  static int findMatchingClose(const QString &block, int startPos, const QString &openPrefix,
                               const QString &closeTag);

  /**
   * @brief 查找匹配的 ${else}（支持嵌套，仅返回当前 if 层的 else）
   *
   * 从 startPos 开始扫描 block，正确计数嵌套的 ${if ...} / ${/if}，
   * 仅在当前 if 层级（depth == 1）时返回 ${else} 的位置。
   * 如果遇到当前 if 的匹配 ${/if}（depth 归零），返回 -1 表示没有 else。
   *
   * @param block    完整的模板字符串
   * @param startPos 开始搜索的位置（应在 ${if ...} 之后）
   * @return ${else} 的位置，未找到返回 -1
   */
  static int findElsePos(const QString &block, int startPos, bool *isElseIf = nullptr);

  /**
   * @brief 处理一个模板表达式
   * @param block   完整模板片段
   * @param pos     当前解析位置（会被修改为表达式结束之后的位置）
   * @param expr    ${...} 内的表达式内容
   * @param context 变量上下文
   * @param result  渲染结果输出
   * @return true 表示处理成功，false 表示出错
   */
  virtual bool handle(const QString &block, int &pos, const QString &expr,
                      const QJsonObject &context, QString &result) const = 0;
};