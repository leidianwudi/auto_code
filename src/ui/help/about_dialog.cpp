/**
 * @file about_dialog.cpp
 * @brief 关于对话框 — 实现
 */

#include "about_dialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "src/util/ui/aui_window.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  关于信息常量
// ════════════════════════════════════════════════════════════

namespace AboutInfo {
inline const char *kAppName = "Auto Code";  ///< 软件名称
inline const char *kVersion = "1.0.1";      ///< 版本号
}  // namespace AboutInfo

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) { setupUI(); }

// ════════════════════════════════════════════════════════════
//  setupUI — 布局界面
// ════════════════════════════════════════════════════════════

void AboutDialog::setupUI() {
  setWindowTitle(QString::fromUtf8(AboutInfo::kAppName));
  setFixedSize(360, 240);

  // ── 无边框对话框 ──
  AuiWindow::setupFramelessDialog(this);

  // ── 自定义标题栏 ──
  TitleBarOptions opts;
  opts.title = QStringLiteral("关于");
  opts.showMinButton = false;
  opts.showMaxButton = false;
  opts.closeRejectsDialog = true;
  auto tb = AuiWindow::createTitleBar(this, opts);

  // ── 内容区域 ──
  auto *contentWidget = new QWidget;
  auto *contentLayout = new QVBoxLayout(contentWidget);
  contentLayout->setContentsMargins(24, 20, 24, 20);
  contentLayout->setSpacing(12);

  // ── AC 图标 ──
  QLabel *iconLabel = AuiWindow::createAppIcon(this, 48);
  iconLabel->setAlignment(Qt::AlignCenter);
  contentLayout->addWidget(iconLabel);

  // ── 软件名称 ──
  auto *nameLabel = new QLabel(QString::fromUtf8(AboutInfo::kAppName), this);
  QFont nameFont = AuiStyle::createEditorFont();
  nameFont.setBold(true);
  nameFont.setPointSize(nameFont.pointSize() + 6);
  nameLabel->setAlignment(Qt::AlignCenter);
  nameLabel->setFont(nameFont);
  contentLayout->addWidget(nameLabel);

  // ── 版本号 ──
  auto *versionLabel =
      new QLabel(QStringLiteral("版本号 %1").arg(QString::fromUtf8(AboutInfo::kVersion)), this);
  QFont versionFont = AuiStyle::createEditorFont();
  versionFont.setPointSize(versionFont.pointSize() + 1);
  versionLabel->setAlignment(Qt::AlignCenter);
  versionLabel->setFont(versionFont);
  versionLabel->setStyleSheet(
      QStringLiteral("color: %1;").arg(AuiStyle::secondaryTextColor().name()));
  contentLayout->addWidget(versionLabel);

  contentLayout->addStretch();

  // ── 关闭按钮 ──
  auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
  AuiButton::applyDialogButtonStyle(closeBtn);
  closeBtn->setMinimumWidth(90);
  auto *btnLayout = new QHBoxLayout;
  btnLayout->addStretch();
  btnLayout->addWidget(closeBtn);
  btnLayout->addStretch();
  contentLayout->addLayout(btnLayout);

  connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

  // ── 应用窗口框架 ──
  AuiWindow::applyWindowFrame(this, tb.titleBar, contentWidget);
}

// ════════════════════════════════════════════════════════════
//  nativeEvent — Win32 原生事件（标题栏拖拽 / 边框拉伸）
// ════════════════════════════════════════════════════════════

#if defined(Q_OS_WIN)
bool AboutDialog::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
  if (AuiWindow::handleNativeEvent(this, m_titleBar, eventType, message, result)) return true;
  return QDialog::nativeEvent(eventType, message, result);
}
#endif
