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
#include <QShowEvent>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"

/// 是否为标识符字符（用于"全词匹配"边界判断）
static inline bool isWordChar(const QChar &c) {
  return c.isLetterOrNumber() || c == QLatin1Char('_');
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

  // ── 汇总行末尾：全部折叠按钮（基类已放置汇总标签 + 弹性空间）──
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

  // 遍历搜索根目录下所有文件
  QDirIterator it(searchRoot(), QDir::Files,
                  QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
  while (it.hasNext()) {
    it.next();
    const QString filePath = it.filePath();
    if (!shouldScanFile(filePath)) continue;

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
    QTextStream in(&f);
    int lineNo = 0;
    while (!in.atEnd()) {
      QString lineText = in.readLine();
      ++lineNo;
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
    f.close();
  }

  // ── 构建结果树：文件分组节点 → 匹配行节点（基类统一实现）──
  buildResultTree(m_matches);

  updateSummary();
  // 通知外部：搜索完成，同步编辑器查找高亮
  emit searchPerformed(needle);
}
