/**
 * @file create_ui.cpp
 * @brief 新建文件对话框 — 视图层实现
 */

#include "create_ui.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

#include "create_model.h"
#include "src/tool/ui/component/aui_combo_box.h"
#include "src/tool/ui/component/aui_style.h"
#include "src/tool/ui/component/aui_window.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

CreateUi::CreateUi(QWidget *parent) : QDialog(parent) { setupUI(); }

// ════════════════════════════════════════════════════════════
//  setupUI — 布局界面
// ════════════════════════════════════════════════════════════

void CreateUi::setupUI() {
  setWindowTitle(QStringLiteral("新建"));
  setFixedSize(360, 200);
  setModal(true);

  // ── 应用统一样式（复用 AuiWindow）
  // 保留原生标题栏，通过 DWM 设置标题栏颜色与窗口背景一致
  AuiWindow::setupDialogStyle(this);

  // ── 设置窗口图标为 AC 图标
  setWindowIcon(QIcon(AuiWindow::appIconPixmap(16)));

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(16, 16, 16, 16);
  mainLayout->setSpacing(12);

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
  mainLayout->addLayout(typeLayout);

  // ── 名称 ──
  auto *nameLayout = new QHBoxLayout;
  auto *nameLabel = new QLabel(QStringLiteral("名    称:"), this);
  nameLabel->setFixedWidth(70);
  m_nameEdit = new QLineEdit(this);
  m_nameEdit->setPlaceholderText(QStringLiteral("请输入文件或文件夹名称"));
  nameLayout->addWidget(nameLabel);
  nameLayout->addWidget(m_nameEdit, 1);
  mainLayout->addLayout(nameLayout);

  // ── 错误提示 ──
  m_errorLabel = new QLabel(this);
  m_errorLabel->setStyleSheet(
      QStringLiteral("color: %1; font-size: %2px;")
          .arg(AuiStyle::errorTextColor().name(), QString::number(AuiStyle::titleFontSize())));
  m_errorLabel->hide();
  mainLayout->addWidget(m_errorLabel);

  // ── 弹性空间 ──
  mainLayout->addStretch();

  // ── 按钮 ──
  auto *btnLayout = new QHBoxLayout;
  btnLayout->addStretch();
  m_okBtn = new QPushButton(QStringLiteral("确定"), this);
  m_okBtn->setDefault(true);
  m_cancelBtn = new QPushButton(QStringLiteral("取消"), this);
  btnLayout->addWidget(m_okBtn);
  btnLayout->addWidget(m_cancelBtn);
  mainLayout->addLayout(btnLayout);

  // ── 信号连接 ──
  connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &CreateUi::onFileTypeChanged);
  connect(m_okBtn, &QPushButton::clicked, this, &CreateUi::onAccept);
  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

  // 初始焦点在名称输入框
  m_nameEdit->setFocus();
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