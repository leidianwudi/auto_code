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
    setWindowModality(Qt::WindowModal);

    AuiWindow::setupFramelessDialog(this);

    TitleBarOptions opts;
    opts.title = title;
    opts.showMinButton = false;
    opts.showMaxButton = false;
    opts.closeRejectsDialog = true;
    auto tb = AuiWindow::createTitleBar(this, opts);

    auto *contentWidget = new QWidget;
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(16, 12, 16, 16);
    contentLayout->setSpacing(10);

    auto *label = new QLabel(text, this);
    label->setWordWrap(true);
    contentLayout->addWidget(label);
    contentLayout->addStretch();

    auto *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    auto *okBtn = new QPushButton(QStringLiteral("确定"), this);
    okBtn->setDefault(true);
    okBtn->setMinimumWidth(80);
    AuiButton::applyDialogButtonStyle(okBtn);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(okBtn);

    if (m_showCancel) {
      auto *cancelBtn = new QPushButton(QStringLiteral("取消"), this);
      cancelBtn->setMinimumWidth(80);
      AuiButton::applyDialogButtonStyle(cancelBtn);
      connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
      btnLayout->addWidget(cancelBtn);
    }

    contentLayout->addLayout(btnLayout);

    okBtn->setFocus();

    AuiWindow::applyWindowFrame(this, tb.titleBar, contentWidget);
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