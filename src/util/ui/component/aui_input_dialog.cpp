/**
 * @file aui_input_dialog.cpp
 * @brief 自绘样式单行输入对话框实现
 */

#include "aui_input_dialog.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "../aui_window.h"
#include "aui_button.h"

namespace {

/// 输入对话框 — 私有实现，不暴露给外部
class InputDialog : public QDialog {
public:
  InputDialog(const QString &title, const QString &label, const QString &init, QWidget *parent)
      : QDialog(parent) {
    setupUI(title, label, init);
  }

  /// 返回输入框的去除首尾空白后的文本
  QString text() const { return m_edit->text().trimmed(); }

private:
  void setupUI(const QString &title, const QString &label, const QString &init) {
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
    contentLayout->setSpacing(8);

    auto *labelPtr = new QLabel(label, contentWidget);
    contentLayout->addWidget(labelPtr);

    m_edit = new QLineEdit(init, contentWidget);
    m_edit->setFocus();
    m_edit->selectAll();
    m_edit->clearFocus();
    contentLayout->addWidget(m_edit);
    connect(m_edit, &QLineEdit::returnPressed, this, &QDialog::accept);

    auto btns = AuiButton::createDialogButtons(this, true);
    connect(btns.okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(btns.cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    contentLayout->addLayout(btns.layout);

    AuiWindow::applyWindowFrame(this, tb.titleBar, contentWidget);

    setMinimumWidth(360);
    resize(400, sizeHint().height());
    // 标题栏绘制完成后，重新聚焦输入框
    m_edit->setFocus();
  }

  QLineEdit *m_edit = nullptr;
};

}  // namespace

QString AuiInputDialog::getText(QWidget *parent, const QString &title, const QString &label,
                                const QString &init) {
  InputDialog dlg(title, label, init, parent);
  const bool accepted = dlg.exec() == QDialog::Accepted;
  if (!accepted) return QString();
  return dlg.text();
}