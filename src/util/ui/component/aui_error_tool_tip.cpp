/**
 * @file aui_error_tool_tip.cpp
 * @brief 可选中/复制的错误提示弹窗实现
 */

#include "aui_error_tool_tip.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QVBoxLayout>

#include "aui_style.h"

// ──────────────────────────────────────────────────────────────
//  构造
// ──────────────────────────────────────────────────────────────

AuiErrorToolTip::AuiErrorToolTip(const QString &text, QWidget *parent)
    : QFrame(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) {
  setAttribute(Qt::WA_DeleteOnClose);
  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_TransparentForMouseEvents, false);
  setObjectName(QStringLiteral("AuiErrorToolTip"));

  // 样式：圆角边框 + 浅黄背景，与 QToolTip 风格一致
  setStyleSheet(AuiStyle::errorToolTipStyleSheet());

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  m_label = new QLabel(text);
  m_label->setWordWrap(false);
  m_label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
  m_label->setStyleSheet(QStringLiteral("color: %1;").arg(AuiStyle::errorTextColor().name()));
  m_label->setCursor(Qt::IBeamCursor);
  m_label->setFont(AuiStyle::createEditorFont());
  layout->addWidget(m_label);
}

// ──────────────────────────────────────────────────────────────
//  事件处理
// ──────────────────────────────────────────────────────────────

void AuiErrorToolTip::keyPressEvent(QKeyEvent *event) {
  if (event->matches(QKeySequence::Copy)) {
    QString sel = m_label->selectedText();
    if (!sel.isEmpty()) {
      QGuiApplication::clipboard()->setText(sel);
    }
    return;
  }
  if (event->key() == Qt::Key_Escape) {
    close();
    return;
  }
  QFrame::keyPressEvent(event);
}

void AuiErrorToolTip::mouseReleaseEvent(QMouseEvent *event) { QFrame::mouseReleaseEvent(event); }