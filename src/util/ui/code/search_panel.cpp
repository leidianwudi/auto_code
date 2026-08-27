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
#include <QMap>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
#include <QTextStream>
#include <QVBoxLayout>

#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"

/// 是否为标识符字符（用于"全词匹配"边界判断）
static inline bool isWordChar(const QChar &c) {
  return c.isLetterOrNumber() || c == QLatin1Char('_');
}

// ══════════════════════════════════════════════════════════════════════════════
//  构造 / UI
// ══════════════════════════════════════════════════════════════════════════════

SearchPanel::SearchPanel(QWidget *parent) : QWidget(parent) {
  setupUI();
  // 主题切换时刷新面板背景 / 标签文字色（结果树由 VscResultTree 自行响应）
  connect(&SettingStore::ins(), &SettingStore::themeChanged, this, &SearchPanel::refreshStyle);
}

void SearchPanel::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  // ── 第一行：输入框 + 选项复选框（上方控件保留合适外边距）──
  auto *searchRow = new QHBoxLayout;
  searchRow->setContentsMargins(6, 6, 6, 0);
  searchRow->setSpacing(4);

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

  searchRow->addWidget(m_searchEdit, 1);
  searchRow->addWidget(m_caseCheck);
  searchRow->addWidget(m_wordCheck);

  // ── 第二行：汇总标签（靠左）+ 全部折叠按钮（靠右，VSCode 搜索视图）──
  auto *summaryRow = new QHBoxLayout;
  summaryRow->setContentsMargins(6, 0, 6, 0);
  summaryRow->setSpacing(4);
  m_summaryLabel = new QLabel(QStringLiteral("0 个文件有 0 个结果"));
  m_summaryLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                    .arg(AuiStyle::mutedTextColor().name()));
  summaryRow->addWidget(m_summaryLabel);
  summaryRow->addStretch(1);
  m_collapseBtn = AuiButton::createCollapseAllButton();
  connect(m_collapseBtn, &QPushButton::clicked, this,
          [this]() { m_resultTree->collapseAll(); });
  summaryRow->addWidget(m_collapseBtn);

  // ── 结果树：VSCode 风格（文件分组 → 匹配行）──
  m_resultTree = new VscResultTree;
  connect(m_resultTree, &QTreeWidget::itemClicked, this, &SearchPanel::onItemClicked);

  layout->addLayout(searchRow);
  layout->addLayout(summaryRow);
  layout->addWidget(m_resultTree, 1);

  // 面板统一样式：背景随主题（setStyleSheet 触发重新抛光，主题切换立即生效）；
  // 复选框指示器交给 AuiStyle 全局代理绘制，不在 per-widget 样式表里覆盖
  applyPanelStyle();
}

void SearchPanel::applyPanelStyle() {
  setStyleSheet(QStringLiteral(
                    "SearchPanel { background-color: %1; }"
                    "QCheckBox { spacing: 3px; font-size: 11px; }"
                    "QLineEdit { padding: 2px 4px; }")
                    .arg(AuiStyle::panelBackground().name()));
}

// ══════════════════════════════════════════════════════════════════════════════
//  对外接口
// ══════════════════════════════════════════════════════════════════════════════

void SearchPanel::setSearchRoot(const QString &rootPath) {
  m_searchRoot = rootPath;
}

void SearchPanel::startSearch(const QString &text) {
  // 屏蔽 setText 触发的 textChanged，避免重复扫描（只执行一次搜索）
  m_searchEdit->blockSignals(true);
  m_searchEdit->setText(text);
  m_searchEdit->blockSignals(false);
  performSearch();
}

void SearchPanel::refreshStyle() {
  // 面板背景随主题重建（setStyleSheet 触发重新抛光，立即生效）
  applyPanelStyle();
  // 汇总标签文字色随当前主题重建（避免固化旧主题颜色）
  m_summaryLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                    .arg(AuiStyle::mutedTextColor().name()));
  // 结果树背景 / 滚动条 / 字体 / 图标随主题刷新
  m_resultTree->reloadStyle();
  update();
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
  performSearch();
}

void SearchPanel::onOptionsChanged() {
  performSearch();
}

void SearchPanel::clearResults() {
  m_resultTree->clear();
  m_matches.clear();
}

void SearchPanel::updateSummary() {
  // 统计文件数（按文件路径去重）
  QSet<QString> files;
  for (const SearchMatch &m : m_matches) files.insert(m.filePath);
  const int fileCount = files.size();
  const int resultCount = m_matches.size();
  m_summaryLabel->setText(QStringLiteral("%1 个文件有 %2 个结果").arg(fileCount).arg(resultCount));
}

bool SearchPanel::shouldSearchFile(const QString &filePath) const {
  const QString rel = filePath;
  // 排除常见构建/依赖/版本控制目录（相对路径判断）
  static const QStringList kSkipDirs = {
      QStringLiteral("/build/"),    QStringLiteral("/.git/"),
      QStringLiteral("/.vs/"),      QStringLiteral("/node_modules/"),
      QStringLiteral("/dist/"),     QStringLiteral("/out/"),
      QStringLiteral("/bin/"),      QStringLiteral("/obj/"),
      QStringLiteral("/.cache/"),   QStringLiteral("/cmake-build-"),
  };
  QString norm = rel;
  norm.replace(QLatin1Char('\\'), QLatin1Char('/'));
  for (const QString &d : kSkipDirs) {
    if (norm.contains(d)) return false;
  }
  // 排除 tree.config（勾选状态配置，非源码）
  if (norm.endsWith(QStringLiteral("/tree.config"))) return false;
  // 排除二进制扩展名
  static const QStringList kSkipExt = {
      QStringLiteral("exe"), QStringLiteral("dll"), QStringLiteral("obj"),
      QStringLiteral("pdb"), QStringLiteral("o"),   QStringLiteral("lib"),
      QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
      QStringLiteral("gif"), QStringLiteral("ico"), QStringLiteral("bmp"),
      QStringLiteral("qm"),  QStringLiteral("qrc"), QStringLiteral("res"),
  };
  const QString ext = QFileInfo(filePath).suffix().toLower();
  if (kSkipExt.contains(ext)) return false;
  return true;
}

void SearchPanel::performSearch() {
  clearResults();
  const QString needle = m_searchEdit->text();
  if (needle.isEmpty() || m_searchRoot.isEmpty()) {
    updateSummary();
    return;
  }

  const bool caseSensitive = m_caseCheck->isChecked();
  const bool wholeWord = m_wordCheck->isChecked();

  // 遍历搜索根目录下所有文件
  QDirIterator it(m_searchRoot, QDir::Files,
                  QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
  while (it.hasNext()) {
    it.next();
    const QString filePath = it.filePath();
    if (!shouldSearchFile(filePath)) continue;

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

  // ── 构建结果树：文件分组节点 → 匹配行节点（VSCode 风格）──
  QMap<QString, QTreeWidgetItem *> fileItems;
  for (const SearchMatch &m : m_matches) {
    QTreeWidgetItem *fileItem = fileItems.value(m.filePath);
    if (!fileItem) {
      QString rel = QDir(m_searchRoot).relativeFilePath(m.filePath);
      fileItem = m_resultTree->addFileNode(m.filePath, rel.isEmpty() ? m.filePath : rel);
      fileItems.insert(m.filePath, fileItem);
    }
    m_resultTree->addMatchNode(fileItem, m.filePath, m.line, m.column,
                               static_cast<int>(m.length), m.lineText.trimmed());
  }
  // 文件分组节点显示匹配数
  for (auto it2 = fileItems.begin(); it2 != fileItems.end(); ++it2) {
    it2.value()->setText(0, QStringLiteral("%1 (%2)")
                                .arg(it2.value()->text(0))
                                .arg(it2.value()->childCount()));
  }
  m_resultTree->expandAll();

  updateSummary();
}

// ══════════════════════════════════════════════════════════════════════════════
//  结果点击 → 跳转
// ══════════════════════════════════════════════════════════════════════════════

void SearchPanel::onItemClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  if (!item) return;
  // 仅结果行（有行号）触发跳转；文件分组节点不跳转
  const int line = item->data(0, VscTreeRole::Line).toInt();
  if (line <= 0) return;
  const QString filePath = item->data(0, VscTreeRole::FilePath).toString();
  const int col = item->data(0, VscTreeRole::Column).toInt();
  const int len = item->data(0, VscTreeRole::Length).toInt();
  emit openRequested(filePath, line, col, len);
}
