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
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

#include "combobox_config_dialog.h"
#include "config_dialog_common.h"
#include "src/ui/json_global_enum/json_global_enum_model.h"
#include "src/ui/json_source/json_source_finder.h"
#include "src/ui/json_source/json_source_model.h"
#include "src/ui/json_source/json_upload_model.h"
#include "src/util/common/code_constants.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/component/aui_message_box.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/component/aui_tree_combo.h"

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
//  静态辅助：取值域候选的布尔文字与选项预览（方案 B）
// ════════════════════════════════════════════════════════════

/// 真/假文字提取（与 admin_data.ac 的布尔值约定一致）：
/// value 为 1/true → 真值、0/false → 假值；无法判断时按顺序（第 1 项假、第 2 项真）
static QPair<QString, QString> boolTextsOfLabelValues(
    const QList<QPair<QString, QString>> &labelValues) {
  QString trueText;
  QString falseText;
  if (labelValues.size() == 2) {
    falseText = labelValues.at(0).first;
    trueText = labelValues.at(1).first;
  }
  for (const auto &lv : labelValues) {
    const QString v = lv.second.trimmed().toLower();
    if (v == QStringLiteral("1") || v == QStringLiteral("true")) trueText = lv.first;
    if (v == QStringLiteral("0") || v == QStringLiteral("false")) falseText = lv.first;
  }
  return {trueText, falseText};
}

/// 选项预览文字（"开启=1 / 关闭=0"，超过 4 项截断）
static QString optionPreviewOfLabelValues(const QList<QPair<QString, QString>> &labelValues) {
  QStringList parts;
  int count = 0;
  for (const auto &lv : labelValues) {
    if (count >= 4) {
      parts << QStringLiteral("…");
      break;
    }
    parts << QStringLiteral("%1=%2").arg(lv.first, lv.second);
    ++count;
  }
  return parts.join(QStringLiteral(" / "));
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
  auto frame = beginConfigDialog(this, QStringLiteral("字段设置"));
  auto *mainLayout = frame.contentLayout;

  m_formLayout = new QFormLayout;
  m_formLayout->setContentsMargins(0, 0, 0, 0);
  m_formLayout->setSpacing(6);
  m_formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  mainLayout->addLayout(m_formLayout);

  // 字段名（面板顶部只读提示：本面板是围绕单个字段的属性设置）
  m_fieldNameLabel = new QLabel(this);
  m_fieldNameLabel->setStyleSheet(
      QStringLiteral("font-weight: 600; font-size: 13px;"));
  m_formLayout->addRow(QStringLiteral("字段:"), m_fieldNameLabel);

  // ════════════════════════════════════════
  //  字段取值域（方案 B：声明一次，列渲染/编辑控件/查询筛选三处共用）
  // ════════════════════════════════════════
  auto *domainSep = new QLabel(QStringLiteral("── 字段取值域 ──"), this);
  domainSep->setAlignment(Qt::AlignCenter);
  m_formLayout->addRow(QString(), domainSep);

  // 取值域类型只有两个业务选项（方案 B 简化）：
  //   数据源   → 引用 .jsonsource（静态源/动态源）或全局枚举；
  //              恰 2 项静态源自动获得"枚举"能力（布尔样式可用），动态源走接口选项
  //   远程接口 → 手动 URL（不引用数据源文件）
  // "枚举"不是类型而是能力，由选中源的选项数推导，避免两份高度重叠的候选列表
  m_domainTypeCombo = AuiComboBox::create(this);
  m_domainTypeCombo->addItem(QStringLiteral("未声明（无数据源）"), QString());
  m_domainTypeCombo->addItem(QStringLiteral("数据源（.jsonsource / 全局枚举）"),
                             QStringLiteral("source"));
  m_domainTypeCombo->addItem(QStringLiteral("远程接口（手动 URL）"),
                             QString::fromLatin1(JsonVueDomain::kRemote));
  addRow(QStringLiteral("取值域类型:"), m_domainTypeCombo);

  // 数据源树（枚举/静态域时显示）：候选按「右键设为项目」作用域过滤
  m_domainSourceRow = new QWidget(this);
  auto *domainSrcLay = new QHBoxLayout(m_domainSourceRow);
  domainSrcLay->setContentsMargins(0, 0, 0, 0);
  domainSrcLay->setSpacing(2);
  domainSrcLay->addWidget(AuiButton::createHelpButton(QStringLiteral("数据源作用域"),
                                                      jsonVueSourceScopeHelpText(), m_domainSourceRow));
  m_domainSourceCombo = new AuiTreeCombo(this);
  domainSrcLay->addWidget(m_domainSourceCombo, 1);
  addRow(QStringLiteral("取值数据源:"), m_domainSourceRow);

  // 远程源按钮（远程域时显示）：复用 ComboboxConfigDialog 配置 URL/字段/分页
  m_domainUrlBtn = new QPushButton(QStringLiteral("配置远程数据源..."), this);
  addRow(QStringLiteral("远程源:"), m_domainUrlBtn);
  connect(m_domainUrlBtn, &QPushButton::clicked, this, [this]() {
    ComboboxConfigDialog dlg(this);
    dlg.setSearchRoot(m_searchRoot);
    dlg.setSourceRef(m_cachedSelectSourceFile, m_cachedSelectSourceId);
    dlg.setConfig(m_cachedSelectUrl, m_cachedSelectValueField, m_cachedSelectLabelField);
    dlg.setPagedConfig(m_cachedSelectPaged, m_cachedSelectPageKey, m_cachedSelectPageSizeKey,
                       m_cachedSelectPageSize, m_cachedSelectSearchTitle, m_cachedSelectSearchField,
                       m_cachedSelectMethod);
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

  // 选项预览（数据源域时显示）
  m_domainPreviewLabel = new QLabel(this);
  m_domainPreviewLabel->setStyleSheet(
      QStringLiteral("color: %1; font-size: 12px;").arg(AuiStyle::mutedTextColor().name()));
  addRow(QStringLiteral("选项预览:"), m_domainPreviewLabel);

  // 选中数据源 → 更新取值域缓存（同步布尔/select 分支缓存）+ 预览 + 样式推导
  // （推导规则：恰 2 项 → 布尔文字/布尔编辑可用；其他项数 → 下拉）
  connect(m_domainSourceCombo, &AuiTreeCombo::itemSelected, this, [this](const QVariant &refVar) {
    const QString ref = refVar.toString();
    m_cachedDomainSourceFile.clear();
    m_cachedDomainSourceId.clear();
    m_cachedBoolSourceFile.clear();
    m_cachedBoolSourceId.clear();
    m_cachedSelectSourceFile.clear();
    m_cachedSelectSourceId.clear();
    if (!ref.isEmpty()) {
      const int sep = ref.lastIndexOf(QLatin1Char('#'));
      m_cachedDomainSourceFile = ref.left(sep);
      m_cachedDomainSourceId = ref.mid(sep + 1);
      m_cachedBoolSourceFile = m_cachedDomainSourceFile;
      m_cachedBoolSourceId = m_cachedDomainSourceId;
      m_cachedSelectSourceFile = m_cachedDomainSourceFile;
      m_cachedSelectSourceId = m_cachedDomainSourceId;
    }
    applyDomainBoolTexts(ref);
    updateDomainPreview();
    deriveStylesFromDomain();
  });

  // 取值域类型切换：activated 仅在用户操作时触发——程序性恢复（setDomainType）不重推导样式
  connect(m_domainTypeCombo, &QComboBox::activated, this, [this](int) {
    rebuildDomainControls();
    deriveStylesFromDomain();
  });
  // 初始显隐（默认"未声明"：隐藏数据源/远程源/预览行）
  rebuildDomainControls();

  // ════════════════════════════════════════
  //  列表页展示
  // ════════════════════════════════════════
  auto *tableSep = new QLabel(QStringLiteral("── 列表页展示 ──"), this);
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
    if (!m_syncing) m_stylesTouched = true;  // 用户手动改样式 → 取值域推导让位
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
  //  编辑表单
  // ════════════════════════════════════════
  auto *editSep = new QLabel(QStringLiteral("── 编辑表单 ──"), this);
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
    if (!m_syncing) m_stylesTouched = true;  // 用户手动改样式 → 取值域推导让位
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
//  字段取值域（方案 B）
// ════════════════════════════════════════════════════════════

void ColumnStyleDialog::buildDomainCandidates(bool enumOnly) {
  if (!m_domainSourceCombo) return;
  // 候选构建统一在 config_dialog_common（与查询设置共享同一份实现）。
  // 数据源域涵盖全部数据源文件：静态源（N 项）、动态源（接口）、全局枚举
  buildDomainSourceCandidates(m_domainSourceCombo, m_searchRoot, enumOnly, &m_boolSourceTexts,
                              &m_domainOptionPreviews, &m_domainOptionCounts,
                              &m_domainDynamicFlags);
}

void ColumnStyleDialog::rebuildDomainControls() {
  if (!m_domainTypeCombo) return;
  const QString t = m_domainTypeCombo->currentData().toString();
  m_cachedDomainType = t;
  const bool hasSource = t == QStringLiteral("source");
  const bool isRemote = t == QString::fromLatin1(JsonVueDomain::kRemote);
  setDomainRowVisible(m_domainSourceRow, hasSource);
  setDomainRowVisible(m_domainUrlBtn, isRemote);
  setDomainRowVisible(m_domainPreviewLabel, hasSource);
  if (hasSource) {
    buildDomainCandidates();
    // 恢复选中项（未选时落到顶层"未选择"条目）
    const QString ref = currentDomainRef();
    m_domainSourceCombo->selectByData(ref.isEmpty() ? QVariant(QString()) : QVariant(ref));
  }
  updateDomainPreview();
  adjustToContents();
}

void ColumnStyleDialog::deriveStylesFromDomain() {
  if (m_stylesTouched || m_syncing) return;  // 用户手动改过样式 → 推导让位
  const QString uiType = m_domainTypeCombo ? m_domainTypeCombo->currentData().toString() : QString();
  QString displayValue;
  EditStyle edit = m_editStyle;
  if (uiType == QStringLiteral("source")) {
    // 按选中源的类型推导能力：动态源 → 下拉（选项来自接口）；
    // 静态源恰 2 项 → 布尔文字（枚举能力）；其他项数 → 下拉
    if (m_domainDynamicFlags.value(currentDomainRef(), false)) {
      displayValue = QString::fromLatin1(JsonVueStyle::kSelect);
      edit = EditStyle::Select;
    } else {
      const auto it = m_domainOptionCounts.constFind(currentDomainRef());
      const int n = it == m_domainOptionCounts.constEnd() ? 0 : it.value();
      if (n == 2) {
        displayValue = QString::fromLatin1(JsonVueStyle::kBoolean);
        edit = EditStyle::Boolean;
      } else if (n > 0) {
        displayValue = QString::fromLatin1(JsonVueStyle::kSelect);
        edit = EditStyle::Select;
      } else {
        return;  // 尚未选择源：不动样式
      }
    }
  } else if (uiType == QString::fromLatin1(JsonVueDomain::kRemote)) {
    displayValue = QString::fromLatin1(JsonVueStyle::kSelect);
    edit = EditStyle::Select;
  } else {
    return;  // 未声明：不动样式
  }
  // 程序性改样式：m_syncing 屏蔽 m_stylesTouched 标记与互触联动，
  // 组合框 currentTextChanged 处理器内部会按需重建子控件
  m_syncing = true;
  if (m_displayTypeCombo) comboSelectData(m_displayTypeCombo, displayValue);
  if (m_editStyleCombo) comboSelectData(m_editStyleCombo, editStyleToString(edit));
  m_editStyle = edit;
  m_syncing = false;
}

void ColumnStyleDialog::updateDomainPreview() {
  if (!m_domainPreviewLabel) return;
  if (m_domainTypeCombo && m_domainTypeCombo->currentData().toString() !=
                               QStringLiteral("source")) {
    m_domainPreviewLabel->clear();
    return;
  }
  const auto it = m_domainOptionPreviews.constFind(currentDomainRef());
  m_domainPreviewLabel->setText(it == m_domainOptionPreviews.constEnd()
                                    ? QStringLiteral("（未选择数据源）")
                                    : it.value());
}

void ColumnStyleDialog::applyDomainBoolTexts(const QString &ref) {
  if (!m_boolTrueTextEdit || !m_boolFalseTextEdit) return;
  m_boolTrueTextEdit->setReadOnly(false);
  m_boolFalseTextEdit->setReadOnly(false);
  if (ref.isEmpty()) return;  // 未选源：解锁手动输入
  const auto it = m_boolSourceTexts.constFind(ref);
  if (it == m_boolSourceTexts.constEnd()) return;
  m_cachedBoolTrueText = it.value().first;
  m_cachedBoolFalseText = it.value().second;
  m_boolTrueTextEdit->setText(it.value().first);
  m_boolFalseTextEdit->setText(it.value().second);
  m_boolTrueTextEdit->setReadOnly(true);
  m_boolFalseTextEdit->setReadOnly(true);
}

void ColumnStyleDialog::setDomainRowVisible(QWidget *field, bool visible) {
  if (!field || !m_formLayout) return;
  field->setVisible(visible);
  if (QLabel *lab = qobject_cast<QLabel *>(m_formLayout->labelForField(field))) {
    lab->setVisible(visible);
  }
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

    // 数据源由顶部「字段取值域」提供（枚举域时选项文字自动锁定）；
    // 未声明取值域或未选源时真假文字可自由填写（旧行为）
    m_boolTrueTextEdit = new QLineEdit(m_displayTypeWidget);
    m_boolTrueTextEdit->setPlaceholderText(QStringLiteral("如: 显示"));
    m_boolTrueTextEdit->setText(m_cachedBoolTrueText);
    form->addRow(QStringLiteral("  真值文字:"), m_boolTrueTextEdit);

    m_boolFalseTextEdit = new QLineEdit(m_displayTypeWidget);
    m_boolFalseTextEdit->setPlaceholderText(QStringLiteral("如: 隐藏"));
    m_boolFalseTextEdit->setText(m_cachedBoolFalseText);
    form->addRow(QStringLiteral("  假值文字:"), m_boolFalseTextEdit);

    // 取值域为枚举且已选源 → 真假文字从候选缓存填充并锁定；否则解锁（手动输入）
    applyDomainBoolTexts(currentDomainRef());

    // 开关可编辑（仅 boolean/tag 显示）
    m_switchEditableCheck = new QCheckBox(QStringLiteral("列表页可直接切换"), m_displayTypeWidget);
    m_switchEditableCheck->setChecked(m_cachedSwitchEditable);
    form->addRow(QStringLiteral("  开关可编辑:"), m_switchEditableCheck);
  } else if (dtype == JsonVueStyle::kSelect) {
    m_displayTypeWidget->setMaximumHeight(QWIDGETSIZE_MAX);  // 恢复高度限制
    m_displayTypeWidget->setVisible(true);

    // 数据源统一由顶部「字段取值域」提供（列表/编辑/查询三处共享）；
    // 未声明取值域时提示先声明，否则生成空下拉
    auto *hint = new QLabel(
        domainType().isEmpty()
            ? QStringLiteral("  下拉框数据源同时用于列表显示、编辑页下拉与查询；"
                             "尚未声明取值域，请在上方「字段取值域」选择数据源")
            : QStringLiteral("  下拉框数据源由上方「字段取值域」提供（列表/编辑/查询三处共享）"),
        m_displayTypeWidget);
    form->addRow(QString(), hint);
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
      auto *hint = new QLabel(
          QStringLiteral("  （真假文字在上方显示样式区配置；取值域为枚举时自动锁定）"),
          m_editStyleWidget);
      form->addRow(QString(), hint);
      break;
    }
    case EditStyle::Image: {
      // 上传预设下拉：列出工作区/项目作用域内所有 .jsonupload 上传预设。
      // 选中后编辑页生成上传组件（图片路径随表单 JSON 提交）；
      // 选"手动输入"保留旧行为（图片 URL 手工填写，无引用键写出）
      m_uploadSourceCombo = AuiComboBox::create(m_editStyleWidget);
      m_uploadSourceCombo->addItem(QStringLiteral("（不上传，手动输入图片 URL）"), QString());
      int restoreIdx = 0;
      const QStringList uploadFiles = findJsonuploadFiles(m_searchRoot);
      for (const QString &uf : uploadFiles) {
        JsonUploadConfig cfg;
        {
          QFile f(uf);
          if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            cfg = JsonUploadConfig::fromJsonString(QString::fromUtf8(f.readAll()));
            f.close();
          }
        }
        for (const auto &u : cfg.uploads) {
          const QString remark = u.remark.isEmpty() ? QStringLiteral("(未命名)") : u.remark;
          // 显示：说明 - 上传地址（文件名）
          const QString text =
              QStringLiteral("%1 - %2（%3）").arg(remark, u.url, QFileInfo(uf).fileName());
          m_uploadSourceCombo->addItem(text, uf + QStringLiteral("#") + u.id);
          if (uf == m_cachedUploadSourceFile && u.id == m_cachedUploadSourceId) {
            restoreIdx = m_uploadSourceCombo->count() - 1;
          }
        }
      }
      m_uploadSourceCombo->setCurrentIndex(restoreIdx);
      // 上传预设行：标签用 QFormLayout 的 QString 形式（避免复合 widget 被压缩截断），
      // 问号按钮放在 field 侧、下拉框之前，视觉上仍紧跟标签文字
      auto *uploadSrcField = new QWidget(m_editStyleWidget);
      auto *uploadSrcLay = new QHBoxLayout(uploadSrcField);
      uploadSrcLay->setContentsMargins(0, 0, 0, 0);
      uploadSrcLay->setSpacing(2);
      uploadSrcLay->addWidget(AuiButton::createHelpButton(
          QStringLiteral("上传预设作用域"), jsonVueUploadScopeHelpText(), uploadSrcField));
      uploadSrcLay->addWidget(m_uploadSourceCombo, 1);
      form->addRow(QStringLiteral("  上传预设:"), uploadSrcField);
      break;
    }
    case EditStyle::Select: {
      // 下拉框：数据源由顶部「字段取值域」统一提供，列表/编辑/查询三处共享
      auto *hint = new QLabel(QStringLiteral("  （数据源由上方「字段取值域」提供，列表/编辑/查询共享）"),
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

void ColumnStyleDialog::setFieldName(const QString &v) {
  if (m_fieldNameLabel) m_fieldNameLabel->setText(v);
}

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
  // 方案 B：布尔源统一由取值域承担，此处仅同步缓存（旧保存路径兼容）
  m_cachedBoolSourceFile = file;
  m_cachedBoolSourceId = id;
}

QString ColumnStyleDialog::boolSourceFile() const { return m_cachedBoolSourceFile; }

QString ColumnStyleDialog::boolSourceId() const { return m_cachedBoolSourceId; }

// ── 字段取值域（方案 B）──────────────────────────────────────

void ColumnStyleDialog::setDomainType(const QString &v) {
  // schema 值 → UI 类型：enum/static 统一呈现为「数据源」（枚举是能力不是类型），
  // remote → 远程接口；"" → 未声明
  m_cachedDomainType = v;
  if (m_domainTypeCombo) {
    QString uiType;
    if (v == QString::fromLatin1(JsonVueDomain::kEnum) ||
        v == QString::fromLatin1(JsonVueDomain::kStatic)) {
      uiType = QStringLiteral("source");
    } else if (v == QString::fromLatin1(JsonVueDomain::kRemote)) {
      uiType = QString::fromLatin1(JsonVueDomain::kRemote);
    }
    comboSelectData(m_domainTypeCombo, uiType);
    rebuildDomainControls();  // 类型恢复 → 显隐/候选/选中同步（不触发样式推导）
  }
}

QString ColumnStyleDialog::domainType() const {
  // UI 类型 → schema 值：数据源域按选中源的选项数细分（恰 2 项 → enum，其他 → static）
  const QString uiType =
      m_domainTypeCombo ? m_domainTypeCombo->currentData().toString() : m_cachedDomainType;
  if (uiType == QStringLiteral("source")) {
    const auto it = m_domainOptionCounts.constFind(currentDomainRef());
    const int n = it == m_domainOptionCounts.constEnd() ? 0 : it.value();
    return QString::fromLatin1(n == 2 ? JsonVueDomain::kEnum : JsonVueDomain::kStatic);
  }
  if (uiType == QString::fromLatin1(JsonVueDomain::kRemote)) return uiType;
  return QString();
}

void ColumnStyleDialog::setDomainSourceRef(const QString &file, const QString &id) {
  m_cachedDomainSourceFile = file;
  m_cachedDomainSourceId = id;
  // 同步布尔/select 分支缓存：三处保存路径引用同一来源（内存态一致）
  m_cachedBoolSourceFile = file;
  m_cachedBoolSourceId = id;
  m_cachedSelectSourceFile = file;
  m_cachedSelectSourceId = id;
  if (m_domainSourceCombo) {
    const QString ref = file + QStringLiteral("#") + id;
    m_domainSourceCombo->selectByData(ref == QStringLiteral("#") ? QVariant(QString())
                                                                 : QVariant(ref));
  }
  updateDomainPreview();
}

QString ColumnStyleDialog::domainSourceFile() const { return m_cachedDomainSourceFile; }

QString ColumnStyleDialog::domainSourceId() const { return m_cachedDomainSourceId; }

void ColumnStyleDialog::setDomainUrl(const QString &v) { setSelectUrl(v); }

QString ColumnStyleDialog::domainUrl() const { return selectUrl(); }

QString ColumnStyleDialog::currentDomainRef() const {
  if (m_cachedDomainSourceFile.isEmpty() && m_cachedDomainSourceId.isEmpty()) return QString();
  return m_cachedDomainSourceFile + QStringLiteral("#") + m_cachedDomainSourceId;
}

void ColumnStyleDialog::setUploadSourceRef(const QString &file, const QString &id) {
  m_cachedUploadSourceFile = file;
  m_cachedUploadSourceId = id;
  // 控件已存在时直接选中对应项（无联动信号，仅改选中态）
  if (m_uploadSourceCombo) {
    for (int i = 0; i < m_uploadSourceCombo->count(); ++i) {
      if (m_uploadSourceCombo->itemData(i).toString() == file + QStringLiteral("#") + id) {
        m_uploadSourceCombo->setCurrentIndex(i);
        break;
      }
    }
  }
}

QString ColumnStyleDialog::uploadSourceFile() const {
  // 直接解析下拉当前选中项（"文件路径#id"），无联动信号缓存
  if (!m_uploadSourceCombo) return m_cachedUploadSourceFile;
  const QString ref = m_uploadSourceCombo->currentData().toString();
  if (ref.isEmpty()) return QString();
  return ref.section(QStringLiteral("#"), 0, 0);
}

QString ColumnStyleDialog::uploadSourceId() const {
  if (!m_uploadSourceCombo) return m_cachedUploadSourceId;
  const QString ref = m_uploadSourceCombo->currentData().toString();
  if (ref.isEmpty()) return QString();
  return ref.section(QStringLiteral("#"), 1, -1);
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
  // 取值域校验（方案 B）：数据源域必须选择数据源
  const QString uiType =
      m_domainTypeCombo ? m_domainTypeCombo->currentData().toString() : QString();
  if (uiType == QStringLiteral("source") && currentDomainRef().isEmpty()) {
    AuiMessageBox::show(this, QStringLiteral("数据验证失败"),
                        QStringLiteral("取值域为数据源时必须选择数据源"));
    return;
  }
  QDialog::accept();
}
