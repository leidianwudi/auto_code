/**
 * @file aui_message_box.cpp
 * @brief 消息对话框工具类实现
 */

#include "aui_message_box.h"

#include <QCursor>
#include <QDialog>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

#include "../aui_window.h"
#include "aui_button.h"
#include "aui_style.h"

namespace {

/// 消息对话框 — 私有实现，不暴露给外部
class MessageBoxDialog : public QDialog {
public:
  MessageBoxDialog(const QString &title, const QString &text, bool showCancel, QWidget *parent)
      : QDialog(parent), m_showCancel(showCancel) {
    setupUI(title, text);
  }

private:
  void setupUI(const QString &title, const QString &text) {
    setWindowTitle(title);

    AuiWindow::setupFramelessDialog(this);

    TitleBarOptions opts;
    opts.title = title;
    opts.showMinButton = false;
    opts.showMaxButton = false;
    opts.closeRejectsDialog = true;
    auto tb = AuiWindow::createTitleBar(this, opts);

    auto *contentWidget = new QWidget;
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(16, 12, 16, 12);
    contentLayout->setSpacing(5);

    auto *label = new QLabel(text, this);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);
    // 允许鼠标/键盘选中文本，方便复制错误内容
    label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    contentLayout->addWidget(label);

    auto btns = AuiButton::createDialogButtons(this, m_showCancel);
    connect(btns.okBtn, &QPushButton::clicked, this, &QDialog::accept);
    if (btns.cancelBtn) connect(btns.cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    contentLayout->addLayout(btns.layout);

    btns.okBtn->setFocus();

    AuiWindow::applyWindowFrame(this, tb.titleBar, contentWidget);

    applyAutoSize(text, label, contentLayout, tb.titleBar, btns.layout);
  }

  /// 根据内容自动缩放窗口：内容少时缩小，避免固定大窗口留白过多；内容多时受 maxW 约束并换行。
  void applyAutoSize(const QString &text, QLabel *label, QLayout *contentLayout, QWidget *titleBar,
                     QLayout *btnsLayout) {
    constexpr int kMinW = 400;  // 最小宽度
    constexpr int kMaxW = 620;  // 最大宽度（超长文本在此宽度内换行）
    constexpr int frameHMargin = 2;      // WindowFrame 左右边框 1px×2
    constexpr int frameVMargin = 1;      // WindowFrame 底部边框 1px（顶部被标题栏覆盖）

    const QMargins cm = contentLayout->contentsMargins();
    const int hMargins = cm.left() + cm.right();   // 水平内边距
    const int vMargins = cm.top() + cm.bottom() + frameVMargin;  // 垂直内边距 + 底部边框

    // 计算文本在多行情况下的最宽单行宽度，作为理想内容宽度
    QStringList lines = text.split(QLatin1Char('\n'));
    QFontMetrics fm = label->fontMetrics();
    int maxLineW = 0;
    for (const QString &line : lines) maxLineW = qMax(maxLineW, fm.horizontalAdvance(line));

    // 文本可用宽度：介于下限与上限之间（不足下限则宁可在下限宽度内换行居中）
    const int availTextW = qBound(kMinW - frameHMargin - hMargins, maxLineW,
                                  kMaxW - frameHMargin - hMargins);

    // 固定文本渲染宽度并据此取换行后高度
    label->setFixedWidth(availTextW);
    int textH = label->heightForWidth(availTextW);
    if (textH <= 0) textH = fm.height() * qMax(1, lines.size());

    const int titleH = titleBar->sizeHint().height();
    const int buttonsH = btnsLayout->sizeHint().height();
    const int spacing = contentLayout->spacing();

    int desiredW = qBound(kMinW, availTextW + frameHMargin + hMargins, kMaxW);
    int desiredH = titleH + vMargins + textH + spacing + buttonsH + 6;  // 额外 6px 视觉留白

    // 防止在低分辨率屏幕上超高
    if (QScreen *screen = QGuiApplication::screenAt(QCursor::pos())) {
      desiredH = qMin(desiredH, screen->availableGeometry().height());
    }

    resize(desiredW, desiredH);
  }

  bool m_showCancel;
};

}  // namespace

void AuiMessageBox::show(QWidget *parent, const QString &title, const QString &text) {
  MessageBoxDialog dlg(title, text, false, parent);
  dlg.exec();
}

bool AuiMessageBox::confirm(QWidget *parent, const QString &title, const QString &text) {
  MessageBoxDialog dlg(title, text, true, parent);
  return dlg.exec() == QDialog::Accepted;
}