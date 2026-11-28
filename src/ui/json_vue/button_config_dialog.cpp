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
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "combobox_config_dialog.h"
#include "icon_loader.h"
#include "icon_picker_dialog.h"
#include "src/util/common/code_constants.h"
#include "src/util/ui/component/aui_cell_edit_combo.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/component/aui_config_dialog.h"
#include "src/util/ui/component/aui_style.h"

namespace {

/// 编辑样式下拉 显示文本 ↔ 存储值（JsonVueStyle::k* 常量）
QString editStyleTextToData(const QString &text) {
  if (text == QStringLiteral("整数")) return QString::fromLatin1(JsonVueStyle::kInt);
  if (text == QStringLiteral("小数")) return QString::fromLatin1(JsonVueStyle::kFloat);
  if (text == QStringLiteral("日期")) return QString::fromLatin1(JsonVueStyle::kDate);
  if (text == QStringLiteral("下拉")) return QString::fromLatin1(JsonVueStyle::kSelect);
  if (text == QStringLiteral("文本域")) return QString::fromLatin1(JsonVueStyle::kTextarea);
  return QString::fromLatin1(JsonVueStyle::kText);
}

QString editStyleDataToText(const QString &data) {
  if (data == QString::fromLatin1(JsonVueStyle::kInt)) return QStringLiteral("整数");
  if (data == QString::fromLatin1(JsonVueStyle::kFloat)) return QStringLiteral("小数");
  if (data == QString::fromLatin1(JsonVueStyle::kDate)) return QStringLiteral("日期");
  if (data == QString::fromLatin1(JsonVueStyle::kSelect)) return QStringLiteral("下拉");
  if (data == QString::fromLatin1(JsonVueStyle::kTextarea)) return QStringLiteral("文本域");
  return QStringLiteral("文本");
}

}  // namespace

// ════════════════════════════════════════════════════════════
//  ButtonConfigForm：单个状态的配置表单
// ════════════════════════════════════════════════════════════

ButtonConfigForm::ButtonConfigForm(QWidget *parent) : QWidget(parent) { setupUI(); }

// ════════════════════════════════════════════════════════════
//  界面构建
// ════════════════════════════════════════════════════════════

void ButtonConfigForm::setupUI() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
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

  // label 改变时自动推导 actionKey，并刷新多状态页签标题
  connect(m_labelEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
    if (m_actionKeyEdit->text().isEmpty()) {
      m_actionKeyEdit->setText(deriveActionKey(text));
    }
    emit identityChanged();
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

  m_showLabelCheck =
      new QCheckBox(QStringLiteral("显示按钮文字（不勾选则仅图标，悬停显示文字提示）"));
  m_showLabelCheck->setChecked(true);
  basicForm->addRow(QStringLiteral("显示方式:"), m_showLabelCheck);

  m_positionCombo = AuiComboBox::create();
  m_positionCombo->setMinimumHeight(28);
  m_positionCombo->addItem(QStringLiteral("行操作列"), static_cast<int>(ButtonPosition::Row));
  m_positionCombo->addItem(QStringLiteral("顶部工具栏"), static_cast<int>(ButtonPosition::Toolbar));
  basicForm->addRow(QStringLiteral("位置:"), m_positionCombo);

  // 显示条件：仅行按钮生效；字段=值时显示，空=始终显示
  auto *condLayout = new QHBoxLayout;
  condLayout->setSpacing(4);
  m_visibleFieldCombo = AuiComboBox::create();
  m_visibleFieldCombo->setMinimumHeight(28);
  m_visibleFieldCombo->addItem(QStringLiteral("不限制"), QString());
  m_visibleFieldCombo->setToolTip(
      QStringLiteral("选择本表字段或联表输出别名；留空表示按钮始终显示"));
  m_visibleOpCombo = AuiComboBox::create();
  m_visibleOpCombo->setMinimumHeight(28);
  m_visibleOpCombo->addItem(QStringLiteral("等于"), QStringLiteral("eq"));
  m_visibleOpCombo->addItem(QStringLiteral("不等于"), QStringLiteral("ne"));
  m_visibleValueEdit = new QLineEdit;
  m_visibleValueEdit->setMinimumHeight(28);
  m_visibleValueEdit->setPlaceholderText(QStringLiteral("如 1 / normal"));
  m_visibleValueEdit->setToolTip(QStringLiteral("按字符串比较：String(row.字段) == 值"));
  condLayout->addWidget(m_visibleFieldCombo, 2);
  condLayout->addWidget(m_visibleOpCombo, 1);
  condLayout->addWidget(m_visibleValueEdit, 1);
  basicForm->addRow(QStringLiteral("显示条件:"), condLayout);
  connect(m_positionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { updateVisibilityControls(); });
  connect(m_visibleFieldCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { updateVisibilityControls(); });
  connect(m_visibleValueEdit, &QLineEdit::textChanged, this,
          [this](const QString &) { emit identityChanged(); });

  m_buttonTypeCombo = AuiComboBox::create();
  m_buttonTypeCombo->setMinimumHeight(28);
  m_buttonTypeCombo->addItem(QStringLiteral("默认"), QString());
  m_buttonTypeCombo->addItem(QStringLiteral("主要"), QString::fromLatin1(JsonVueColor::kPrimary));
  m_buttonTypeCombo->addItem(QStringLiteral("成功"), QString::fromLatin1(JsonVueColor::kSuccess));
  m_buttonTypeCombo->addItem(QStringLiteral("警告"), QString::fromLatin1(JsonVueColor::kWarning));
  m_buttonTypeCombo->addItem(QStringLiteral("危险"), QString::fromLatin1(JsonVueColor::kDanger));
  basicForm->addRow(QStringLiteral("按钮样式:"), m_buttonTypeCombo);

  m_actionTypeCombo = AuiComboBox::create();
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

  m_dialogFieldsTable = makeConfigTable(
      {{QStringLiteral("标签"), QHeaderView::Stretch, 0},
       {QStringLiteral("字段名"), QHeaderView::Stretch, 0},
       {QStringLiteral("样式"), QHeaderView::Interactive, 90},
       {QString::fromUtf8(CodeConstants::UiText::kRequired), QHeaderView::Interactive, 50},
       {QStringLiteral("配置"), QHeaderView::Interactive, 70},
       {QString::fromUtf8(CodeConstants::UiText::kDelete), QHeaderView::Interactive, 60}},
      this, 120);
  dialogLayout->addWidget(m_dialogFieldsTable);

  m_addFieldBtn = new QPushButton(QStringLiteral("+ 添加字段"));
  connect(m_addFieldBtn, &QPushButton::clicked, this, &ButtonConfigForm::onAddDialogField);
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

  updateBehaviorVisibility();
  updateVisibilityControls();
}

void ButtonConfigForm::updateVisibilityControls() {
  const auto position =
      static_cast<ButtonPosition>(m_positionCombo->currentData().toInt());
  const bool isRow = (position == ButtonPosition::Row);
  m_visibleFieldCombo->setEnabled(isRow);
  const bool hasField =
      isRow && !m_visibleFieldCombo->currentData().toString().isEmpty();
  m_visibleOpCombo->setEnabled(hasField);
  m_visibleValueEdit->setEnabled(hasField);
}

void ButtonConfigForm::setVisibilityFields(
    const QVector<QPair<QString, QString>> &fields) {
  const QString pending = m_pendingVisibleField;
  m_visibleFieldCombo->clear();
  m_visibleFieldCombo->addItem(QStringLiteral("不限制"), QString());
  for (const auto &f : fields) {
    if (f.first.isEmpty()) continue;
    const QString label = f.second.isEmpty() ? f.first : f.second;
    m_visibleFieldCombo->addItem(
        QStringLiteral("%1（%2）").arg(label, f.first), f.first);
  }
  const int idx = m_visibleFieldCombo->findData(pending);
  m_visibleFieldCombo->setCurrentIndex(idx >= 0 ? idx : 0);
  updateVisibilityControls();
}

QString ButtonConfigForm::stateTitle(int index) const {
  QString name = m_labelEdit->text().trimmed();
  if (name.isEmpty()) name = QStringLiteral("状态 %1").arg(index + 1);
  const QString value = m_visibleValueEdit->text().trimmed();
  return value.isEmpty() ? name : QStringLiteral("%1（值=%2）").arg(name, value);
}

// ════════════════════════════════════════════════════════════
//  行为类型可见性更新
// ════════════════════════════════════════════════════════════

void ButtonConfigForm::updateBehaviorVisibility() {
  auto actionType = static_cast<ButtonActionType>(m_actionTypeCombo->currentData().toInt());

  m_ajaxWidget->setVisible(actionType == ButtonActionType::Ajax);
  m_confirmWidget->setVisible(actionType == ButtonActionType::Confirm);
  m_dialogWidget->setVisible(actionType == ButtonActionType::Dialog);
  m_linkWidget->setVisible(actionType == ButtonActionType::Link);
}

// ════════════════════════════════════════════════════════════
//  actionKey 自动推导
// ════════════════════════════════════════════════════════════

QString ButtonConfigForm::deriveActionKey(const QString &label) {
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

void ButtonConfigForm::setIconName(const QString &iconName) {
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

void ButtonConfigForm::onAddDialogField() {
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
  // 样式：常驻只读下拉（文字可拖选/双击全选/Ctrl+C 复制，点箭头展开候选）
  auto *styleCombo = new AuiCellEditCombo(m_dialogFieldsTable, 2,
                                          {QStringLiteral("文本"), QStringLiteral("整数"),
                                           QStringLiteral("小数"), QStringLiteral("日期"),
                                           QStringLiteral("下拉"), QStringLiteral("文本域")},
                                          m_dialogFieldsTable, true);
  styleCombo->setText(QStringLiteral("文本"));
  m_dialogFieldsTable->setCellWidget(row, 2, styleCombo);

  // 必填
  auto *requiredCheck = new QCheckBox;
  requiredCheck->setChecked(false);
  m_dialogFieldsTable->setCellWidget(row, 3, requiredCheck);

  // 配置按钮（仅 select 样式可用；紧凑样式适合表格单元格，disabled 时变灰）
  auto *configBtn = makeCompactButton(QStringLiteral("配置"), m_dialogFieldsTable);
  configBtn->setEnabled(false);  // 默认 text 样式，禁用
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
  auto *delBtn =
      makeCompactButton(QString::fromUtf8(CodeConstants::UiText::kDelete), m_dialogFieldsTable);
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
  styleCombo->valueCommitted = [this, styleCombo]() {
    for (int i = 0; i < m_dialogFieldsTable->rowCount(); ++i) {
      if (m_dialogFieldsTable->cellWidget(i, 2) != styleCombo) continue;
      m_dialogFieldsData[i].editStyle =
          stringToEditStyle(editStyleTextToData(styleCombo->text()));
      // 仅 select 样式启用配置按钮
      auto *btn = qobject_cast<QPushButton *>(m_dialogFieldsTable->cellWidget(i, 4));
      if (btn) {
        btn->setEnabled(m_dialogFieldsData[i].editStyle == EditStyle::Select);
      }
      break;
    }
  };
}

void ButtonConfigForm::onRemoveDialogField(int row) {
  if (row < 0 || row >= m_dialogFieldsTable->rowCount()) {
    return;
  }
  m_dialogFieldsTable->removeRow(row);
  if (row < m_dialogFieldsData.size()) {
    m_dialogFieldsData.removeAt(row);
  }
}

void ButtonConfigForm::onConfigureFieldStyle(int row) {
  if (row < 0 || row >= m_dialogFieldsTable->rowCount()) {
    return;
  }
  // 确保基本属性同步到 m_dialogFieldsData
  auto *styleCell = dynamic_cast<AuiCellEditCombo *>(m_dialogFieldsTable->cellWidget(row, 2));
  if (!styleCell) {
    return;
  }
  m_dialogFieldsData[row].editStyle = stringToEditStyle(editStyleTextToData(styleCell->text()));

  // select 样式：弹出 ComboboxConfigDialog
  if (m_dialogFieldsData[row].editStyle == EditStyle::Select) {
    ComboboxConfigDialog dlg(this);
    dlg.setSearchRoot(m_searchRoot);
    dlg.setSourceRef(m_dialogFieldsData[row].selectSourceFile,
                     m_dialogFieldsData[row].selectSourceId);
    dlg.setConfig(m_dialogFieldsData[row].selectUrl, m_dialogFieldsData[row].selectValueField,
                  m_dialogFieldsData[row].selectLabelField);
    dlg.setPagedConfig(
        m_dialogFieldsData[row].selectPaged, m_dialogFieldsData[row].selectPageKey,
        m_dialogFieldsData[row].selectPageSizeKey, m_dialogFieldsData[row].selectPageSize,
        m_dialogFieldsData[row].selectSearchTitle, m_dialogFieldsData[row].selectSearchField,
        m_dialogFieldsData[row].selectMethod);
    if (dlg.exec() == QDialog::Accepted) {
      m_dialogFieldsData[row].selectSourceFile = dlg.sourceFile();
      m_dialogFieldsData[row].selectSourceId = dlg.sourceId();
      m_dialogFieldsData[row].selectUrl = dlg.url();
      m_dialogFieldsData[row].selectValueField = dlg.valueField();
      m_dialogFieldsData[row].selectLabelField = dlg.labelField();
      m_dialogFieldsData[row].selectPaged = dlg.paged();
      m_dialogFieldsData[row].selectPageKey = dlg.pageKey();
      m_dialogFieldsData[row].selectPageSizeKey = dlg.pageSizeKey();
      m_dialogFieldsData[row].selectPageSize = dlg.pageSize();
      m_dialogFieldsData[row].selectSearchTitle = dlg.searchTitle();
      m_dialogFieldsData[row].selectSearchField = dlg.searchField();
      m_dialogFieldsData[row].selectMethod = dlg.method();
    }
  }
  // 其他样式暂时不提供额外配置 UI（用默认值）
}

// ════════════════════════════════════════════════════════════
//  搜索根目录
// ════════════════════════════════════════════════════════════

void ButtonConfigForm::setSearchRoot(const QString &dir) { m_searchRoot = dir; }

// ════════════════════════════════════════════════════════════
//  表格数据收集 / 填充
// ════════════════════════════════════════════════════════════

QVector<DialogFieldConfig> ButtonConfigForm::collectDialogFields() const {
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
    auto *styleCell = dynamic_cast<AuiCellEditCombo *>(m_dialogFieldsTable->cellWidget(i, 2));
    auto *requiredCheck = qobject_cast<QCheckBox *>(m_dialogFieldsTable->cellWidget(i, 3));
    if (nameEdit) {
      f.fieldName = nameEdit->text();
    }
    if (labelEdit) {
      f.label = labelEdit->text();
    }
    if (styleCell) {
      f.editStyle = stringToEditStyle(editStyleTextToData(styleCell->text()));
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

void ButtonConfigForm::fillDialogFields(const QVector<DialogFieldConfig> &fields) {
  m_dialogFieldsTable->setRowCount(0);
  m_dialogFieldsData.clear();
  for (const auto &f : fields) {
    onAddDialogField();
    int row = m_dialogFieldsTable->rowCount() - 1;
    // 先更新 m_dialogFieldsData，再设置控件值（避免信号回调读取到默认值）
    m_dialogFieldsData[row] = f;
    auto *labelEdit = qobject_cast<QLineEdit *>(m_dialogFieldsTable->cellWidget(row, 0));
    auto *nameEdit = qobject_cast<QLineEdit *>(m_dialogFieldsTable->cellWidget(row, 1));
    auto *styleCell = dynamic_cast<AuiCellEditCombo *>(m_dialogFieldsTable->cellWidget(row, 2));
    auto *requiredCheck = qobject_cast<QCheckBox *>(m_dialogFieldsTable->cellWidget(row, 3));
    if (nameEdit) {
      nameEdit->setText(f.fieldName);
    }
    if (labelEdit) {
      labelEdit->setText(f.label);
    }
    if (styleCell) {
      styleCell->setText(editStyleDataToText(editStyleToString(f.editStyle)));
      // 配置按钮启用态随样式同步（原实现依赖 setCurrentIndex 触发信号，此处显式同步）
      auto *btn = qobject_cast<QPushButton *>(m_dialogFieldsTable->cellWidget(row, 4));
      if (btn) {
        btn->setEnabled(f.editStyle == EditStyle::Select);
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

ButtonConfig ButtonConfigForm::getData() const {
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

  // 显示控制（工具栏按钮无行状态，条件不生效）
  b.showLabel = m_showLabelCheck->isChecked();
  b.visibleField = m_visibleFieldCombo->currentData().toString();
  b.visibleOp = m_visibleOpCombo->currentData().toString();
  b.visibleValue = m_visibleValueEdit->text().trimmed();
  if (b.position != ButtonPosition::Row) {
    b.visibleField.clear();
    b.visibleOp = QStringLiteral("eq");
    b.visibleValue.clear();
  }
  // 仅图标模式必须有图标，否则回退为文字显示
  if (!b.showLabel && b.icon.isEmpty()) {
    b.showLabel = true;
  }

  return b;
}

void ButtonConfigForm::setData(const ButtonConfig &config) {
  m_labelEdit->setText(config.label);
  m_actionKeyEdit->setText(config.actionKey);
  // 图标
  setIconName(config.icon);

  // 位置
  comboSelectData(m_positionCombo, static_cast<int>(config.position));

  // 按钮样式
  comboSelectData(m_buttonTypeCombo, config.buttonType);

  // 行为类型
  comboSelectData(m_actionTypeCombo, static_cast<int>(config.actionType));

  // 行为特定字段
  m_apiNameEdit->setText(config.apiName);
  m_confirmApiEdit->setText(config.apiName);
  m_confirmTextEdit->setText(config.confirmText);
  m_dialogTitleEdit->setText(config.dialogTitle);
  m_dialogApiEdit->setText(config.dialogApi);
  fillDialogFields(config.dialogFields);
  m_linkPathEdit->setText(config.linkPath);

  // 显示控制
  m_showLabelCheck->setChecked(config.showLabel);
  m_pendingVisibleField = config.visibleField;
  comboSelectData(m_visibleFieldCombo, config.visibleField);
  comboSelectData(m_visibleOpCombo,
                  config.visibleOp.isEmpty() ? QStringLiteral("eq") : config.visibleOp);
  m_visibleValueEdit->setText(config.visibleValue);

  updateBehaviorVisibility();
  updateVisibilityControls();
}

// ════════════════════════════════════════════════════════════
//  ButtonConfigDialog：统一弹窗（单状态无页签 / 多状态页签）
// ════════════════════════════════════════════════════════════

ButtonConfigDialog::ButtonConfigDialog(QWidget *parent) : QDialog(parent) { setupUI(1); }

ButtonConfigDialog::ButtonConfigDialog(const ButtonConfig &config, QWidget *parent)
    : QDialog(parent) {
  setupUI(1);
  setData(config);
}

ButtonConfigDialog::ButtonConfigDialog(const QVector<ButtonConfig> &states, QWidget *parent)
    : QDialog(parent) {
  setupUI(qMax(1, states.size()));
  setStates(states);
}

void ButtonConfigDialog::setupUI(int stateCount) {
  setMinimumWidth(700);
  setMinimumHeight(600);
  auto frame = beginConfigDialog(this, QStringLiteral("按钮配置"), QMargins(12, 8, 12, 12), 8);
  auto *mainLayout = frame.contentLayout;

  // ── 状态模式：单状态 / 双状态（双状态在同一弹窗内用页签呈现）──
  auto *modeRow = new QHBoxLayout;
  modeRow->setSpacing(6);
  modeRow->addWidget(new QLabel(QStringLiteral("按钮状态:")));
  m_modeCombo = AuiComboBox::create();
  m_modeCombo->setMinimumHeight(28);
  m_modeCombo->addItem(QStringLiteral("单状态（一个按钮）"), 0);
  m_modeCombo->addItem(QStringLiteral("双状态（两个互斥按钮）"), 1);
  m_modeCombo->setToolTip(
      QStringLiteral("双状态 = 同一业务的两个互斥状态（如 解冻/冻结），各用一份配置"));
  modeRow->addWidget(m_modeCombo, 1);
  mainLayout->addLayout(modeRow);

  m_modeHint = new QLabel(
      QStringLiteral("双状态：每个页签独立配置文字、接口与显示条件；"
                     "生成后按行数据只显示其中一个。"));
  m_modeHint->setWordWrap(true);
  m_modeHint->setStyleSheet(
      QStringLiteral("color: %1;").arg(AuiStyle::secondaryTextColor().name()));
  mainLayout->addWidget(m_modeHint);

  // 表单呈现区：单状态 = 表单直接铺开；双状态 = 两份表单放进页签
  m_contentHost = new QWidget;
  m_contentLayout = new QVBoxLayout(m_contentHost);
  m_contentLayout->setContentsMargins(0, 0, 0, 0);
  m_contentLayout->setSpacing(0);
  mainLayout->addWidget(m_contentHost, 1);

  m_tabs = new QTabWidget;
  auto *firstForm = new ButtonConfigForm;
  m_forms.append(firstForm);
  connect(firstForm, &ButtonConfigForm::identityChanged, this,
          &ButtonConfigDialog::refreshTabTitles);
  m_contentLayout->addWidget(firstForm, 1);

  connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { updateModeUI(); });
  // 双状态构造时直接进入双状态模式（信号触发第二个表单的懒创建）
  if (stateCount > 1) m_modeCombo->setCurrentIndex(1);
  updateModeUI();  // 初始化显隐（单状态时隐藏说明与页签）

  finishConfigDialog(this, frame);
}

void ButtonConfigDialog::updateModeUI() {
  if (!m_modeCombo || !m_contentLayout || m_forms.isEmpty()) return;
  const bool dual = isDualMode();
  ButtonConfigForm *first = m_forms.first();

  if (dual) {
    ensureDualForm();
    // 表单 0/1 按状态顺序放进页签（先摘出再插入，保证页签顺序固定）
    m_contentLayout->removeWidget(first);
    const int i0 = m_tabs->indexOf(first);
    if (i0 >= 0) m_tabs->removeTab(i0);
    m_tabs->insertTab(0, first, first->stateTitle(0));
    ButtonConfigForm *second = m_forms.at(1);
    const int i1 = m_tabs->indexOf(second);
    if (i1 >= 0) m_tabs->removeTab(i1);
    m_tabs->addTab(second, second->stateTitle(1));
    m_tabs->setCurrentIndex(0);
    if (m_contentLayout->indexOf(m_tabs) < 0) m_contentLayout->addWidget(m_tabs, 1);
    m_tabs->show();
    m_modeHint->show();
  } else {
    // 单状态：表单 0 回到内容区，页签整体隐藏（第二份配置暂存，切回时可恢复）
    const int i0 = m_tabs->indexOf(first);
    if (i0 >= 0) m_tabs->removeTab(i0);
    m_contentLayout->removeWidget(m_tabs);
    m_tabs->hide();
    if (m_contentLayout->indexOf(first) < 0) m_contentLayout->addWidget(first, 1);
    first->show();
    m_modeHint->hide();
  }
  refreshTabTitles();
}

void ButtonConfigDialog::ensureDualForm() {
  if (m_forms.size() >= 2) return;
  auto *form = new ButtonConfigForm;
  form->setSearchRoot(m_searchRoot);
  form->setVisibilityFields(m_visibilityFields);
  m_forms.append(form);
  connect(form, &ButtonConfigForm::identityChanged, this,
          &ButtonConfigDialog::refreshTabTitles);
}

void ButtonConfigDialog::refreshTabTitles() {
  if (!m_tabs) return;
  for (int i = 0; i < m_forms.size(); ++i) {
    const int idx = m_tabs->indexOf(m_forms[i]);
    if (idx >= 0) m_tabs->setTabText(idx, m_forms[i]->stateTitle(i));
  }
}

void ButtonConfigDialog::setDualMode(bool dual) {
  if (!m_modeCombo) return;
  m_modeCombo->setCurrentIndex(dual ? 1 : 0);
}

bool ButtonConfigDialog::isDualMode() const {
  return m_modeCombo && m_modeCombo->currentIndex() == 1;
}

ButtonConfig ButtonConfigDialog::getData() const {
  return m_forms.isEmpty() ? ButtonConfig() : m_forms.first()->getData();
}

QVector<ButtonConfig> ButtonConfigDialog::getStates() const {
  QVector<ButtonConfig> out;
  const int count = qMin(isDualMode() ? 2 : 1, m_forms.size());
  out.reserve(count);
  for (int i = 0; i < count; ++i) {
    ButtonConfig cfg = m_forms[i]->getData();
    // 页签外字段（stateGroup 分组键）沿用入参，编辑不影响分组关系
    if (i < m_initialStates.size()) cfg.stateGroup = m_initialStates[i].stateGroup;
    out.append(cfg);
  }
  return out;
}

void ButtonConfigDialog::setData(const ButtonConfig &config) {
  if (m_forms.isEmpty()) return;
  m_forms.first()->setData(config);
  if (m_initialStates.isEmpty()) {
    m_initialStates.append(config);
  } else {
    m_initialStates[0] = config;
  }
  refreshTabTitles();
}

void ButtonConfigDialog::setStates(const QVector<ButtonConfig> &states) {
  m_initialStates = states;
  setDualMode(states.size() > 1);  // 状态数与模式保持一致
  for (int i = 0; i < m_forms.size() && i < states.size(); ++i) {
    m_forms[i]->setData(states[i]);
  }
  refreshTabTitles();
}

void ButtonConfigDialog::setSearchRoot(const QString &dir) {
  m_searchRoot = dir;
  for (auto *form : m_forms) form->setSearchRoot(dir);
}

void ButtonConfigDialog::setVisibilityFields(
    const QVector<QPair<QString, QString>> &fields) {
  m_visibilityFields = fields;
  for (auto *form : m_forms) form->setVisibilityFields(fields);
}
