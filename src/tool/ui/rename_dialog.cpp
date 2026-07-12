/**
 * @file rename_dialog.cpp
 * @brief 重命名对话框 — 实现
 */

#include "rename_dialog.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

#include "aui_window.h"
#include "component/aui_button.h"
#include "component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

RenameDialog::RenameDialog(const QString &oldName, QWidget *parent)
    : QDialog(parent), m_oldName(oldName) {
  setupUI();
}

// ════════════════════════════════════════════════════════════
//  setupUI — 布局界面
// ════════════════════════════════════════════════════════════

void RenameDialog::setupUI() {
  setWindowTitle(QStringLiteral("重命名"));
  setFixedSize(360, 180);
  setModal(true);

  // ── 无边框对话框 ──
  AuiWindow::setupFramelessDialog(this);

  // ── 自定义标题栏 ──
  TitleBarOptions opts;
  opts.title = QStringLiteral("重命名");
  opts.showMinButton = false;
  opts.showMaxButton = false;
  opts.closeRejectsDialog = true;
  auto tb = AuiWindow::createTitleBar(this, opts);

  // ── 内容区域 ──
  auto *contentWidget = new QWidget;
  auto *contentLayout = new QVBoxLayout(contentWidget);
  contentLayout->setContentsMargins(16, 12, 16, 16);
  contentLayout->setSpacing(10);

  // ── 名称输入 ──
  auto *nameLayout = new QHBoxLayout;
  auto *nameLabel = new QLabel(QStringLiteral("新名称："), this);
  m_nameEdit = new QLineEdit(m_oldName, this);
  m_nameEdit->selectAll();
  nameLayout->addWidget(nameLabel);
  nameLayout->addWidget(m_nameEdit, 1);
  contentLayout->addLayout(nameLayout);

  // ── 错误提示 ──
  m_errorLabel = new QLabel(this);
  m_errorLabel->setStyleSheet(
      QStringLiteral("color: %1; font-size: %2px;")
          .arg(AuiStyle::errorTextColor().name(), QString::number(AuiStyle::titleFontSize())));
  m_errorLabel->hide();
  contentLayout->addWidget(m_errorLabel);

  contentLayout->addStretch();

  // ── 按钮 ──
  auto *btnLayout = new QHBoxLayout;
  btnLayout->addStretch();
  m_okBtn = new QPushButton(QStringLiteral("确定"), this);
  m_okBtn->setDefault(true);
  m_okBtn->setMinimumWidth(80);
  auto *cancelBtn = new QPushButton(QStringLiteral("取消"), this);
  cancelBtn->setMinimumWidth(80);
  AuiButton::applyDialogButtonStyle(m_okBtn);
  AuiButton::applyDialogButtonStyle(cancelBtn);
  btnLayout->addWidget(m_okBtn);
  btnLayout->addWidget(cancelBtn);
  contentLayout->addLayout(btnLayout);

  // ── 信号连接 ──
  connect(m_okBtn, &QPushButton::clicked, this, &RenameDialog::onAccept);
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

  m_nameEdit->setFocus();

  // ── 应用窗口框架 ──
  AuiWindow::applyWindowFrame(this, tb.titleBar, contentWidget);

  contentLayout->setContentsMargins(16, 12, 16, 16);
  contentLayout->setSpacing(10);
}

// ════════════════════════════════════════════════════════════
//  showEvent — 显示时安装遮罩层
// ════════════════════════════════════════════════════════════

void RenameDialog::showEvent(QShowEvent *event) {
  QDialog::showEvent(event);
  m_overlay = AuiWindow::installModalOverlay(this);
}

// ════════════════════════════════════════════════════════════
//  done — 关闭时移除遮罩层
// ════════════════════════════════════════════════════════════

void RenameDialog::done(int r) {
  AuiWindow::removeModalOverlay(m_overlay);
  m_overlay = nullptr;
  QDialog::done(r);
}

// ════════════════════════════════════════════════════════════
//  onAccept — 确认按钮
// ════════════════════════════════════════════════════════════

void RenameDialog::onAccept() {
  QString name = m_nameEdit->text().trimmed();
  if (name.isEmpty()) {
    m_errorLabel->setText(QStringLiteral("名称不能为空"));
    m_errorLabel->show();
    m_nameEdit->setFocus();
    return;
  }
  if (name == m_oldName) {
    m_errorLabel->setText(QStringLiteral("名称未发生变化"));
    m_errorLabel->show();
    m_nameEdit->setFocus();
    return;
  }
  accept();
}

// ════════════════════════════════════════════════════════════
//  newName — 获取新名称
// ════════════════════════════════════════════════════════════

QString RenameDialog::newName() const { return m_nameEdit->text().trimmed(); }

// ════════════════════════════════════════════════════════════
//  getNewName — 静态便捷方法
// ════════════════════════════════════════════════════════════

QString RenameDialog::getNewName(QWidget *parent, const QString &oldName, bool *ok) {
  RenameDialog dlg(oldName, parent);
  if (dlg.exec() == QDialog::Accepted) {
    if (ok) *ok = true;
    return dlg.newName();
  }
  if (ok) *ok = false;
  return QString();
}