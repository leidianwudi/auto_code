/**
 * @file aui_error_tool_tip.cpp
 * @brief 可选中/复制的错误提示弹窗实现
 */

#include "aui_error_tool_tip.h"

#include <QApplication>
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

  // 安装全局事件过滤器，检测点击外部区域
  qApp->installEventFilter(this);

  // 配置关闭定时器：鼠标离开后延迟 200ms 关闭（给用户时间复制文本）
  m_closeTimer.setSingleShot(true);
  m_closeTimer.setInterval(200);
  connect(&m_closeTimer, &QTimer::timeout, this, &AuiErrorToolTip::close);
}

// ──────────────────────────────────────────────────────────────
//  事件处理
// ──────────────────────────────────────────────────────────────

bool AuiErrorToolTip::eventFilter(QObject *watched, QEvent *event) {
  // 检测全局鼠标按下事件
  if (event->type() == QEvent::MouseButtonPress) {
    QWidget *widget = qobject_cast<QWidget *>(watched);
    if (widget && !isAncestorOf(widget) && widget != this) {
      // 点击了本窗口以外的区域，立即关闭
      close();
      return true;
    }
  }

  return QFrame::eventFilter(watched, event);
}

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

void AuiErrorToolTip::leaveEvent(QEvent *event) {
  QFrame::leaveEvent(event);
  // 鼠标离开时启动定时器（即使有选中文本也会关闭）
  scheduleClose();
}

void AuiErrorToolTip::scheduleClose() {
  // 如果定时器已在运行，不重复启动
  if (!m_closeTimer.isActive()) {
    m_closeTimer.start();
  }
}