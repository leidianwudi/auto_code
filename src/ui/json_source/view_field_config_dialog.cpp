/**
 * @file view_field_config_dialog.cpp
 * @brief jsontable 字段呈现 ⚙ 字段设置面板实现
 */

#include "view_field_config_dialog.h"

#include <QCheckBox>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "json_source_finder.h"
#include "json_upload_model.h"
#include "src/ui/common/domain_source_service.h"
#include "src/util/ui/component/aui_code_tab_bar.h"
#include "src/ui/json_vue/combobox_config_dialog.h"
#include "src/ui/json_vue/json_vue_model.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/component/aui_config_dialog.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/component/aui_tree_combo.h"

namespace {

/// 缩略图尺寸输入框：0 = 跟随预设（显示为空 + 灰色占位文本「跟随预设」）。
///
/// QSpinBox 原生 specialValueText 有交互缺陷：占位文本态下，键盘数字会把数字
/// 追加到占位文本后面、粘贴/输入法（IME 预编辑）同样无法改写占位文本，校验
/// 必然失败，表现为「跟随预设时无法输入数值」。这里改用 lineEdit 原生
/// placeholder 承载「跟随预设」提示：textFromValue(0) 返回空文本 → 输入框显示
/// 灰色占位，任何输入路径（键盘 / 输入法 / 粘贴）都从空文本开始，天然可用；
/// 清空失焦后 valueFromText("") 回到 0（跟随预设）。
class ThumbSpinBox : public QSpinBox {
public:
  using QSpinBox::QSpinBox;

  /// 「跟随预设」占位提示（0 = 跟随预设；空文本时显示灰色占位）
  void setFollowPresetText(const QString &text) { lineEdit()->setPlaceholderText(text); }

protected:
  QString textFromValue(int v) const override {
    return v == minimum() ? QString() : QSpinBox::textFromValue(v);
  }

  int valueFromText(const QString &text) const override {
    return text.isEmpty() ? minimum() : QSpinBox::valueFromText(text);
  }
};

// ── 数据库类型分类（与 main_admin.ac 的 widgetFromMysqlType 保持一致）──────
QString lowerType(const QString &t) { return t.trimmed().toLower(); }

bool isBooleanType(const QString &t) {
  const QString s = lowerType(t);
  return s.contains(QStringLiteral("tinyint(1)")) || s == QStringLiteral("bool") ||
         s == QStringLiteral("boolean");
}

bool isDateType(const QString &t) {
  const QString s = lowerType(t);
  return s.contains(QStringLiteral("date")) || s.contains(QStringLiteral("time"));
}

bool isFloatType(const QString &t) {
  const QString s = lowerType(t);
  return s.contains(QStringLiteral("decimal")) || s.contains(QStringLiteral("numeric")) ||
         s.contains(QStringLiteral("float")) || s.contains(QStringLiteral("double"));
}

bool isNumericType(const QString &t) {
  const QString s = lowerType(t);
  return s.contains(QStringLiteral("int")) || isFloatType(s);
}

bool isStringType(const QString &t) {
  const QString s = lowerType(t);
  return s.isEmpty() || s.contains(QStringLiteral("char")) || s.contains(QStringLiteral("text")) ||
         s.contains(QStringLiteral("json")) || s.contains(QStringLiteral("enum")) ||
         s.contains(QStringLiteral("set"));
}

QString inferWidgetFromType(const QString &type) {
  if (isBooleanType(type)) return QStringLiteral("boolean");
  const QString s = lowerType(type);
  if (s.contains(QStringLiteral("datetime")) || s.contains(QStringLiteral("timestamp"))) {
    return QStringLiteral("date");
  }
  if (isNumericType(s)) return QStringLiteral("number");
  return QStringLiteral("input");
}

QString widgetLabel(const QString &w) {
  if (w.isEmpty()) return QStringLiteral("自动");
  if (w == QStringLiteral("input")) return QStringLiteral("单行文本");
  if (w == QStringLiteral("textarea")) return QStringLiteral("多行文本");
  if (w == QStringLiteral("select")) return QStringLiteral("下拉选择");
  if (w == QStringLiteral("date")) return QStringLiteral("日期选择");
  if (w == QStringLiteral("number") || w == QStringLiteral("int") || w == QStringLiteral("float") ||
      w == QStringLiteral("money")) {
    return QStringLiteral("数字输入");
  }
  if (w == QStringLiteral("image")) return QStringLiteral("图片");
  if (w == QStringLiteral("upload")) return QStringLiteral("上传");
  if (w == QStringLiteral("boolean")) return QStringLiteral("开关");
  return w;
}

QString listModeLabel(const QString &mode) {
  if (mode == QStringLiteral("text")) return QStringLiteral("文本");
  if (mode == QStringLiteral("switch")) return QStringLiteral("开关");
  if (mode == QStringLiteral("money")) return QStringLiteral("金额");
  if (mode == QStringLiteral("tag")) return QStringLiteral("标签");
  if (mode == QStringLiteral("image")) return QStringLiteral("图片");
  return QStringLiteral("自动");
}

/// 字段设置专用 Tab 容器：QTabWidget::setTabBar 是 protected，
/// 通过子类注入项目通用 AuiCodeTabBar（VSCode 风格自绘，与编辑器/底部面板同款）。
class FieldTabWidget : public QTabWidget {
public:
  explicit FieldTabWidget(QWidget *parent = nullptr) : QTabWidget(parent) {
    auto *bar = new AuiCodeTabBar(this);
    bar->setObjectName(QStringLiteral("field_tabBar"));
    bar->setShowCloseButton(false);
    bar->setTabHeight(28);
    setTabBar(bar);
  }
};

QToolButton *makeAdvancedButton(QWidget *parent) {
  auto *btn = new QToolButton(parent);
  btn->setCheckable(true);
  btn->setAutoRaise(true);
  btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
  btn->setText(QStringLiteral("高级设置 ▸"));
  return btn;
}

void wireAdvanced(QToolButton *btn, QWidget *box) {
  box->setVisible(false);
  QObject::connect(btn, &QToolButton::toggled, box, [btn, box](bool on) {
    box->setVisible(on);
    btn->setText(on ? QStringLiteral("高级设置 ▾") : QStringLiteral("高级设置 ▸"));
  });
}

}  // namespace

// ════════════════════════════════════════════════════════════
//  构造 / 界面构建
// ════════════════════════════════════════════════════════════

ViewFieldConfigDialog::ViewFieldConfigDialog(const QString &ref, bool isJoinRow, QWidget *parent)
    : QDialog(parent), m_ref(ref), m_isJoinRow(isJoinRow) {
  setupUI();
}

void ViewFieldConfigDialog::setupUI() {
  // 三分区 Tab：字段头常显，列表/编辑/取值域各自独立，避免 20+ 行纵向堆叠。
  setMinimumWidth(580);
  ConfigDialogFrame frame = beginConfigDialog(this, QStringLiteral("字段设置"),
                                              QMargins(12, 10, 12, 10), 8);

  auto *header = new QHBoxLayout;
  m_fieldNameLabel = new QLabel(m_ref, this);
  m_fieldNameLabel->setStyleSheet(QStringLiteral("font-weight: 600; font-size: 13px;"));
  m_typeLabel = new QLabel(this);
  m_typeLabel->setObjectName(QStringLiteral("field_mysqlType"));
  m_typeLabel->setStyleSheet(
      QStringLiteral("color: %1; font-size: 12px;").arg(AuiStyle::mutedTextColor().name()));
  header->addWidget(m_fieldNameLabel);
  header->addSpacing(10);
  header->addWidget(m_typeLabel);
  header->addStretch();
  frame.contentLayout->addLayout(header);

  // 复用项目通用 VSCode 风格标签栏：与编辑器/底部面板同一套主题色与选中指示条，
  // 不依赖 Windows 原生 QTabBar 绘制（原生风格会忽略文字颜色/主题）。
  m_tabs = new FieldTabWidget(this);
  m_tabs->setObjectName(QStringLiteral("field_tabs"));
  m_tabs->setDocumentMode(true);
  // 内容区与 AuiCodeTabBar 选中页同色（editor.background），避免选中 tab
  // 与对话框 panel 背景出现一块色差；同时去掉设计器 QSS 可能继承来的 pane 边框。
  m_tabs->setStyleSheet(
      QStringLiteral("QTabWidget::pane { border: none; background: %1; }")
          .arg(AuiStyle::editorBackground().name()));
  frame.contentLayout->addWidget(m_tabs, 1);

  buildListTab();
  buildEditTab();
  buildDomainTab();

  if (m_isJoinRow) {
    // 联表列只读：编辑/取值域页不加入 Tab（控件仍构造，指针可用且随对话框释放）。
    m_tabs->tabBar()->setVisible(false);
  }

  m_summaryLabel = new QLabel(this);
  m_summaryLabel->setObjectName(QStringLiteral("field_summary"));
  m_summaryLabel->setWordWrap(true);
  m_summaryLabel->setStyleSheet(
      QStringLiteral("color: %1; font-size: 12px;").arg(AuiStyle::mutedTextColor().name()));
  frame.contentLayout->addWidget(m_summaryLabel);

  finishConfigDialog(this, frame);

  if (m_typeLabel->text().isEmpty()) {
    m_typeLabel->setText(m_isJoinRow ? QStringLiteral("联表列 · 只读") : QStringLiteral("类型未知"));
  }
  updateListRows();
  updateEditFormRows();
  refreshHints();
}

void ViewFieldConfigDialog::buildListTab() {
  auto *tab = new QWidget(this);
  auto *lay = new QVBoxLayout(tab);
  lay->setContentsMargins(0, 8, 0, 0);
  lay->setSpacing(6);

  auto *form = new QFormLayout;
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(6);
  form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_forms.append(form);

  m_listModeCombo = AuiComboBox::create(tab);
  m_listModeCombo->setObjectName(QStringLiteral("field_listMode"));
  m_listModeCombo->addItem(QStringLiteral("自动（按数据库类型推导）"), QStringLiteral("auto"));
  m_listModeCombo->addItem(QStringLiteral("文本"), QStringLiteral("text"));
  if (!m_isJoinRow) {
    m_listModeCombo->addItem(QStringLiteral("开关（0/1）"), QStringLiteral("switch"));
  }
  m_listModeCombo->addItem(QStringLiteral("金额"), QStringLiteral("money"));
  m_listModeCombo->addItem(QStringLiteral("标签（枚举）"), QStringLiteral("tag"));
  m_listModeCombo->addItem(QStringLiteral("图片"), QStringLiteral("image"));
  form->addRow(QStringLiteral("呈现方式:"), m_listModeCombo);

  m_listHint = new QLabel(tab);
  m_listHint->setObjectName(QStringLiteral("field_listHint"));
  m_listHint->setStyleSheet(
      QStringLiteral("color: %1; font-size: 12px;").arg(AuiStyle::mutedTextColor().name()));
  form->addRow(QString(), m_listHint);

  m_widthCombo = createNumericCombo(tab, {60, 80, 100, 120, 150, 180, 220}, 0);
  m_widthCombo->setEditable(true);
  m_widthCombo->lineEdit()->setPlaceholderText(QStringLiteral("空 = 自适应"));
  form->addRow(QStringLiteral("列表列宽(px):"), m_widthCombo);
  lay->addLayout(form);

  m_listAdvBtn = makeAdvancedButton(tab);
  m_listAdvBtn->setObjectName(QStringLiteral("field_listAdvanced"));
  m_listAdvBox = new QWidget(tab);
  auto *advForm = new QFormLayout(m_listAdvBox);
  advForm->setContentsMargins(0, 0, 0, 0);
  advForm->setSpacing(6);
  advForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_forms.append(advForm);

  m_listThumbRow = new QWidget(m_listAdvBox);
  auto *listThumbLay = new QHBoxLayout(m_listThumbRow);
  listThumbLay->setContentsMargins(0, 0, 0, 0);
  listThumbLay->setSpacing(4);
  m_listThumbW = new ThumbSpinBox(m_listThumbRow);
  m_listThumbH = new ThumbSpinBox(m_listThumbRow);
  for (QSpinBox *spin : {m_listThumbW, m_listThumbH}) {
    spin->setRange(0, 999);
    static_cast<ThumbSpinBox *>(spin)->setFollowPresetText(QStringLiteral("跟随预设"));
    spin->setMinimumWidth(80);
    listThumbLay->addWidget(spin);
  }
  listThumbLay->addWidget(new QLabel(QStringLiteral("宽×高（0=跟随预设）"), m_listThumbRow));
  listThumbLay->addStretch();
  advForm->addRow(QStringLiteral("列表缩略图:"), m_listThumbRow);

  wireAdvanced(m_listAdvBtn, m_listAdvBox);
  lay->addWidget(m_listAdvBtn);
  lay->addWidget(m_listAdvBox);
  lay->addStretch();

  if (m_isJoinRow) {
    auto *tip = new QLabel(QStringLiteral("联表列只读：仅列表展示配置生效"), tab);
    tip->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px;").arg(AuiStyle::mutedTextColor().name()));
    lay->addWidget(tip);
  }

  m_tabs->addTab(tab, QStringLiteral("列表展示"));

  connect(m_listModeCombo, &QComboBox::activated, this, [this](int) {
    updateListRows();
    refreshHints();
  });
  connect(m_widthCombo->lineEdit(), &QLineEdit::textChanged, this, [this](const QString &) {
    refreshHints();
  });
  connect(m_listThumbW, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int) { refreshHints(); });
  connect(m_listThumbH, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int) { refreshHints(); });
}

void ViewFieldConfigDialog::buildEditTab() {
  auto *tab = new QWidget(this);
  tab->setObjectName(QStringLiteral("field_editTab"));
  m_editTab = tab;
  auto *lay = new QVBoxLayout(tab);
  lay->setContentsMargins(0, 8, 0, 0);
  lay->setSpacing(6);

  auto *form = new QFormLayout;
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(6);
  form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_forms.append(form);

  m_widgetCombo = AuiComboBox::create(tab);
  m_widgetCombo->setObjectName(QStringLiteral("field_widget"));
  form->addRow(QStringLiteral("编辑控件:"), m_widgetCombo);

  m_widgetHint = new QLabel(tab);
  m_widgetHint->setObjectName(QStringLiteral("field_widgetHint"));
  m_widgetHint->setWordWrap(true);
  m_widgetHint->setStyleSheet(
      QStringLiteral("color: %1; font-size: 12px;").arg(AuiStyle::mutedTextColor().name()));
  form->addRow(QString(), m_widgetHint);

  m_editEditableCheck = new QCheckBox(QStringLiteral("允许在表单中输入"), tab);
  m_editEditableCheck->setChecked(true);
  m_editEditableCheck->setToolTip(
      QStringLiteral("取消勾选 = 编辑/详情表单内只读展示（仍展示字段值）\n"
                     "字段是否提交由「字段呈现」的写入列控制"));
  form->addRow(QStringLiteral("允许输入:"), m_editEditableCheck);

  m_requiredCheck = new QCheckBox(QStringLiteral("必填（提交时校验）"), tab);
  form->addRow(QStringLiteral("必填:"), m_requiredCheck);

  m_placeholderEdit = new QLineEdit(tab);
  m_placeholderEdit->setPlaceholderText(QStringLiteral("空 = 模板缺省占位"));
  form->addRow(QStringLiteral("占位提示:"), m_placeholderEdit);

  m_maxlengthSpin = new QSpinBox(tab);
  m_maxlengthSpin->setRange(0, 9999);
  m_maxlengthSpin->setSpecialValueText(QStringLiteral("不限"));
  form->addRow(QStringLiteral("最大长度:"), m_maxlengthSpin);

  m_textareaRowsSpin = new QSpinBox(tab);
  m_textareaRowsSpin->setRange(0, 20);
  m_textareaRowsSpin->setSpecialValueText(QStringLiteral("跟随模板（4）"));
  form->addRow(QStringLiteral("textarea 行数:"), m_textareaRowsSpin);

  m_rangeRow = new QWidget(tab);
  auto *rangeRow = m_rangeRow;
  auto *rangeLay = new QHBoxLayout(rangeRow);
  rangeLay->setContentsMargins(0, 0, 0, 0);
  rangeLay->setSpacing(4);
  m_minSpin = new QDoubleSpinBox(rangeRow);
  m_maxSpin = new QDoubleSpinBox(rangeRow);
  for (QDoubleSpinBox *s : {m_minSpin, m_maxSpin}) {
    s->setRange(-1000000000.0, 1000000000.0);
    s->setDecimals(2);
    s->setSpecialValueText(QStringLiteral("不限"));
    rangeLay->addWidget(s);
  }
  rangeLay->addWidget(new QLabel(QStringLiteral("～（最小/最大）"), rangeRow));
  rangeLay->addStretch();
  form->addRow(QStringLiteral("数值范围:"), rangeRow);

  m_precisionSpin = new QSpinBox(tab);
  m_precisionSpin->setRange(0, 8);
  m_precisionSpin->setSpecialValueText(QStringLiteral("跟随模板（2）"));
  form->addRow(QStringLiteral("小数位:"), m_precisionSpin);

  m_dateFormatCombo = AuiComboBox::create(tab);
  m_dateFormatCombo->addItem(QStringLiteral("自动（按 mysqlType 推导）"), QString());
  m_dateFormatCombo->addItem(QStringLiteral("date（年-月-日）"), QStringLiteral("date"));
  m_dateFormatCombo->addItem(QStringLiteral("datetime（年-月-日 时:分:秒）"),
                             QStringLiteral("datetime"));
  m_dateFormatCombo->addItem(QStringLiteral("month（年-月）"), QStringLiteral("month"));
  m_dateFormatCombo->addItem(QStringLiteral("year（年）"), QStringLiteral("year"));
  m_dateFormatCombo->addItem(QStringLiteral("daterange（日期范围）"), QStringLiteral("daterange"));
  form->addRow(QStringLiteral("日期格式:"), m_dateFormatCombo);

  m_uploadSourceCombo = AuiComboBox::create(tab);
  m_uploadSourceCombo->addItem(QStringLiteral("（不上传，手动输入图片 URL）"), QString());
  form->addRow(QStringLiteral("上传预设:"), m_uploadSourceCombo);

  m_editThumbRow = new QWidget(tab);
  auto *editThumbLay = new QHBoxLayout(m_editThumbRow);
  editThumbLay->setContentsMargins(0, 0, 0, 0);
  editThumbLay->setSpacing(4);
  m_editThumbW = new ThumbSpinBox(m_editThumbRow);
  m_editThumbH = new ThumbSpinBox(m_editThumbRow);
  for (QSpinBox *spin : {m_editThumbW, m_editThumbH}) {
    spin->setRange(0, 999);
    static_cast<ThumbSpinBox *>(spin)->setFollowPresetText(QStringLiteral("跟随预设"));
    spin->setMinimumWidth(80);
    editThumbLay->addWidget(spin);
  }
  editThumbLay->addWidget(new QLabel(QStringLiteral("宽×高（0=跟随预设）"), m_editThumbRow));
  editThumbLay->addStretch();
  form->addRow(QStringLiteral("编辑缩略图:"), m_editThumbRow);

  lay->addLayout(form);

  m_editAdvBtn = makeAdvancedButton(tab);
  m_editAdvBtn->setObjectName(QStringLiteral("field_editAdvanced"));
  m_editAdvBox = new QWidget(tab);
  auto *advForm = new QFormLayout(m_editAdvBox);
  advForm->setContentsMargins(0, 0, 0, 0);
  advForm->setSpacing(6);
  advForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_forms.append(advForm);

  m_formSpanCombo = AuiComboBox::create(m_editAdvBox);
  m_formSpanCombo->addItem(QStringLiteral("自动（模板缺省）"), 0);
  m_formSpanCombo->addItem(QStringLiteral("半行（24 栅格占 12）"), 12);
  m_formSpanCombo->addItem(QStringLiteral("整行（24 栅格占 24）"), 24);
  advForm->addRow(QStringLiteral("表单栅格:"), m_formSpanCombo);

  m_formDefaultEdit = new QLineEdit(m_editAdvBox);
  m_formDefaultEdit->setPlaceholderText(QStringLiteral("新建时表单预填（空 = 不预填）"));
  advForm->addRow(QStringLiteral("表单默认值:"), m_formDefaultEdit);

  wireAdvanced(m_editAdvBtn, m_editAdvBox);
  lay->addWidget(m_editAdvBtn);
  lay->addWidget(m_editAdvBox);
  lay->addStretch();

  if (!m_isJoinRow) m_tabs->addTab(tab, QStringLiteral("编辑表单"));

  connect(m_widgetCombo, &QComboBox::activated, this, [this](int) {
    updateEditFormRows();
    refreshHints();
  });
  connect(m_editEditableCheck, &QCheckBox::toggled, this, [this](bool) {
    updateEditFormRows();
    refreshHints();
  });
  connect(m_requiredCheck, &QCheckBox::toggled, this, [this](bool) { refreshHints(); });
  connect(m_minSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double) { refreshHints(); });
  connect(m_maxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double) { refreshHints(); });
  connect(m_precisionSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int) { refreshHints(); });
  connect(m_formSpanCombo, &QComboBox::activated, this, [this](int) { refreshHints(); });
}

void ViewFieldConfigDialog::buildDomainTab() {
  auto *tab = new QWidget(this);
  tab->setObjectName(QStringLiteral("field_domainTab"));
  m_domainTab = tab;
  auto *lay = new QVBoxLayout(tab);
  lay->setContentsMargins(0, 8, 0, 0);
  lay->setSpacing(6);

  auto *form = new QFormLayout;
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(6);
  form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_forms.append(form);

  m_domainTypeCombo = AuiComboBox::create(tab);
  m_domainTypeCombo->addItem(QStringLiteral("未声明（无数据源）"), QString());
  m_domainTypeCombo->addItem(QStringLiteral("数据源（.jsonsource / 全局枚举）"),
                             QStringLiteral("source"));
  m_domainTypeCombo->addItem(QStringLiteral("远程接口（手动 URL）"),
                             QString::fromLatin1(JsonVueDomain::kRemote));
  form->addRow(QStringLiteral("取值域类型:"), m_domainTypeCombo);

  m_domainSourceRow = new QWidget(tab);
  auto *domainSrcLay = new QHBoxLayout(m_domainSourceRow);
  domainSrcLay->setContentsMargins(0, 0, 0, 0);
  domainSrcLay->setSpacing(2);
  domainSrcLay->addWidget(AuiButton::createHelpButton(QStringLiteral("数据源作用域"),
                                                      jsonVueSourceScopeHelpText(),
                                                      m_domainSourceRow));
  m_domainSourceCombo = new AuiTreeCombo(tab);
  domainSrcLay->addWidget(m_domainSourceCombo, 1);
  form->addRow(QStringLiteral("取值数据源:"), m_domainSourceRow);

  m_domainUrlBtn = new QPushButton(QStringLiteral("配置远程数据源..."), tab);
  form->addRow(QStringLiteral("远程源:"), m_domainUrlBtn);

  m_domainPreviewLabel = new QLabel(tab);
  m_domainPreviewLabel->setWordWrap(true);
  m_domainPreviewLabel->setStyleSheet(
      QStringLiteral("color: %1; font-size: 12px;").arg(AuiStyle::mutedTextColor().name()));
  form->addRow(QStringLiteral("选项预览:"), m_domainPreviewLabel);

  lay->addLayout(form);
  lay->addStretch();
  if (!m_isJoinRow) m_tabs->addTab(tab, QStringLiteral("取值域"));

  connect(m_domainUrlBtn, &QPushButton::clicked, this, [this]() {
    ComboboxConfigDialog dlg(this);
    dlg.setSearchRoot(m_searchRoot);
    dlg.setSourceRef(m_domainSourceFile, m_domainSourceId);
    dlg.setConfig(m_selectUrl, m_selectValueField, m_selectLabelField);
    dlg.setPagedConfig(m_selectPaged, m_selectPageKey, m_selectPageSizeKey, m_selectPageSize,
                       m_selectSearchTitle, m_selectSearchField, m_selectMethod);
    if (dlg.exec() == QDialog::Accepted) {
      m_domainSourceFile = dlg.sourceFile();
      m_domainSourceId = dlg.sourceId();
      m_selectUrl = dlg.url();
      m_selectValueField = dlg.valueField();
      m_selectLabelField = dlg.labelField();
      m_selectPaged = dlg.paged();
      m_selectPageKey = dlg.pageKey();
      m_selectPageSizeKey = dlg.pageSizeKey();
      m_selectPageSize = dlg.pageSize();
      m_selectSearchTitle = dlg.searchTitle();
      m_selectSearchField = dlg.searchField();
      m_selectMethod = dlg.method();
    }
  });

  connect(m_domainSourceCombo, &AuiTreeCombo::itemSelected, this, [this](const QVariant &refVar) {
    const QString ref = refVar.toString();
    m_domainSourceFile.clear();
    m_domainSourceId.clear();
    if (!ref.isEmpty()) {
      const int sep = ref.lastIndexOf(QLatin1Char('#'));
      m_domainSourceFile = ref.left(sep);
      m_domainSourceId = ref.mid(sep + 1);
    }
    updateDomainPreview();
    deriveWidgetFromDomain();
    refreshHints();
  });

  connect(m_domainTypeCombo, &QComboBox::activated, this, [this](int) {
    buildDomainCandidates();
    rebuildDomainControls();
    deriveWidgetFromDomain();
    refreshHints();
  });
}

void ViewFieldConfigDialog::setSearchRoot(const QString &dir) {
  if (m_searchRoot == dir) return;
  m_searchRoot = dir;
  if (!m_isJoinRow) {
    buildDomainCandidates();
    rebuildDomainControls();
  }
  // 重建上传预设候选（保留当前选中）
  const QString keepRef = currentUploadRef();
  m_uploadSourceCombo->clear();
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
      const QString text =
          QStringLiteral("%1 - %2（%3）").arg(remark, u.url, QFileInfo(uf).fileName());
      m_uploadSourceCombo->addItem(text, uf + QStringLiteral("#") + u.id);
      if (uf + QStringLiteral("#") + u.id == keepRef) {
        restoreIdx = m_uploadSourceCombo->count() - 1;
      }
    }
  }
  m_uploadSourceCombo->setCurrentIndex(restoreIdx);
}

// ════════════════════════════════════════════════════════════
//  当前行配置载入
// ════════════════════════════════════════════════════════════

void ViewFieldConfigDialog::setMysqlType(const QString &mysqlType, bool hasEnum) {
  m_mysqlType = mysqlType.trimmed();
  m_hasEnum = hasEnum;
  if (!m_typeLabel) return;
  // 空 mysqlType 且非联表 = 多语言平铺字段（只参与列表展示；编辑由多语言组件处理）。
  QString text = m_mysqlType.isEmpty() ? QStringLiteral("多语言字段 · 仅列表展示") : m_mysqlType;
  if (m_hasEnum) text += QStringLiteral(" · 表内枚举");
  if (m_isJoinRow) {
    text = QStringLiteral("联表列 · ") +
           (m_mysqlType.isEmpty() ? QStringLiteral("类型未知") : m_mysqlType) +
           QStringLiteral(" · 只读");
  }
  m_typeLabel->setText(text);
}

void ViewFieldConfigDialog::rebuildWidgetOptions(const QString &preferred) {
  if (!m_widgetCombo) return;
  QSignalBlocker blocker(m_widgetCombo);
  m_widgetCombo->clear();
  auto add = [this](const QString &label, const QString &data) {
    m_widgetCombo->addItem(label, data);
  };
  add(QStringLiteral("自动（按数据库类型推导）"), QString());
  if (m_hasEnum) {
    // 表内枚举的编辑控件由生成端强制为标签下拉，不再提供会冲突的控件。
    add(QStringLiteral("下拉选择(select)"), QStringLiteral("select"));
  } else {
    add(QStringLiteral("单行文本(input)"), QStringLiteral("input"));
    if (isStringType(m_mysqlType)) {
      add(QStringLiteral("多行文本(textarea)"), QStringLiteral("textarea"));
      add(QStringLiteral("图片(image)"), QStringLiteral("image"));
      add(QStringLiteral("上传(upload)"), QStringLiteral("upload"));
    }
    if (isNumericType(m_mysqlType)) add(QStringLiteral("数字(number)"), QStringLiteral("number"));
    if (isDateType(m_mysqlType)) add(QStringLiteral("日期(date)"), QStringLiteral("date"));
    add(QStringLiteral("下拉选择(select)"), QStringLiteral("select"));
    if (!isDateType(m_mysqlType)) add(QStringLiteral("开关(boolean)"), QStringLiteral("boolean"));
  }

  int idx = preferred.isEmpty() ? 0 : m_widgetCombo->findData(preferred);
  if (idx < 0 && !preferred.isEmpty()) {
    // 旧配置/不兼容控件保留可见，保存时不会被静默改掉；上方提示会说明风险。
    add(widgetLabel(preferred) + QStringLiteral("（当前配置）"), preferred);
    idx = m_widgetCombo->count() - 1;
  }
  m_widgetCombo->setCurrentIndex(idx < 0 ? 0 : idx);
}

void ViewFieldConfigDialog::setCurrent(const JsonTableSelCol &sc, const JsonTableEditCol &ec) {
  // 列宽是可编辑数值下拉：非预设值必须清掉选中项并写入编辑框，
  // 否则 currentData 会停留在预设 60，保存时把任意宽度静默改成 60。
  const int colWidth = sc.width > 0 ? sc.width : 0;
  const int widthIdx = m_widthCombo->findData(colWidth);
  if (colWidth > 0 && widthIdx >= 0) {
    m_widthCombo->setCurrentIndex(widthIdx);
  } else if (colWidth > 0) {
    m_widthCombo->setCurrentIndex(-1);
    m_widthCombo->setEditText(QString::number(colWidth));
  } else {
    m_widthCombo->setCurrentIndex(-1);
    m_widthCombo->setEditText(QString());
  }

  // 编辑控件：旧 int/float/money 统一归并为 number，再按数据库类型过滤候选。
  QString widget = ec.widget;
  if (widget == QStringLiteral("int") || widget == QStringLiteral("float") ||
      widget == QStringLiteral("money")) {
    widget = QStringLiteral("number");
  }
  rebuildWidgetOptions(widget);

  // 列表呈现方式：开关优先于 displayType（生成端也是开关覆盖展示类型）。
  QString listMode = QStringLiteral("auto");
  if (ec.switchEditable) {
    listMode = QStringLiteral("switch");
  } else if (sc.displayType == QStringLiteral("boolean")) {
    listMode = QStringLiteral("switch");
  } else if (sc.displayType == QStringLiteral("image") ||
             sc.displayType == QStringLiteral("money") ||
             sc.displayType == QStringLiteral("tag") || sc.displayType == QStringLiteral("text")) {
    listMode = sc.displayType;
  }
  comboSelectData(m_listModeCombo, listMode);

  m_editEditableCheck->setChecked(ec.editEditable);
  m_requiredCheck->setChecked(ec.required);
  m_listThumbW->setValue(sc.listThumbWidth);
  m_listThumbH->setValue(sc.listThumbHeight);
  if (m_uploadSourceCombo) {
    const QString wantRef = ec.uploadSourceFile + QStringLiteral("#") + ec.uploadSourceId;
    int idx = 0;
    for (int i = 0; i < m_uploadSourceCombo->count(); ++i) {
      if (m_uploadSourceCombo->itemData(i).toString() == wantRef) {
        idx = i;
        break;
      }
    }
    m_uploadSourceCombo->setCurrentIndex(idx);
  }
  m_editThumbW->setValue(ec.editThumbWidth);
  m_editThumbH->setValue(ec.editThumbHeight);
  // 精细参数
  m_placeholderEdit->setText(ec.placeholder);
  m_maxlengthSpin->setValue(ec.maxlength);
  m_textareaRowsSpin->setValue(ec.textareaRows);
  m_minSpin->setValue(ec.minValue);
  m_maxSpin->setValue(ec.maxValue);
  m_precisionSpin->setValue(ec.precision);
  comboSelectData(m_dateFormatCombo, ec.dateFormat);
  comboSelectData(m_formSpanCombo, ec.formSpan > 0 ? QVariant(ec.formSpan) : QVariant(0));
  m_formDefaultEdit->setText(ec.formDefaultValue);

  // 已有高级配置时自动展开，避免“配置存在但看不见”。
  if (m_listAdvBtn) {
    m_listAdvBtn->setChecked(sc.listThumbWidth > 0 || sc.listThumbHeight > 0);
  }
  if (m_editAdvBtn) {
    m_editAdvBtn->setChecked(ec.formSpan > 0 || !ec.formDefaultValue.isEmpty());
  }

  updateListRows();
  updateEditFormRows();
  refreshHints();
}

void ViewFieldConfigDialog::setDomain(const JsonTableColumn &col) {
  if (m_isJoinRow || !m_domainTypeCombo) return;
  QString uiType;
  if (col.domainType == QString::fromLatin1(JsonVueDomain::kEnum) ||
      col.domainType == QString::fromLatin1(JsonVueDomain::kStatic)) {
    uiType = QStringLiteral("source");
    m_domainSourceFile = col.domainSourceFile;
    m_domainSourceId = col.domainSourceId;
  } else if (col.domainType == QString::fromLatin1(JsonVueDomain::kRemote)) {
    uiType = QString::fromLatin1(JsonVueDomain::kRemote);
    m_selectUrl = col.domainUrl;
  }
  comboSelectData(m_domainTypeCombo, uiType);
  buildDomainCandidates();
  rebuildDomainControls();
  deriveWidgetFromDomain();
  refreshHints();
}

// ════════════════════════════════════════════════════════════
//  取值域
// ════════════════════════════════════════════════════════════

void ViewFieldConfigDialog::buildDomainCandidates() {
  if (!m_domainSourceCombo) return;
  buildDomainSourceCandidates(m_domainSourceCombo, m_searchRoot, /*enumOnly=*/false,
                              &m_domainBoolTexts, &m_domainPreviews, &m_domainOptionCounts,
                              &m_domainDynamicFlags);
  // 恢复选中（缓存引用）
  const QString ref = currentDomainRef();
  if (!ref.isEmpty()) m_domainSourceCombo->selectByData(ref);
}

void ViewFieldConfigDialog::rebuildDomainControls() {
  if (!m_domainTypeCombo) return;
  const QString uiType = m_domainTypeCombo->currentData().toString();
  const bool hasSource = (uiType == QStringLiteral("source"));
  const bool hasUrl = (uiType == QString::fromLatin1(JsonVueDomain::kRemote));
  setFormRowVisible(m_domainSourceRow, hasSource);
  setFormRowVisible(m_domainUrlBtn, hasUrl);
  setFormRowVisible(m_domainPreviewLabel, hasSource);
  if (hasSource) updateDomainPreview();
}

void ViewFieldConfigDialog::updateDomainPreview() {
  if (!m_domainPreviewLabel) return;
  const QString ref = currentDomainRef();
  if (ref.isEmpty()) {
    m_domainPreviewLabel->setText(QStringLiteral("（未选择数据源）"));
    return;
  }
  const auto it = m_domainOptionCounts.constFind(ref);
  const int n = it == m_domainOptionCounts.constEnd() ? 0 : it.value();
  const auto pIt = m_domainPreviews.constFind(ref);
  QString text = pIt == m_domainPreviews.constEnd() ? QString() : pIt.value();
  if (text.size() > 60) text = text.left(60) + QStringLiteral("…");
  m_domainPreviewLabel->setText(QStringLiteral("%1 项：%2").arg(n).arg(text));
}

QString ViewFieldConfigDialog::currentDomainRef() const {
  if (m_domainSourceFile.isEmpty() && m_domainSourceId.isEmpty()) return QString();
  return m_domainSourceFile + QStringLiteral("#") + m_domainSourceId;
}

QString ViewFieldConfigDialog::currentUploadRef() const {
  if (!m_uploadSourceCombo) return QString();
  return m_uploadSourceCombo->currentData().toString();
}

QString ViewFieldConfigDialog::mysqlWidget() const {
  return m_hasEnum ? QStringLiteral("select") : inferWidgetFromType(m_mysqlType);
}

QString ViewFieldConfigDialog::effectiveWidget() const {
  QString w = m_widgetCombo ? m_widgetCombo->currentData().toString() : QString();
  if (w == QStringLiteral("int") || w == QStringLiteral("float") || w == QStringLiteral("money")) {
    w = QStringLiteral("number");
  }
  return w.isEmpty() ? mysqlWidget() : w;
}

QString ViewFieldConfigDialog::listModeResult() const {
  if (!m_listModeCombo) return QStringLiteral("文本");
  const QString mode = m_listModeCombo->currentData().toString();
  if (mode != QStringLiteral("auto")) return listModeLabel(mode);
  if (m_isJoinRow) return QStringLiteral("文本");
  if (m_hasEnum) return QStringLiteral("标签");
  const QString w = effectiveWidget();
  if (w == QStringLiteral("boolean")) return QStringLiteral("开关");
  if (w == QStringLiteral("image")) return QStringLiteral("图片");
  return QStringLiteral("文本");
}

QString ViewFieldConfigDialog::listModeReason() const {
  if (!m_listModeCombo || m_listModeCombo->currentData().toString() != QStringLiteral("auto")) {
    return QString();
  }
  if (m_hasEnum) return QStringLiteral("表内枚举");
  if (effectiveWidget() == QStringLiteral("boolean") && !m_mysqlType.isEmpty()) {
    return m_mysqlType;
  }
  return QString();
}

void ViewFieldConfigDialog::refreshHints() {
  if (!m_widgetHint || !m_listHint || !m_summaryLabel) return;
  const QString w = effectiveWidget();
  const QString type = m_mysqlType.isEmpty() ? QStringLiteral("未知") : m_mysqlType;
  const bool autoWidget =
      !m_widgetCombo || m_widgetCombo->currentData().toString().isEmpty();

  QString widgetText;
  QString warning;
  if (autoWidget) {
    widgetText = QStringLiteral("自动 → %1（%2）").arg(widgetLabel(w), type);
  } else {
    widgetText = QStringLiteral("指定 → %1").arg(widgetLabel(w));
    if (m_hasEnum && w != QStringLiteral("select")) {
      warning = QStringLiteral("表内枚举会强制渲染为标签下拉，此编辑控件不会生效");
    } else if (w == QStringLiteral("input") && isNumericType(m_mysqlType)) {
      warning = QStringLiteral("数据库为数值类型，文本输入会按字符串提交");
    } else if (w == QStringLiteral("number") && !isNumericType(m_mysqlType)) {
      warning = QStringLiteral("数据库不是数值类型，仍会生成数字输入，提交值可能与列类型不匹配");
    } else if (w == QStringLiteral("date") && !isDateType(m_mysqlType)) {
      warning = QStringLiteral("数据库不是日期类型，仍会生成日期选择");
    } else if ((w == QStringLiteral("image") || w == QStringLiteral("upload")) &&
               !isStringType(m_mysqlType)) {
      warning = QStringLiteral("数据库不是字符串类型，图片 URL 可能无法保存");
    } else if (w == QStringLiteral("textarea") && !isStringType(m_mysqlType)) {
      warning = QStringLiteral("数据库不是字符串类型，多行文本提交值可能不匹配");
    }
  }
  if (!warning.isEmpty()) widgetText += QStringLiteral("；") + warning;
  m_widgetHint->setText(widgetText);
  m_widgetHint->setStyleSheet(
      QStringLiteral("color: %1; font-size: 12px;")
          .arg((warning.isEmpty() ? AuiStyle::mutedTextColor() : AuiStyle::errorTextColor()).name()));

  const bool autoMode =
      !m_listModeCombo || m_listModeCombo->currentData().toString() == QStringLiteral("auto");
  QString listText = (autoMode ? QStringLiteral("自动 → ") : QStringLiteral("指定 → ")) +
                     listModeResult();
  const QString listReason = listModeReason();
  if (!listReason.isEmpty()) listText += QStringLiteral("（") + listReason + QStringLiteral("）");
  m_listHint->setText(listText);

  if (m_isJoinRow) {
    m_summaryLabel->setText(
        QStringLiteral("生成结果：列表：%1（联表只读）").arg(listModeResult()));
    return;
  }

  QStringList parts;
  parts << QStringLiteral("列表：") + listModeResult();
  parts << QStringLiteral("编辑：") + widgetLabel(w);
  if (w == QStringLiteral("number") && isNumericType(m_mysqlType)) {
    const double minV = m_minSpin ? m_minSpin->value() : 0;
    const double maxV = m_maxSpin ? m_maxSpin->value() : 0;
    if (minV != 0 && maxV != 0) {
      parts << QStringLiteral("范围：%1～%2").arg(minV).arg(maxV);
    } else if (minV != 0) {
      parts << QStringLiteral("最小：%1").arg(minV);
    } else if (maxV != 0) {
      parts << QStringLiteral("最大：%1").arg(maxV);
    }
    if (isFloatType(m_mysqlType) && m_precisionSpin) {
      const int precision = m_precisionSpin->value() > 0 ? m_precisionSpin->value() : 2;
      parts << QStringLiteral("%1 位小数").arg(precision);
    }
  }
  if (m_requiredCheck && m_requiredCheck->isChecked() && m_editEditableCheck &&
      m_editEditableCheck->isChecked()) {
    parts << QStringLiteral("必填");
  }
  if (m_editEditableCheck && !m_editEditableCheck->isChecked()) {
    parts << QStringLiteral("表单只读");
  }
  if (m_formSpanCombo) {
    const int span = m_formSpanCombo->currentData().toInt();
    if (span == 12) parts << QStringLiteral("半行");
    if (span == 24) parts << QStringLiteral("整行");
  }
  m_summaryLabel->setText(QStringLiteral("生成结果：") + parts.join(QStringLiteral(" ｜ ")));
  updateTabVisibility();
}

void ViewFieldConfigDialog::updateTabVisibility() {
  if (!m_tabs) return;
  if (m_isJoinRow) {
    m_tabs->tabBar()->setVisible(false);
    return;
  }
  // 物理列才有编辑表单；多语言平铺字段只展示列表（编辑由多语言组件接管）。
  const bool physical = !m_mysqlType.isEmpty();
  const QString domainType = m_domainTypeCombo ? m_domainTypeCombo->currentData().toString() : QString();
  // 取值域是物理列的基础配置（查询下拉/列表标签都可能用到），普通物理列常显；
  // 仅表内枚举且未配置域时隐藏（枚举由「枚举」页维护，避免两个来源混淆）。
  const bool domainNeeded = physical && (!m_hasEnum || !domainType.isEmpty());
  if (m_editTab) {
    const int idx = m_tabs->indexOf(m_editTab);
    if (idx >= 0) m_tabs->setTabVisible(idx, physical);
  }
  if (m_domainTab) {
    const int idx = m_tabs->indexOf(m_domainTab);
    if (idx >= 0) m_tabs->setTabVisible(idx, domainNeeded);
  }
  // 只剩列表页时隐藏 Tab 栏（与联表行一致）。
  m_tabs->tabBar()->setVisible(physical);
}

void ViewFieldConfigDialog::updateListRows() {
  if (!m_listModeCombo) return;
  const bool imageMode =
      m_listModeCombo->currentData().toString() == QStringLiteral("image");
  // 列表缩略图仅 image 呈现方式有意义（整行折叠）
  setFormRowVisible(m_listThumbRow, imageMode);
  // 高级设置里只有缩略图；非图片模式没有任何可配项时，整个入口一起隐藏。
  if (m_listAdvBtn) {
    if (!imageMode) m_listAdvBtn->setChecked(false);
    m_listAdvBtn->setVisible(imageMode);
  }
  if (m_listAdvBox) {
    m_listAdvBox->setVisible(imageMode && m_listAdvBtn && m_listAdvBtn->isChecked());
  }
}

void ViewFieldConfigDialog::updateEditFormRows() {
  if (!m_widgetCombo) return;
  const QString w = effectiveWidget();
  const bool isImageLike = (w == QStringLiteral("image") || w == QStringLiteral("upload"));
  const bool stringDb = isStringType(m_mysqlType);
  const bool numericDb = isNumericType(m_mysqlType);
  const bool floatDb = isFloatType(m_mysqlType);
  const bool dateDb = isDateType(m_mysqlType);
  // 参数行按「最终生效控件」+ 数据库类型整行折叠：
  // auto 时也要按推导结果展示范围/长度/日期参数，不能因为没显式选控件就全部隐藏。
  setFormRowVisible(m_placeholderEdit,
                    isImageLike || w == QStringLiteral("input") ||
                        w == QStringLiteral("textarea"));
  setFormRowVisible(m_maxlengthSpin, w == QStringLiteral("input") && stringDb);
  setFormRowVisible(m_textareaRowsSpin, w == QStringLiteral("textarea") && stringDb);
  setFormRowVisible(m_rangeRow, w == QStringLiteral("number") && numericDb);
  setFormRowVisible(m_precisionSpin, w == QStringLiteral("number") && floatDb);
  setFormRowVisible(m_dateFormatCombo, w == QStringLiteral("date") && dateDb);
  setFormRowVisible(m_uploadSourceCombo, isImageLike && stringDb);
  setFormRowVisible(m_editThumbRow, isImageLike && stringDb);
  if (m_requiredCheck) m_requiredCheck->setEnabled(m_editEditableCheck->isChecked());
  refreshHints();
}

void ViewFieldConfigDialog::setFormRowVisible(QWidget *field, bool on) {
  if (!field) return;
  // 字段可能位于任一 Tab 的表单；跨表单查找 FieldRole 对应的行。
  for (QFormLayout *form : m_forms) {
    for (int r = 0; r < form->rowCount(); ++r) {
      const QLayoutItem *it = form->itemAt(r, QFormLayout::FieldRole);
      if (it && it->widget() == field) {
        form->setRowVisible(r, on);
        return;
      }
    }
  }
}

void ViewFieldConfigDialog::deriveWidgetFromDomain() {
  if (m_isJoinRow || m_hasEnum || !m_widgetCombo) return;
  // 仅控件为"自动"时联动（用户显式选择优先，不覆盖）
  if (m_widgetCombo->currentData().toString() != QString()) return;
  const QString t = m_domainTypeCombo ? m_domainTypeCombo->currentData().toString() : QString();
  QString w;
  if (t == QStringLiteral("source")) {
    const int n = m_domainOptionCounts.value(currentDomainRef(), 0);
    if (n == 2) {
      w = QStringLiteral("boolean");  // 恰 2 项 → 布尔开关
    } else if (n > 0) {
      w = QStringLiteral("select");  // 其它项数 → 下拉
    }
  } else if (t == QString::fromLatin1(JsonVueDomain::kRemote)) {
    w = QStringLiteral("select");  // 远程接口 → 远程下拉
  }
  if (!w.isEmpty()) {
    comboSelectData(m_widgetCombo, w);
    updateEditFormRows();
  }
}

// ════════════════════════════════════════════════════════════
//  结果读取
// ════════════════════════════════════════════════════════════

int ViewFieldConfigDialog::width() const {
  if (!m_widthCombo) return 0;
  // 直接读编辑框文本：预设项与非预设自定义值都按用户看到的数字落库；
  // 空文本 = 自适应（0），不再被 currentData 的预设值覆盖。
  return m_widthCombo->currentText().trimmed().toInt();
}

void ViewFieldConfigDialog::fillListResult(JsonTableSelCol *sc) const {
  if (!sc) return;
  sc->width = width();
  const QString mode = m_listModeCombo ? m_listModeCombo->currentData().toString() : QString();
  // switch 由 editCols.switchEditable 承接，displayType 保持空由生成端推导；
  // 其余显式模式直接写入 displayType。
  sc->displayType =
      (mode.isEmpty() || mode == QStringLiteral("auto") || mode == QStringLiteral("switch"))
          ? QString()
          : mode;
  sc->listThumbWidth = m_listThumbW ? m_listThumbW->value() : 0;
  sc->listThumbHeight = m_listThumbH ? m_listThumbH->value() : 0;
}

void ViewFieldConfigDialog::fillEditResult(JsonTableEditCol *ec) const {
  if (!ec) return;
  if (m_widgetCombo) ec->widget = m_widgetCombo->currentData().toString();
  ec->editEditable = m_editEditableCheck ? m_editEditableCheck->isChecked() : true;
  ec->required = m_requiredCheck ? m_requiredCheck->isChecked() : false;
  ec->switchEditable =
      m_listModeCombo && m_listModeCombo->currentData().toString() == QStringLiteral("switch");
  if (m_placeholderEdit) ec->placeholder = m_placeholderEdit->text().trimmed();
  ec->maxlength = m_maxlengthSpin ? m_maxlengthSpin->value() : 0;
  ec->textareaRows = m_textareaRowsSpin ? m_textareaRowsSpin->value() : 0;
  ec->minValue = m_minSpin ? m_minSpin->value() : 0;
  ec->maxValue = m_maxSpin ? m_maxSpin->value() : 0;
  ec->precision = m_precisionSpin ? m_precisionSpin->value() : 0;
  if (m_dateFormatCombo) ec->dateFormat = m_dateFormatCombo->currentData().toString();
  ec->formSpan = m_formSpanCombo ? m_formSpanCombo->currentData().toInt() : 0;
  if (m_formDefaultEdit) ec->formDefaultValue = m_formDefaultEdit->text().trimmed();
  const QString ref = currentUploadRef();
  if (ref.isEmpty()) {
    ec->uploadSourceFile.clear();
    ec->uploadSourceId.clear();
  } else {
    const int sep = ref.lastIndexOf(QLatin1Char('#'));
    ec->uploadSourceFile = ref.left(sep);
    ec->uploadSourceId = ref.mid(sep + 1);
  }
  ec->editThumbWidth = m_editThumbW ? m_editThumbW->value() : 0;
  ec->editThumbHeight = m_editThumbH ? m_editThumbH->value() : 0;
}

QString ViewFieldConfigDialog::domainType() const {
  if (!m_domainTypeCombo) return QString();
  const QString uiType = m_domainTypeCombo->currentData().toString();
  if (uiType == QStringLiteral("source")) {
    const auto it = m_domainOptionCounts.constFind(currentDomainRef());
    const int n = it == m_domainOptionCounts.constEnd() ? 0 : it.value();
    return QString::fromLatin1(n == 2 ? JsonVueDomain::kEnum : JsonVueDomain::kStatic);
  }
  if (uiType == QString::fromLatin1(JsonVueDomain::kRemote)) return uiType;
  return QString();
}

QString ViewFieldConfigDialog::domainSourceFile() const { return m_domainSourceFile; }

QString ViewFieldConfigDialog::domainSourceId() const { return m_domainSourceId; }

QString ViewFieldConfigDialog::domainUrl() const {
  const QString t = domainType();
  return t == QString::fromLatin1(JsonVueDomain::kRemote) ? m_selectUrl : QString();
}
