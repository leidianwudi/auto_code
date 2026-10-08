/**
 * @file indent_guide.h
 * @brief 缩进参考线模块（VS Code 逐行判定方案）
 *
 * 架构（参照 VS Code indentation guides）：
 * - 不做 range 闭包/栈扫描——每行独立判定其 guide 列集合，从根源消除
 *   "区间断裂/超长/闭合行缺口"类 bug（旧栈扫描方案的边界条件无底洞）
 * - guide 列 C = L × granularity（L=1,2,...），在行 i 显示 ⟺ 行有效缩进 ≥ C
 * - 空白行继承前后最近非空行缩进的较小值（上下文延续）
 * - 粒度从文档相邻非空行缩进差的最小正值自动推断（2/4 空格等距缩进均适配）
 * - X 坐标由调用方从各行 QTextLayout::cursorToX 取（渲染事实源，
 *   字体/字号/缩放变化自动正确，严禁"空格宽 × 列数"估算）
 */

#pragma once

#include <QTextBlock>
#include <QVector>


/**
 * @class IndentGuide
 * @brief 缩进参考线计算（纯静态工具，无状态）
 */
class IndentGuide {
public:
  /**
   * @brief 计算单行的缩进空格数
   * @param line 行文本内容
   * @param tabWidth tab 展开的空格数
   * @return 前导空白宽度（空格数；tab 按展开宽折算）
   */
  static int lineIndentLevel(const QString &line, int tabWidth);

  /**
   * @brief 检测文档的缩进粒度
   *
   * 取相邻非空行缩进差的最小正值（等距缩进文档即为每级缩进宽）；
   * 无差值（单行/全同缩进）时返回 fallback。
   *
   * @param doc 文档
   * @param tabWidth tab 展开空格数（缩进计算用）
   * @param fallback 推断失败时的兜底粒度
   * @return 缩进粒度（≥1）
   */
  static int detectGranularity(const QTextDocument *doc, int tabWidth, int fallback);

  /**
   * @brief 计算单个块的 guide 列集合（块内列号，0-based 字符位置）
   *
   * 规则：非空行返回 {gran, 2×gran, ... ≤ 行缩进}；
   * 空白行继承前后最近非空行缩进的较小值后再展开。
   *
   * @param blk 目标块
   * @param granularity 缩进粒度
   * @param tabWidth tab 展开空格数
   * @return guide 列号数组（升序）；调用方用各行 QTextLayout::cursorToX(列) 取像素
   */
  static QVector<int> guideColumnsForLine(const QTextBlock &blk, int granularity, int tabWidth);
};