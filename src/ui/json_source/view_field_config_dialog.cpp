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
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "json_source_finder.h"
#include "json_upload_model.h"
#include "src/ui/json_vue/combobox_config_dialog.h"
#include "src/ui/json_vue/config_dialog_common.h"
#include "src/ui/json_vue/json_vue_model.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/component/aui_tree_combo.h"

// ════════════════════════════════════════════════════════════
//  构造 / 界面构建
// ════════════════════════════════════════════════════════════

ViewFieldConfigDialog::ViewFieldConfigDialog(const QString &ref, bool isJoinRow, QWidget *parent)
    : QDialog(parent), m_ref(ref), m_isJoinRow(isJoinRow) {
  setupUI();
}

void ViewFieldConfigDialog::setupUI() {
  // 对齐 jsonvue 字段设置面板：宽度 520（长标签行不被挤压）
  setMinimumWidth(520);
  ConfigDialogFrame frame = beginConfigDialog(this, QStringLiteral("字段设置"),
                                              QMargins(12, 10, 12, 10), 6);
  m_form = new QFormLayout;
  auto *form = m_form;
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(6);
  // 标签右对齐（对齐 jsonvue）：控件列起始位置一致，标签长短不一不再参差
  form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

  // 字段名首行（对齐 jsonvue：面板围绕单字段属性展开，字段名常显）
  m_fieldNameLabel = new QLabel(m_ref, this);
  m_fieldNameLabel->setStyleSheet(QStringLiteral("font-weight: 600; font-size: 13px;"));
  form->addRow(QStringLiteral("字段:"), m_fieldNameLabel);

  if (!m_isJoinRow) {
    // ── 字段取值域（方案 B 中心化；类型简化为 未声明/数据源/远程接口）──
    auto *domainSep = new QLabel(QStringLiteral("── 字段取值域 ──"), this);
    domainSep->setAlignment(Qt::AlignCenter);
    form->addRow(QString(), domainSep);
    m_domainTypeCombo = AuiComboBox::create(this);
    m_domainTypeCombo->addItem(QStringLiteral("未声明（无数据源）"), QString());
    m_domainTypeCombo->addItem(QStringLiteral("数据源（.jsonsource / 全局枚举）"),
                               QStringLiteral("source"));
    m_domainTypeCombo->addItem(QStringLiteral("远程接口（手动 URL）"),
                               QString::fromLatin1(JsonVueDomain::kRemote));
    form->addRow(QStringLiteral("取值域类型:"), m_domainTypeCombo);

    m_domainSourceRow = new QWidget(this);
    auto *domainSrcLay = new QHBoxLayout(m_domainSourceRow);
    domainSrcLay->setContentsMargins(0, 0, 0, 0);
    domainSrcLay->setSpacing(2);
    domainSrcLay->addWidget(AuiButton::createHelpButton(QStringLiteral("数据源作用域"),
                                                        jsonVueSourceScopeHelpText(),
                                                        m_domainSourceRow));
    m_domainSourceCombo = new AuiTreeCombo(this);
    domainSrcLay->addWidget(m_domainSourceCombo, 1);
    form->addRow(QStringLiteral("取值数据源:"), m_domainSourceRow);

    // 远程源按钮：复用 ComboboxConfigDialog 配置 URL/字段/分页（与 jsonvue 同款）
    m_domainUrlBtn = new QPushButton(QStringLiteral("配置远程数据源..."), this);
    form->addRow(QStringLiteral("远程源:"), m_domainUrlBtn);
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

    m_domainPreviewLabel = new QLabel(this);
    m_domainPreviewLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px;").arg(AuiStyle::mutedTextColor().name()));
    form->addRow(QStringLiteral("选项预览:"), m_domainPreviewLabel);

    // 选中数据源 → 更新缓存 + 预览
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
    });

    // 取值域类型切换：activated 仅用户操作触发，显隐子控件并重建候选
    connect(m_domainTypeCombo, &QComboBox::activated, this, [this](int) {
      buildDomainCandidates();
      rebuildDomainControls();
      deriveWidgetFromDomain();
    });
  }

  // ── 列表页展示 ──
  auto *listSep = new QLabel(QStringLiteral("── 列表页展示 ──"), this);
  listSep->setAlignment(Qt::AlignCenter);
  form->addRow(QString(), listSep);
  m_displayTypeCombo = AuiComboBox::create(this);
  m_displayTypeCombo->addItem(QStringLiteral("自动（按编辑控件/类型推导）"), QString());
  m_displayTypeCombo->addItem(QStringLiteral("image（图片缩略图）"), QStringLiteral("image"));
  form->addRow(QStringLiteral("列表展示:"), m_displayTypeCombo);
  connect(m_displayTypeCombo, &QComboBox::activated, this,
          [this](int) { updateListRows(); });

  m_widthCombo = createNumericCombo(this, {60, 80, 100, 120, 150, 180, 220}, 0);
  m_widthCombo->setEditable(true);
  m_widthCombo->lineEdit()->setPlaceholderText(QStringLiteral("空 = 自适应"));
  form->addRow(QStringLiteral("列表列宽(px):"), m_widthCombo);

  m_listThumbRow = new QWidget(this);
  auto *listThumbLay = new QHBoxLayout(m_listThumbRow);
  listThumbLay->setContentsMargins(0, 0, 0, 0);
  listThumbLay->setSpacing(4);
  m_listThumbW = new QSpinBox(m_listThumbRow);
  m_listThumbH = new QSpinBox(m_listThumbRow);
  for (QSpinBox *spin : {m_listThumbW, m_listThumbH}) {
    spin->setRange(0, 999);
    spin->setSpecialValueText(QStringLiteral("跟随预设"));
    listThumbLay->addWidget(spin);
  }
  listThumbLay->addWidget(new QLabel(QStringLiteral("宽×高（0=跟随预设）"), m_listThumbRow));
  listThumbLay->addStretch();
  form->addRow(QStringLiteral("列表缩略图:"), m_listThumbRow);

  // ── 编辑表单（参数行按控件类型动态出现；联表行整区折叠仅列表展示生效）──
  m_editSep = new QLabel(QStringLiteral("── 编辑表单 ──"), this);
  m_editSep->setAlignment(Qt::AlignCenter);
  form->addRow(QString(), m_editSep);
  m_widgetCombo = AuiComboBox::create(this);
  // 候选文案对齐 jsonvue / 主流低代码惯例：业务词为主，括号内技术名
  m_widgetCombo->addItem(QStringLiteral("自动（按 mysqlType 推导）"), QStringLiteral(""));
  m_widgetCombo->addItem(QStringLiteral("单行文本(input)"), QStringLiteral("input"));
  m_widgetCombo->addItem(QStringLiteral("多行文本(textarea)"), QStringLiteral("textarea"));
  m_widgetCombo->addItem(QStringLiteral("下拉选择(select)"), QStringLiteral("select"));
  m_widgetCombo->addItem(QStringLiteral("日期(date)"), QStringLiteral("date"));
  m_widgetCombo->addItem(QStringLiteral("数字(number)"), QStringLiteral("number"));
  m_widgetCombo->addItem(QStringLiteral("图片(image)"), QStringLiteral("image"));
  m_widgetCombo->addItem(QStringLiteral("上传(upload)"), QStringLiteral("upload"));
  m_widgetCombo->addItem(QStringLiteral("开关(boolean)"), QStringLiteral("boolean"));
  form->addRow(QStringLiteral("编辑控件:"), m_widgetCombo);
  connect(m_widgetCombo, &QComboBox::activated, this,
          [this](int) { updateEditFormRows(); });

  // ── 控件子参数（紧随编辑控件；updateEditFormRows 按类型整行折叠，
  //    可见行总是连续跟在选择器下方——对齐 jsonvue 子容器重建的视觉效果，
  //    且控件值常驻无需缓存回写）──
  m_placeholderEdit = new QLineEdit(this);
  m_placeholderEdit->setPlaceholderText(QStringLiteral("空 = 模板缺省占位"));
  form->addRow(QStringLiteral("占位提示:"), m_placeholderEdit);

  m_maxlengthSpin = new QSpinBox(this);
  m_maxlengthSpin->setRange(0, 9999);
  m_maxlengthSpin->setSpecialValueText(QStringLiteral("不限"));
  form->addRow(QStringLiteral("最大长度:"), m_maxlengthSpin);

  m_textareaRowsSpin = new QSpinBox(this);
  m_textareaRowsSpin->setRange(0, 20);
  m_textareaRowsSpin->setSpecialValueText(QStringLiteral("跟随模板（4）"));
  form->addRow(QStringLiteral("textarea 行数:"), m_textareaRowsSpin);

  auto *rangeRow = new QWidget(this);
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
  rangeLay->addWidget(new QLabel(QStringLiteral("～（最小/最大，0=不限）"), rangeRow));
  rangeLay->addStretch();
  form->addRow(QStringLiteral("数值范围:"), rangeRow);

  m_precisionSpin = new QSpinBox(this);
  m_precisionSpin->setRange(0, 8);
  m_precisionSpin->setSpecialValueText(QStringLiteral("跟随模板（2）"));
  form->addRow(QStringLiteral("小数位:"), m_precisionSpin);

  m_dateFormatCombo = AuiComboBox::create(this);
  m_dateFormatCombo->addItem(QStringLiteral("自动（按 mysqlType 推导）"), QString());
  m_dateFormatCombo->addItem(QStringLiteral("date（年-月-日）"), QStringLiteral("date"));
  m_dateFormatCombo->addItem(QStringLiteral("datetime（年-月-日 时:分:秒）"),
                             QStringLiteral("datetime"));
  m_dateFormatCombo->addItem(QStringLiteral("month（年-月）"), QStringLiteral("month"));
  m_dateFormatCombo->addItem(QStringLiteral("year（年）"), QStringLiteral("year"));
  m_dateFormatCombo->addItem(QStringLiteral("daterange（日期范围）"), QStringLiteral("daterange"));
  form->addRow(QStringLiteral("日期格式覆盖:"), m_dateFormatCombo);

  // 上传预设下拉：列出作用域内所有 .jsonupload 预设（同 jsonvue 字段设置面板）
  m_uploadSourceCombo = AuiComboBox::create(this);
  m_uploadSourceCombo->addItem(QStringLiteral("（不上传，手动输入图片 URL）"), QString());
  form->addRow(QStringLiteral("上传预设:"), m_uploadSourceCombo);

  m_editThumbRow = new QWidget(this);
  auto *editThumbLay = new QHBoxLayout(m_editThumbRow);
  editThumbLay->setContentsMargins(0, 0, 0, 0);
  editThumbLay->setSpacing(4);
  m_editThumbW = new QSpinBox(m_editThumbRow);
  m_editThumbH = new QSpinBox(m_editThumbRow);
  for (QSpinBox *spin : {m_editThumbW, m_editThumbH}) {
    spin->setRange(0, 999);
    spin->setSpecialValueText(QStringLiteral("跟随预设"));
    editThumbLay->addWidget(spin);
  }
  editThumbLay->addWidget(new QLabel(QStringLiteral("宽×高（0=跟随预设）"), m_editThumbRow));
  editThumbLay->addStretch();
  form->addRow(QStringLiteral("编辑缩略图:"), m_editThumbRow);
  // 编辑可编辑/必填（说明走 tooltip，行内保持干净复选框；位置对齐 jsonvue）
  m_editEditableCheck = new QCheckBox(this);
  m_editEditableCheck->setChecked(true);
  m_editEditableCheck->setToolTip(QStringLiteral("取消勾选 = 编辑表单内只读展示\n（详情模式整表单只读，无需逐字段设置）"));
  form->addRow(QStringLiteral("编辑可编辑:"), m_editEditableCheck);

  // 必填（编辑提交时校验；位置与 jsonvue 字段设置一致：编辑可编辑 → 必填 → 栅格）
  m_requiredCheck = new QCheckBox(this);
  form->addRow(QStringLiteral("必填:"), m_requiredCheck);

  // 表单栅格：combo 三态（对齐 jsonvue 半行/整行；自动 = 模板缺省 12）
  m_formSpanCombo = AuiComboBox::create(this);
  m_formSpanCombo->addItem(QStringLiteral("自动（模板缺省）"), 0);
  m_formSpanCombo->addItem(QStringLiteral("半行（24 栅格占 12）"), 12);
  m_formSpanCombo->addItem(QStringLiteral("整行（24 栅格占 24）"), 24);
  form->addRow(QStringLiteral("表单栅格:"), m_formSpanCombo);

  m_formDefaultEdit = new QLineEdit(this);
  m_formDefaultEdit->setPlaceholderText(QStringLiteral("新建时表单预填（空 = 不预填）"));
  form->addRow(QStringLiteral("表单默认值:"), m_formDefaultEdit);

  // 注：编辑表单标题不在此配置——界面显示标题统一取「字段呈现」的标题列

  frame.contentLayout->addLayout(form);

  // 初始行显隐：列表缩略图随 displayType；编辑参数行随控件类型；联表行整区折叠
  updateListRows();
  if (!m_isJoinRow) {
    rebuildDomainControls();
    updateEditFormRows();
  } else {
    setEditRowsVisible(false);
  }

  if (m_isJoinRow) {
    auto *tip = new QLabel(QStringLiteral("联表列只读：无取值域/编辑控件配置，仅列表展示生效"), this);
    frame.contentLayout->addWidget(tip);
  }
  finishConfigDialog(this, frame);
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

void ViewFieldConfigDialog::setCurrent(const JsonTableSelCol &sc, const JsonTableEditCol &ec) {
  comboSelectData(m_widthCombo, sc.width > 0 ? QVariant(sc.width) : QVariant(QString()));
  comboSelectData(m_widgetCombo, ec.widget);
  m_editEditableCheck->setChecked(ec.editEditable);
  m_requiredCheck->setChecked(ec.required);
  comboSelectData(m_displayTypeCombo, sc.displayType);
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
  // 联动行显隐（displayType=image → 列表缩略图行；控件类型 → 编辑参数行；
  // 联表行编辑区整体折叠，仅列表展示生效）
  updateListRows();
  if (m_isJoinRow) {
    setEditRowsVisible(false);
  } else {
    updateEditFormRows();
  }
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

void ViewFieldConfigDialog::updateListRows() {
  if (!m_displayTypeCombo) return;
  // 列表缩略图仅 image 展示类型有意义（整行折叠）
  const bool isImg =
      m_displayTypeCombo->currentData().toString() == QStringLiteral("image");
  setFormRowVisible(m_listThumbRow, isImg);
}

void ViewFieldConfigDialog::updateEditFormRows() {
  if (!m_widgetCombo) return;
  const QString w = m_widgetCombo->currentData().toString();
  const bool isImageLike =
      (w == QStringLiteral("image") || w == QStringLiteral("upload"));
  const bool isNumber = (w == QStringLiteral("number") || w == QStringLiteral("int") ||
                         w == QStringLiteral("float"));
  // 参数行按控件类型整行折叠（对齐 jsonvue 字段设置面板的按需出现模式）
  setFormRowVisible(m_placeholderEdit,
                    isImageLike || w == QStringLiteral("input") ||
                        w == QStringLiteral("textarea"));
  setFormRowVisible(m_maxlengthSpin, w == QStringLiteral("input"));
  setFormRowVisible(m_textareaRowsSpin, w == QStringLiteral("textarea"));
  setFormRowVisible(m_minSpin, isNumber);
  setFormRowVisible(m_precisionSpin, w == QStringLiteral("float"));
  setFormRowVisible(m_dateFormatCombo, w == QStringLiteral("date"));
  setFormRowVisible(m_uploadSourceCombo, isImageLike);
  setFormRowVisible(m_editThumbRow, isImageLike);
}

void ViewFieldConfigDialog::setEditRowsVisible(bool on) {
  if (m_editSep) m_editSep->setVisible(on);
  QWidget *fields[] = {m_widgetCombo,      m_placeholderEdit, m_maxlengthSpin,
                       m_textareaRowsSpin, m_minSpin,         m_precisionSpin,
                       m_dateFormatCombo,  m_editEditableCheck, m_requiredCheck,
                       m_formSpanCombo,    m_formDefaultEdit, m_uploadSourceCombo,
                       m_editThumbRow};
  for (QWidget *f : fields) {
    if (f) setFormRowVisible(f, on);
  }
}

int ViewFieldConfigDialog::formRowOf(QWidget *field) const {
  for (int r = 0; r < m_form->rowCount(); ++r) {
    const QLayoutItem *it = m_form->itemAt(r, QFormLayout::FieldRole);
    if (it && it->widget() == field) return r;
  }
  return -1;
}

void ViewFieldConfigDialog::setFormRowVisible(QWidget *field, bool on) {
  const int r = formRowOf(field);
  if (r >= 0) m_form->setRowVisible(r, on);
}

void ViewFieldConfigDialog::deriveWidgetFromDomain() {
  if (m_isJoinRow || !m_widgetCombo) return;
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
  return int(numericComboValue(m_widthCombo, 0));
}

void ViewFieldConfigDialog::fillListResult(JsonTableSelCol *sc) const {
  if (!sc) return;
  sc->width = width();
  sc->displayType = m_displayTypeCombo ? m_displayTypeCombo->currentData().toString() : QString();
  sc->listThumbWidth = m_listThumbW ? m_listThumbW->value() : 0;
  sc->listThumbHeight = m_listThumbH ? m_listThumbH->value() : 0;
}

void ViewFieldConfigDialog::fillEditResult(JsonTableEditCol *ec) const {
  if (!ec) return;
  if (m_widgetCombo) ec->widget = m_widgetCombo->currentData().toString();
  ec->editEditable = m_editEditableCheck ? m_editEditableCheck->isChecked() : true;
  ec->required = m_requiredCheck ? m_requiredCheck->isChecked() : false;
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
