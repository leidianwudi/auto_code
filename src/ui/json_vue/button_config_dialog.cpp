/**
 * @file button_config_dialog.cpp
 * @brief 自定义操作按钮配置对话框实现
 */

#include "button_config_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "combobox_config_dialog.h"
#include "icon_loader.h"
#include "icon_picker_dialog.h"
#include "src/util/common/code_constants.h"
#include "src/util/ui/aui_window.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

ButtonConfigDialog::ButtonConfigDialog(QWidget *parent) : QDialog(parent) { setupUI(); }

ButtonConfigDialog::ButtonConfigDialog(const ButtonConfig &config, QWidget *parent)
    : QDialog(parent) {
  setupUI();
  setData(config);
}

// ════════════════════════════════════════════════════════════
//  界面构建
// ════════════════════════════════════════════════════════════

void ButtonConfigDialog::setupUI() {
  setWindowTitle(QStringLiteral("按钮配置"));
  setMinimumWidth(700);
  setMinimumHeight(600);

  // ── 无边框对话框 ──
  AuiWindow::setupFramelessDialog(this);

  // ── 自定义标题栏 ──
  TitleBarOptions opts;
  opts.title = QStringLiteral("按钮配置");
  opts.showMinButton = false;
  opts.showMaxButton = false;
  opts.closeRejectsDialog = true;
  auto tb = AuiWindow::createTitleBar(this, opts);

  // ── 内容区域 ──
  auto *contentWidget = new QWidget;
  auto *mainLayout = new QVBoxLayout(contentWidget);
  mainLayout->setContentsMargins(12, 8, 12, 12);
  mainLayout->setSpacing(8);

  // ── 基本信息 ──
  auto *basicGroup = new QLabel(QStringLiteral("基本信息"));
  basicGroup->setStyleSheet(
      QStringLiteral("font-weight: bold; color: %1;").arg(AuiStyle::secondaryTextColor().name()));
  mainLayout->addWidget(basicGroup);

  auto *basicForm = new QFormLayout();
  basicForm->setSpacing(6);
  basicForm->setLabelAlignment(Qt::AlignRight);

  m_labelEdit = new QLineEdit;
  m_labelEdit->setPlaceholderText(QStringLiteral("如：修改密码"));
  m_labelEdit->setMinimumHeight(28);
  basicForm->addRow(QStringLiteral("按钮文字:"), m_labelEdit);

  m_actionKeyEdit = new QLineEdit;
  m_actionKeyEdit->setPlaceholderText(QStringLiteral("如：resetPwd（英文标识符）"));
  m_actionKeyEdit->setMinimumHeight(28);
  basicForm->addRow(QStringLiteral("动作标识:"), m_actionKeyEdit);

  // label 改变时自动推导 actionKey
  connect(m_labelEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
    if (m_actionKeyEdit->text().isEmpty()) {
      m_actionKeyEdit->setText(deriveActionKey(text));
    }
  });

  // 图标预览按钮（点击弹出图标选择器）
  m_iconBtn = new QPushButton(QStringLiteral(" 无 "));
  m_iconBtn->setMinimumHeight(28);
  m_iconBtn->setIconSize(QSize(18, 18));
  m_iconBtn->setStyleSheet(
      QStringLiteral("QPushButton { text-align: left; padding: 2px 8px; border: 1px solid %1; "
                     "border-radius: 3px; background: %2; }"
                     "QPushButton:hover { border: 1px solid %3; }")
          .arg(AuiStyle::borderColor().name(), AuiStyle::background().name(),
               AuiStyle::hoverBackground().name()));
  connect(m_iconBtn, &QPushButton::clicked, this, [this]() {
    IconPickerDialog dlg(m_currentIconName, this);
    if (dlg.exec() == QDialog::Accepted) {
      setIconName(dlg.selectedIcon());
    }
  });

  // 清除图标按钮
  m_iconClearBtn = new QPushButton(QStringLiteral("✕"));
  m_iconClearBtn->setFixedSize(24, 28);
  m_iconClearBtn->setStyleSheet(
      QStringLiteral("QPushButton { border: none; color: %1; font-size: 14px; }"
                     "QPushButton:hover { color: %2; }")
          .arg(AuiStyle::mutedTextColor().name(), AuiStyle::textColor().name()));
  m_iconClearBtn->setVisible(false);  // 无图标时隐藏
  connect(m_iconClearBtn, &QPushButton::clicked, this, [this]() { setIconName(QString()); });

  auto *iconLayout = new QHBoxLayout;
  iconLayout->setSpacing(2);
  iconLayout->addWidget(m_iconBtn);
  iconLayout->addWidget(m_iconClearBtn);
  basicForm->addRow(QStringLiteral("图标:"), iconLayout);

  m_positionCombo = new QComboBox;
  m_positionCombo->setMinimumHeight(28);
  m_positionCombo->addItem(QStringLiteral("行操作列"), static_cast<int>(ButtonPosition::Row));
  m_positionCombo->addItem(QStringLiteral("顶部工具栏"), static_cast<int>(ButtonPosition::Toolbar));
  basicForm->addRow(QStringLiteral("位置:"), m_positionCombo);

  m_buttonTypeCombo = new QComboBox;
  m_buttonTypeCombo->setMinimumHeight(28);
  m_buttonTypeCombo->addItem(QStringLiteral("默认"), QString());
  m_buttonTypeCombo->addItem(QStringLiteral("主要"), QString::fromLatin1(JsonVueColor::kPrimary));
  m_buttonTypeCombo->addItem(QStringLiteral("成功"), QString::fromLatin1(JsonVueColor::kSuccess));
  m_buttonTypeCombo->addItem(QStringLiteral("警告"), QString::fromLatin1(JsonVueColor::kWarning));
  m_buttonTypeCombo->addItem(QStringLiteral("危险"), QString::fromLatin1(JsonVueColor::kDanger));
  basicForm->addRow(QStringLiteral("按钮样式:"), m_buttonTypeCombo);

  m_actionTypeCombo = new QComboBox;
  m_actionTypeCombo->setMinimumHeight(28);
  m_actionTypeCombo->addItem(QStringLiteral("Ajax（直接调 API）"),
                             static_cast<int>(ButtonActionType::Ajax));
  m_actionTypeCombo->addItem(QStringLiteral("Confirm（确认后调 API）"),
                             static_cast<int>(ButtonActionType::Confirm));
  m_actionTypeCombo->addItem(QStringLiteral("Dialog（打开对话框）"),
                             static_cast<int>(ButtonActionType::Dialog));
  m_actionTypeCombo->addItem(QStringLiteral("Link（跳转页面）"),
                             static_cast<int>(ButtonActionType::Link));
  basicForm->addRow(QStringLiteral("行为类型:"), m_actionTypeCombo);
  connect(m_actionTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { updateBehaviorVisibility(); });

  mainLayout->addLayout(basicForm);

  // ── Ajax 行为配置区（整体容器）──
  m_ajaxWidget = new QWidget;
  auto *ajaxLayout = new QVBoxLayout(m_ajaxWidget);
  ajaxLayout->setContentsMargins(0, 0, 0, 0);
  ajaxLayout->setSpacing(4);
  auto *ajaxTitle = new QLabel(QStringLiteral("API 配置"));
  ajaxTitle->setStyleSheet(
      QStringLiteral("font-weight: bold; color: %1;").arg(AuiStyle::secondaryTextColor().name()));
  ajaxLayout->addWidget(ajaxTitle);
  auto *ajaxForm = new QFormLayout();
  ajaxForm->setSpacing(6);
  ajaxForm->setLabelAlignment(Qt::AlignRight);
  m_apiNameEdit = new QLineEdit;
  m_apiNameEdit->setPlaceholderText(QStringLiteral("如：disableUserApi"));
  ajaxForm->addRow(QStringLiteral("API 函数名:"), m_apiNameEdit);
  ajaxLayout->addLayout(ajaxForm);
  mainLayout->addWidget(m_ajaxWidget);

  // ── Confirm 行为配置区（整体容器）──
  m_confirmWidget = new QWidget;
  auto *confirmLayout = new QVBoxLayout(m_confirmWidget);
  confirmLayout->setContentsMargins(0, 0, 0, 0);
  confirmLayout->setSpacing(4);
  auto *confirmTitle = new QLabel(QStringLiteral("确认配置"));
  confirmTitle->setStyleSheet(
      QStringLiteral("font-weight: bold; color: %1;").arg(AuiStyle::secondaryTextColor().name()));
  confirmLayout->addWidget(confirmTitle);
  auto *confirmForm = new QFormLayout();
  confirmForm->setSpacing(6);
  confirmForm->setLabelAlignment(Qt::AlignRight);
  m_confirmTextEdit = new QLineEdit;
  m_confirmTextEdit->setPlaceholderText(QStringLiteral("如：确定禁用该用户？"));
  confirmForm->addRow(QStringLiteral("确认提示:"), m_confirmTextEdit);
  m_confirmApiEdit = new QLineEdit;
  m_confirmApiEdit->setPlaceholderText(QStringLiteral("如：disableUserApi"));
  confirmForm->addRow(QStringLiteral("API 函数名:"), m_confirmApiEdit);
  confirmLayout->addLayout(confirmForm);
  mainLayout->addWidget(m_confirmWidget);

  // ── Dialog 行为配置区（整体容器）──
  m_dialogWidget = new QWidget;
  auto *dialogLayout = new QVBoxLayout(m_dialogWidget);
  dialogLayout->setContentsMargins(0, 0, 0, 0);
  dialogLayout->setSpacing(4);
  auto *dialogTitle = new QLabel(QStringLiteral("对话框配置"));
  dialogTitle->setStyleSheet(
      QStringLiteral("font-weight: bold; color: %1;").arg(AuiStyle::secondaryTextColor().name()));
  dialogLayout->addWidget(dialogTitle);
  auto *dialogForm = new QFormLayout();
  dialogForm->setSpacing(6);
  dialogForm->setLabelAlignment(Qt::AlignRight);
  m_dialogTitleEdit = new QLineEdit;
  m_dialogTitleEdit->setPlaceholderText(QStringLiteral("如：修改密码"));
  dialogForm->addRow(QStringLiteral("对话框标题:"), m_dialogTitleEdit);
  m_dialogApiEdit = new QLineEdit;
  m_dialogApiEdit->setPlaceholderText(QStringLiteral("如：resetUserPasswordApi"));
  dialogForm->addRow(QStringLiteral("提交 API:"), m_dialogApiEdit);
  dialogLayout->addLayout(dialogForm);

  auto *fieldsLabel = new QLabel(QStringLiteral("表单字段:"));
  dialogLayout->addWidget(fieldsLabel);

  m_dialogFieldsTable = new QTableWidget(0, 6);
  m_dialogFieldsTable->setHorizontalHeaderLabels(
      {QStringLiteral("标签"), QStringLiteral("字段名"), QStringLiteral("样式"),
       QString::fromUtf8(CodeConstants::UiText::kRequired), QStringLiteral("配置"),
       QString::fromUtf8(CodeConstants::UiText::kDelete)});
  m_dialogFieldsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_dialogFieldsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_dialogFieldsTable->setColumnWidth(2, 90);  // 样式
  m_dialogFieldsTable->setColumnWidth(3, 50);  // 必填
  m_dialogFieldsTable->setColumnWidth(4, 70);  // 配置
  m_dialogFieldsTable->setColumnWidth(5, 60);  // 删除
  m_dialogFieldsTable->verticalHeader()->setVisible(false);
  m_dialogFieldsTable->setMinimumHeight(120);
  dialogLayout->addWidget(m_dialogFieldsTable);

  m_addFieldBtn = new QPushButton(QStringLiteral("+ 添加字段"));
  connect(m_addFieldBtn, &QPushButton::clicked, this, &ButtonConfigDialog::onAddDialogField);
  dialogLayout->addWidget(m_addFieldBtn);
  mainLayout->addWidget(m_dialogWidget);

  // ── Link 行为配置区（整体容器）──
  m_linkWidget = new QWidget;
  auto *linkLayout = new QVBoxLayout(m_linkWidget);
  linkLayout->setContentsMargins(0, 0, 0, 0);
  linkLayout->setSpacing(4);
  auto *linkTitle = new QLabel(QStringLiteral("跳转配置"));
  linkTitle->setStyleSheet(
      QStringLiteral("font-weight: bold; color: %1;").arg(AuiStyle::secondaryTextColor().name()));
  linkLayout->addWidget(linkTitle);
  auto *linkForm = new QFormLayout();
  linkForm->setSpacing(6);
  linkForm->setLabelAlignment(Qt::AlignRight);
  m_linkPathEdit = new QLineEdit;
  m_linkPathEdit->setPlaceholderText(QStringLiteral("如：/user/log?id={id}"));
  linkForm->addRow(QStringLiteral("跳转路径:"), m_linkPathEdit);
  linkLayout->addLayout(linkForm);
  mainLayout->addWidget(m_linkWidget);

  // ── 弹性空间 ──
  mainLayout->addStretch();

  // ── 底部按钮 ──
  auto *btnLayout = new QHBoxLayout();
  btnLayout->addStretch();
  auto *okBtn = new QPushButton(QString::fromUtf8(CodeConstants::UiText::kConfirm));
  auto *cancelBtn = new QPushButton(QString::fromUtf8(CodeConstants::UiText::kCancel));
  okBtn->setMinimumWidth(80);
  cancelBtn->setMinimumWidth(80);
  AuiButton::applyDialogButtonStyle(okBtn);
  AuiButton::applyDialogButtonStyle(cancelBtn);
  connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  btnLayout->addWidget(okBtn);
  btnLayout->addSpacing(8);
  btnLayout->addWidget(cancelBtn);
  mainLayout->addLayout(btnLayout);

  // ── 应用窗口框架 ──
  AuiWindow::applyWindowFrame(this, tb.titleBar, contentWidget);

  updateBehaviorVisibility();
}

// ════════════════════════════════════════════════════════════
//  行为类型可见性更新
// ════════════════════════════════════════════════════════════

void ButtonConfigDialog::updateBehaviorVisibility() {
  auto actionType = static_cast<ButtonActionType>(m_actionTypeCombo->currentData().toInt());

  m_ajaxWidget->setVisible(actionType == ButtonActionType::Ajax);
  m_confirmWidget->setVisible(actionType == ButtonActionType::Confirm);
  m_dialogWidget->setVisible(actionType == ButtonActionType::Dialog);
  m_linkWidget->setVisible(actionType == ButtonActionType::Link);
}

// ════════════════════════════════════════════════════════════
//  actionKey 自动推导
// ════════════════════════════════════════════════════════════

QString ButtonConfigDialog::deriveActionKey(const QString &label) {
  // 从 label 提取 ASCII 字母，首字母小写
  QString result;
  for (const QChar &c : label) {
    if (c.isLetter() && c.unicode() < 128) {
      result += c;
    }
  }
  if (result.isEmpty()) {
    return QString();
  }
  result[0] = result[0].toLower();
  return result;
}

void ButtonConfigDialog::setIconName(const QString &iconName) {
  m_currentIconName = iconName;
  if (iconName.isEmpty()) {
    m_iconBtn->setText(QStringLiteral(" 无 "));
    m_iconBtn->setIcon(QIcon());
    m_iconClearBtn->setVisible(false);
    return;
  }
  // 立即显示占位图标，后台加载真实图标
  QIcon icon = IconLoader::instance().getOrCreateIcon(iconName, 18);
  m_iconBtn->setIcon(icon);
  m_iconBtn->setText(QStringLiteral(" %1").arg(IconLoader::extractShortName(iconName)));
  m_iconClearBtn->setVisible(true);

  // 后台尝试加载真实图标（成功后会替换占位图标）
  if (!IconLoader::instance().cached(iconName).isNull()) return;  // 已有真实图标
  IconLoader::instance().requestIcon(iconName, [this](const QString &name) {
    if (name != m_currentIconName) return;
    QIcon realIcon = IconLoader::instance().cached(name);
    if (!realIcon.isNull()) m_iconBtn->setIcon(realIcon);
  });
}

// ════════════════════════════════════════════════════════════
//  dialogFields 表格操作
// ════════════════════════════════════════════════════════════

void ButtonConfigDialog::onAddDialogField() {
  int row = m_dialogFieldsTable->rowCount();
  m_dialogFieldsTable->insertRow(row);

  // 标签
  auto *labelEdit = new QLineEdit;
  labelEdit->setPlaceholderText(QStringLiteral("如：新密码"));
  m_dialogFieldsTable->setCellWidget(row, 0, labelEdit);

  // 字段名
  auto *nameEdit = new QLineEdit;
  nameEdit->setPlaceholderText(QStringLiteral("如：newPassword"));
  m_dialogFieldsTable->setCellWidget(row, 1, nameEdit);

  // 样式
  auto *styleCombo = new QComboBox;
  styleCombo->addItem(QStringLiteral("文本"), QString::fromLatin1(JsonVueStyle::kText));
  styleCombo->addItem(QStringLiteral("整数"), QString::fromLatin1(JsonVueStyle::kInt));
  styleCombo->addItem(QStringLiteral("小数"), QString::fromLatin1(JsonVueStyle::kFloat));
  styleCombo->addItem(QStringLiteral("日期"), QString::fromLatin1(JsonVueStyle::kDate));
  styleCombo->addItem(QStringLiteral("下拉"), QString::fromLatin1(JsonVueStyle::kSelect));
  styleCombo->addItem(QStringLiteral("文本域"), QString::fromLatin1(JsonVueStyle::kTextarea));
  m_dialogFieldsTable->setCellWidget(row, 2, styleCombo);

  // 必填
  auto *requiredCheck = new QCheckBox;
  requiredCheck->setChecked(false);
  m_dialogFieldsTable->setCellWidget(row, 3, requiredCheck);

  // 配置按钮（仅 select 样式可用）
  auto *configBtn = new QPushButton(QStringLiteral("配置"));
  configBtn->setEnabled(false);  // 默认 text 样式，禁用
  // 紧凑样式（适合表格单元格），disabled 时变灰
  configBtn->setStyleSheet(
      QStringLiteral("QPushButton { background: %1; border: 1px solid %2; border-radius: 3px;"
                     "  padding: 2px 6px; font-size: 12px;"
                     "}"
                     "QPushButton:hover { background: %3; }"
                     "QPushButton:disabled { color: %4; background: %1; }")
          .arg(AuiStyle::background().name(), AuiStyle::borderColor().name(),
               AuiStyle::hoverBackground().name(), AuiStyle::mutedTextColor().name()));
  configBtn->setProperty("row", row);
  connect(configBtn, &QPushButton::clicked, this, [this, configBtn]() {
    // 动态获取当前行（按钮可能在行移动后位置变化）
    int r = configBtn->property("row").toInt();
    // 从表格控件找当前行索引
    for (int i = 0; i < m_dialogFieldsTable->rowCount(); ++i) {
      if (m_dialogFieldsTable->cellWidget(i, 4) == configBtn) {
        r = i;
        break;
      }
    }
    onConfigureFieldStyle(r);
  });
  m_dialogFieldsTable->setCellWidget(row, 4, configBtn);

  // 删除按钮
  auto *delBtn = new QPushButton(QString::fromUtf8(CodeConstants::UiText::kDelete));
  delBtn->setStyleSheet(
      QStringLiteral("QPushButton { background: %1; border: 1px solid %2; border-radius: 3px;"
                     "  padding: 2px 6px; font-size: 12px;"
                     "}"
                     "QPushButton:hover { background: %3; }"
                     "QPushButton:disabled { color: %4; }")
          .arg(AuiStyle::background().name(), AuiStyle::borderColor().name(),
               AuiStyle::hoverBackground().name(), AuiStyle::mutedTextColor().name()));
  connect(delBtn, &QPushButton::clicked, this, [this, delBtn]() {
    for (int i = 0; i < m_dialogFieldsTable->rowCount(); ++i) {
      if (m_dialogFieldsTable->cellWidget(i, 5) == delBtn) {
        onRemoveDialogField(i);
        break;
      }
    }
  });
  m_dialogFieldsTable->setCellWidget(row, 5, delBtn);

  // 同步到 m_dialogFieldsData
  DialogFieldConfig f;
  f.fieldName = QString();
  f.label = QString();
  f.editStyle = EditStyle::Text;
  f.required = false;
  m_dialogFieldsData.append(f);

  // 样式改变时同步到 m_dialogFieldsData，并更新配置按钮状态
  connect(styleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this, styleCombo](int) {
            for (int i = 0; i < m_dialogFieldsTable->rowCount(); ++i) {
              if (m_dialogFieldsTable->cellWidget(i, 2) == styleCombo) {
                QString styleStr = styleCombo->currentData().toString();
                m_dialogFieldsData[i].editStyle = stringToEditStyle(styleStr);
                // 仅 select 样式启用配置按钮
                auto *btn = qobject_cast<QPushButton *>(m_dialogFieldsTable->cellWidget(i, 4));
                if (btn) {
                  btn->setEnabled(m_dialogFieldsData[i].editStyle == EditStyle::Select);
                }
                break;
              }
            }
          });
}

void ButtonConfigDialog::onRemoveDialogField(int row) {
  if (row < 0 || row >= m_dialogFieldsTable->rowCount()) {
    return;
  }
  m_dialogFieldsTable->removeRow(row);
  if (row < m_dialogFieldsData.size()) {
    m_dialogFieldsData.removeAt(row);
  }
}

void ButtonConfigDialog::onConfigureFieldStyle(int row) {
  if (row < 0 || row >= m_dialogFieldsTable->rowCount()) {
    return;
  }
  // 确保基本属性同步到 m_dialogFieldsData
  auto *styleCombo = qobject_cast<QComboBox *>(m_dialogFieldsTable->cellWidget(row, 2));
  if (!styleCombo) {
    return;
  }
  QString styleStr = styleCombo->currentData().toString();
  m_dialogFieldsData[row].editStyle = stringToEditStyle(styleStr);

  // select 样式：弹出 ComboboxConfigDialog
  if (m_dialogFieldsData[row].editStyle == EditStyle::Select) {
    ComboboxConfigDialog dlg(this);
    dlg.setConfig(m_dialogFieldsData[row].selectUrl, m_dialogFieldsData[row].selectValueField,
                  m_dialogFieldsData[row].selectLabelField);
    if (dlg.exec() == QDialog::Accepted) {
      m_dialogFieldsData[row].selectUrl = dlg.url();
      m_dialogFieldsData[row].selectValueField = dlg.valueField();
      m_dialogFieldsData[row].selectLabelField = dlg.labelField();
    }
  }
  // 其他样式暂时不提供额外配置 UI（用默认值）
}

// ════════════════════════════════════════════════════════════
//  表格数据收集 / 填充
// ════════════════════════════════════════════════════════════

QVector<DialogFieldConfig> ButtonConfigDialog::collectDialogFields() const {
  QVector<DialogFieldConfig> result;
  int count = m_dialogFieldsTable->rowCount();
  for (int i = 0; i < count; ++i) {
    DialogFieldConfig f;
    // 从 m_dialogFieldsData 获取样式特定配置（如果有）
    if (i < m_dialogFieldsData.size()) {
      f = m_dialogFieldsData[i];
    }
    // 从表格控件获取基本属性
    auto *labelEdit = qobject_cast<QLineEdit *>(m_dialogFieldsTable->cellWidget(i, 0));
    auto *nameEdit = qobject_cast<QLineEdit *>(m_dialogFieldsTable->cellWidget(i, 1));
    auto *styleCombo = qobject_cast<QComboBox *>(m_dialogFieldsTable->cellWidget(i, 2));
    auto *requiredCheck = qobject_cast<QCheckBox *>(m_dialogFieldsTable->cellWidget(i, 3));
    if (nameEdit) {
      f.fieldName = nameEdit->text();
    }
    if (labelEdit) {
      f.label = labelEdit->text();
    }
    if (styleCombo) {
      f.editStyle = stringToEditStyle(styleCombo->currentData().toString());
    }
    if (requiredCheck) {
      f.required = requiredCheck->isChecked();
    }
    // 跳过字段名和标签都为空的行
    if (f.fieldName.isEmpty() && f.label.isEmpty()) {
      continue;
    }
    result.append(f);
  }
  return result;
}

void ButtonConfigDialog::fillDialogFields(const QVector<DialogFieldConfig> &fields) {
  m_dialogFieldsTable->setRowCount(0);
  m_dialogFieldsData.clear();
  for (const auto &f : fields) {
    onAddDialogField();
    int row = m_dialogFieldsTable->rowCount() - 1;
    // 先更新 m_dialogFieldsData，再设置控件值（避免信号回调读取到默认值）
    m_dialogFieldsData[row] = f;
    auto *labelEdit = qobject_cast<QLineEdit *>(m_dialogFieldsTable->cellWidget(row, 0));
    auto *nameEdit = qobject_cast<QLineEdit *>(m_dialogFieldsTable->cellWidget(row, 1));
    auto *styleCombo = qobject_cast<QComboBox *>(m_dialogFieldsTable->cellWidget(row, 2));
    auto *requiredCheck = qobject_cast<QCheckBox *>(m_dialogFieldsTable->cellWidget(row, 3));
    if (nameEdit) {
      nameEdit->setText(f.fieldName);
    }
    if (labelEdit) {
      labelEdit->setText(f.label);
    }
    if (styleCombo) {
      int idx = styleCombo->findData(editStyleToString(f.editStyle));
      if (idx >= 0) {
        styleCombo->setCurrentIndex(idx);  // 触发信号，此时 m_dialogFieldsData[row] 已更新
      } else {
        // 没找到匹配项时手动更新配置按钮状态
        auto *btn = qobject_cast<QPushButton *>(m_dialogFieldsTable->cellWidget(row, 4));
        if (btn) {
          btn->setEnabled(f.editStyle == EditStyle::Select);
        }
      }
    }
    if (requiredCheck) {
      requiredCheck->setChecked(f.required);
    }
  }
}

// ════════════════════════════════════════════════════════════
//  数据读写
// ════════════════════════════════════════════════════════════

ButtonConfig ButtonConfigDialog::getData() const {
  ButtonConfig b;
  b.label = m_labelEdit->text();
  b.actionKey = m_actionKeyEdit->text();
  b.icon = m_currentIconName;
  b.position = static_cast<ButtonPosition>(m_positionCombo->currentData().toInt());
  b.buttonType = m_buttonTypeCombo->currentData().toString();
  b.actionType = static_cast<ButtonActionType>(m_actionTypeCombo->currentData().toInt());

  // 行为特定字段：Ajax 和 Confirm 各用各自的 apiNameEdit
  b.apiName = m_apiNameEdit->text();
  if (b.actionType == ButtonActionType::Confirm) {
    b.apiName = m_confirmApiEdit->text();
  }
  b.confirmText = m_confirmTextEdit->text();
  b.dialogTitle = m_dialogTitleEdit->text();
  b.dialogApi = m_dialogApiEdit->text();
  b.dialogFields = collectDialogFields();
  b.linkPath = m_linkPathEdit->text();

  return b;
}

void ButtonConfigDialog::setData(const ButtonConfig &config) {
  m_labelEdit->setText(config.label);
  m_actionKeyEdit->setText(config.actionKey);
  // 图标
  setIconName(config.icon);

  // 位置
  int posIdx = m_positionCombo->findData(static_cast<int>(config.position));
  if (posIdx >= 0) {
    m_positionCombo->setCurrentIndex(posIdx);
  }

  // 按钮样式
  int typeIdx = m_buttonTypeCombo->findData(config.buttonType);
  if (typeIdx >= 0) {
    m_buttonTypeCombo->setCurrentIndex(typeIdx);
  }

  // 行为类型
  int actionIdx = m_actionTypeCombo->findData(static_cast<int>(config.actionType));
  if (actionIdx >= 0) {
    m_actionTypeCombo->setCurrentIndex(actionIdx);
  }

  // 行为特定字段
  m_apiNameEdit->setText(config.apiName);
  m_confirmApiEdit->setText(config.apiName);
  m_confirmTextEdit->setText(config.confirmText);
  m_dialogTitleEdit->setText(config.dialogTitle);
  m_dialogApiEdit->setText(config.dialogApi);
  fillDialogFields(config.dialogFields);
  m_linkPathEdit->setText(config.linkPath);

  updateBehaviorVisibility();
}
