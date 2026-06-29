/**
 * @file ts_highlighter.h
 * @brief TypeScript 语法高亮器
 *
 * 为输出结果提供 TypeScript 语法高亮，包括：
 * - 关键字（蓝色）: export, class, interface, async, await 等
 * - 类型（绿色）: string, number, boolean, Promise 等
 * - 字符串（橙色）: "..." 或 '...'
 * - 注释（灰色）: 单行注释和多行注释
 * - 数字（紫色）: 123, 3.14
 */

#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>


/**
 * @class TypeScriptHighlighter
 * @brief TypeScript 语法高亮器
 *
 * 对 TypeScript 代码进行语法高亮着色，使生成的代码更易读。
 */
class TypeScriptHighlighter : public QSyntaxHighlighter {
  Q_OBJECT

public:
  explicit TypeScriptHighlighter(QTextDocument *parent = nullptr);

protected:
  /**
   * @brief 对单个文本块进行高亮处理
   * @param text 文本内容
   */
  void highlightBlock(const QString &text) override;

private:
  /**
   * @brief 高亮规则
   */
  struct HighlightRule {
    QRegularExpression pattern; ///< 匹配模式
    QTextCharFormat format;     ///< 文本格式
  };

  QVector<HighlightRule> m_rules; ///< 高亮规则列表
};