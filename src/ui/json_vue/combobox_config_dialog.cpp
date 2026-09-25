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
#include <QStandardItem>
#include <QTableWidget>
#include <QVBoxLayout>

#include "config_dialog_common.h"
#include "select_source_panel.h"
#include "src/ui/json_global_enum/json_global_enum_model.h"
#include "src/ui/json_source/json_source_finder.h"
#include "src/ui/json_source/json_source_model.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_message_box.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/component/aui_tree_combo.h"


// ════════════════════════════════════════════════════════════
//  构造 / 界面构建
// ════════════════════════════════════════════════════════════

ComboboxConfigDialog::ComboboxConfigDialog(QWidget *parent) : QDialog(parent) { setupUI(); }

void ComboboxConfigDialog::setupUI() {
  // 复用与其它 jsonvue 配置对话框一致的框架（标题栏 + 确定/取消）
  ConfigDialogFrame frame =
      beginConfigDialog(this, QStringLiteral("下拉框数据源配置"), QMargins(12, 10, 12, 10), 6);
  auto *layout = frame.contentLayout;

  // ── 数据源树形选择行（按文件分组：.jsonsource 数据源 + 全局枚举）──
  auto *srcRow = new QHBoxLayout;
  srcRow->addWidget(new QLabel(QStringLiteral("数据源:")));
  // 问号帮助按钮：说明数据源列表受「右键设为项目」作用域过滤，并含全局枚举
  srcRow->addWidget(AuiButton::createHelpButton(QStringLiteral("数据源作用域"),
                                                jsonVueSourceScopeHelpText(), frame.contentWidget));
  srcRow->addSpacing(2);
  m_sourceTree = new AuiTreeCombo(frame.contentWidget);
  m_sourceTree->setMinimumWidth(220);
  srcRow->addWidget(m_sourceTree, 1);
  layout->addLayout(srcRow);

  // 顶层"手动配置"条目：不引用数据源文件（向后兼容）
  m_sourceTree->addEntry(nullptr, QStringLiteral("（手动配置 URL，不引用数据源文件）"), -1);

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

  // ── 静态数据源选项只读预览（选中静态数据源/全局枚举时展示实际选项内容）──
  // 不设最大高度：拉伸填充提示行以下的全部剩余空间，避免大面积空白
  m_staticOptions = makeConfigTable(
      {{QStringLiteral("显示文本"), QHeaderView::Stretch, 0},
       {QStringLiteral("实际值"), QHeaderView::Stretch, 0},
       {QStringLiteral("值类型"), QHeaderView::ResizeToContents, 0}},
      frame.contentWidget, 96, 0);
  m_staticOptions->setSelectionMode(QAbstractItemView::NoSelection);  // 纯展示，不可选择
  m_staticOptions->setEditTriggers(QAbstractItemView::NoEditTriggers);  // 不可编辑
  m_staticOptions->setFocusPolicy(Qt::NoFocus);
  m_staticOptions->hide();
  layout->addWidget(m_staticOptions, 1);

  // ── 动态数据源配置面板（URL/字段/分页等）──
  m_panel = new SelectSourcePanel(frame.contentWidget);
  layout->addWidget(m_panel, 1);

  finishConfigDialog(this, frame);
  setMinimumSize(620, 520);

  // 初始构建数据源树
  rebuildSourceTree();

  // 信号：选中数据源条目 → 应用到面板
  connect(m_sourceTree, &AuiTreeCombo::itemSelected, this, [this](const QVariant &data) {
    if (m_loading) return;
    // 用户手动切换数据源：清除上一数据源的覆盖值，避免误带入选中的新数据源
    m_storedValue.clear();
    m_storedLabel.clear();
    applySelectedSource(data.toInt());
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
//  数据源树形下拉（.jsonsource 文件分组 + 全局枚举分组）
// ════════════════════════════════════════════════════════════

void ComboboxConfigDialog::rebuildSourceTree() {
  m_loading = true;
  m_sourceTree->clear();
  m_candidates.clear();

  // 顶层"手动配置"条目：不引用数据源文件（向后兼容）。
  // 注意：clear() 会清空全部行，手动条目必须在每次重建时重新添加
  m_sourceTree->addEntry(nullptr, QStringLiteral("（手动配置 URL，不引用数据源文件）"), -1);

  // 候选一：作用域内的 .jsonsource 数据源文件（每个文件一组，动态/静态均可选）
  const QStringList files = findJsonsourceFiles(m_searchRoot);
  for (const QString &f : files) {
    JsonSourceConfig cfg;
    {
      QFile f2(f);
      if (f2.open(QIODevice::ReadOnly | QIODevice::Text)) {
        cfg = JsonSourceConfig::fromJsonString(QString::fromUtf8(f2.readAll()));
        f2.close();
      }
    }
    QStandardItem *group = nullptr;
    for (const auto &s : cfg.sources) {
      if (!group) group = m_sourceTree->addGroup(QStringLiteral("▍数据源 · ") + QFileInfo(f).fileName());
      // 统一「说明 - 标识（N 项）」格式（与字段设置面板的取值数据源树一致）：
      // 动态源显示接口 url；静态源显示 url 值（函数名推导值，如 status）+ 选项数；
      // 旧静态数据无 url 时回退为派生函数名
      QString text = s.remark.isEmpty() ? QStringLiteral("(未命名)") : s.remark;
      if (s.isDynamic()) {
        text += QStringLiteral(" - %1").arg(s.url);
      } else {
        const QString ident =
            s.url.isEmpty() ? staticSourceFuncName(QFileInfo(f).baseName(), s.url) : s.url;
        text += QStringLiteral(" - %1（%2 项）").arg(ident, QString::number(s.options.size()));
      }
      SourceCandidate c;
      c.fileRef = f;      // 绝对路径（既有行为，生成侧按 basename 匹配真实路径）
      c.filePath = f;
      c.sourceId = s.id;
      c.display = text;
      c.isGlobal = false;
      c.source = s;
      m_candidates.append(c);
      m_sourceTree->addEntry(group, text, int(m_candidates.size() - 1));
    }
  }

  // 候选二：全局枚举（.jsonglobalenum，含平台共享层与兄弟后端项目）。
  // 转换为与 .jsonsource 同构的静态数据源；引用存文件基名 global_enum.jsonsource
  // （与生成侧同步产物一致，枚举文件移动/重命名不影响引用解析）
  const QStringList enumFiles = findGlobalEnumFiles(m_searchRoot);
  for (const QString &ef : enumFiles) {
    JsonGlobalEnumConfig cfg;
    {
      QFile f(ef);
      if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        cfg = JsonGlobalEnumConfig::fromJsonString(QString::fromUtf8(f.readAll()));
        f.close();
      }
    }
    // 组标题标注层级：直接位于 crud_nest/ 下为共享层，位于 crud_nest/<项目>/ 下为项目级
    const QString parentName = QFileInfo(ef).absolutePath().section(QLatin1Char('/'), -1);
    const bool isShared = parentName == QStringLiteral("crud_nest");
    QStandardItem *group = nullptr;
    for (const auto &e : cfg.enums) {
      if (!group) {
        group = m_sourceTree->addGroup(
            isShared ? QStringLiteral("▍全局枚举（共享层） · ") + QFileInfo(ef).fileName()
                     : QStringLiteral("▍全局枚举（%1） · %2").arg(parentName, QFileInfo(ef).fileName()));
      }
      // 转换为与 .jsonsource 同构的静态数据源（id 缺省用 name 兜底，与生成侧一致）
      JsonSource js;
      js.id = e.id.isEmpty() ? e.name : e.id;
      js.type = QString::fromLatin1(JsonSourceType::kStatic);
      js.url = e.name;  // 前端函数名由此推导（与同步产物 global_enum.ts 一致）
      js.remark = e.remark;
      for (const auto &o : e.options) {
        JsonSourceOption op;
        op.label = o.label;
        op.value = o.value;
        op.valueType = o.valueType;
        js.options.append(op);
      }
      // 显示：说明（remark）在前 + 枚举名（如 is_open），与「说明 - url」格式统一；
      // 生成侧函数名由 js.url（= 枚举名）按 global_enum 基名推导，不在列表展示
      const QString remark = e.remark.isEmpty() ? QStringLiteral("(未命名)") : e.remark;
      QString text = QStringLiteral("%1 - %2").arg(remark, e.name);
      SourceCandidate c;
      c.fileRef = QStringLiteral("global_enum.jsonsource");
      c.filePath = ef;
      c.sourceId = js.id;
      c.display = text;
      c.isGlobal = true;
      c.source = js;
      m_candidates.append(c);
      m_sourceTree->addEntry(group, text, int(m_candidates.size() - 1));
    }
  }
  m_loading = false;

  // 默认选中第一个候选（setSourceRef 稍后按已存引用重新选中正确的项）
  applySelectedSource(m_candidates.isEmpty() ? -1 : 0);
}

void ComboboxConfigDialog::applySelectedSource(int candidateIdx) {
  if (m_loading) return;
  m_selectedCandidate = candidateIdx;
  // 同步控件内部选中引用（再次打开弹层时按此高亮）；m_loading 抑制 itemSelected 回环。
  // candidateIdx=-1 时匹配顶层「手动配置」条目（data 同为 -1），手动模式也保留高亮
  m_loading = true;
  m_sourceTree->selectByData(candidateIdx);
  m_loading = false;
  if (candidateIdx < 0 || candidateIdx >= m_candidates.size()) {
    // 手动模式：无数据源可选择，面板全部可编辑；显示文字同步为手动条目
    m_staticHint->hide();
    m_staticOptions->hide();
    m_modeWidget->hide();
    m_hasDynamicSource = false;
    m_panel->setEnabled(true);
    m_panel->setVisible(true);
    m_panel->setTags({});  // 清空数据源 tag 残留（手动模式无数据源）
    m_sourceTree->setEditText(QStringLiteral("（手动配置 URL，不引用数据源文件）"));
    updatePanelLocks();
    return;
  }

  const SourceCandidate &c = m_candidates[candidateIdx];
  const JsonSource &s = c.source;
  const bool isGlobal = c.isGlobal;

  if (s.isStatic()) {
    // 静态数据源：隐藏动态配置面板与使用方式行，显示提示；按钮显示条目文字
    m_panel->setVisible(false);
    m_modeWidget->hide();
    m_hasDynamicSource = false;
    m_sourceTree->setEditText(c.display);
    m_staticHint->setText(isGlobal ? QStringLiteral("该数据源为全局枚举（%1 项选项），无需 URL 配置，"
                                                   "生成时直接使用选项。")
                                         .arg(s.options.size())
                                   : QStringLiteral("该数据源为静态数据源（%1 项选项），无需 URL 配置，"
                                                    "生成时直接使用选项。")
                                         .arg(s.options.size()));
    m_staticHint->show();
    // 只读预览实际选项内容；如需修改请编辑对应的数据源/枚举文件后重开本对话框
    m_staticOptions->setRowCount(0);
    for (const auto &o : s.options) {
      const int r = m_staticOptions->rowCount();
      m_staticOptions->insertRow(r);
      m_staticOptions->setItem(r, 0, new QTableWidgetItem(o.label));
      m_staticOptions->setItem(r, 1, new QTableWidgetItem(o.value));
      m_staticOptions->setItem(r, 2, new QTableWidgetItem(o.valueType));
    }
    m_staticOptions->show();
    // 清空动态配置残留
    m_panel->setData(QString(), QString(), QString(), QString(), false, QString(), QString(), 20,
                     QString(), QString());
  } else {
    // 动态数据源：基础配置跟随数据源并锁定，显示使用方式行
    m_staticHint->hide();
    m_staticOptions->hide();
    m_panel->setVisible(true);
    m_modeWidget->setVisible(true);
    m_sourceTree->setEditText(c.display);
    m_appliedSource = s;
    m_hasDynamicSource = true;
    m_panel->setData(s.url, s.method, s.valueField, s.labelField, s.paged, s.pageKey,
                     s.pageSizeKey, s.pageSize, s.searchTitle, s.searchField);
    // 展示数据源配置的 tag 显示样式（全部使用数据源时只读，部分使用时可覆盖）
    m_panel->setTags(s.tags);
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
  // 保留引用：候选中未找到时（文件被移动等），sourceFile()/sourceId() 原样返回，
  // 保证用户配置不丢失
  m_fallbackFile = sourceFile;
  m_fallbackId = sourceId;
  if (sourceFile.isEmpty()) {
    applySelectedSource(-1);  // 手动模式
    return;
  }
  // 两级匹配：文件引用 + id 精确匹配 → 仅 id 匹配（文件被移动/重命名后的兜底）
  int exact = -1;
  int idOnly = -1;
  for (int i = 0; i < m_candidates.size(); ++i) {
    if (m_candidates[i].sourceId != sourceId) continue;
    if (m_candidates[i].fileRef == sourceFile) {
      exact = i;
      break;
    }
    if (idOnly < 0) idOnly = i;
  }
  applySelectedSource(exact >= 0 ? exact : (idOnly >= 0 ? idOnly : -1));
}

void ComboboxConfigDialog::setSearchRoot(const QString &dir) {
  m_searchRoot = dir;
  rebuildSourceTree();
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
  // 候选已选中 → 返回候选引用（.jsonsource 绝对路径 / 全局枚举基名）；
  // 未选中 → 返回保留引用（候选中未找到时的原样保留，供保存）
  if (m_selectedCandidate >= 0 && m_selectedCandidate < m_candidates.size())
    return m_candidates[m_selectedCandidate].fileRef;
  return m_fallbackFile;
}

QString ComboboxConfigDialog::sourceId() const {
  if (m_selectedCandidate >= 0 && m_selectedCandidate < m_candidates.size())
    return m_candidates[m_selectedCandidate].sourceId;
  return m_fallbackId;
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
