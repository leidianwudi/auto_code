/**
 * @file aui_window.cpp
 * @brief 无边框窗口工具类实现
 */

#include "aui_window.h"

#include <QComboBox>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
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
  window->setWindowIcon(QIcon(appIconPixmap(256)));
}

// ════════════════════════════════════════════════════════════
//  无边框对话框设置
// ════════════════════════════════════════════════════════════

void AuiWindow::setupFramelessDialog(QDialog *dialog) {
  // 保留 Qt::Dialog 标志，否则 QDialog::exec() 无法正常显示
  dialog->setWindowFlags(dialog->windowFlags() | Qt::FramelessWindowHint);
  dialog->setStyleSheet(AuiStyle::mainStyleSheet() + AuiStyle::dialogStyleSheet());
  dialog->setWindowIcon(QIcon(appIconPixmap(256)));
}

// ════════════════════════════════════════════════════════════
//  模态对话框样式设置（保留原生标题栏）
// ════════════════════════════════════════════════════════════

void AuiWindow::setupDialogStyle(QDialog *dialog) {
  dialog->setWindowIcon(QIcon(appIconPixmap(256)));

  // 设置标题栏颜色与窗口背景一致（Windows DWM API）
#ifdef Q_OS_WIN
  HWND hwnd = reinterpret_cast<HWND>(dialog->winId());
  COLORREF captionColor = RGB(AuiStyle::background().red(), AuiStyle::background().green(),
                              AuiStyle::background().blue());
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

  // ── 标题栏背景色圆角矩形 ──
  const qreal r = size * 0.2;
  QPainterPath path;
  path.addRoundedRect(QRectF(0, 0, size, size), r, r);
  p.fillPath(path, AuiStyle::titleBarBackground());

  // ── AC 文字 ──
  QPen pen(AuiStyle::appIconColor(), qMax(1.0, size * 0.08));
  pen.setCapStyle(Qt::RoundCap);
  p.setPen(pen);
  QFont f(QStringLiteral("Consolas"), qMax(1, size * 12 / 20), QFont::Black);
  p.setFont(f);
  p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, AuiStyle::appName());
  p.end();
  return px;
}

QLabel *AuiWindow::createAppIcon(QWidget *parent, int size) {
  auto *label = new QLabel(parent);
  label->setPixmap(appIconPixmap(size));
  return label;
}

// ════════════════════════════════════════════════════════════
//  内部辅助类
// ════════════════════════════════════════════════════════════

namespace {
class MaximizeButtonFilter : public QObject {
public:
  MaximizeButtonFilter(QWidget *window, QPushButton *maxBtn, QObject *parent = nullptr)
      : QObject(parent), m_window(window), m_maxBtn(maxBtn) {}

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (watched == m_window && event->type() == QEvent::WindowStateChange) {
      AuiButton::updateMaximizeIcon(m_maxBtn, m_window->isMaximized());
    }
    return false;
  }

private:
  QWidget *m_window;
  QPushButton *m_maxBtn;
};

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

// ════════════════════════════════════════════════════════════
//  创建标准标题栏
// ════════════════════════════════════════════════════════════

TitleBarResult AuiWindow::createTitleBar(QWidget *window, const TitleBarOptions &options) {
  TitleBarResult result;

  auto *titleBar = new QWidget;
  AuiStyle::applyTitleBarStyle(titleBar);
  auto *titleLayout = new QHBoxLayout(titleBar);
  titleLayout->setContentsMargins(AuiStyle::titleBarMargins());
  titleLayout->setSpacing(AuiStyle::titleBarSpacing());
  result.titleBar = titleBar;

  // ── AC 程序图标 ──
  titleLayout->addWidget(createAppIcon());
  result.contentInsertIndex = titleLayout->count();

  // ── 标题文字（左对齐模式：在 AC 图标之后、stretch 之前） ──
  QLabel *titleLabel = nullptr;
  if (!options.title.isEmpty() && !options.titleRightAligned) {
    titleLabel = new QLabel(options.title);
    AuiStyle::applyTitleLabelStyle(titleLabel);
    titleLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    titleLayout->addWidget(titleLabel);
    result.contentInsertIndex = titleLayout->count();
  }

  // ── 弹性空间 ──
  titleLayout->addStretch();

  // ── 标题文字（右对齐模式：在 stretch 之后、控制按钮之前） ──
  if (!options.title.isEmpty() && options.titleRightAligned) {
    titleLabel = new QLabel(options.title);
    AuiStyle::applyTitleLabelStyle(titleLabel);
    titleLayout->addWidget(titleLabel);
    titleLayout->addSpacing(8);
  }
  result.titleLabel = titleLabel;

  // ── 窗口控制按钮 ──
  QPushButton *maxBtn = nullptr;

  if (options.showMinButton) {
    auto *minBtn = AuiButton::createMinButton();
    QObject::connect(minBtn, &QPushButton::clicked, window, &QWidget::showMinimized);
    titleLayout->addWidget(minBtn);
  }

  if (options.showMaxButton) {
    maxBtn = AuiButton::createMaxButton();
    QObject::connect(maxBtn, &QPushButton::clicked, window, [window]() { toggleMaximize(window); });
    titleLayout->addWidget(maxBtn);
  }

  if (options.showCloseButton) {
    auto *closeBtn = AuiButton::createCloseButton();
    if (options.closeRejectsDialog) {
      QObject::connect(closeBtn, &QPushButton::clicked, window, [window]() {
        if (auto *dlg = qobject_cast<QDialog *>(window)) dlg->reject();
      });
    } else {
      QObject::connect(closeBtn, &QPushButton::clicked, window, &QWidget::close);
    }
    titleLayout->addWidget(closeBtn);
  }

  // ── 监听窗口状态变化，自动更新最大化按钮图标 ──
  if (maxBtn) {
    AuiButton::updateMaximizeIcon(maxBtn, window->isMaximized());
    window->installEventFilter(new MaximizeButtonFilter(window, maxBtn));
  }

  return result;
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
  // 设置窗口边框：四周 1px，标题栏通过负 margin 覆盖左右边框区域
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