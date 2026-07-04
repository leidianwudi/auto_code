/**
 * @file ac_highlighter.h
 * @brief AC 脚本语法高亮器
 *
 * 为 main.ac 脚本文件提供语法高亮，包括：
 * - 关键字（蓝色加粗）: main, for, in, if, else, call, return
 * - 内置函数（紫色加粗）: readJson, merge, basename, render, write, print,
 * getCheckedFiles
 * - 注释（灰色斜体）: //
 * - 字符串（橙色）: "..." 和 '...'
 * - 数字（橙色加粗）: 123, 3.14
 * - 布尔值（红色加粗）: true, false
 */

#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

/**
 * @class AcHighlighter
 * @brief AC 脚本语法高亮器
 */
class AcHighlighter : public QSyntaxHighlighter {
  Q_OBJECT

public:
  explicit AcHighlighter(QTextDocument *parent = nullptr);

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