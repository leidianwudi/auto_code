/**
 * @file indent_guide.h
 * @brief 缩进参考线模块（VS Code 风格）
 *
 * 基于 VS Code 的 IndentRange 栈扫描算法，
 * 通过缩进层级变化确定每条参考线的起止行，
 * 支持当前行高亮和文档版本缓存。
 *
 * 职责分离：
 * - compute(): 纯数据计算，不依赖 Qt 控件
 * - 绘制由 CodeEditor 在 paintEvent 中完成
 *   （因为 blockBoundingGeometry/contentOffset 是 protected）
 */

#pragma once

#include <QVector>

/**
 * @class IndentGuide
 * @brief 缩进参考线计算模块
 *
 * 采用 VS Code 的 computeIndentRanges 栈扫描算法：
 * - 逐行计算缩进层级
 * - 缩进增加时入栈，减少时弹出形成 IndentRange
 * - 结果缓存，文档不变时直接复用
 */
class IndentGuide {
public:
  /// 一条缩进参考线的数据
  struct IndentRange {
    int startLine;  ///< 起始行号（1-based）
    int endLine;    ///< 结束行号（1-based，含）
    int indent;     ///< 缩进空格数
  };

  IndentGuide() = default;

  /**
   * @brief 计算缩进参考线数据（带缓存）
   * @param text 文档文本内容
   * @param revision 文档版本号（用于缓存失效检测）
   * @param tabWidth 一个 tab 对应的空格数
   */
  void compute(const QString &text, int revision, int tabWidth);

  /// 获取计算结果
  const QVector<IndentRange> &ranges() const { return m_ranges; }

  /**
   * @brief 计算单行的缩进空格数
   * @param line 行文本内容
   * @param tabWidth tab 对应空格数
   * @return 缩进空格数
   */
  static int lineIndentLevel(const QString &line, int tabWidth);

private:
  QVector<IndentRange> m_ranges;  ///< 缩进参考线数据
  int m_cacheRevision = -1;       ///< 缓存对应的文档版本号
};