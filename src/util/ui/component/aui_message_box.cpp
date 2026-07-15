/**
 * @file aui_message_box.cpp
 * @brief 消息对话框工具类实现
 */

#include "aui_message_box.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
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

protected:
  void showEvent(QShowEvent *event) override {
    QDialog::showEvent(event);
    m_overlay = AuiWindow::installModalOverlay(this);
  }

  void done(int r) override {
    AuiWindow::removeModalOverlay(m_overlay);
    m_overlay = nullptr;
    QDialog::done(r);
  }

private:
  void setupUI(const QString &title, const QString &text) {
    setWindowTitle(title);
    setFixedSize(360, 180);

    AuiWindow::setupFramelessDialog(this);

    TitleBarOptions opts;
    opts.title = title;
    opts.showMinButton = false;
    opts.showMaxButton = false;
    opts.closeRejectsDialog = true;
    auto tb = AuiWindow::createTitleBar(this, opts);

    auto *contentWidget = new QWidget;
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(16, 12, 16, 24);
    contentLayout->setSpacing(10);

    auto *label = new QLabel(text, this);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(label, 1);

    auto btns = AuiButton::createDialogButtons(this, m_showCancel);
    connect(btns.okBtn, &QPushButton::clicked, this, &QDialog::accept);
    if (btns.cancelBtn) connect(btns.cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    contentLayout->addLayout(btns.layout);

    btns.okBtn->setFocus();

    AuiWindow::applyWindowFrame(this, tb.titleBar, contentWidget);

    // applyWindowFrame 会重置内容布局的边距和间距，需要恢复
    contentLayout->setContentsMargins(16, 12, 16, 12);
    contentLayout->setSpacing(5);
  }

  bool m_showCancel;
  QWidget *m_overlay = nullptr;
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