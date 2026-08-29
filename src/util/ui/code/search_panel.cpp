/**
 * @file search_panel.cpp
 * @brief 跨文件搜索面板实现
 */

#include "search_panel.h"

#include <QCheckBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSet>
#include <QSettings>
#include <QShowEvent>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include "src/ui/create/create_model.h"
#include "src/util/common/code_constants.h"
#include "src/util/common/workspace_iter.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"

/// 是否为标识符字符（用于"全词匹配"边界判断）
static inline bool isWordChar(const QChar &c) {
  return c.isLetterOrNumber() || c == QLatin1Char('_');
}

// ══════════════════════════════════════════════════════════════════════════════
//  文件类型过滤勾选持久化（AppData/search.ini，启动时还原）
// ══════════════════════════════════════════════════════════════════════════════

/// 查找面板状态存储路径（AppData 目录）
static QString searchSettingsPath() {
  QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (dir.isEmpty())
    dir = QDir::homePath() + QString::fromUtf8(CodeConstants::Paths::kAppDataDirName);
  QDir().mkpath(dir);
  return dir + QStringLiteral("/search.ini");
}

/// 保存勾选的文件类型后缀列表
static void saveCheckedTypes(const QStringList &checked) {
  QSettings s(searchSettingsPath(), QSettings::IniFormat);
  s.setValue(QStringLiteral("search/checkedTypes"), checked);
}

/// 读取勾选的文件类型后缀列表（从未保存过返回空列表 → 调用方按默认全选处理）
static QStringList loadCheckedTypes() {
  QSettings s(searchSettingsPath(), QSettings::IniFormat);
  return s.value(QStringLiteral("search/checkedTypes")).toStringList();
}

/// 搜索防抖间隔（ms）：连续输入合并为一次跨文件扫描，避免每个按键都扫全工作区导致卡顿
static constexpr int kSearchDebounceMs = 250;

// ══════════════════════════════════════════════════════════════════════════════
//  构造 / UI
// ══════════════════════════════════════════════════════════════════════════════

SearchPanel::SearchPanel(QWidget *parent) : VscResultPanel(parent) {
  // 搜索防抖定时器：输入停顿后才真正执行跨文件扫描
  m_searchDebounceTimer = new QTimer(this);
  m_searchDebounceTimer->setSingleShot(true);
  m_searchDebounceTimer->setInterval(kSearchDebounceMs);
  connect(m_searchDebounceTimer, &QTimer::timeout, this, &SearchPanel::performSearch);

  setupUI();
  // 主题切换时刷新面板背景 / 标签文字色（结果树由基类 refreshStyle 处理）
  connect(&SettingStore::ins(), &SettingStore::themeChanged, this, &SearchPanel::refreshStyle);
}

void SearchPanel::setupUI() {
  // 主布局骨架：头部行 + 汇总行 + 结果树（由基类创建）
  setupSkeleton();

  // ── 头部行：输入框 + 选项复选框（Aa / \b）──
  m_searchEdit = new QLineEdit;
  m_searchEdit->setPlaceholderText(QStringLiteral("搜索"));
  m_searchEdit->setClearButtonEnabled(true);
  connect(m_searchEdit, &QLineEdit::textChanged, this, &SearchPanel::onSearchTextChanged);
  connect(m_searchEdit, &QLineEdit::returnPressed, this, &SearchPanel::onSearchTextChanged);

  // VSCode 风格小复选框：Aa(区分大小写) / \b(全词匹配)
  // （复选框指示器由 AuiStyle 全局代理统一绘制，浅色/深色主题自适应）
  m_caseCheck = new QCheckBox(QStringLiteral("Aa"));
  m_caseCheck->setToolTip(QStringLiteral("区分大小写"));
  m_caseCheck->setFixedSize(36, 24);
  m_wordCheck = new QCheckBox(QStringLiteral("\\b"));
  m_wordCheck->setToolTip(QStringLiteral("全词匹配"));
  m_wordCheck->setFixedSize(36, 24);
  connect(m_caseCheck, &QCheckBox::toggled, this, &SearchPanel::onOptionsChanged);
  connect(m_wordCheck, &QCheckBox::toggled, this, &SearchPanel::onOptionsChanged);

  headerLayout()->addWidget(m_searchEdit, 1);
  headerLayout()->addWidget(m_caseCheck);
  headerLayout()->addWidget(m_wordCheck);

  // ── 汇总行：文件类型过滤下拉框 + 全部折叠按钮（基类已放置汇总标签 + 弹性空间）──
  // 复选文件后缀过滤搜索范围；选项来源与「新建文件」下拉框一致（CreateModel，扩展类型方便）
  m_typeFilter = new AuiMultiCheckCombo;
  m_typeFilter->setToolTip(QStringLiteral("按文件类型过滤搜索结果"));
  const QVector<CreateModel::FileTypeOption> typeOpts = CreateModel::fileTypeOptions();
  for (const CreateModel::FileTypeOption &opt : typeOpts)
    m_typeFilter->addOption(opt.label, opt.suffix);
  // 默认全选；若之前保存过勾选则还原（只还原仍存在的选项，全无效时回退全选）
  m_typeFilter->setAllChecked(true);
  const QStringList savedTypes = loadCheckedTypes();
  if (!savedTypes.isEmpty()) {
    m_typeFilter->setAllChecked(false);
    for (const QString &s : savedTypes)
      if (m_typeFilter->hasOption(s)) m_typeFilter->setChecked(s, true);
    if (m_typeFilter->checkedData().isEmpty()) m_typeFilter->setAllChecked(true);
  }
  connect(m_typeFilter, &AuiMultiCheckCombo::checkedChanged, this, [this]() {
    saveCheckedTypes(m_typeFilter->checkedData());  // 勾选变化自动保存，启动时还原
    onOptionsChanged();
  });
  // 弹层展开时让搜索框失去焦点：Qt::Popup 不夺取键盘焦点，搜索框聚焦边框会一直残留，
  // 需主动 clearFocus 立即恢复非聚焦（白）边框，避免要点击应用外部才变白
  connect(m_typeFilter, &AuiMultiCheckCombo::popupOpened, this,
          [this]() { m_searchEdit->clearFocus(); });
  summaryLayout()->addWidget(m_typeFilter);
  summaryLayout()->addWidget(collapseButton());

  // ── 结果树：itemClicked → 基类统一跳转处理（文件分组节点不跳转）──
  connect(resultTree(), &QTreeWidget::itemClicked, this, &SearchPanel::onResultClicked);

  // 面板统一样式：背景随主题（setStyleSheet 触发重新抛光，主题切换立即生效）；
  // 复选框指示器交给 AuiStyle 全局代理绘制，不在 per-widget 样式表里覆盖
  applyPanelStyle();
}

void SearchPanel::applyPanelStyle() {
  // 面板背景随主题；搜索输入框复用 AuiStyle 统一的面板输入框样式（与文件面板过滤框一致）
  setStyleSheet(
      QStringLiteral(
          "SearchPanel { background-color: %1; }"
          "QCheckBox { spacing: 3px; font-size: 11px; }")
          .arg(AuiStyle::panelBackground().name()) +
      AuiStyle::inputBoxStyleSheet(QStringLiteral("QLineEdit")));
}

// ══════════════════════════════════════════════════════════════════════════════
//  对外接口
// ══════════════════════════════════════════════════════════════════════════════

void SearchPanel::startSearch(const QString &text) {
  // 屏蔽 setText 触发的 textChanged，避免重复扫描（只执行一次搜索）
  m_searchEdit->blockSignals(true);
  m_searchEdit->setText(text);
  m_searchEdit->blockSignals(false);
  performSearch();
}

const QString &SearchPanel::currentText() const {
  return m_searchEdit->text();
}

void SearchPanel::refreshStyle() {
  // 面板背景随主题重建（setStyleSheet 触发重新抛光，立即生效）
  applyPanelStyle();
  // 文件类型过滤下拉框（按钮 + 弹出菜单）颜色随主题刷新
  if (m_typeFilter) m_typeFilter->refreshStyle();
  // 汇总标签文字色 / 结果树背景 / 滚动条 / 字体 / 图标由基类统一刷新
  VscResultPanel::refreshStyle();
}

void SearchPanel::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  // 首次显示聚焦输入框（不自动搜索，等待用户输入）
  m_searchEdit->setFocus();
}

// ══════════════════════════════════════════════════════════════════════════════
//  搜索逻辑
// ══════════════════════════════════════════════════════════════════════════════

void SearchPanel::onSearchTextChanged() {
  // 输入防抖：连续输入合并为一次跨文件扫描（选项变化仍即时搜索）
  m_searchDebounceTimer->start();
}

void SearchPanel::onOptionsChanged() {
  performSearch();
}

void SearchPanel::updateSummary() {
  // 统计文件数（按文件路径去重）
  QSet<QString> files;
  for (const Match &m : m_matches) files.insert(m.filePath);
  const int fileCount = files.size();
  const int resultCount = m_matches.size();
  setSummaryText(QStringLiteral("%1 个文件有 %2 个结果").arg(fileCount).arg(resultCount));
}

void SearchPanel::performSearch() {
  clearResults();
  m_matches.clear();
  const QString needle = m_searchEdit->text();
  if (needle.isEmpty() || searchRoot().isEmpty()) {
    updateSummary();
    // 通知外部（清空关键词时同步清除编辑器查找高亮）
    emit searchPerformed(needle);
    return;
  }

  const bool caseSensitive = m_caseCheck->isChecked();
  const bool wholeWord = m_wordCheck->isChecked();

  // 主线程构建实时内容快照（已打开编辑器 + 缓冲文件），搜索优先读缓冲而非磁盘旧内容
  const QHash<QString, QString> liveContents =
      m_liveContentProvider ? m_liveContentProvider() : QHash<QString, QString>();

  // 文件类型过滤：只搜勾选的后缀（默认全选 = 全部代码类型）
  QSet<QString> typeSet;
  if (m_typeFilter) {
    const QStringList checked = m_typeFilter->checkedData();
    for (const QString &s : checked) typeSet.insert(s);
  }

  // 遍历搜索根目录下所有文件（统一工作区遍历 + shouldScanFile 过滤）
  forEachWorkspaceFile(
      searchRoot(), true,
      [&typeSet, this](const QString &p) {
        if (!shouldScanFile(p)) return false;
        if (typeSet.isEmpty()) return false;  // 一个类型都没选 → 不搜任何文件
        for (const QString &suf : typeSet)
          if (p.endsWith(suf, Qt::CaseInsensitive)) return true;
        return false;
      },
      [&](const QString &filePath) {
        // 优先缓冲内容，否则读磁盘
        QString text = liveContents.value(filePath);
        if (text.isEmpty()) {
          QFile f(filePath);
          if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
          QTextStream in(&f);
          text = in.readAll();
        }
        const QStringList lines = text.split(QLatin1Char('\n'));
        for (int i = 0; i < lines.size(); ++i) {
          const QString &lineText = lines[i];
          const int lineNo = i + 1;
          // 逐行查找所有匹配（含大小写、全词选项）
          QString hay = lineText;
          QString ndl = needle;
          if (!caseSensitive) {
            hay = hay.toLower();
            ndl = ndl.toLower();
          }
          int from = 0;
          const int nlen = static_cast<int>(ndl.size());
          while (from <= hay.size() - nlen) {
            const int idx = static_cast<int>(hay.indexOf(ndl, from));
            if (idx < 0) break;
            // 全词匹配：匹配前后都不是标识符字符
            bool ok = true;
            if (wholeWord) {
              if (idx > 0 && isWordChar(lineText[idx - 1])) ok = false;
              const int end = idx + static_cast<int>(needle.size());
              if (ok && end < lineText.size() && isWordChar(lineText[end])) ok = false;
            }
            if (ok) {
              m_matches.append({filePath, lineNo, idx, static_cast<int>(needle.size()), lineText});
            }
            from = idx + nlen;
          }
        }
      });

  // ── 构建结果树：文件分组节点 → 匹配行节点（基类统一实现）──
  buildResultTree(m_matches);

  updateSummary();
  // 通知外部：搜索完成，同步编辑器查找高亮。
  // 无结果时发空文本 → 外部清除编辑器高亮，避免类型过滤后残留旧变色（与 VSCode 一致）
  emit searchPerformed(m_matches.isEmpty() ? QString() : needle);
}
