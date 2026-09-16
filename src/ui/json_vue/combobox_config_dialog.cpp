/**
 * @file combobox_config_dialog.cpp
 * @brief 下拉框数据源配置对话框实现
 *
 * 顶层提供 .jsonsource 数据源文件 + 数据源选择，下层复用 SelectSourcePanel
 * 配置动态数据源的 URL / 字段 / 分页等。选中数据源后基础配置（URL/请求方式/
 * 加载方式/分页）完全跟随数据源并锁定，通过「使用方式」单选决定显示文本/实际值
 * 是否可覆盖（全部使用 / 部分使用）。
 */

#include "combobox_config_dialog.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

#include "config_dialog_common.h"
#include "select_source_panel.h"
#include "src/ui/json_source/json_source_finder.h"
#include "src/ui/json_source/json_source_model.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/component/aui_message_box.h"
#include "src/util/ui/component/aui_style.h"


// ════════════════════════════════════════════════════════════
//  构造 / 界面构建
// ════════════════════════════════════════════════════════════

ComboboxConfigDialog::ComboboxConfigDialog(QWidget *parent) : QDialog(parent) { setupUI(); }

void ComboboxConfigDialog::setupUI() {
  // 复用与其它 jsonvue 配置对话框一致的框架（标题栏 + 确定/取消）
  ConfigDialogFrame frame =
      beginConfigDialog(this, QStringLiteral("下拉框数据源配置"), QMargins(12, 10, 12, 10), 6);
  auto *layout = frame.contentLayout;

  // ── .jsonsource 数据源选择行 ──
  auto *srcRow = new QHBoxLayout;
  srcRow->addWidget(new QLabel(QStringLiteral("数据源文件:")));
  // 问号帮助按钮：说明数据源文件列表受「右键设为项目」作用域过滤
  srcRow->addWidget(AuiButton::createHelpButton(QStringLiteral("数据源作用域"),
                                                jsonVueSourceScopeHelpText(), frame.contentWidget));
  srcRow->addSpacing(2);
  m_fileCombo = AuiComboBox::create(frame.contentWidget);
  m_fileCombo->setMinimumWidth(220);
  srcRow->addWidget(m_fileCombo, 1);
  layout->addLayout(srcRow);

  auto *sourceRow = new QHBoxLayout;
  sourceRow->addWidget(new QLabel(QStringLiteral("数据源:")));
  m_sourceCombo = AuiComboBox::create(frame.contentWidget);
  m_sourceCombo->setMinimumWidth(220);
  sourceRow->addWidget(m_sourceCombo, 1);
  layout->addLayout(sourceRow);

  // ── 使用方式行（引用动态数据源时显示）──
  m_modeWidget = new QWidget(frame.contentWidget);
  auto *modeRow = new QHBoxLayout(m_modeWidget);
  modeRow->setContentsMargins(0, 0, 0, 0);
  modeRow->setSpacing(8);
  modeRow->addWidget(new QLabel(QStringLiteral("使用方式:"), m_modeWidget));
  m_fullRadio = new QRadioButton(QStringLiteral("全部使用数据源"), m_modeWidget);
  m_partialRadio =
      new QRadioButton(QStringLiteral("部分使用数据源(可改显示文本/实际值)"), m_modeWidget);
  auto *modeGroup = new QButtonGroup(this);
  modeGroup->addButton(m_fullRadio);
  modeGroup->addButton(m_partialRadio);
  m_fullRadio->setChecked(true);
  modeRow->addWidget(m_fullRadio);
  modeRow->addWidget(m_partialRadio);
  modeRow->addStretch();
  m_modeWidget->hide();
  layout->addWidget(m_modeWidget);

  // ── 静态数据源提示（选中静态数据源时显示，隐藏动态配置面板）──
  m_staticHint = new QLabel(frame.contentWidget);
  m_staticHint->setStyleSheet(
      QStringLiteral("color: %1; font-size: 12px;").arg(AuiStyle::mutedTextColor().name()));
  m_staticHint->hide();
  layout->addWidget(m_staticHint);

  // ── 动态数据源配置面板（URL/字段/分页等）──
  m_panel = new SelectSourcePanel(frame.contentWidget);
  layout->addWidget(m_panel, 1);

  finishConfigDialog(this, frame);
  setMinimumSize(620, 520);

  // 初始化文件下拉框
  refreshJsonsourceFiles();

  // 信号：文件切换 → 刷新数据源；数据源切换 → 应用到面板
  connect(m_fileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { refreshSources(); });
  connect(m_sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
    if (m_loading) return;
    // 用户手动切换数据源：清除上一数据源的覆盖值，避免误带入选中的新数据源
    m_storedValue.clear();
    m_storedLabel.clear();
    applySelectedSource();
  });

  // 使用方式切换：全部使用 → 恢复数据源配置的显示文本/实际值并锁定字段
  connect(m_fullRadio, &QRadioButton::toggled, this, [this](bool checked) {
    if (!checked || m_loading || !m_hasDynamicSource) return;
    const auto &s = m_appliedSource;
    m_panel->setData(s.url, s.method, s.valueField, s.labelField, s.paged, s.pageKey, s.pageSizeKey,
                     s.pageSize, s.searchTitle, s.searchField);
    updatePanelLocks();
  });
  // 部分使用 → 仅解锁显示文本/实际值字段
  connect(m_partialRadio, &QRadioButton::toggled, this, [this](bool checked) {
    if (!checked || m_loading || !m_hasDynamicSource) return;
    updatePanelLocks();
  });
}

// ════════════════════════════════════════════════════════════
//  .jsonsource 文件 / 数据源下拉框
// ════════════════════════════════════════════════════════════

void ComboboxConfigDialog::refreshJsonsourceFiles() {
  m_loading = true;
  m_fileCombo->clear();
  // 第 0 项：不引用 jsonsource，手动配置 URL（向后兼容）
  m_fileCombo->addItem(QStringLiteral("（手动配置 URL，不引用数据源文件）"), QString());
  const QStringList files = findJsonsourceFiles(m_searchRoot);
  for (const QString &f : files) {
    m_fileCombo->addItem(QFileInfo(f).fileName(), f);
  }
  m_loading = false;
}

void ComboboxConfigDialog::refreshSources() {
  m_loading = true;
  m_sourceCombo->clear();

  const QString filePath = m_fileCombo->currentData().toString();
  if (filePath.isEmpty()) {
    // 手动模式：无数据源可选择，面板全部可编辑
    m_sourceCombo->setEnabled(false);
    m_staticHint->hide();
    m_modeWidget->hide();
    m_hasDynamicSource = false;
    m_panel->setEnabled(true);
    m_panel->setVisible(true);
    m_panel->setTags({});  // 清空数据源 tag 残留（手动模式无数据源）
    m_loading = false;
    updatePanelLocks();
    return;
  }

  m_sourceCombo->setEnabled(true);
  JsonSourceConfig cfg;
  {
    QFile f(filePath);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
      cfg = JsonSourceConfig::fromJsonString(QString::fromUtf8(f.readAll()));
      f.close();
    }
  }

  for (const auto &s : cfg.sources) {
    QString text = s.remark.isEmpty() ? QStringLiteral("(未命名)") : s.remark;
    if (s.isDynamic()) {
      text += QStringLiteral(" - %1").arg(s.url);
    } else {
      text += QStringLiteral(" - 静态 %1 项").arg(s.options.size());
    }
    m_sourceCombo->addItem(text, s.id);
  }
  m_loading = false;

  if (m_sourceCombo->count() > 0) {
    // 文件切换后自动选中并应用第一条数据源：
    // addItem 期间 m_loading=true 抑制了信号，须显式应用，否则面板残留上一个文件的值
    m_sourceCombo->setCurrentIndex(0);
    applySelectedSource();
  } else {
    // 文件内无数据源：收起使用方式行并解除锁定
    m_modeWidget->hide();
    m_hasDynamicSource = false;
    updatePanelLocks();
  }
}

void ComboboxConfigDialog::applySelectedSource() {
  if (m_loading) return;
  const QString filePath = m_fileCombo->currentData().toString();
  const QString sourceId = m_sourceCombo->currentData().toString();
  if (filePath.isEmpty() || sourceId.isEmpty()) return;

  JsonSourceConfig cfg;
  {
    QFile f(filePath);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
      cfg = JsonSourceConfig::fromJsonString(QString::fromUtf8(f.readAll()));
      f.close();
    }
  }
  const JsonSource *s = cfg.sourceById(sourceId);
  if (!s) return;

  if (s->isStatic()) {
    // 静态数据源：隐藏动态配置面板与使用方式行，显示提示
    m_panel->setVisible(false);
    m_modeWidget->hide();
    m_hasDynamicSource = false;
    m_staticHint->setText(QStringLiteral("该数据源为静态数据源（%1 项选项），无需 URL 配置，"
                                         "生成时直接使用选项。")
                              .arg(s->options.size()));
    m_staticHint->show();
    // 清空动态配置残留
    m_panel->setData(QString(), QString(), QString(), QString(), false, QString(), QString(), 20,
                     QString(), QString());
  } else {
    // 动态数据源：基础配置跟随数据源并锁定，显示使用方式行
    m_staticHint->hide();
    m_panel->setVisible(true);
    m_modeWidget->setVisible(true);
    m_appliedSource = *s;
    m_hasDynamicSource = true;
    m_panel->setData(s->url, s->method, s->valueField, s->labelField, s->paged, s->pageKey,
                     s->pageSizeKey, s->pageSize, s->searchTitle, s->searchField);
    // 展示数据源配置的 tag 显示样式（全部使用数据源时只读，部分使用时可覆盖）
    m_panel->setTags(s->tags);
    reconcileOverrides();
    updatePanelLocks();
  }
}

void ComboboxConfigDialog::reconcileOverrides() {
  if (!m_hasDynamicSource) return;
  const auto &s = m_appliedSource;
  // 已存储的字段与数据源不同 → 曾做过覆盖，恢复为部分使用；否则全部使用
  const bool diffValue = !m_storedValue.isEmpty() && m_storedValue != s.valueField;
  const bool diffLabel = !m_storedLabel.isEmpty() && m_storedLabel != s.labelField;
  if (!diffValue && !diffLabel) {
    m_fullRadio->setChecked(true);
    return;
  }
  m_partialRadio->setChecked(true);
  m_panel->setData(s.url, s.method, diffValue ? m_storedValue : s.valueField,
                   diffLabel ? m_storedLabel : s.labelField, s.paged, s.pageKey, s.pageSizeKey,
                   s.pageSize, s.searchTitle, s.searchField);
}

void ComboboxConfigDialog::updatePanelLocks() {
  if (m_hasDynamicSource) {
    // 引用动态数据源：基础配置始终锁定，字段按使用方式决定
    m_panel->setBaseLocked(true);
    m_panel->setFieldsLocked(m_fullRadio->isChecked());
    // 全部使用数据源 → tag 显示样式只读展示数据源原始配置，不可自行配置
    m_panel->setTagsLocked(m_fullRadio->isChecked());
  } else {
    // 手动/静态/无数据源：不锁定
    m_panel->setBaseLocked(false);
    m_panel->setFieldsLocked(false);
    m_panel->setTagsLocked(false);
  }
}

// ════════════════════════════════════════════════════════════
//  配置读写
// ════════════════════════════════════════════════════════════

void ComboboxConfigDialog::setConfig(const QString &url, const QString &valueField,
                                     const QString &labelField) {
  // 记录已存储的覆盖值：与数据源不同时恢复为「部分使用数据源」模式
  m_storedValue = valueField;
  m_storedLabel = labelField;
  if (url.isEmpty() && valueField.isEmpty() && labelField.isEmpty()) return;
  if (m_hasDynamicSource) {
    // 引用动态数据源：URL 等基础配置跟随数据源，仅按覆盖值恢复字段
    reconcileOverrides();
    updatePanelLocks();
    return;
  }
  // 手动模式：直接填充面板
  m_panel->setData(url, QString(), valueField, labelField, false, QString(), QString(), 0,
                   QString(), QString());
}

void ComboboxConfigDialog::setPagedConfig(bool paged, const QString &pageKey,
                                          const QString &pageSizeKey, int pageSize,
                                          const QString &searchTitle, const QString &searchField,
                                          const QString &method) {
  // 引用动态数据源：分页等基础配置跟随数据源，忽略存储值
  if (m_hasDynamicSource) return;
  m_panel->setData(m_panel->url(), method, m_panel->valueField(), m_panel->labelField(), paged,
                   pageKey, pageSizeKey, pageSize, searchTitle, searchField);
}

void ComboboxConfigDialog::setSourceRef(const QString &sourceFile, const QString &sourceId) {
  if (sourceFile.isEmpty()) {
    m_fileCombo->setCurrentIndex(0);
    m_sourceCombo->setCurrentIndex(-1);
    return;
  }
  // 查找文件在下拉框中的位置（绝对路径匹配）
  const QString abs = QFileInfo(sourceFile).absoluteFilePath();
  int fi = m_fileCombo->findData(abs);
  if (fi < 0) {
    // 文件不在搜索范围内：保留手动模式，但记录引用以便保存
    m_fileCombo->setCurrentIndex(0);
    return;
  }
  m_fileCombo->setCurrentIndex(fi);
  int si = m_sourceCombo->findData(sourceId);
  if (si >= 0) {
    m_sourceCombo->setCurrentIndex(si);
    // setCurrentIndex 不触发信号时（如目标恰为第 0 条，refreshSources 已自动选中），
    // 显式应用数据源，否则面板不会被填充
    applySelectedSource();
  }
}

void ComboboxConfigDialog::setSearchRoot(const QString &dir) {
  m_searchRoot = dir;
  refreshJsonsourceFiles();
}

void ComboboxConfigDialog::setHttpConfig(const QString &baseUrl, const QString &authHeader,
                                         const QString &postData) {
  m_baseUrl = baseUrl;
  m_authHeader = authHeader;
  m_postData = postData;
  if (m_panel) m_panel->setHttpConfig(baseUrl, authHeader, postData);
}

QString ComboboxConfigDialog::url() const { return m_panel ? m_panel->url() : QString(); }

QString ComboboxConfigDialog::valueField() const {
  return m_panel ? m_panel->valueField() : QString();
}

QString ComboboxConfigDialog::labelField() const {
  return m_panel ? m_panel->labelField() : QString();
}

QString ComboboxConfigDialog::sourceFile() const {
  return m_fileCombo ? m_fileCombo->currentData().toString() : QString();
}

QString ComboboxConfigDialog::sourceId() const {
  return m_sourceCombo ? m_sourceCombo->currentData().toString() : QString();
}

bool ComboboxConfigDialog::paged() const { return m_panel ? m_panel->paged() : false; }

QString ComboboxConfigDialog::pageKey() const {
  return m_panel ? m_panel->pageKey() : QStringLiteral("page");
}

QString ComboboxConfigDialog::pageSizeKey() const {
  return m_panel ? m_panel->pageSizeKey() : QStringLiteral("pageSize");
}

int ComboboxConfigDialog::pageSize() const { return m_panel ? m_panel->pageSize() : 20; }

QString ComboboxConfigDialog::searchTitle() const {
  return m_panel ? m_panel->searchTitle() : QString();
}

QString ComboboxConfigDialog::searchField() const {
  return m_panel ? m_panel->searchField() : QString();
}

QString ComboboxConfigDialog::method() const {
  return m_panel ? m_panel->method() : QString::fromLatin1(JsonVueHttp::kPost);
}
