/**
 * @file view_field_config_dialog.cpp
 * @brief jsontable 字段呈现 ⚙ 字段设置面板实现
 */

#include "view_field_config_dialog.h"

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
  ConfigDialogFrame frame = beginConfigDialog(this, QStringLiteral("字段设置：%1").arg(m_ref),
                                              QMargins(12, 10, 12, 10), 6);
  auto *form = new QFormLayout;
  form->setSpacing(6);

  if (!m_isJoinRow) {
    // ── 取值域（方案 B 中心化；类型简化为 未声明/数据源/远程接口）──
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
  m_displayTypeCombo = AuiComboBox::create(this);
  m_displayTypeCombo->addItem(QStringLiteral("自动（按编辑控件/类型推导）"), QString());
  m_displayTypeCombo->addItem(QStringLiteral("image（图片缩略图）"), QStringLiteral("image"));
  form->addRow(QStringLiteral("列表展示:"), m_displayTypeCombo);
  connect(m_displayTypeCombo, &QComboBox::activated, this,
          [this](int) { m_listThumbW->setEnabled(m_displayTypeCombo->currentData().toString() ==
                                                 QStringLiteral("image"));
                        m_listThumbH->setEnabled(m_listThumbW->isEnabled()); });

  m_widthCombo = createNumericCombo(this, {60, 80, 100, 120, 150, 180, 220}, 0);
  m_widthCombo->setEditable(true);
  m_widthCombo->lineEdit()->setPlaceholderText(QStringLiteral("空 = 自适应"));
  form->addRow(QStringLiteral("列表列宽(px):"), m_widthCombo);

  auto *listThumbRow = new QWidget(this);
  auto *listThumbLay = new QHBoxLayout(listThumbRow);
  listThumbLay->setContentsMargins(0, 0, 0, 0);
  listThumbLay->setSpacing(4);
  m_listThumbW = new QSpinBox(listThumbRow);
  m_listThumbH = new QSpinBox(listThumbRow);
  for (QSpinBox *spin : {m_listThumbW, m_listThumbH}) {
    spin->setRange(0, 999);
    spin->setSpecialValueText(QStringLiteral("跟随预设"));
    listThumbLay->addWidget(spin);
  }
  listThumbLay->addWidget(new QLabel(QStringLiteral("×（宽 × 高，0=跟随上传预设）"), listThumbRow));
  listThumbLay->addStretch();
  form->addRow(QStringLiteral("列表缩略图宽高(px):"), listThumbRow);

  // ── 编辑表单（联表行只读，无编辑控件配置）──
  m_widgetCombo = AuiComboBox::create(this);
  m_widgetCombo->addItem(QStringLiteral("自动（按 mysqlType 推导）"), QStringLiteral(""));
  m_widgetCombo->addItem(QStringLiteral("input"), QStringLiteral("input"));
  m_widgetCombo->addItem(QStringLiteral("textarea"), QStringLiteral("textarea"));
  m_widgetCombo->addItem(QStringLiteral("select"), QStringLiteral("select"));
  m_widgetCombo->addItem(QStringLiteral("date"), QStringLiteral("date"));
  m_widgetCombo->addItem(QStringLiteral("number"), QStringLiteral("number"));
  m_widgetCombo->addItem(QStringLiteral("image"), QStringLiteral("image"));
  m_widgetCombo->addItem(QStringLiteral("upload"), QStringLiteral("upload"));
  m_widgetCombo->addItem(QStringLiteral("boolean"), QStringLiteral("boolean"));
  form->addRow(QStringLiteral("编辑控件:"), m_widgetCombo);
  connect(m_widgetCombo, &QComboBox::activated, this,
          [this](int) { updateEditFormEnabled(); });

  m_editLabelEdit = new QLineEdit(this);
  m_editLabelEdit->setPlaceholderText(QStringLiteral("空 = 用主表格标题"));
  form->addRow(QStringLiteral("编辑标题覆盖:"), m_editLabelEdit);

  // ── 编辑表单精细参数（键名与 .jsonvue 一致；空/0=未配置，合成页由模板按类型缺省）──
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

  m_formSpanSpin = new QSpinBox(this);
  m_formSpanSpin->setRange(0, 24);
  m_formSpanSpin->setSpecialValueText(QStringLiteral("跟随模板（12）"));
  form->addRow(QStringLiteral("表单栅格 span:"), m_formSpanSpin);

  m_formDefaultEdit = new QLineEdit(this);
  m_formDefaultEdit->setPlaceholderText(QStringLiteral("新建时表单预填（空 = 不预填）"));
  form->addRow(QStringLiteral("表单默认值:"), m_formDefaultEdit);

  // 上传预设下拉：列出作用域内所有 .jsonupload 预设（同 jsonvue 字段设置面板）
  m_uploadSourceCombo = AuiComboBox::create(this);
  m_uploadSourceCombo->addItem(QStringLiteral("（不上传，手动输入图片 URL）"), QString());
  form->addRow(QStringLiteral("上传预设:"), m_uploadSourceCombo);

  auto *editThumbRow = new QWidget(this);
  auto *editThumbLay = new QHBoxLayout(editThumbRow);
  editThumbLay->setContentsMargins(0, 0, 0, 0);
  editThumbLay->setSpacing(4);
  m_editThumbW = new QSpinBox(editThumbRow);
  m_editThumbH = new QSpinBox(editThumbRow);
  for (QSpinBox *spin : {m_editThumbW, m_editThumbH}) {
    spin->setRange(0, 999);
    spin->setSpecialValueText(QStringLiteral("跟随预设"));
    editThumbLay->addWidget(spin);
  }
  editThumbLay->addWidget(new QLabel(QStringLiteral("×（宽 × 高，0=跟随上传预设）"), editThumbRow));
  editThumbLay->addStretch();
  form->addRow(QStringLiteral("编辑缩略图宽高(px):"), editThumbRow);

  frame.contentLayout->addLayout(form);

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
  m_editLabelEdit->setText(ec.label);
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
  m_formSpanSpin->setValue(ec.formSpan);
  m_formDefaultEdit->setText(ec.formDefaultValue);
  // 联动启用态（displayType=image → 列表缩略图；控件类型 → 各参数行）
  const bool listOn = m_displayTypeCombo->currentData().toString() == QStringLiteral("image");
  m_listThumbW->setEnabled(listOn);
  m_listThumbH->setEnabled(listOn);
  if (!m_isJoinRow) {
    m_editLabelEdit->setEnabled(true);
    updateEditFormEnabled();
  } else {
    m_widgetCombo->setEnabled(false);
    m_editLabelEdit->setEnabled(false);
    m_placeholderEdit->setEnabled(false);
    m_maxlengthSpin->setEnabled(false);
    m_textareaRowsSpin->setEnabled(false);
    m_minSpin->setEnabled(false);
    m_maxSpin->setEnabled(false);
    m_precisionSpin->setEnabled(false);
    m_dateFormatCombo->setEnabled(false);
    m_formSpanSpin->setEnabled(false);
    m_formDefaultEdit->setEnabled(false);
    m_uploadSourceCombo->setEnabled(false);
    m_editThumbW->setEnabled(false);
    m_editThumbH->setEnabled(false);
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
  if (m_domainSourceRow) m_domainSourceRow->setVisible(hasSource);
  if (m_domainUrlBtn) m_domainUrlBtn->setVisible(hasUrl);
  if (m_domainPreviewLabel) m_domainPreviewLabel->setVisible(hasSource);
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

void ViewFieldConfigDialog::updateEditFormEnabled() {
  if (!m_uploadSourceCombo) return;
  const QString w = m_widgetCombo->currentData().toString();
  const bool on = (w == QStringLiteral("image") || w == QStringLiteral("upload"));
  m_uploadSourceCombo->setEnabled(on);
  m_editThumbW->setEnabled(on);
  m_editThumbH->setEnabled(on);
  // 精细参数随控件类型启用（formDefault 全控件通用）
  const bool isInput = (w == QStringLiteral("input"));
  const bool isTextArea = (w == QStringLiteral("textarea"));
  const bool isNumber = (w == QStringLiteral("number") || w == QStringLiteral("int") ||
                         w == QStringLiteral("float"));
  const bool isFloat = (w == QStringLiteral("float"));
  const bool isDate = (w == QStringLiteral("date"));
  m_placeholderEdit->setEnabled(isInput || isTextArea || on);  // input/textarea/image/upload
  m_maxlengthSpin->setEnabled(isInput);
  m_textareaRowsSpin->setEnabled(isTextArea);
  m_minSpin->setEnabled(isNumber);
  m_maxSpin->setEnabled(isNumber);
  m_precisionSpin->setEnabled(isFloat);
  m_dateFormatCombo->setEnabled(isDate);
  m_formSpanSpin->setEnabled(true);
  m_formDefaultEdit->setEnabled(true);
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
    updateEditFormEnabled();
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
  if (m_editLabelEdit) ec->label = m_editLabelEdit->text().trimmed();
  if (m_placeholderEdit) ec->placeholder = m_placeholderEdit->text().trimmed();
  ec->maxlength = m_maxlengthSpin ? m_maxlengthSpin->value() : 0;
  ec->textareaRows = m_textareaRowsSpin ? m_textareaRowsSpin->value() : 0;
  ec->minValue = m_minSpin ? m_minSpin->value() : 0;
  ec->maxValue = m_maxSpin ? m_maxSpin->value() : 0;
  ec->precision = m_precisionSpin ? m_precisionSpin->value() : 0;
  if (m_dateFormatCombo) ec->dateFormat = m_dateFormatCombo->currentData().toString();
  ec->formSpan = m_formSpanSpin ? m_formSpanSpin->value() : 0;
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
