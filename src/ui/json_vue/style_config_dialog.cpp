/**
 * @file style_config_dialog.cpp
 * @brief 字段样式配置对话框实现
 *
 * ColumnStyleDialog：列配置样式（显示样式/编辑样式/通用配置）。
 * QueryStyleDialog：查询字段样式（占位提示/日期格式）。
 */

#include "style_config_dialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

#include "combobox_config_dialog.h"
#include "src/util/common/code_constants.h"
#include "src/util/ui/aui_window.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  辅助：清空 QFormLayout
// ════════════════════════════════════════════════════════════

/// 清空 QFormLayout（用 removeRow 正确删除 label 和 field widget）
/// 注：不能用通用的 takeAt 循环清空 QFormLayout——takeAt 只返回 field item，
/// QFormLayout 内部创建的 label 会残留。析构时 label 先被 parent widget 删除，
/// QFormLayout 析构访问悬空 label 指针导致崩溃。removeRow 会正确删除整行。
static void clearFormLayout(QFormLayout *form) {
  if (!form) return;
  while (form->rowCount() > 0) {
    form->removeRow(0);
  }
}

/// 递归失效 widget 及其所有子 widget 的 layout 的 sizeHint 缓存。
/// setVisible(false) 后，子布局的 sizeHint 不会立即重算（Qt 通过异步 LayoutRequest 事件触发），
/// 导致 layout()->sizeHint() 仍返回切换前的旧值。手动递归 invalidate 才能立即拿到正确大小。
static void invalidateAllLayouts(QWidget *widget) {
  if (!widget) return;
  if (QLayout *lay = widget->layout()) lay->invalidate();
  const auto children = widget->children();
  for (QObject *obj : children) {
    if (auto *w = qobject_cast<QWidget *>(obj)) invalidateAllLayouts(w);
  }
}

// ════════════════════════════════════════════════════════════
//  ColumnStyleDialog 构造/析构
// ════════════════════════════════════════════════════════════

ColumnStyleDialog::ColumnStyleDialog(EditStyle style, QWidget *parent) : QDialog(parent) {
  m_editStyle = style;
  setupUI();
}

ColumnStyleDialog::~ColumnStyleDialog() {
  // 析构前清空 QFormLayout，用 removeRow 正确删除其管理的 label 和 field widget
  // 避免 QFormLayout 析构时访问已被 parent widget 删除的 label 悬空指针导致崩溃
  if (m_displayTypeLayout) {
    auto *item0 = m_displayTypeLayout->itemAt(0);
    if (item0 && item0->layout()) {
      clearFormLayout(qobject_cast<QFormLayout *>(item0->layout()));
    }
  }
  if (m_editStyleLayout) {
    auto *item0 = m_editStyleLayout->itemAt(0);
    if (item0 && item0->layout()) {
      clearFormLayout(qobject_cast<QFormLayout *>(item0->layout()));
    }
  }
}

// ════════════════════════════════════════════════════════════
//  ColumnStyleDialog 界面构建
// ════════════════════════════════════════════════════════════

void ColumnStyleDialog::setupUI() {
  setWindowTitle(QStringLiteral("列样式配置"));
  setMinimumWidth(520);
  // 不设置 minimumHeight/maximumHeight，让对话框高度随内容自适应

  // ── 无边框对话框 ──
  AuiWindow::setupFramelessDialog(this);

  // ── 自定义标题栏 ──
  TitleBarOptions opts;
  opts.title = QStringLiteral("列样式配置");
  opts.showMinButton = false;
  opts.showMaxButton = false;
  opts.closeRejectsDialog = true;
  auto tb = AuiWindow::createTitleBar(this, opts);

  // ── 内容区域 ──
  auto *contentWidget = new QWidget;
  auto *mainLayout = new QVBoxLayout(contentWidget);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(6);

  m_formLayout = new QFormLayout;
  m_formLayout->setContentsMargins(0, 0, 0, 0);
  m_formLayout->setSpacing(6);
  m_formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  mainLayout->addLayout(m_formLayout);

  // ════════════════════════════════════════
  //  列表页配置
  // ════════════════════════════════════════
  auto *tableSep = new QLabel(QStringLiteral("── 列表页配置 ──"), this);
  tableSep->setAlignment(Qt::AlignCenter);
  m_formLayout->addRow(QString(), tableSep);

  // 显示样式（列表页渲染方式）—— 置于列表页配置最顶部，便于标签映射表获得更大空间
  m_displayTypeCombo = new QComboBox(this);
  m_displayTypeCombo->addItem(QStringLiteral("纯文本(text)"), QStringLiteral(""));
  m_displayTypeCombo->addItem(QStringLiteral("金额(money)"),
                              QString::fromLatin1(JsonVueStyle::kMoney));
  m_displayTypeCombo->addItem(QStringLiteral("标签(tag)"), QString::fromLatin1(JsonVueStyle::kTag));
  m_displayTypeCombo->addItem(QStringLiteral("布尔文字(boolean)"),
                              QString::fromLatin1(JsonVueStyle::kBoolean));
  m_displayTypeCombo->addItem(QStringLiteral("图片(image)"),
                              QString::fromLatin1(JsonVueStyle::kImage));
  addRow(QStringLiteral("显示样式:"), m_displayTypeCombo);

  // 显示样式子控件容器（全宽区域，用于显示标签映射表等）
  m_displayTypeWidget = new QWidget(this);
  m_displayTypeLayout = new QVBoxLayout(m_displayTypeWidget);
  m_displayTypeLayout->setContentsMargins(0, 0, 0, 0);
  m_displayTypeLayout->setSpacing(6);
  auto *displayTypeForm = new QFormLayout;
  displayTypeForm->setContentsMargins(0, 0, 0, 0);
  displayTypeForm->setSpacing(6);
  displayTypeForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_displayTypeLayout->addLayout(displayTypeForm);
  m_formLayout->addRow(QString(), m_displayTypeWidget);
  m_displayTypeWidget->setVisible(false);  // 初始隐藏，由 rebuildDisplayTypeControls 控制

  // 切换显示样式时重建子控件
  connect(m_displayTypeCombo, &QComboBox::currentTextChanged, this, [this]() {
    // 缓存当前值
    if (m_tagItemsTable) m_cachedTagItems = collectTagItems();
    if (m_boolTrueTextEdit) m_cachedBoolTrueText = m_boolTrueTextEdit->text().trimmed();
    if (m_boolFalseTextEdit) m_cachedBoolFalseText = m_boolFalseTextEdit->text().trimmed();
    if (m_switchEditableCheck) m_cachedSwitchEditable = m_switchEditableCheck->isChecked();
    rebuildDisplayTypeControls();
  });

  // 初次构建
  rebuildDisplayTypeControls();

  // 表格列宽
  m_columnWidthCombo = new QComboBox(this);
  m_columnWidthCombo->addItem(QStringLiteral("自动"), 0);
  for (int w : {60, 80, 100, 120, 150, 200, 250, 300}) {
    m_columnWidthCombo->addItem(QString::number(w), w);
  }
  addRow(QStringLiteral("表格列宽:"), m_columnWidthCombo);

  // 固定列
  m_columnFixedCombo = new QComboBox(this);
  m_columnFixedCombo->addItem(QStringLiteral("不固定"), QString());
  m_columnFixedCombo->addItem(QStringLiteral("固定左侧"), QStringLiteral("left"));
  m_columnFixedCombo->addItem(QStringLiteral("固定右侧"), QStringLiteral("right"));
  addRow(QStringLiteral("固定列:"), m_columnFixedCombo);

  // 格式化类型
  m_formatterCombo = new QComboBox(this);
  m_formatterCombo->addItem(QStringLiteral("无"), QString());
  m_formatterCombo->addItem(QStringLiteral("日期"), QString::fromLatin1(JsonVueStyle::kDate));
  m_formatterCombo->addItem(QStringLiteral("状态"), QStringLiteral("status"));
  m_formatterCombo->addItem(QStringLiteral("金额"), QStringLiteral("currency"));
  addRow(QStringLiteral("格式化:"), m_formatterCombo);

  // ════════════════════════════════════════
  //  编辑页配置
  // ════════════════════════════════════════
  auto *editSep = new QLabel(QStringLiteral("── 编辑页配置 ──"), this);
  editSep->setAlignment(Qt::AlignCenter);
  m_formLayout->addRow(QString(), editSep);

  // 编辑样式
  m_editStyleCombo = new QComboBox(this);
  m_editStyleCombo->addItem(QStringLiteral("纯文本(text)"),
                            QString::fromLatin1(JsonVueStyle::kText));
  m_editStyleCombo->addItem(QStringLiteral("整数(int)"), QString::fromLatin1(JsonVueStyle::kInt));
  m_editStyleCombo->addItem(QStringLiteral("小数(float)"),
                            QString::fromLatin1(JsonVueStyle::kFloat));
  m_editStyleCombo->addItem(QStringLiteral("金额(money)"),
                            QString::fromLatin1(JsonVueStyle::kMoney));
  m_editStyleCombo->addItem(QStringLiteral("日期(date)"), QString::fromLatin1(JsonVueStyle::kDate));
  m_editStyleCombo->addItem(QStringLiteral("标签(tag)"), QString::fromLatin1(JsonVueStyle::kTag));
  m_editStyleCombo->addItem(QStringLiteral("布尔文字(boolean)"),
                            QString::fromLatin1(JsonVueStyle::kBoolean));
  m_editStyleCombo->addItem(QStringLiteral("图片(image)"),
                            QString::fromLatin1(JsonVueStyle::kImage));
  m_editStyleCombo->addItem(QStringLiteral("下拉框(select)"),
                            QString::fromLatin1(JsonVueStyle::kSelect));
  m_editStyleCombo->addItem(QStringLiteral("多行文本(textarea)"),
                            QString::fromLatin1(JsonVueStyle::kTextarea));
  // 设置当前编辑样式
  int editIdx = m_editStyleCombo->findData(editStyleToString(m_editStyle));
  if (editIdx >= 0) m_editStyleCombo->setCurrentIndex(editIdx);
  addRow(QStringLiteral("编辑样式:"), m_editStyleCombo);

  // 编辑样式子控件容器
  m_editStyleWidget = new QWidget(this);
  m_editStyleLayout = new QVBoxLayout(m_editStyleWidget);
  m_editStyleLayout->setContentsMargins(0, 0, 0, 0);
  m_editStyleLayout->setSpacing(6);
  auto *editStyleForm = new QFormLayout;
  editStyleForm->setContentsMargins(0, 0, 0, 0);
  editStyleForm->setSpacing(6);
  editStyleForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_editStyleLayout->addLayout(editStyleForm);
  m_formLayout->addRow(QString(), m_editStyleWidget);

  // 切换编辑样式时重建子控件
  connect(m_editStyleCombo, &QComboBox::currentTextChanged, this, [this]() {
    // 缓存当前值
    if (m_placeholderEdit) m_cachedPlaceholder = m_placeholderEdit->text().trimmed();
    if (m_maxlengthCombo) m_cachedMaxlength = m_maxlengthCombo->currentData().toInt();
    if (m_minValueCombo) {
      QVariant d = m_minValueCombo->currentData();
      m_cachedMinValue = d.isValid() ? d.toDouble() : m_minValueCombo->currentText().toDouble();
    }
    if (m_maxValueCombo) {
      QVariant d = m_maxValueCombo->currentData();
      m_cachedMaxValue = d.isValid() ? d.toDouble() : m_maxValueCombo->currentText().toDouble();
    }
    if (m_precisionCombo) m_cachedPrecision = m_precisionCombo->currentData().toInt();
    if (m_dateFormatCombo) m_cachedDateFormat = m_dateFormatCombo->currentData().toString();
    if (m_textareaRowsCombo) m_cachedTextareaRows = m_textareaRowsCombo->currentData().toInt();
    // 更新 m_editStyle
    m_editStyle = stringToEditStyle(m_editStyleCombo->currentData().toString());
    rebuildEditStyleControls();
  });

  // 初次构建
  rebuildEditStyleControls();

  // 编辑可编辑
  m_editEditableCheck = new QCheckBox(this);
  m_editEditableCheck->setChecked(true);
  addRow(QStringLiteral("编辑可编辑:"), m_editEditableCheck);

  // 必填
  m_requiredCheck = new QCheckBox(this);
  addRow(QStringLiteral("必填:"), m_requiredCheck);

  // 表单布局
  m_formSpanCombo = new QComboBox(this);
  m_formSpanCombo->addItem(QStringLiteral("整行"), 24);
  m_formSpanCombo->addItem(QStringLiteral("半行"), 12);
  m_formSpanCombo->addItem(QStringLiteral("三分之一"), 8);
  addRow(QStringLiteral("表单布局:"), m_formSpanCombo);

  // ════════════════════════════════════════
  //  通用配置
  // ════════════════════════════════════════
  auto *commonSep = new QLabel(QStringLiteral("── 通用配置 ──"), this);
  commonSep->setAlignment(Qt::AlignCenter);
  m_formLayout->addRow(QString(), commonSep);

  m_defaultValueEdit = new QLineEdit(this);
  m_defaultValueEdit->setPlaceholderText(QStringLiteral("新增记录时的默认值"));
  addRow(QStringLiteral("默认值:"), m_defaultValueEdit);

  m_defaultSortCombo = new QComboBox(this);
  m_defaultSortCombo->addItem(QStringLiteral("无"), QString());
  m_defaultSortCombo->addItem(QStringLiteral("升序"), QStringLiteral("asc"));
  m_defaultSortCombo->addItem(QStringLiteral("降序"), QStringLiteral("desc"));
  addRow(QStringLiteral("默认排序:"), m_defaultSortCombo);

  // ── 底部按钮 ──
  auto btns = AuiButton::createDialogButtons(this);
  mainLayout->addLayout(btns.layout);

  connect(btns.okBtn, &QPushButton::clicked, this, &QDialog::accept);
  connect(btns.cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

  AuiWindow::applyWindowFrame(this, tb.titleBar, contentWidget);

  // 初次构建后按内容自适应高度
  adjustToContents();
}

void ColumnStyleDialog::addRow(const QString &labelText, QWidget *widget) {
  if (m_formLayout) m_formLayout->addRow(labelText, widget);
}

void ColumnStyleDialog::adjustToContents() {
  // 让对话框高度随内容自适应（支持缩小）
  // 难点：从大尺寸内容（如 tag 标签映射表）切换到小尺寸内容（如纯文本）时，
  //   - setVisible(false) 后子布局的 sizeHint 不会立即重算（Qt 通过异步 LayoutRequest 事件触发）
  //   - 导致 layout()->sizeHint() 仍返回切换前的旧值，resize 到旧值当然不会缩小
  //   - 即使 sizeHint 正确，resize(sizeHint) 对已显示窗口只能扩大、不能缩小
  // 解决：
  //   1. 递归 invalidate 所有子布局，清除 sizeHint 缓存
  //   2. activate 重算并应用几何
  //   3. setFixedSize 强制 Qt 立即应用新大小（包括缩小）
  //   4. 恢复 min/max 为默认值，允许后续扩展和用户拖拽
  //   5. QTimer::singleShot(0) 在事件循环后再次调整（处理异步 LayoutRequest）
  if (!layout()) return;
  auto doAdjust = [this]() {
    if (!layout()) return;
    invalidateAllLayouts(this);
    layout()->activate();
    QSize hint = layout()->sizeHint();
    setFixedSize(width(), hint.height());
    setMinimumSize(minimumWidth(), 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
  };
  doAdjust();
  // 事件循环后再调整一次，处理异步 LayoutRequest 事件
  QTimer::singleShot(0, this, doAdjust);
}

// ════════════════════════════════════════════════════════════
//  动态重建：显示类型子控件
// ════════════════════════════════════════════════════════════

void ColumnStyleDialog::rebuildDisplayTypeControls() {
  if (!m_displayTypeWidget) return;
  // 清空旧控件（用 removeRow 正确删除 QFormLayout 的 label 和 field）
  auto *item0 = m_displayTypeLayout->itemAt(0);
  if (!item0) return;
  auto *oldLayout = item0->layout();
  auto *form = qobject_cast<QFormLayout *>(oldLayout);
  if (!form) return;
  clearFormLayout(form);
  // 重置指针
  m_tagItemsTable = nullptr;
  m_boolTrueTextEdit = nullptr;
  m_boolFalseTextEdit = nullptr;
  m_switchEditableCheck = nullptr;

  QString dtype = m_displayTypeCombo ? m_displayTypeCombo->currentData().toString() : QString();

  if (dtype == JsonVueStyle::kTag) {
    m_displayTypeWidget->setMaximumHeight(QWIDGETSIZE_MAX);  // 恢复高度限制
    m_displayTypeWidget->setVisible(true);
    // 标签映射表（动态增删行）—— 占据全宽
    m_tagItemsTable = new QTableWidget(0, 4, m_displayTypeWidget);
    m_tagItemsTable->setHorizontalHeaderLabels({QStringLiteral("值"), QStringLiteral("文字"),
                                                QString::fromUtf8(CodeConstants::UiText::kColor),
                                                QStringLiteral("操作")});
    m_tagItemsTable->verticalHeader()->setVisible(false);
    m_tagItemsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tagItemsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tagItemsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tagItemsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tagItemsTable->setMinimumHeight(160);
    m_tagItemsTable->setMaximumHeight(260);
    populateTagItems(m_cachedTagItems.isEmpty() ? defaultTagItems() : m_cachedTagItems);
    form->addRow(QStringLiteral("  标签映射:"), m_tagItemsTable);

    // 开关可编辑（仅 boolean/tag 显示）
    m_switchEditableCheck = new QCheckBox(QStringLiteral("列表页可直接切换"), m_displayTypeWidget);
    m_switchEditableCheck->setChecked(m_cachedSwitchEditable);
    form->addRow(QStringLiteral("  开关可编辑:"), m_switchEditableCheck);
  } else if (dtype == JsonVueStyle::kBoolean) {
    m_displayTypeWidget->setMaximumHeight(QWIDGETSIZE_MAX);  // 恢复高度限制
    m_displayTypeWidget->setVisible(true);
    m_boolTrueTextEdit = new QLineEdit(m_displayTypeWidget);
    m_boolTrueTextEdit->setPlaceholderText(QStringLiteral("如: 显示"));
    m_boolTrueTextEdit->setText(m_cachedBoolTrueText);
    form->addRow(QStringLiteral("  真值文字:"), m_boolTrueTextEdit);

    m_boolFalseTextEdit = new QLineEdit(m_displayTypeWidget);
    m_boolFalseTextEdit->setPlaceholderText(QStringLiteral("如: 隐藏"));
    m_boolFalseTextEdit->setText(m_cachedBoolFalseText);
    form->addRow(QStringLiteral("  假值文字:"), m_boolFalseTextEdit);

    // 开关可编辑（仅 boolean/tag 显示）
    m_switchEditableCheck = new QCheckBox(QStringLiteral("列表页可直接切换"), m_displayTypeWidget);
    m_switchEditableCheck->setChecked(m_cachedSwitchEditable);
    form->addRow(QStringLiteral("  开关可编辑:"), m_switchEditableCheck);
  } else {
    // 非标签/布尔样式时隐藏容器，避免占用空间
    m_displayTypeWidget->setVisible(false);
    // 强制高度为 0，绕过 QFormLayout 可能仍计算隐藏 field 高度的问题
    m_displayTypeWidget->setMaximumHeight(0);
  }
  adjustToContents();
}

/// 颜色选项列表（success/primary/warning/info/danger）
static const QStringList kTagColorOptions = {
    QString::fromLatin1(JsonVueColor::kSuccess), QString::fromLatin1(JsonVueColor::kPrimary),
    QString::fromLatin1(JsonVueColor::kWarning), QString::fromLatin1(JsonVueColor::kInfo),
    QString::fromLatin1(JsonVueColor::kDanger)};

/// 创建颜色选择下拉框（含"自定义..."选项，支持任意十六进制颜色）
static QComboBox *createColorCombo(QWidget *parent, const QString &currentColor) {
  auto *combo = new QComboBox(parent);
  combo->addItems(kTagColorOptions);
  combo->addItem(QStringLiteral("自定义..."));
  // 检查当前颜色是否在预设中
  int idx = combo->findText(currentColor);
  if (idx >= 0) {
    combo->setCurrentIndex(idx);
  } else if (!currentColor.isEmpty()) {
    // 自定义颜色，插入到"自定义..."之前
    combo->insertItem(combo->count() - 1, currentColor);
    combo->setCurrentIndex(combo->count() - 2);
  }
  // 选择"自定义..."时弹出颜色选择器
  QObject::connect(combo, &QComboBox::currentTextChanged, combo, [combo](const QString &text) {
    if (text == QStringLiteral("自定义...")) {
      QColor color = QColorDialog::getColor();
      if (color.isValid()) {
        QString colorName = color.name();  // 如 "#ff0000"
        int existing = combo->findText(colorName);
        if (existing < 0) {
          combo->insertItem(combo->count() - 1, colorName);
        }
        combo->setCurrentText(colorName);
      } else {
        combo->setCurrentIndex(0);
      }
    }
  });
  return combo;
}

void ColumnStyleDialog::populateTagItems(const QList<TagItem> &items) {
  if (!m_tagItemsTable) return;
  m_tagItemsTable->setRowCount(0);
  for (const auto &item : items) {
    int row = m_tagItemsTable->rowCount();
    m_tagItemsTable->insertRow(row);
    m_tagItemsTable->setItem(row, 0, new QTableWidgetItem(item.value));
    m_tagItemsTable->setItem(row, 1, new QTableWidgetItem(item.text));
    auto *colorCombo = createColorCombo(m_tagItemsTable, item.color);
    m_tagItemsTable->setCellWidget(row, 2, colorCombo);
    auto *delBtn =
        new QPushButton(QString::fromUtf8(CodeConstants::UiText::kDelete), m_tagItemsTable);
    m_tagItemsTable->setCellWidget(row, 3, delBtn);
    // 删除按钮：删除当前行
    connect(delBtn, &QPushButton::clicked, this, [this, delBtn]() {
      for (int r = 0; r < m_tagItemsTable->rowCount(); ++r) {
        if (m_tagItemsTable->cellWidget(r, 3) == delBtn) {
          m_tagItemsTable->removeRow(r);
          break;
        }
      }
    });
  }
  // 添加"+"行按钮（在表格下方）
  // 用一行特殊行表示新增
  int addRowIdx = m_tagItemsTable->rowCount();
  m_tagItemsTable->insertRow(addRowIdx);
  auto *addBtn = new QPushButton(QStringLiteral("+ 添加"), m_tagItemsTable);
  m_tagItemsTable->setCellWidget(addRowIdx, 3, addBtn);
  connect(addBtn, &QPushButton::clicked, this, [this, addBtn]() {
    // 找到当前 + 行的位置，在其前面插入新行
    for (int r = 0; r < m_tagItemsTable->rowCount(); ++r) {
      if (m_tagItemsTable->cellWidget(r, 3) == addBtn) {
        TagItem empty;
        empty.value = QStringLiteral("0");
        empty.text = QString();
        empty.color = QString::fromLatin1(JsonVueColor::kInfo);
        m_tagItemsTable->insertRow(r);
        m_tagItemsTable->setItem(r, 0, new QTableWidgetItem(empty.value));
        m_tagItemsTable->setItem(r, 1, new QTableWidgetItem(empty.text));
        auto *colorCombo = createColorCombo(m_tagItemsTable, empty.color);
        m_tagItemsTable->setCellWidget(r, 2, colorCombo);
        auto *delBtn =
            new QPushButton(QString::fromUtf8(CodeConstants::UiText::kDelete), m_tagItemsTable);
        m_tagItemsTable->setCellWidget(r, 3, delBtn);
        connect(delBtn, &QPushButton::clicked, this, [this, delBtn]() {
          for (int rr = 0; rr < m_tagItemsTable->rowCount(); ++rr) {
            if (m_tagItemsTable->cellWidget(rr, 3) == delBtn) {
              m_tagItemsTable->removeRow(rr);
              break;
            }
          }
        });
        break;
      }
    }
  });
}

QList<TagItem> ColumnStyleDialog::collectTagItems() const {
  QList<TagItem> items;
  if (!m_tagItemsTable) return items;
  for (int r = 0; r < m_tagItemsTable->rowCount(); ++r) {
    // 跳过"+"行（其第0列无 item 或为空）
    auto *valItem = m_tagItemsTable->item(r, 0);
    if (!valItem) continue;
    QString value = valItem->text().trimmed();
    auto *textItem = m_tagItemsTable->item(r, 1);
    auto *colorCombo = qobject_cast<QComboBox *>(m_tagItemsTable->cellWidget(r, 2));
    // 跳过"+"行（无 colorCombo 或为添加按钮行）
    if (!colorCombo) continue;
    TagItem t;
    t.value = value;
    t.text = textItem ? textItem->text().trimmed() : QString();
    t.color = colorCombo->currentText();
    items.append(t);
  }
  return items;
}

// ════════════════════════════════════════════════════════════
//  动态重建：编辑样式子控件
// ════════════════════════════════════════════════════════════

void ColumnStyleDialog::rebuildEditStyleControls() {
  if (!m_editStyleWidget) return;
  auto *item0 = m_editStyleLayout->itemAt(0);
  if (!item0) return;
  auto *oldLayout = item0->layout();
  auto *form = qobject_cast<QFormLayout *>(oldLayout);
  if (!form) return;
  clearFormLayout(form);
  // 重置指针
  m_placeholderEdit = nullptr;
  m_maxlengthCombo = nullptr;
  m_minValueCombo = nullptr;
  m_maxValueCombo = nullptr;
  m_precisionCombo = nullptr;
  m_dateFormatCombo = nullptr;
  m_textareaRowsCombo = nullptr;
  m_selectSourceBtn = nullptr;

  switch (m_editStyle) {
    case EditStyle::Text: {
      m_placeholderEdit = new QLineEdit(m_editStyleWidget);
      m_placeholderEdit->setPlaceholderText(QStringLiteral("请输入占位提示"));
      m_placeholderEdit->setText(m_cachedPlaceholder);
      form->addRow(QStringLiteral("  占位提示:"), m_placeholderEdit);

      m_maxlengthCombo = new QComboBox(m_editStyleWidget);
      m_maxlengthCombo->addItem(QStringLiteral("不限"), 0);
      for (int ml : {10, 20, 50, 100, 200, 500, 1000}) {
        m_maxlengthCombo->addItem(QString::number(ml), ml);
      }
      int idx = m_maxlengthCombo->findData(m_cachedMaxlength);
      m_maxlengthCombo->setCurrentIndex(idx >= 0 ? idx : 0);
      form->addRow(QStringLiteral("  最大长度:"), m_maxlengthCombo);
      break;
    }
    case EditStyle::Int: {
      m_minValueCombo = new QComboBox(m_editStyleWidget);
      m_minValueCombo->setEditable(true);
      for (double v : {0.0, 1.0, 10.0, 100.0, 1000.0, -1.0, -10.0, -100.0}) {
        m_minValueCombo->addItem(QString::number(v), v);
      }
      int idx = m_minValueCombo->findData(m_cachedMinValue);
      if (idx >= 0)
        m_minValueCombo->setCurrentIndex(idx);
      else
        m_minValueCombo->setEditText(QString::number(m_cachedMinValue));
      form->addRow(QStringLiteral("  最小值:"), m_minValueCombo);

      m_maxValueCombo = new QComboBox(m_editStyleWidget);
      m_maxValueCombo->setEditable(true);
      for (double v : {1.0, 10.0, 100.0, 1000.0, 9999.0, 99999.0, 999999.0}) {
        m_maxValueCombo->addItem(QString::number(v), v);
      }
      idx = m_maxValueCombo->findData(m_cachedMaxValue);
      if (idx >= 0)
        m_maxValueCombo->setCurrentIndex(idx);
      else
        m_maxValueCombo->setEditText(QString::number(m_cachedMaxValue));
      form->addRow(QStringLiteral("  最大值:"), m_maxValueCombo);
      break;
    }
    case EditStyle::Float: {
      m_precisionCombo = new QComboBox(m_editStyleWidget);
      for (int p : {0, 1, 2, 3, 4, 6}) {
        m_precisionCombo->addItem(QString::number(p), p);
      }
      int idx = m_precisionCombo->findData(m_cachedPrecision);
      m_precisionCombo->setCurrentIndex(idx >= 0 ? idx : 2);
      form->addRow(QStringLiteral("  小数位数:"), m_precisionCombo);

      m_minValueCombo = new QComboBox(m_editStyleWidget);
      m_minValueCombo->setEditable(true);
      for (double v : {0.0, 1.0, 10.0, 100.0, 1000.0, -1.0, -10.0, -100.0}) {
        m_minValueCombo->addItem(QString::number(v), v);
      }
      idx = m_minValueCombo->findData(m_cachedMinValue);
      if (idx >= 0)
        m_minValueCombo->setCurrentIndex(idx);
      else
        m_minValueCombo->setEditText(QString::number(m_cachedMinValue));
      form->addRow(QStringLiteral("  最小值:"), m_minValueCombo);

      m_maxValueCombo = new QComboBox(m_editStyleWidget);
      m_maxValueCombo->setEditable(true);
      for (double v : {1.0, 10.0, 100.0, 1000.0, 9999.0, 99999.0, 999999.0}) {
        m_maxValueCombo->addItem(QString::number(v), v);
      }
      idx = m_maxValueCombo->findData(m_cachedMaxValue);
      if (idx >= 0)
        m_maxValueCombo->setCurrentIndex(idx);
      else
        m_maxValueCombo->setEditText(QString::number(m_cachedMaxValue));
      form->addRow(QStringLiteral("  最大值:"), m_maxValueCombo);
      break;
    }
    case EditStyle::Money: {
      // 金额：小数位数（复用 precision 字段）
      m_precisionCombo = new QComboBox(m_editStyleWidget);
      for (int p : {0, 1, 2, 3, 4, 6}) {
        m_precisionCombo->addItem(QString::number(p), p);
      }
      int idx = m_precisionCombo->findData(m_cachedPrecision);
      m_precisionCombo->setCurrentIndex(idx >= 0 ? idx : 2);
      form->addRow(QStringLiteral("  小数位数:"), m_precisionCombo);
      break;
    }
    case EditStyle::Date: {
      m_dateFormatCombo = new QComboBox(m_editStyleWidget);
      m_dateFormatCombo->addItem(QString::fromUtf8(CodeConstants::UiText::kDatetimeFull),
                                 QString::fromLatin1(JsonVueStyle::kDatetime));
      m_dateFormatCombo->addItem(QStringLiteral("时分秒"),
                                 QString::fromLatin1(JsonVueStyle::kTime));
      m_dateFormatCombo->addItem(QStringLiteral("年月日"),
                                 QString::fromLatin1(JsonVueStyle::kDate));
      m_dateFormatCombo->addItem(QString::fromUtf8(CodeConstants::UiText::kYearMonth),
                                 QString::fromLatin1(JsonVueStyle::kMonth));
      m_dateFormatCombo->addItem(QStringLiteral("年"), QString::fromLatin1(JsonVueStyle::kYear));
      m_dateFormatCombo->addItem(QString::fromUtf8(CodeConstants::UiText::kDateRange),
                                 QString::fromLatin1(JsonVueStyle::kDaterange));
      int idx = m_dateFormatCombo->findData(m_cachedDateFormat);
      m_dateFormatCombo->setCurrentIndex(idx >= 0 ? idx : 1);
      form->addRow(QStringLiteral("  日期格式:"), m_dateFormatCombo);
      break;
    }
    case EditStyle::Tag: {
      auto *hint =
          new QLabel(QStringLiteral("  （标签映射在上方显示样式配置）"), m_editStyleWidget);
      form->addRow(QString(), hint);
      break;
    }
    case EditStyle::Boolean: {
      auto *hint =
          new QLabel(QStringLiteral("  （真假文字在上方显示样式配置）"), m_editStyleWidget);
      form->addRow(QString(), hint);
      break;
    }
    case EditStyle::Image: {
      auto *hint = new QLabel(QStringLiteral("  （图片 URL 输入）"), m_editStyleWidget);
      form->addRow(QString(), hint);
      break;
    }
    case EditStyle::Select: {
      // 下拉框：显示"数据源"按钮，点击弹出 ComboboxConfigDialog
      m_selectSourceBtn = new QPushButton(QStringLiteral("配置数据源..."), m_editStyleWidget);
      form->addRow(QStringLiteral("  数据源:"), m_selectSourceBtn);
      connect(m_selectSourceBtn, &QPushButton::clicked, this, [this]() {
        ComboboxConfigDialog dlg(this);
        dlg.setConfig(m_cachedSelectUrl, m_cachedSelectValueField, m_cachedSelectLabelField);
        dlg.setHttpConfig(m_baseUrl, m_authHeader, m_postData);
        if (dlg.exec() == QDialog::Accepted) {
          m_cachedSelectUrl = dlg.url();
          m_cachedSelectValueField = dlg.valueField();
          m_cachedSelectLabelField = dlg.labelField();
        }
      });
      break;
    }
    case EditStyle::TextArea: {
      m_textareaRowsCombo = new QComboBox(m_editStyleWidget);
      for (int r : {2, 3, 4, 5, 6, 8, 10}) {
        m_textareaRowsCombo->addItem(QString::number(r), r);
      }
      int idx = m_textareaRowsCombo->findData(m_cachedTextareaRows);
      m_textareaRowsCombo->setCurrentIndex(idx >= 0 ? idx : 1);
      form->addRow(QStringLiteral("  行数:"), m_textareaRowsCombo);

      m_placeholderEdit = new QLineEdit(m_editStyleWidget);
      m_placeholderEdit->setPlaceholderText(QStringLiteral("请输入占位提示"));
      m_placeholderEdit->setText(m_cachedPlaceholder);
      form->addRow(QStringLiteral("  占位提示:"), m_placeholderEdit);
      break;
    }
  }
  adjustToContents();
}

// ════════════════════════════════════════════════════════════
//  ColumnStyleDialog 配置读写
// ════════════════════════════════════════════════════════════

void ColumnStyleDialog::setEditStyle(EditStyle style) {
  m_editStyle = style;
  if (m_editStyleCombo) {
    int idx = m_editStyleCombo->findData(editStyleToString(style));
    if (idx >= 0) m_editStyleCombo->setCurrentIndex(idx);
  }
}

EditStyle ColumnStyleDialog::editStyle() const {
  if (m_editStyleCombo) {
    return stringToEditStyle(m_editStyleCombo->currentData().toString());
  }
  return m_editStyle;
}

void ColumnStyleDialog::setEditEditable(bool v) {
  if (m_editEditableCheck) m_editEditableCheck->setChecked(v);
}

bool ColumnStyleDialog::editEditable() const {
  return m_editEditableCheck ? m_editEditableCheck->isChecked() : true;
}

void ColumnStyleDialog::setSwitchEditable(bool v) {
  m_cachedSwitchEditable = v;
  if (m_switchEditableCheck) m_switchEditableCheck->setChecked(v);
}

bool ColumnStyleDialog::switchEditable() const {
  return m_switchEditableCheck ? m_switchEditableCheck->isChecked() : m_cachedSwitchEditable;
}

void ColumnStyleDialog::setPlaceholder(const QString &v) {
  m_cachedPlaceholder = v;
  if (m_placeholderEdit) m_placeholderEdit->setText(v);
}

QString ColumnStyleDialog::placeholder() const {
  return m_placeholderEdit ? m_placeholderEdit->text().trimmed() : m_cachedPlaceholder;
}

void ColumnStyleDialog::setMaxlength(int v) {
  m_cachedMaxlength = v;
  if (m_maxlengthCombo) {
    int idx = m_maxlengthCombo->findData(v);
    if (idx >= 0) m_maxlengthCombo->setCurrentIndex(idx);
  }
}

int ColumnStyleDialog::maxlength() const {
  return m_maxlengthCombo ? m_maxlengthCombo->currentData().toInt() : m_cachedMaxlength;
}

void ColumnStyleDialog::setMinValue(double v) {
  m_cachedMinValue = v;
  if (m_minValueCombo) {
    int idx = m_minValueCombo->findData(v);
    if (idx >= 0)
      m_minValueCombo->setCurrentIndex(idx);
    else
      m_minValueCombo->setEditText(QString::number(v));
  }
}

double ColumnStyleDialog::minValue() const {
  if (!m_minValueCombo) return m_cachedMinValue;
  QVariant d = m_minValueCombo->currentData();
  return d.isValid() ? d.toDouble() : m_minValueCombo->currentText().toDouble();
}

void ColumnStyleDialog::setMaxValue(double v) {
  m_cachedMaxValue = v;
  if (m_maxValueCombo) {
    int idx = m_maxValueCombo->findData(v);
    if (idx >= 0)
      m_maxValueCombo->setCurrentIndex(idx);
    else
      m_maxValueCombo->setEditText(QString::number(v));
  }
}

double ColumnStyleDialog::maxValue() const {
  if (!m_maxValueCombo) return m_cachedMaxValue;
  QVariant d = m_maxValueCombo->currentData();
  return d.isValid() ? d.toDouble() : m_maxValueCombo->currentText().toDouble();
}

void ColumnStyleDialog::setPrecision(int v) {
  m_cachedPrecision = v;
  if (m_precisionCombo) {
    int idx = m_precisionCombo->findData(v);
    if (idx >= 0) m_precisionCombo->setCurrentIndex(idx);
  }
}

int ColumnStyleDialog::precision() const {
  return m_precisionCombo ? m_precisionCombo->currentData().toInt() : m_cachedPrecision;
}

void ColumnStyleDialog::setDateFormat(const QString &v) {
  m_cachedDateFormat = v;
  if (m_dateFormatCombo) {
    int idx = m_dateFormatCombo->findData(v);
    if (idx >= 0) m_dateFormatCombo->setCurrentIndex(idx);
  }
}

QString ColumnStyleDialog::dateFormat() const {
  return m_dateFormatCombo ? m_dateFormatCombo->currentData().toString() : m_cachedDateFormat;
}

void ColumnStyleDialog::setTextareaRows(int v) {
  m_cachedTextareaRows = v;
  if (m_textareaRowsCombo) {
    int idx = m_textareaRowsCombo->findData(v);
    if (idx >= 0) m_textareaRowsCombo->setCurrentIndex(idx);
  }
}

int ColumnStyleDialog::textareaRows() const {
  return m_textareaRowsCombo ? m_textareaRowsCombo->currentData().toInt() : m_cachedTextareaRows;
}

// ── 通用配置 ──

void ColumnStyleDialog::setRequired(bool v) {
  if (m_requiredCheck) m_requiredCheck->setChecked(v);
}

bool ColumnStyleDialog::required() const {
  return m_requiredCheck ? m_requiredCheck->isChecked() : false;
}

void ColumnStyleDialog::setColumnWidth(int v) {
  if (m_columnWidthCombo) {
    int idx = m_columnWidthCombo->findData(v);
    if (idx >= 0) m_columnWidthCombo->setCurrentIndex(idx);
  }
}

int ColumnStyleDialog::columnWidth() const {
  return m_columnWidthCombo ? m_columnWidthCombo->currentData().toInt() : 0;
}

void ColumnStyleDialog::setColumnFixed(const QString &v) {
  if (m_columnFixedCombo) {
    int idx = m_columnFixedCombo->findData(v);
    if (idx >= 0) m_columnFixedCombo->setCurrentIndex(idx);
  }
}

QString ColumnStyleDialog::columnFixed() const {
  return m_columnFixedCombo ? m_columnFixedCombo->currentData().toString() : QString();
}

void ColumnStyleDialog::setFormatter(const QString &v) {
  if (m_formatterCombo) {
    int idx = m_formatterCombo->findData(v);
    if (idx >= 0) m_formatterCombo->setCurrentIndex(idx);
  }
}

QString ColumnStyleDialog::formatter() const {
  return m_formatterCombo ? m_formatterCombo->currentData().toString() : QString();
}

void ColumnStyleDialog::setFormSpan(int v) {
  if (m_formSpanCombo) {
    int idx = m_formSpanCombo->findData(v);
    if (idx >= 0) m_formSpanCombo->setCurrentIndex(idx);
  }
}

int ColumnStyleDialog::formSpan() const {
  return m_formSpanCombo ? m_formSpanCombo->currentData().toInt() : 24;
}

// ── 表格列显示样式 ──

void ColumnStyleDialog::setDisplayType(const QString &v) {
  if (m_displayTypeCombo) {
    int idx = m_displayTypeCombo->findData(v);
    int newIdx = idx >= 0 ? idx : 0;
    if (m_displayTypeCombo->currentIndex() != newIdx) {
      // 选项改变，currentTextChanged 信号会触发 rebuildDisplayTypeControls
      m_displayTypeCombo->setCurrentIndex(newIdx);
    } else {
      // 选项未改变，手动重建（首次初始化场景）
      rebuildDisplayTypeControls();
    }
  }
}

QString ColumnStyleDialog::displayType() const {
  return m_displayTypeCombo ? m_displayTypeCombo->currentData().toString() : QString();
}

void ColumnStyleDialog::setTagItems(const QList<TagItem> &items) {
  m_cachedTagItems = items;
  if (m_tagItemsTable) populateTagItems(items);
}

QList<TagItem> ColumnStyleDialog::tagItems() const {
  if (m_tagItemsTable) return collectTagItems();
  return m_cachedTagItems;
}

void ColumnStyleDialog::setBoolTrueText(const QString &v) {
  m_cachedBoolTrueText = v;
  if (m_boolTrueTextEdit) m_boolTrueTextEdit->setText(v);
}

QString ColumnStyleDialog::boolTrueText() const {
  return m_boolTrueTextEdit ? m_boolTrueTextEdit->text().trimmed() : m_cachedBoolTrueText;
}

void ColumnStyleDialog::setBoolFalseText(const QString &v) {
  m_cachedBoolFalseText = v;
  if (m_boolFalseTextEdit) m_boolFalseTextEdit->setText(v);
}

QString ColumnStyleDialog::boolFalseText() const {
  return m_boolFalseTextEdit ? m_boolFalseTextEdit->text().trimmed() : m_cachedBoolFalseText;
}

// ── 下拉框数据源 ──

void ColumnStyleDialog::setSelectUrl(const QString &v) { m_cachedSelectUrl = v; }

QString ColumnStyleDialog::selectUrl() const { return m_cachedSelectUrl; }

void ColumnStyleDialog::setSelectValueField(const QString &v) { m_cachedSelectValueField = v; }

QString ColumnStyleDialog::selectValueField() const { return m_cachedSelectValueField; }

void ColumnStyleDialog::setSelectLabelField(const QString &v) { m_cachedSelectLabelField = v; }

QString ColumnStyleDialog::selectLabelField() const { return m_cachedSelectLabelField; }

void ColumnStyleDialog::setHttpConfig(const QString &baseUrl, const QString &authHeader,
                                      const QString &postData) {
  m_baseUrl = baseUrl;
  m_authHeader = authHeader;
  m_postData = postData;
}

// ── 通用配置（默认值/排序）──

void ColumnStyleDialog::setDefaultValue(const QString &v) {
  if (m_defaultValueEdit) m_defaultValueEdit->setText(v);
}

QString ColumnStyleDialog::defaultValue() const {
  return m_defaultValueEdit ? m_defaultValueEdit->text().trimmed() : QString();
}

void ColumnStyleDialog::setDefaultSort(const QString &v) {
  if (m_defaultSortCombo) {
    int idx = m_defaultSortCombo->findData(v);
    m_defaultSortCombo->setCurrentIndex(idx >= 0 ? idx : 0);
  }
}

QString ColumnStyleDialog::defaultSort() const {
  return m_defaultSortCombo ? m_defaultSortCombo->currentData().toString() : QString();
}

// ════════════════════════════════════════════════════════════
//  ColumnStyleDialog 数据验证
// ════════════════════════════════════════════════════════════

bool ColumnStyleDialog::validateTagItems(QString *error) const {
  if (!m_tagItemsTable) return true;
  QStringList values;
  for (int r = 0; r < m_tagItemsTable->rowCount(); ++r) {
    auto *valItem = m_tagItemsTable->item(r, 0);
    auto *colorCombo = qobject_cast<QComboBox *>(m_tagItemsTable->cellWidget(r, 2));
    if (!valItem || !colorCombo) continue;  // 跳过"+"行
    QString value = valItem->text().trimmed();
    if (value.isEmpty()) {
      if (error) *error = QStringLiteral("标签映射中存在空值，请填写所有值");
      return false;
    }
    if (values.contains(value)) {
      if (error) *error = QStringLiteral("标签映射中值 '%1' 重复，请确保每个值唯一").arg(value);
      return false;
    }
    values.append(value);
  }
  return true;
}

void ColumnStyleDialog::accept() {
  // 验证 tagItems 数据
  QString dtype = m_displayTypeCombo ? m_displayTypeCombo->currentData().toString() : QString();
  if (dtype == JsonVueStyle::kTag && m_tagItemsTable) {
    QString error;
    if (!validateTagItems(&error)) {
      QMessageBox::warning(this, QStringLiteral("数据验证失败"), error);
      return;
    }
  }
  QDialog::accept();
}

// ════════════════════════════════════════════════════════════
//  QueryStyleDialog：查询字段样式对话框
// ════════════════════════════════════════════════════════════

QueryStyleDialog::QueryStyleDialog(QueryInputStyle style, QWidget *parent) : QDialog(parent) {
  m_queryStyle = style;
  setupUI();
}

void QueryStyleDialog::setupUI() {
  setWindowTitle(QStringLiteral("查询样式配置"));
  setMinimumWidth(420);
  // 不设置 minimumHeight/maximumHeight，让对话框高度随内容自适应

  // ── 无边框对话框 ──
  AuiWindow::setupFramelessDialog(this);

  // ── 自定义标题栏 ──
  TitleBarOptions opts;
  opts.title = QStringLiteral("查询样式配置");
  opts.showMinButton = false;
  opts.showMaxButton = false;
  opts.closeRejectsDialog = true;
  auto tb = AuiWindow::createTitleBar(this, opts);

  // ── 内容区域 ──
  auto *contentWidget = new QWidget;
  auto *mainLayout = new QVBoxLayout(contentWidget);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(6);

  auto *form = new QFormLayout;
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(6);
  form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  mainLayout->addLayout(form);

  switch (m_queryStyle) {
    case QueryInputStyle::Text: {
      m_placeholderEdit = new QLineEdit(this);
      m_placeholderEdit->setPlaceholderText(QStringLiteral("请输入占位提示"));
      m_placeholderEdit->setText(m_cachedPlaceholder);
      form->addRow(QStringLiteral("占位提示:"), m_placeholderEdit);
      break;
    }
    case QueryInputStyle::Date: {
      m_dateFormatCombo = new QComboBox(this);
      m_dateFormatCombo->addItem(QString::fromUtf8(CodeConstants::UiText::kDatetimeFull),
                                 QStringLiteral("datetime"));
      m_dateFormatCombo->addItem(QStringLiteral("时分秒"), QStringLiteral("time"));
      m_dateFormatCombo->addItem(QStringLiteral("年月日"), QStringLiteral("date"));
      m_dateFormatCombo->addItem(QString::fromUtf8(CodeConstants::UiText::kYearMonth),
                                 QStringLiteral("month"));
      m_dateFormatCombo->addItem(QStringLiteral("年"), QStringLiteral("year"));
      // 注：日期范围不属于格式，应通过查询关系（QColRelation）配置
      int idx = m_dateFormatCombo->findData(m_cachedDateFormat);
      m_dateFormatCombo->setCurrentIndex(idx >= 0 ? idx : 1);
      form->addRow(QStringLiteral("日期格式:"), m_dateFormatCombo);
      break;
    }
    case QueryInputStyle::Select:
      // Select 样式由 ComboboxConfigDialog 配置，此处不构建任何控件
      // 理论上不应进入此对话框（onConfigureQuerySelect 中 Select 直接走 ComboboxConfigDialog）
      break;
  }

  // ── 底部按钮 ──
  auto btns = AuiButton::createDialogButtons(this);
  mainLayout->addLayout(btns.layout);

  connect(btns.okBtn, &QPushButton::clicked, this, &QDialog::accept);
  connect(btns.cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

  AuiWindow::applyWindowFrame(this, tb.titleBar, contentWidget);

  // 按内容自适应大小
  layout()->activate();
  resize(layout()->sizeHint());
}

// ── 配置读写 ──

void QueryStyleDialog::setPlaceholder(const QString &v) {
  m_cachedPlaceholder = v;
  if (m_placeholderEdit) m_placeholderEdit->setText(v);
}

QString QueryStyleDialog::placeholder() const {
  return m_placeholderEdit ? m_placeholderEdit->text().trimmed() : m_cachedPlaceholder;
}

void QueryStyleDialog::setDateFormat(const QString &v) {
  m_cachedDateFormat = v;
  if (m_dateFormatCombo) {
    int idx = m_dateFormatCombo->findData(v);
    // 若旧值为 "daterange"（已废弃），回退到默认 "date"
    if (idx < 0) idx = m_dateFormatCombo->findData(QString::fromLatin1(JsonVueStyle::kDate));
    if (idx >= 0) m_dateFormatCombo->setCurrentIndex(idx);
  }
}

QString QueryStyleDialog::dateFormat() const {
  return m_dateFormatCombo ? m_dateFormatCombo->currentData().toString() : m_cachedDateFormat;
}
