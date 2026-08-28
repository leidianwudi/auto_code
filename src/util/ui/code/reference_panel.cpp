/**
 * @file reference_panel.cpp
 * @brief 引用面板实现
 */

#include "reference_panel.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QVBoxLayout>

#include "comment_scan.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/setting_store.h"

/// 单个命中位置（行内匹配）
struct RefHit {
  int line = 0;     // 1-based 行号
  int column = 0;   // 0-based 匹配起始列
  QString lineText; // 整行文本
};

/// 在整段文本中查找符号的所有引用位置。
/// 跳过注释与字符串中的出现（VSCode 规则：注释/字符串里的标识符不算真实引用）。
static QVector<RefHit> findReferencesInText(const QString &text, const QString &name) {
  QVector<RefHit> hits;
  if (name.isEmpty()) return hits;

  const QVector<QPair<int, int>> nonCode = collectNonCodeRanges(text);
  const QStringList lines = text.split(QLatin1Char('\n'));
  QRegularExpression re(QStringLiteral("\\b") + QRegularExpression::escape(name) +
                        QStringLiteral("\\b"));

  int lineStart = 0;
  for (int i = 0; i < lines.size(); ++i) {
    // 遍历该行所有匹配（一行可能多次引用同一符号）
    auto it = re.globalMatch(lines[i]);
    while (it.hasNext()) {
      const auto match = it.next();
      const int absPos = lineStart + match.capturedStart();
      if (posInComments(nonCode, absPos)) continue;
      RefHit h;
      h.line = i + 1;
      h.column = match.capturedStart();
      h.lineText = lines[i];
      hits.append(h);
    }
    lineStart += lines[i].size() + 1;  // +1 为行尾换行符
  }
  return hits;
}

// ══════════════════════════════════════════════════════════════════════════════
//  构造 / UI
// ══════════════════════════════════════════════════════════════════════════════

ReferencePanel::ReferencePanel(QWidget *parent) : QWidget(parent) {
  setupUI();
  // 主题变化时刷新头部样式（结果树样式由 VscResultTree 自行响应）
  connect(&SettingStore::ins(), &SettingStore::themeChanged, this,
          &ReferencePanel::reloadStyle);
  reloadStyle();
}

void ReferencePanel::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  // ── 第一行：符号名 + 全部折叠按钮（VSCode 引用视图头部，保留合适外边距）──
  auto *headerRow = new QHBoxLayout;
  headerRow->setContentsMargins(6, 6, 6, 0);
  headerRow->setSpacing(4);

  m_symbolLabel = new QLabel;
  m_symbolLabel->setTextFormat(Qt::PlainText);
  headerRow->addWidget(m_symbolLabel, 1);

  m_collapseBtn = AuiButton::createCollapseAllButton();
  connect(m_collapseBtn, &QPushButton::clicked, this,
          [this]() { m_resultTree->collapseAll(); });
  headerRow->addWidget(m_collapseBtn);

  // ── 第二行：汇总标签（保留左右外边距）──
  auto *summaryRow = new QHBoxLayout;
  summaryRow->setContentsMargins(6, 0, 6, 0);
  m_summaryLabel = new QLabel;
  summaryRow->addWidget(m_summaryLabel);
  summaryRow->addStretch(1);

  // ── 结果树：VSCode 风格（文件分组 → 引用行），与查找面板一致 ──
  m_resultTree = new VscResultTree;
  connect(m_resultTree, &QTreeWidget::itemClicked, this, &ReferencePanel::onItemClicked);

  layout->addLayout(headerRow);
  layout->addLayout(summaryRow);
  layout->addWidget(m_resultTree, 1);
}

// ══════════════════════════════════════════════════════════════════════════════
//  对外接口
// ══════════════════════════════════════════════════════════════════════════════

void ReferencePanel::setSearchRoot(const QString &rootPath) {
  m_searchRoot = rootPath;
}

void ReferencePanel::findReferences(const QString &symbolName) {
  m_symbolName = symbolName;
  m_matches.clear();
  m_symbolLabel->setText(symbolName);

  if (symbolName.isEmpty() || m_searchRoot.isEmpty()) {
    buildTree();
    updateSummary();
    return;
  }

  // 遍历工作区文件，逐文件扫描符号引用（跳过注释）
  QDirIterator it(m_searchRoot, QDir::Files,
                  QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
  while (it.hasNext()) {
    it.next();
    const QString filePath = it.filePath();
    if (!shouldScanFile(filePath)) continue;

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
    QTextStream in(&f);
    const QString text = in.readAll();
    f.close();

    const auto hits = findReferencesInText(text, symbolName);
    for (const RefHit &h : hits) {
      ReferenceMatch m;
      m.filePath = filePath;
      m.line = h.line;
      m.column = h.column;
      m.length = symbolName.size();
      m.lineText = h.lineText;
      m_matches.append(m);
    }
  }

  buildTree();
  updateSummary();
}

void ReferencePanel::clear() {
  m_symbolName.clear();
  m_symbolLabel->clear();
  m_matches.clear();
  m_resultTree->clear();
  updateSummary();
}

void ReferencePanel::refreshStyle() {
  reloadStyle();
  // 强制重新抛光 + 重绘：主题切换后 QStyleSheetStyle 下仅靠 update() 不会
  // 重算背景，必须 unpolish/polish 让面板重新读取新调色板
  style()->unpolish(this);
  style()->polish(this);
  update();
}

// ══════════════════════════════════════════════════════════════════════════════
//  样式（面板背景 / 头部标签 / 刷新按钮；结果树由 VscResultTree 统一处理）
// ══════════════════════════════════════════════════════════════════════════════

void ReferencePanel::reloadStyle() {
  // 面板背景用样式表：setStyleSheet 会触发重新抛光，主题切换后无需重启立即生效；
  // 选择器限定本面板类型，不影响子控件（结果树背景由 VscResultTree 自行处理）
  setStyleSheet(QStringLiteral("ReferencePanel { background-color: %1; }")
                    .arg(AuiStyle::panelBackground().name()));

  // 符号名：中等字重正文色
  m_symbolLabel->setStyleSheet(QStringLiteral("font-weight: 600; color: %1;")
                                   .arg(AuiStyle::textColor().name()));

  // 汇总：弱化色
  m_summaryLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                    .arg(AuiStyle::mutedTextColor().name()));

  update();
}

// ══════════════════════════════════════════════════════════════════════════════
//  结果构建
// ══════════════════════════════════════════════════════════════════════════════

void ReferencePanel::buildTree() {
  m_resultTree->clear();

  // 文件分组节点显示相对工作区的路径（同目录下更直观，VSCode 风格）
  QMap<QString, QTreeWidgetItem *> fileItems;
  for (const ReferenceMatch &m : m_matches) {
    QTreeWidgetItem *fileItem = fileItems.value(m.filePath);
    if (!fileItem) {
      QString rel = QDir(m_searchRoot).relativeFilePath(m.filePath);
      fileItem = m_resultTree->addFileNode(m.filePath, rel.isEmpty() ? m.filePath : rel);
      fileItems.insert(m.filePath, fileItem);
    }
    m_resultTree->addMatchNode(fileItem, m.filePath, m.line, m.column, m.length,
                               m.lineText.trimmed());
  }
  // 文件分组节点显示引用数
  for (auto it = fileItems.begin(); it != fileItems.end(); ++it) {
    it.value()->setText(0, QStringLiteral("%1 (%2)")
                               .arg(it.value()->text(0))
                               .arg(it.value()->childCount()));
  }
  m_resultTree->expandAll();
}

void ReferencePanel::updateSummary() {
  // 统计文件数（按文件路径去重）
  QSet<QString> files;
  for (const ReferenceMatch &m : m_matches) files.insert(m.filePath);
  const int fileCount = files.size();
  const int resultCount = m_matches.size();
  m_summaryLabel->setText(QStringLiteral("%1 个文件中的 %2 个结果").arg(fileCount).arg(resultCount));
}

bool ReferencePanel::shouldScanFile(const QString &filePath) const {
  // 排除常见构建/依赖/版本控制目录
  static const QStringList kSkipDirs = {
      QStringLiteral("/build/"),    QStringLiteral("/.git/"),
      QStringLiteral("/.vs/"),      QStringLiteral("/node_modules/"),
      QStringLiteral("/dist/"),     QStringLiteral("/out/"),
      QStringLiteral("/bin/"),      QStringLiteral("/obj/"),
      QStringLiteral("/.cache/"),   QStringLiteral("/cmake-build-"),
  };
  QString norm = filePath;
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

// ══════════════════════════════════════════════════════════════════════════════
//  交互
// ══════════════════════════════════════════════════════════════════════════════

void ReferencePanel::onItemClicked(QTreeWidgetItem *item, int column) {
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
