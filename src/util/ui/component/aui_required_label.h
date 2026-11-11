/**
 * @file aui_required_label.h
 * @brief 必填项标签控件（红色星号前缀 + 主题色文字）
 */

#pragma once

#include <QLabel>

/// 必填标识色（全程序统一的必填星号颜色，深浅主题下均醒目）
inline constexpr const char *kRequiredMarkColor = "#d03050";

/**
 * @class AuiRequiredLabel
 * @brief 「* 文字」形态的必填标签
 *
 * 必填输入项的标签统一用本控件：红色星号前缀标识必填，标签文字保持
 * 主题色（不随星号变红）。文字走富文本渲染，动态文本会做 HTML 转义。
 */
class AuiRequiredLabel : public QLabel {
 public:
  explicit AuiRequiredLabel(const QString &text = {}, QWidget *parent = nullptr)
      : QLabel(parent) {
    setTextFormat(Qt::RichText);
    setText(QStringLiteral("<span style='color:%1;font-weight:600'>*</span> ")
                .arg(kRequiredMarkColor) + text.toHtmlEscaped());
  }
};
