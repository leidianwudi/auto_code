/**
 * @file light_json.h
 * @brief JSON 语法高亮器
 *
 * 为 JSON 编辑器提供语法高亮功能。
 * 注释显示风格与 AC 脚本高亮器保持一致（灰色斜体）。
 *
 * 支持的语法元素：
 * - 单行注释（//）
 * - 块注释（支持跨行）
 * - JSON 键名、字符串值、数字、布尔值、null
 */

#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class LightJson : public QSyntaxHighlighter {
  Q_OBJECT

public:
  explicit LightJson(QTextDocument *parent = nullptr);  // 构造函数，初始化所有高亮规则

protected:
  void highlightBlock(
      const QString &text) override;  // 重写：对单行文本进行高亮处理（优先处理注释）

private:
  struct HighlightRule {
    QRegularExpression pattern;  ///< 正则表达式匹配模式
    QTextCharFormat format;      ///< 对应的文本格式（颜色、字体等）
  };

  void highlightNonCommentText(const QString &text);  // 辅助函数：对非注释文本应用所有高亮规则

  QVector<HighlightRule> m_rules;   ///< 非注释高亮规则列表（键名、字符串、数字、布尔值、null）
  QTextCharFormat m_commentFormat;  ///< 注释文本格式（灰色斜体，由 highlightBlock 单独处理）
};