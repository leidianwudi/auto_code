/**
 * @file light_tpl.h
 * @brief 模板语法高亮器
 *
 * 为模板编辑器提供语法高亮，包括：
 * - 模板标签: ${each}, ${if}, ${/each}, ${/if}, ${else}
 * - 模板变量: ${varName}
 * - 算术运算符: +, -, *, /
 * - 注释: //
 */

#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

/**
 * @class LightTpl
 * @brief 模板语法高亮器
 *
 * 对模板文本进行语法高亮着色，使模板结构一目了然。
 */
class LightTpl : public QSyntaxHighlighter {
  Q_OBJECT

public:
  explicit LightTpl(QTextDocument *parent = nullptr);

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