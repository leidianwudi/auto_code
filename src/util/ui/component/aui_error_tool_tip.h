/**
 * @file aui_error_tool_tip.h
 * @brief 可选中/复制的错误提示弹窗
 *
 * 替代 QToolTip，支持：
 * - 鼠标悬停不消失
 * - 文本选中 + Ctrl+C 复制
 * - 点击外部自动关闭
 * - 鼠标离开自动关闭
 * - 按 Escape 关闭
 */

#pragma once

#include <QFrame>
#include <QLabel>
#include <QString>
#include <QTimer>

class QKeyEvent;
class QMouseEvent;
class QFocusEvent;
class QEvent;

/**
 * @class AuiErrorToolTip
 * @brief 可选中/复制的自定义错误提示弹窗
 *
 * 基于 QFrame 的自定义弹窗，使用 Qt::Tool 窗口标志，
 * 支持多种关闭方式：点击外部、鼠标离开、Escape键。
 */
class AuiErrorToolTip : public QFrame {
  Q_OBJECT

public:
  explicit AuiErrorToolTip(const QString &text, QWidget *parent = nullptr);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void leaveEvent(QEvent *event) override;

private slots:
  void scheduleClose();

private:
  QLabel *m_label = nullptr;
  QTimer m_closeTimer;  ///< 延迟关闭定时器
};