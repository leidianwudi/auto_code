/**
 * @file aui_error_tool_tip.h
 * @brief 可选中/复制的错误提示弹窗
 *
 * 替代 QToolTip，支持：
 * - 鼠标悬停不消失
 * - 文本选中 + Ctrl+C 复制
 * - 按 Escape 关闭
 */

#pragma once

#include <QFrame>
#include <QLabel>
#include <QString>

class QKeyEvent;
class QMouseEvent;

/**
 * @class AuiErrorToolTip
 * @brief 可选中/复制的自定义错误提示弹窗
 *
 * 基于 QFrame 的自定义弹窗，使用 Qt::Tool 窗口标志，
 * 不会自动消失，用户可选中文本并使用 Ctrl+C 复制错误信息。
 */
class AuiErrorToolTip : public QFrame {
  Q_OBJECT

public:
  /// @param text 错误提示文本
  /// @param parent 父窗口
  explicit AuiErrorToolTip(const QString &text, QWidget *parent = nullptr);

protected:
  void keyPressEvent(QKeyEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  QLabel *m_label = nullptr;
};