/**
 * @file aui_window.cpp
 * @brief 无边框窗口工具类实现
 */

#include "aui_window.h"

#include <QComboBox>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSizeGrip>
#include <QStyle>
#include <QVBoxLayout>
#include <QWindow>

#include "aui_button.h"
#include "aui_style.h"

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>
#endif

// ════════════════════════════════════════════════════════════
//  无边框窗口基础设置
// ════════════════════════════════════════════════════════════

void AuiWindow::setupFramelessWindow(QWidget *window) {
  window->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint);
  window->setStyleSheet(AuiStyle::mainStyleSheet());
}

// ════════════════════════════════════════════════════════════
//  模态对话框样式设置
// ════════════════════════════════════════════════════════════

void AuiWindow::setupDialogStyle(QDialog *dialog) {
  // 设置标题栏颜色与窗口背景一致（Windows DWM API）
#ifdef Q_OS_WIN
  HWND hwnd = reinterpret_cast<HWND>(dialog->winId());
  COLORREF captionColor = RGB(AuiStyle::barBackground().red(), AuiStyle::barBackground().green(),
                              AuiStyle::barBackground().blue());
  DwmSetWindowAttribute(hwnd, 35, &captionColor, sizeof(captionColor));
  COLORREF textColor =
      RGB(AuiStyle::textColor().red(), AuiStyle::textColor().green(), AuiStyle::textColor().blue());
  DwmSetWindowAttribute(hwnd, 36, &textColor, sizeof(textColor));
#endif

  // 应用统一样式表，确保颜色与 MainDevUi 一致
  dialog->setStyleSheet(AuiStyle::mainStyleSheet() + AuiStyle::dialogStyleSheet());
}

// ════════════════════════════════════════════════════════════
//  AC 程序图标
// ════════════════════════════════════════════════════════════

QPixmap AuiWindow::appIconPixmap(int size) {
  QPixmap px(size, size);
  px.fill(Qt::transparent);
  QPainter p(&px);
  p.setRenderHint(QPainter::Antialiasing);
  QPen pen(AuiStyle::textColor(), 2.0);
  pen.setCapStyle(Qt::RoundCap);
  p.setPen(pen);
  QFont f(QStringLiteral("Consolas"), 11, QFont::Bold);
  p.setFont(f);
  p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, QStringLiteral("AC"));
  p.end();
  return px;
}

QLabel *AuiWindow::createAppIcon(QWidget *parent, int size) {
  auto *label = new QLabel(parent);
  label->setPixmap(appIconPixmap(size));
  return label;
}

// ════════════════════════════════════════════════════════════
//  创建标准标题栏
// ════════════════════════════════════════════════════════

QWidget *AuiWindow::createTitleBar(QWidget *parent, const QString &title, bool showMinMax) {
  auto *bar = new QWidget(parent);
  bar->setObjectName(QStringLiteral("TitleBar"));
  auto *layout = new QHBoxLayout(bar);
  layout->setContentsMargins(AuiStyle::titleBarMargins());
  layout->setSpacing(AuiStyle::titleBarSpacing());

  // ── AC 图标 ──
  layout->addWidget(createAppIcon(bar, 20));

  // ── 标题文字 ──
  auto *titleLabel = new QLabel(title, bar);
  titleLabel->setObjectName(QStringLiteral("TitleLabel"));
  layout->addWidget(titleLabel);

  layout->addStretch();

  // ── 窗口控制按钮 ──
  if (showMinMax) {
    auto *minBtn = AuiButton::createMinButton();
    layout->addWidget(minBtn);
    auto *maxBtn = AuiButton::createMaxButton();
    layout->addWidget(maxBtn);
  }

  auto *closeBtn = AuiButton::createCloseButton();
  layout->addWidget(closeBtn);

  return bar;
}

// ════════════════════════════════════════════════════════════
//  创建底部状态栏（状态文字 + QSizeGrip 可拖拽三角）
// ════════════════════════════════════════════════════════════

QWidget *AuiWindow::createStatusBar(QWidget *parent, const QString &text) {
  auto *label = new QLabel(text);
  label->setContentsMargins(4, 0, 4, 0);
  return createStatusBar(parent, label);
}

QWidget *AuiWindow::createStatusBar(QWidget *parent, QWidget *content) {
  auto *bar = new QWidget(parent);
  auto *layout = new QHBoxLayout(bar);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  layout->addWidget(content, 1);

  auto *grip = new QSizeGrip(bar);
  const QSize gs = AuiStyle::sizeGripSize();
  grip->setStyleSheet(
      QStringLiteral("QSizeGrip { width: %1px; height: %2px; }").arg(gs.width()).arg(gs.height()));
  layout->addWidget(grip);

  return bar;
}

// ════════════════════════════════════════════════════════════
//  应用窗口框架（1px 边框 QFrame）
// ════════════════════════════════════════════════════════════

void AuiWindow::applyWindowFrame(QWidget *window, QWidget *titleBar, QWidget *content) {
  // 统一设置内容控件的布局参数：零边距、零间距
  if (auto *cl = content->layout()) {
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);
  }

  auto *frame = new QFrame(window);
  frame->setObjectName(QStringLiteral("WindowFrame"));
  auto *frameLayout = new QVBoxLayout(frame);
  // 设置chuang口边框为 1px，顶部不需要额外边框（标题栏已经有间距）
  frameLayout->setContentsMargins(1, 0, 1, 1);
  frameLayout->setSpacing(0);

  if (titleBar) {
    frameLayout->addWidget(titleBar);
  }
  frameLayout->addWidget(content, 1);

  // QMainWindow 使用 setCentralWidget，其他窗口使用外层布局
  if (auto *mw = qobject_cast<QMainWindow *>(window)) {
    mw->setCentralWidget(frame);
  } else {
    // 清除原有布局
    QLayout *oldLayout = window->layout();
    if (oldLayout) {
      // 将原有布局中的控件移到 frame 中
      while (auto *item = oldLayout->takeAt(0)) {
        delete item;
      }
      delete oldLayout;
    }
    auto *winLayout = new QVBoxLayout(window);
    winLayout->setContentsMargins(0, 0, 0, 0);
    winLayout->setSpacing(0);
    winLayout->addWidget(frame);
  }

  // 启用标题栏拖拽（使用 startSystemMove 替代 HTCAPTION，避免模态对话框关闭后拖拽失效）
  enableTitleBarDrag(window, titleBar);
}

// ════════════════════════════════════════════════════════════
//  标题栏拖拽（使用 Qt startSystemMove 替代 HTCAPTION）
// ════════════════════════════════════════════════════════════

namespace {
class TitleBarDragFilter : public QObject {
public:
  explicit TitleBarDragFilter(QWidget *window, QObject *parent = nullptr)
      : QObject(parent), m_window(window) {}

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    Q_UNUSED(watched)
    if (event->type() == QEvent::MouseButtonPress) {
      auto *me = static_cast<QMouseEvent *>(event);
      if (me->button() == Qt::LeftButton) {
        if (QWindow *hw = m_window->windowHandle()) {
          hw->startSystemMove();
        }
        return true;
      }
    }
    if (event->type() == QEvent::MouseButtonDblClick) {
      auto *me = static_cast<QMouseEvent *>(event);
      if (me->button() == Qt::LeftButton) {
        AuiWindow::toggleMaximize(m_window);
        return true;
      }
    }
    return false;
  }

private:
  QWidget *m_window;
};
}  // namespace

void AuiWindow::enableTitleBarDrag(QWidget *window, QWidget *titleBar) {
  if (window && titleBar) {
    auto *filter = new TitleBarDragFilter(window, titleBar);
    titleBar->installEventFilter(filter);
  }
}

void AuiWindow::toggleMaximize(QWidget *window) {
  if (!window) return;
  if (window->isMaximized()) {
    window->showNormal();
  } else {
    window->showMaximized();
  }
}

// ════════════════════════════════════════════════════════════
//  Win32 拉伸边框
// ════════════════════════════════════════════════════════════

void AuiWindow::enableWin32Resize(QWidget *window) {
#if defined(Q_OS_WIN)
  HWND hwnd = reinterpret_cast<HWND>(window->winId());
  LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
  style |= WS_THICKFRAME;
  SetWindowLongPtr(hwnd, GWL_STYLE, style);
  SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
#else
  Q_UNUSED(window)
#endif
}

// ════════════════════════════════════════════════════════════
//  Win32 原生事件处理
// ════════════════════════════════════════════════════════════

#if defined(Q_OS_WIN)
bool AuiWindow::handleNativeEvent(QWidget *window, QWidget *titleBar, const QByteArray &eventType,
                                  void *message, qintptr *result) {
  Q_UNUSED(eventType)
  const auto *msg = static_cast<MSG *>(message);

  // ── 抑制非客户区绘制，消除拉伸时黑色方块 ──
  if (msg->message == WM_NCPAINT) {
    *result = 0;
    return true;
  }
  if (msg->message == WM_ERASEBKGND) {
    *result = 1;
    return true;
  }

  if (msg->message == WM_NCHITTEST) {
    const int x = GET_X_LPARAM(msg->lParam);
    const int y = GET_Y_LPARAM(msg->lParam);
    const QPoint pt = window->mapFromGlobal(QPoint(x, y));
    const int bw = 5;  // 边框拉伸宽度

    // ── 四角 ──
    bool left = pt.x() < bw;
    bool right = pt.x() > window->width() - bw;
    bool top = pt.y() < bw;
    bool bottom = pt.y() > window->height() - bw;

    if (top && left) {
      *result = HTTOPLEFT;
      return true;
    }
    if (top && right) {
      *result = HTTOPRIGHT;
      return true;
    }
    if (bottom && left) {
      *result = HTBOTTOMLEFT;
      return true;
    }
    if (bottom && right) {
      *result = HTBOTTOMRIGHT;
      return true;
    }
    if (top) {
      *result = HTTOP;
      return true;
    }
    if (bottom) {
      *result = HTBOTTOM;
      return true;
    }
    if (left) {
      *result = HTLEFT;
      return true;
    }
    if (right) {
      *result = HTRIGHT;
      return true;
    }

    // ── 标题栏区域 → HTCLIENT（由 TitleBarDragFilter 通过 startSystemMove 实现拖拽）──
    // 不使用 HTCAPTION：模态对话框关闭后 Windows 原生 HTCAPTION 拖拽机制会失效。
    // 返回 HTCLIENT 让 Qt 接收鼠标事件，由事件过滤器调用 startSystemMove() 启动拖拽。
    if (titleBar && pt.y() < titleBar->height()) {
      *result = HTCLIENT;
      return true;
    }
  }
  return false;
}
#endif