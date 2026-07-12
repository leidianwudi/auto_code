/**
 * @file create_ui.cpp
 * @brief 新建文件对话框 — 视图层实现
 */

#include "create_ui.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "create_model.h"
#include "src/tool/ui/aui_window.h"
#include "src/tool/ui/component/aui_button.h"
#include "src/tool/ui/component/aui_combo_box.h"
#include "src/tool/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

CreateUi::CreateUi(QWidget *parent) : QDialog(parent) { setupUI(); }

// ════════════════════════════════════════════════════════════
//  setupUI — 布局界面
// ════════════════════════════════════════════════════════════

void CreateUi::setupUI() {
  setWindowTitle(QStringLiteral("新建"));
  setFixedSize(400, 220);
  // 不调用 setModal(true)，避免 Qt 禁用父窗口导致鼠标消息被 Windows 丢弃
  // 模态效果由遮罩层 + 事件过滤器实现

  // ── 无边框对话框（保留 Qt::Dialog 标志，复用 AuiWindow 统一样式） ──
  AuiWindow::setupFramelessDialog(this);

  // ════════════════════════════════════════════════════════════
  //  自定义标题栏（AC 图标 + 标题文字 + 关闭按钮）
  // ════════════════════════════════════════════════════════════
  TitleBarOptions opts;
  opts.title = QStringLiteral("新建");
  opts.showMinButton = false;
  opts.showMaxButton = false;
  opts.closeRejectsDialog = true;
  auto tb = AuiWindow::createTitleBar(this, opts);

  // ════════════════════════════════════════════════════════════
  //  内容区域
  // ════════════════════════════════════════════════════════════
  auto *contentWidget = new QWidget;
  auto *contentLayout = new QVBoxLayout(contentWidget);
  contentLayout->setContentsMargins(16, 8, 16, 16);
  contentLayout->setSpacing(12);

  // ── 文件类型 ──
  auto *typeLayout = new QHBoxLayout;
  auto *typeLabel = new QLabel(QStringLiteral("文件类型:"), this);
  typeLabel->setFixedWidth(70);
  m_typeCombo = AuiComboBox::create(this);
  m_typeCombo->addItem(CreateModel::fileTypeLabel(CreateModel::Folder));
  m_typeCombo->addItem(CreateModel::fileTypeLabel(CreateModel::Ac));
  m_typeCombo->addItem(CreateModel::fileTypeLabel(CreateModel::Tpl));
  m_typeCombo->addItem(CreateModel::fileTypeLabel(CreateModel::Json));
  m_typeCombo->addItem(CreateModel::fileTypeLabel(CreateModel::Tpljson));
  typeLayout->addWidget(typeLabel);
  typeLayout->addWidget(m_typeCombo, 1);
  contentLayout->addLayout(typeLayout);

  // ── 名称 ──
  auto *nameLayout = new QHBoxLayout;
  auto *nameLabel = new QLabel(QStringLiteral("名    称:"), this);
  nameLabel->setFixedWidth(70);
  m_nameEdit = new QLineEdit(this);
  m_nameEdit->setPlaceholderText(QStringLiteral("请输入文件或文件夹名称"));
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

  // ── 弹性空间 ──
  contentLayout->addStretch();

  // ── 按钮 ──
  auto *btnLayout = new QHBoxLayout;
  btnLayout->addStretch();
  m_okBtn = new QPushButton(QStringLiteral("确定"), this);
  m_okBtn->setDefault(true);
  m_okBtn->setMinimumWidth(80);
  m_cancelBtn = new QPushButton(QStringLiteral("取消"), this);
  m_cancelBtn->setMinimumWidth(80);
  AuiButton::applyDialogButtonStyle(m_okBtn);
  AuiButton::applyDialogButtonStyle(m_cancelBtn);
  btnLayout->addWidget(m_okBtn);
  btnLayout->addWidget(m_cancelBtn);
  contentLayout->addLayout(btnLayout);

  // ── 信号连接 ──
  connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &CreateUi::onFileTypeChanged);
  connect(m_okBtn, &QPushButton::clicked, this, &CreateUi::onAccept);
  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

  // 初始焦点在名称输入框
  m_nameEdit->setFocus();

  // ── 应用窗口框架（标题栏 + 内容 + 1px 边框） ──
  AuiWindow::applyWindowFrame(this, tb.titleBar, contentWidget);

  // applyWindowFrame 会重置内容布局的边距和间距，需要恢复
  contentLayout->setContentsMargins(16, 8, 16, 16);
  contentLayout->setSpacing(12);
}

// ════════════════════════════════════════════════════════════
//  文件类型切换 — 自动更新名称后缀提示
// ════════════════════════════════════════════════════════════

void CreateUi::onFileTypeChanged(int index) {
  Q_UNUSED(index)
  QString suffix = currentSuffix();
  m_nameEdit->setPlaceholderText(
      suffix.isEmpty() ? QStringLiteral("请输入文件夹名称")
                       : QStringLiteral("请输入文件名称（自动添加 %1 后缀）").arg(suffix));
}

// ════════════════════════════════════════════════════════════
//  确认按钮
// ════════════════════════════════════════════════════════════

void CreateUi::onAccept() {
  QString name = m_nameEdit->text().trimmed();
  if (name.isEmpty()) {
    setError(QStringLiteral("名称不能为空"));
    return;
  }
  accept();
}

// ════════════════════════════════════════════════════════════
//  工具方法
// ════════════════════════════════════════════════════════════

int CreateUi::fileTypeIndex() const { return m_typeCombo->currentIndex(); }

QString CreateUi::fileName() const { return m_nameEdit->text().trimmed(); }

void CreateUi::setError(const QString &msg) {
  m_errorLabel->setText(msg);
  m_errorLabel->show();
}

void CreateUi::clearError() {
  m_errorLabel->clear();
  m_errorLabel->hide();
}

QString CreateUi::currentSuffix() const {
  return CreateModel::suffix(static_cast<CreateModel::FileType>(m_typeCombo->currentIndex()));
}

// ════════════════════════════════════════════════════════════
//  showEvent — 显示时安装遮罩层
// ════════════════════════════════════════════════════════════

void CreateUi::showEvent(QShowEvent *event) {
  QDialog::showEvent(event);
  m_overlay = AuiWindow::installModalOverlay(this);
}

// ════════════════════════════════════════════════════════════
//  done — 关闭时移除遮罩层
// ════════════════════════════════════════════════════════════

void CreateUi::done(int r) {
  AuiWindow::removeModalOverlay(m_overlay);
  m_overlay = nullptr;
  QDialog::done(r);
}