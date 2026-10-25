/**
 * @file aui_splitter.h
 * @brief 统一风格的可拖动分隔控件
 *
 * 分隔条只画一条 1px 细线 + 中部三点抓手，悬停/拖动时提亮；
 * 保留 8px 命中区，保证仍然容易拖动。
 *
 * 用法示例：
 * @code
 * auto *split = new AuiSplitter(Qt::Vertical, this);
 * split->addWidget(topWidget);
 * split->addWidget(bottomWidget);
 * @endcode
 */

#pragma once

#include <QSplitter>

class QEvent;
class QEnterEvent;
class QPaintEvent;

/// @class AuiSplitterHandle
/// @brief 分隔条绘制：1px 主线 + 三点抓手（替代 QSS 无法精确绘制的抓手图案）
class AuiSplitterHandle : public QSplitterHandle {
public:
  AuiSplitterHandle(Qt::Orientation orientation, QSplitter *parent);

protected:
  void paintEvent(QPaintEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  bool m_hovered = false;  ///< 鼠标是否悬停（悬停/拖动时线条与圆点提亮）
};

/// @class AuiSplitter
/// @brief 统一风格的 QSplitter：固定命中区、不可折叠、使用 AuiSplitterHandle
class AuiSplitter : public QSplitter {
public:
  explicit AuiSplitter(Qt::Orientation orientation, QWidget *parent = nullptr);

protected:
  QSplitterHandle *createHandle() override;
};
