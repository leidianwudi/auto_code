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
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

#include "combobox_config_dialog.h"
#include "config_dialog_common.h"
#include "src/ui/json_source/json_source_finder.h"
#include "src/ui/json_source/json_source_model.h"
#include "src/util/common/code_constants.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/component/aui_message_box.h"
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
//  静态数据源函数名推导（与 AC 脚本 tool_str.ac / admin_data.ac 保持一致）
//  snakeToPascal/snakeToCamel/urlToFuncName 复刻 AC 侧逻辑，
//  保证 jsonvue 下拉框展示的函数名与 source.tpl 生成的一致
// ════════════════════════════════════════════════════════════

/// 下划线命名 → 帕斯卡命名（"user_role" → "UserRole"）
static QString snakeToPascal(const QString &str) {
  QString res;
  const QStringList segs = str.split(QLatin1Char('_'));
  for (const QString &seg : segs) {
    if (seg.isEmpty()) continue;
    res += seg.at(0).toUpper() + seg.mid(1);
  }
  return res;
}

/// 下划线命名 → 小驼峰命名（"user_role" → "userRole"）
static QString snakeToCamel(const QString &str) {
  const QString pascal = snakeToPascal(str);
  if (pascal.isEmpty()) return pascal;
  return pascal.at(0).toLower() + pascal.mid(1);
}

/// 数据源 url → 函数名（第一段小驼峰，后续段帕斯卡拼接）
/// "enableState" → "enableState"；"vipprice/getTime" → "vippriceGetTime"
static QString sourceUrlToFuncName(const QString &url) {
  QString norm = url;
  norm.replace(QLatin1Char('\\'), QLatin1Char('/'));
  const QStringList parts = norm.split(QLatin1Char('/'));
  QString res;
  bool first = true;
  for (const QString &seg : parts) {
    if (seg.isEmpty()) continue;
    if (first) {
      res = snakeToCamel(seg);
      first = false;
    } else {
      res += snakeToPascal(seg);
    }
  }
  return res;
}

/// 静态数据源函数名（与 admin_data.ac processSources 一致）：
/// 文件名小驼峰 + "Static" + url名（帕斯卡），
/// boolean.jsonsource + enableState → booleanStaticEnableState
static QString staticSourceFuncName(const QString &sourceName, const QString &url) {
  QString urlName = sourceUrlToFuncName(url);
  if (!urlName.isEmpty()) {
    urlName[0] = urlName.at(0).toUpper();  // url 名首字母大写（Pascal）
    return snakeToCamel(sourceName) + QStringLiteral("Static") + urlName;
  }
  return snakeToCamel(sourceName) + QStringLiteral("Static");  // 旧数据无 url 时仅前缀
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
  setMinimumWidth(520);
  // 不设置 minimumHeight/maximumHeight，让对话框高度随内容自适应
  auto frame = beginConfigDialog(this, QStringLiteral("列样式配置"));
  auto *mainLayout = frame.contentLayout;

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
  m_displayTypeCombo = AuiComboBox::create(this);
  m_displayTypeCombo->addItem(QStringLiteral("纯文本(text)"), QStringLiteral(""));
  m_displayTypeCombo->addItem(QStringLiteral("金额(money)"),
                              QString::fromLatin1(JsonVueStyle::kMoney));
  m_displayTypeCombo->addItem(QStringLiteral("标签(tag)"), QString::fromLatin1(JsonVueStyle::kTag));
  m_displayTypeCombo->addItem(QStringLiteral("布尔文字(boolean)"),
                              QString::fromLatin1(JsonVueStyle::kBoolean));
  m_displayTypeCombo->addItem(QStringLiteral("图片(image)"),
                              QString::fromLatin1(JsonVueStyle::kImage));
  m_displayTypeCombo->addItem(QStringLiteral("下拉框(select)"),
                              QString::fromLatin1(JsonVueStyle::kSelect));
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
    // 联动：显示样式选"下拉框(select)" → 编辑样式自动同步为下拉框
    if (!m_syncing &&
        m_displayTypeCombo->currentData().toString() ==
            QString::fromLatin1(JsonVueStyle::kSelect) &&
        m_editStyle != EditStyle::Select) {
      m_syncing = true;
      comboSelectData(m_editStyleCombo, editStyleToString(EditStyle::Select));
      m_syncing = false;
    }
    rebuildDisplayTypeControls();
  });

  // 初次构建
  rebuildDisplayTypeControls();

  // 表格列宽
  m_columnWidthCombo = AuiComboBox::create(this);
  m_columnWidthCombo->addItem(QStringLiteral("自动"), 0);
  for (int w : {60, 80, 100, 120, 150, 200, 250, 300}) {
    m_columnWidthCombo->addItem(QString::number(w), w);
  }
  addRow(QStringLiteral("表格列宽:"), m_columnWidthCombo);

  // 固定列
  m_columnFixedCombo = AuiComboBox::create(this);
  m_columnFixedCombo->addItem(QStringLiteral("不固定"), QString());
  m_columnFixedCombo->addItem(QStringLiteral("固定左侧"), QStringLiteral("left"));
  m_columnFixedCombo->addItem(QStringLiteral("固定右侧"), QStringLiteral("right"));
  addRow(QStringLiteral("固定列:"), m_columnFixedCombo);

  // 格式化类型
  m_formatterCombo = AuiComboBox::create(this);
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
  m_editStyleCombo = AuiComboBox::create(this);
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
  comboSelectData(m_editStyleCombo, editStyleToString(m_editStyle));
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
    if (m_minValueCombo) m_cachedMinValue = numericComboValue(m_minValueCombo, m_cachedMinValue);
    if (m_maxValueCombo) m_cachedMaxValue = numericComboValue(m_maxValueCombo, m_cachedMaxValue);
    if (m_precisionCombo) m_cachedPrecision = m_precisionCombo->currentData().toInt();
    if (m_dateFormatCombo) m_cachedDateFormat = m_dateFormatCombo->currentData().toString();
    if (m_textareaRowsCombo) m_cachedTextareaRows = m_textareaRowsCombo->currentData().toInt();
    // 更新 m_editStyle
    const EditStyle newStyle = stringToEditStyle(m_editStyleCombo->currentData().toString());
    // 联动：编辑样式选"下拉框(select)" → 显示样式自动同步为下拉框（m_syncing 防互触）
    if (!m_syncing && newStyle == EditStyle::Select &&
        m_displayTypeCombo->currentData().toString() !=
            QString::fromLatin1(JsonVueStyle::kSelect)) {
      m_syncing = true;
      comboSelectData(m_displayTypeCombo, QString::fromLatin1(JsonVueStyle::kSelect));
      m_syncing = false;
    }
    m_editStyle = newStyle;
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
  m_formSpanCombo = AuiComboBox::create(this);
  m_formSpanCombo->addItem(QStringLiteral("半行"), 12);
  m_formSpanCombo->addItem(QStringLiteral("整行"), 24);
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

  m_defaultSortCombo = AuiComboBox::create(this);
  m_defaultSortCombo->addItem(QStringLiteral("无"), QString());
  m_defaultSortCombo->addItem(QStringLiteral("升序"), QStringLiteral("asc"));
  m_defaultSortCombo->addItem(QStringLiteral("降序"), QStringLiteral("desc"));
  addRow(QStringLiteral("默认排序:"), m_defaultSortCombo);

  finishConfigDialog(this, frame);

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
  m_boolSourceCombo = nullptr;
  m_switchEditableCheck = nullptr;
  m_selectSourceBtn = nullptr;

  QString dtype = m_displayTypeCombo ? m_displayTypeCombo->currentData().toString() : QString();

  if (dtype == JsonVueStyle::kTag) {
    m_displayTypeWidget->setMaximumHeight(QWIDGETSIZE_MAX);  // 恢复高度限制
    m_displayTypeWidget->setVisible(true);
    // 标签映射表（动态增删行）—— 占据全宽
    m_tagItemsTable = makeConfigTable(
        {{QStringLiteral("值"), QHeaderView::Stretch, 0},
         {QStringLiteral("文字"), QHeaderView::Stretch, 0},
         {QString::fromUtf8(CodeConstants::UiText::kColor), QHeaderView::Stretch, 0},
         {QStringLiteral("操作"), QHeaderView::ResizeToContents, 0}},
        m_displayTypeWidget, 160, 260);
    populateTagItems(m_cachedTagItems.isEmpty() ? defaultTagItems() : m_cachedTagItems);
    form->addRow(QStringLiteral("  标签映射:"), m_tagItemsTable);

    // 开关可编辑（仅 boolean/tag 显示）
    m_switchEditableCheck = new QCheckBox(QStringLiteral("列表页可直接切换"), m_displayTypeWidget);
    m_switchEditableCheck->setChecked(m_cachedSwitchEditable);
    form->addRow(QStringLiteral("  开关可编辑:"), m_switchEditableCheck);
  } else if (dtype == JsonVueStyle::kBoolean) {
    m_displayTypeWidget->setMaximumHeight(QWIDGETSIZE_MAX);  // 恢复高度限制
    m_displayTypeWidget->setVisible(true);

    // 静态数据源下拉：列出所有恰好 2 项选项的静态数据源。
    // 下拉项显示"函数名 - 说明（文件名）"，选中后真假文字从数据源实时读取并锁定
    // 不可改（数据源修改后重开对话框自动同步）；选"手动输入"时解锁，可自由填写真假文字
    m_boolSourceCombo = AuiComboBox::create(m_displayTypeWidget);
    m_boolSourceCombo->addItem(QStringLiteral("（手动输入真假文字）"), QString());
    int restoreIdx = 0;
    const QStringList srcFiles = findJsonsourceFiles(m_searchRoot);
    for (const QString &sf : srcFiles) {
      JsonSourceConfig cfg;
      {
        QFile f(sf);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
          cfg = JsonSourceConfig::fromJsonString(QString::fromUtf8(f.readAll()));
          f.close();
        }
      }
      for (const auto &s : cfg.sources) {
        // 仅 2 项选项的静态数据源可选（真假文字只有两个状态）
        if (!s.isStatic() || s.options.size() != 2) continue;
        const QString remark = s.remark.isEmpty() ? QStringLiteral("(未命名)") : s.remark;
        // 显示：函数名 - 说明（文件名），不显示 0/1 具体值
        const QString funcName = staticSourceFuncName(QFileInfo(sf).baseName(), s.url);
        const QString text =
            QStringLiteral("%1 - %2（%3）").arg(funcName, remark, QFileInfo(sf).fileName());
        m_boolSourceCombo->addItem(text, sf + QStringLiteral("#") + s.id);
        if (sf == m_cachedBoolSourceFile && s.id == m_cachedBoolSourceId) {
          restoreIdx = m_boolSourceCombo->count() - 1;
        }
      }
    }
    form->addRow(QStringLiteral("  静态数据源:"), m_boolSourceCombo);

    m_boolTrueTextEdit = new QLineEdit(m_displayTypeWidget);
    m_boolTrueTextEdit->setPlaceholderText(QStringLiteral("如: 显示"));
    m_boolTrueTextEdit->setText(m_cachedBoolTrueText);
    form->addRow(QStringLiteral("  真值文字:"), m_boolTrueTextEdit);

    m_boolFalseTextEdit = new QLineEdit(m_displayTypeWidget);
    m_boolFalseTextEdit->setPlaceholderText(QStringLiteral("如: 隐藏"));
    m_boolFalseTextEdit->setText(m_cachedBoolFalseText);
    form->addRow(QStringLiteral("  假值文字:"), m_boolFalseTextEdit);

    // 切换数据源 → 从数据源实时填充真假文字并锁定；
    // 切回"手动输入" → 解锁（保留当前文字继续编辑）
    // value 映射：1/true → 真值，0/false → 假值；无法判断时按顺序（第 1 项假、第 2 项真）
    connect(m_boolSourceCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
      if (!m_boolSourceCombo || !m_boolTrueTextEdit || !m_boolFalseTextEdit) return;
      m_cachedBoolSourceFile.clear();
      m_cachedBoolSourceId.clear();
      m_boolTrueTextEdit->setReadOnly(false);
      m_boolFalseTextEdit->setReadOnly(false);
      const QString ref = m_boolSourceCombo->itemData(index).toString();
      if (ref.isEmpty()) return;
      const int sep = ref.lastIndexOf(QLatin1Char('#'));
      const QString filePath = ref.left(sep);
      const QString sourceId = ref.mid(sep + 1);
      JsonSourceConfig cfg;
      {
        QFile f(filePath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
          cfg = JsonSourceConfig::fromJsonString(QString::fromUtf8(f.readAll()));
          f.close();
        }
      }
      const JsonSource *s = cfg.sourceById(sourceId);
      if (!s || !s->isStatic() || s->options.size() != 2) return;
      QString trueText = s->options.at(1).label;
      QString falseText = s->options.at(0).label;
      for (const auto &opt : s->options) {
        const QString v = opt.value.trimmed().toLower();
        if (v == QStringLiteral("1") || v == QStringLiteral("true")) trueText = opt.label;
        if (v == QStringLiteral("0") || v == QStringLiteral("false")) falseText = opt.label;
      }
      m_boolTrueTextEdit->setText(trueText);
      m_boolFalseTextEdit->setText(falseText);
      m_boolTrueTextEdit->setReadOnly(true);
      m_boolFalseTextEdit->setReadOnly(true);
      m_cachedBoolSourceFile = filePath;
      m_cachedBoolSourceId = sourceId;
    });
    // 恢复上次选中的数据源（触发上面的信号：实时填充 + 锁定）
    if (restoreIdx > 0) {
      m_boolSourceCombo->setCurrentIndex(restoreIdx);
    }

    // 开关可编辑（仅 boolean/tag 显示）
    m_switchEditableCheck = new QCheckBox(QStringLiteral("列表页可直接切换"), m_displayTypeWidget);
    m_switchEditableCheck->setChecked(m_cachedSwitchEditable);
    form->addRow(QStringLiteral("  开关可编辑:"), m_switchEditableCheck);
  } else if (dtype == JsonVueStyle::kSelect) {
    m_displayTypeWidget->setMaximumHeight(QWIDGETSIZE_MAX);  // 恢复高度限制
    m_displayTypeWidget->setVisible(true);

    auto *hint = new QLabel(QStringLiteral("  下拉框数据源同时用于列表显示、编辑页下拉与查询"),
                            m_displayTypeWidget);
    form->addRow(QString(), hint);

    // 数据源按钮：点击弹出 ComboboxConfigDialog（列表/编辑/查询三处共享同一数据源）
    m_selectSourceBtn = new QPushButton(QStringLiteral("配置数据源..."), m_displayTypeWidget);
    form->addRow(QStringLiteral("  数据源:"), m_selectSourceBtn);
    connect(m_selectSourceBtn, &QPushButton::clicked, this, [this]() {
      ComboboxConfigDialog dlg(this);
      dlg.setSearchRoot(m_searchRoot);
      dlg.setSourceRef(m_cachedSelectSourceFile, m_cachedSelectSourceId);
      dlg.setConfig(m_cachedSelectUrl, m_cachedSelectValueField, m_cachedSelectLabelField);
      dlg.setPagedConfig(m_cachedSelectPaged, m_cachedSelectPageKey, m_cachedSelectPageSizeKey,
                         m_cachedSelectPageSize, m_cachedSelectSearchTitle,
                         m_cachedSelectSearchField, m_cachedSelectMethod);
      dlg.setHttpConfig(m_baseUrl, m_authHeader, m_postData);
      if (dlg.exec() == QDialog::Accepted) {
        m_cachedSelectSourceFile = dlg.sourceFile();
        m_cachedSelectSourceId = dlg.sourceId();
        m_cachedSelectUrl = dlg.url();
        m_cachedSelectValueField = dlg.valueField();
        m_cachedSelectLabelField = dlg.labelField();
        m_cachedSelectPaged = dlg.paged();
        m_cachedSelectPageKey = dlg.pageKey();
        m_cachedSelectPageSizeKey = dlg.pageSizeKey();
        m_cachedSelectPageSize = dlg.pageSize();
        m_cachedSelectSearchTitle = dlg.searchTitle();
        m_cachedSelectSearchField = dlg.searchField();
        m_cachedSelectMethod = dlg.method();
      }
    });
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
  auto *combo = AuiComboBox::create(parent);
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

void ColumnStyleDialog::insertTagRow(int row, const TagItem &item) {
  m_tagItemsTable->insertRow(row);
  m_tagItemsTable->setItem(row, 0, new QTableWidgetItem(item.value));
  m_tagItemsTable->setItem(row, 1, new QTableWidgetItem(item.text));
  m_tagItemsTable->setCellWidget(row, 2, createColorCombo(m_tagItemsTable, item.color));
  m_tagItemsTable->setCellWidget(row, 3,
                                 makeTableDeleteButton(m_tagItemsTable, 3, m_tagItemsTable));
}

void ColumnStyleDialog::populateTagItems(const QList<TagItem> &items) {
  if (!m_tagItemsTable) return;
  m_tagItemsTable->setRowCount(0);
  for (const auto &item : items) {
    insertTagRow(m_tagItemsTable->rowCount(), item);
  }
  // 末尾"+"添加行（特殊行，仅操作列有按钮；点击在其前面插入新行）
  int addRowIdx = m_tagItemsTable->rowCount();
  m_tagItemsTable->insertRow(addRowIdx);
  auto *addBtn = new QPushButton(QStringLiteral("+ 添加"), m_tagItemsTable);
  m_tagItemsTable->setCellWidget(addRowIdx, 3, addBtn);
  connect(addBtn, &QPushButton::clicked, this, [this, addBtn]() {
    for (int r = 0; r < m_tagItemsTable->rowCount(); ++r) {
      if (m_tagItemsTable->cellWidget(r, 3) == addBtn) {
        TagItem empty;
        empty.value = QStringLiteral("0");
        empty.text = QString();
        empty.color = QString::fromLatin1(JsonVueColor::kInfo);
        insertTagRow(r, empty);
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

      m_maxlengthCombo = AuiComboBox::create(m_editStyleWidget);
      m_maxlengthCombo->addItem(QStringLiteral("不限"), 0);
      for (int ml : {10, 20, 50, 100, 200, 500, 1000}) {
        m_maxlengthCombo->addItem(QString::number(ml), ml);
      }
      comboSelectData(m_maxlengthCombo, m_cachedMaxlength, 0);
      form->addRow(QStringLiteral("  最大长度:"), m_maxlengthCombo);
      break;
    }
    case EditStyle::Int: {
      m_minValueCombo = createNumericCombo(m_editStyleWidget,
                                           {0.0, 1.0, 10.0, 100.0, 1000.0, -1.0, -10.0, -100.0},
                                           m_cachedMinValue);
      form->addRow(QStringLiteral("  最小值:"), m_minValueCombo);

      m_maxValueCombo = createNumericCombo(m_editStyleWidget,
                                           {1.0, 10.0, 100.0, 1000.0, 9999.0, 99999.0, 999999.0},
                                           m_cachedMaxValue);
      form->addRow(QStringLiteral("  最大值:"), m_maxValueCombo);
      break;
    }
    case EditStyle::Float: {
      m_precisionCombo = AuiComboBox::create(m_editStyleWidget);
      for (int p : {0, 1, 2, 3, 4, 6}) {
        m_precisionCombo->addItem(QString::number(p), p);
      }
      comboSelectData(m_precisionCombo, m_cachedPrecision, 2);
      form->addRow(QStringLiteral("  小数位数:"), m_precisionCombo);

      m_minValueCombo = createNumericCombo(m_editStyleWidget,
                                           {0.0, 1.0, 10.0, 100.0, 1000.0, -1.0, -10.0, -100.0},
                                           m_cachedMinValue);
      form->addRow(QStringLiteral("  最小值:"), m_minValueCombo);

      m_maxValueCombo = createNumericCombo(m_editStyleWidget,
                                           {1.0, 10.0, 100.0, 1000.0, 9999.0, 99999.0, 999999.0},
                                           m_cachedMaxValue);
      form->addRow(QStringLiteral("  最大值:"), m_maxValueCombo);
      break;
    }
    case EditStyle::Money: {
      // 金额：小数位数（复用 precision 字段）
      m_precisionCombo = AuiComboBox::create(m_editStyleWidget);
      for (int p : {0, 1, 2, 3, 4, 6}) {
        m_precisionCombo->addItem(QString::number(p), p);
      }
      comboSelectData(m_precisionCombo, m_cachedPrecision, 2);
      form->addRow(QStringLiteral("  小数位数:"), m_precisionCombo);
      break;
    }
    case EditStyle::Date: {
      m_dateFormatCombo = AuiComboBox::create(m_editStyleWidget);
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
      comboSelectData(m_dateFormatCombo, m_cachedDateFormat, 1);
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
      // 下拉框：数据源在列表页（显示样式=下拉框）统一配置，列表/编辑/查询三处共享
      auto *hint = new QLabel(QStringLiteral("  （数据源与列表页一致，在上方显示样式配置）"),
                              m_editStyleWidget);
      form->addRow(QString(), hint);
      break;
    }
    case EditStyle::TextArea: {
      m_textareaRowsCombo = AuiComboBox::create(m_editStyleWidget);
      for (int r : {2, 3, 4, 5, 6, 8, 10}) {
        m_textareaRowsCombo->addItem(QString::number(r), r);
      }
      comboSelectData(m_textareaRowsCombo, m_cachedTextareaRows, 1);
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
  if (m_editStyleCombo) comboSelectData(m_editStyleCombo, editStyleToString(style));
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
  if (m_maxlengthCombo) comboSelectData(m_maxlengthCombo, v);
}

int ColumnStyleDialog::maxlength() const {
  return m_maxlengthCombo ? m_maxlengthCombo->currentData().toInt() : m_cachedMaxlength;
}

void ColumnStyleDialog::setMinValue(double v) {
  m_cachedMinValue = v;
  if (m_minValueCombo) {
    if (m_minValueCombo->findData(v) >= 0)
      comboSelectData(m_minValueCombo, v);
    else
      m_minValueCombo->setEditText(QString::number(v));
  }
}

double ColumnStyleDialog::minValue() const {
  return m_minValueCombo ? numericComboValue(m_minValueCombo, m_cachedMinValue) : m_cachedMinValue;
}

void ColumnStyleDialog::setMaxValue(double v) {
  m_cachedMaxValue = v;
  if (m_maxValueCombo) {
    if (m_maxValueCombo->findData(v) >= 0)
      comboSelectData(m_maxValueCombo, v);
    else
      m_maxValueCombo->setEditText(QString::number(v));
  }
}

double ColumnStyleDialog::maxValue() const {
  return m_maxValueCombo ? numericComboValue(m_maxValueCombo, m_cachedMaxValue) : m_cachedMaxValue;
}

void ColumnStyleDialog::setPrecision(int v) {
  m_cachedPrecision = v;
  if (m_precisionCombo) comboSelectData(m_precisionCombo, v);
}

int ColumnStyleDialog::precision() const {
  return m_precisionCombo ? m_precisionCombo->currentData().toInt() : m_cachedPrecision;
}

void ColumnStyleDialog::setDateFormat(const QString &v) {
  m_cachedDateFormat = v;
  if (m_dateFormatCombo) comboSelectData(m_dateFormatCombo, v);
}

QString ColumnStyleDialog::dateFormat() const {
  return m_dateFormatCombo ? m_dateFormatCombo->currentData().toString() : m_cachedDateFormat;
}

void ColumnStyleDialog::setTextareaRows(int v) {
  m_cachedTextareaRows = v;
  if (m_textareaRowsCombo) comboSelectData(m_textareaRowsCombo, v);
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
  if (m_columnWidthCombo) comboSelectData(m_columnWidthCombo, v);
}

int ColumnStyleDialog::columnWidth() const {
  return m_columnWidthCombo ? m_columnWidthCombo->currentData().toInt() : 0;
}

void ColumnStyleDialog::setColumnFixed(const QString &v) {
  if (m_columnFixedCombo) comboSelectData(m_columnFixedCombo, v);
}

QString ColumnStyleDialog::columnFixed() const {
  return m_columnFixedCombo ? m_columnFixedCombo->currentData().toString() : QString();
}

void ColumnStyleDialog::setFormatter(const QString &v) {
  if (m_formatterCombo) comboSelectData(m_formatterCombo, v);
}

QString ColumnStyleDialog::formatter() const {
  return m_formatterCombo ? m_formatterCombo->currentData().toString() : QString();
}

void ColumnStyleDialog::setFormSpan(int v) {
  if (m_formSpanCombo) comboSelectData(m_formSpanCombo, v);
}

int ColumnStyleDialog::formSpan() const {
  return m_formSpanCombo ? m_formSpanCombo->currentData().toInt() : 12;
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

void ColumnStyleDialog::setBoolSourceRef(const QString &file, const QString &id) {
  m_cachedBoolSourceFile = file;
  m_cachedBoolSourceId = id;
  // 控件已存在时直接选中对应项（触发 currentIndexChanged：实时填充 + 锁定）
  if (m_boolSourceCombo) {
    for (int i = 0; i < m_boolSourceCombo->count(); ++i) {
      if (m_boolSourceCombo->itemData(i).toString() == file + QStringLiteral("#") + id) {
        m_boolSourceCombo->setCurrentIndex(i);
        break;
      }
    }
  }
}

QString ColumnStyleDialog::boolSourceFile() const { return m_cachedBoolSourceFile; }

QString ColumnStyleDialog::boolSourceId() const { return m_cachedBoolSourceId; }

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

void ColumnStyleDialog::setSelectSourceFile(const QString &v) { m_cachedSelectSourceFile = v; }

QString ColumnStyleDialog::selectSourceFile() const { return m_cachedSelectSourceFile; }

void ColumnStyleDialog::setSelectSourceId(const QString &v) { m_cachedSelectSourceId = v; }

QString ColumnStyleDialog::selectSourceId() const { return m_cachedSelectSourceId; }

void ColumnStyleDialog::setSearchRoot(const QString &dir) { m_searchRoot = dir; }

void ColumnStyleDialog::setSelectPaged(bool v) { m_cachedSelectPaged = v; }

bool ColumnStyleDialog::selectPaged() const { return m_cachedSelectPaged; }

void ColumnStyleDialog::setSelectPageKey(const QString &v) { m_cachedSelectPageKey = v; }

QString ColumnStyleDialog::selectPageKey() const { return m_cachedSelectPageKey; }

void ColumnStyleDialog::setSelectPageSizeKey(const QString &v) { m_cachedSelectPageSizeKey = v; }

QString ColumnStyleDialog::selectPageSizeKey() const { return m_cachedSelectPageSizeKey; }

void ColumnStyleDialog::setSelectPageSize(int v) { m_cachedSelectPageSize = v; }

int ColumnStyleDialog::selectPageSize() const { return m_cachedSelectPageSize; }

void ColumnStyleDialog::setSelectSearchTitle(const QString &v) { m_cachedSelectSearchTitle = v; }

QString ColumnStyleDialog::selectSearchTitle() const { return m_cachedSelectSearchTitle; }

void ColumnStyleDialog::setSelectSearchField(const QString &v) { m_cachedSelectSearchField = v; }

QString ColumnStyleDialog::selectSearchField() const { return m_cachedSelectSearchField; }

void ColumnStyleDialog::setSelectMethod(const QString &v) { m_cachedSelectMethod = v; }

QString ColumnStyleDialog::selectMethod() const { return m_cachedSelectMethod; }

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
  if (m_defaultSortCombo) comboSelectData(m_defaultSortCombo, v, 0);
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
      AuiMessageBox::show(this, QStringLiteral("数据验证失败"), error);
      return;
    }
  }
  QDialog::accept();
}
