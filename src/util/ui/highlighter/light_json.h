/**
 * @file light_json.h
 * @brief JSON 语法高亮器
 *
 * 为 JSON 编辑器提供语法高亮功能。
 * 注释显示风格与 AC 脚本高亮器保持一致（灰色斜体）。
 *
 * 支持的语法元素：
 * - 单行注释（//）
 * - 块注释（/&#42; &#42;/，支持跨行）
 * - JSON 键名、字符串值、数字、布尔值、null
 */

#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class LightJson : public QSyntaxHighlighter {
  Q_OBJECT

public:
  explicit LightJson(QTextDocument *parent = nullptr);

protected:
  void highlightBlock(const QString &text) override;

private:
  struct HighlightRule {
    QRegularExpression pattern;
    QTextCharFormat format;
  };

  void highlightBlockHelper(const QString &text);

  QVector<HighlightRule> m_rules;
  QTextCharFormat m_commentFormat;
};