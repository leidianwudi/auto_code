/**
 * @file reference_panel.cpp
 * @brief 引用面板实现
 */

#include "reference_panel.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
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

ReferencePanel::ReferencePanel(QWidget *parent) : VscResultPanel(parent) {
  setupUI();
  // 主题变化时刷新头部样式（结果树样式由基类 refreshStyle 处理）
  connect(&SettingStore::ins(), &SettingStore::themeChanged, this,
          &ReferencePanel::refreshStyle);
  refreshStyle();
}

void ReferencePanel::setupUI() {
  // 主布局骨架：头部行 + 汇总行 + 结果树（由基类创建）
  setupSkeleton();

  // ── 头部行：符号名 + 全部折叠按钮（VSCode 引用视图头部）──
  m_symbolLabel = new QLabel;
  m_symbolLabel->setTextFormat(Qt::PlainText);
  headerLayout()->addWidget(m_symbolLabel, 1);
  headerLayout()->addWidget(collapseButton());

  // ── 结果树：itemClicked → 基类统一跳转处理（文件分组节点不跳转）──
  connect(resultTree(), &QTreeWidget::itemClicked, this, &ReferencePanel::onResultClicked);
}

// ══════════════════════════════════════════════════════════════════════════════
//  对外接口
// ══════════════════════════════════════════════════════════════════════════════

void ReferencePanel::findReferences(const QString &symbolName) {
  m_symbolName = symbolName;
  m_matches.clear();
  m_symbolLabel->setText(symbolName);

  if (symbolName.isEmpty() || searchRoot().isEmpty()) {
    buildResultTree(m_matches);
    updateSummary();
    return;
  }

  // 遍历工作区文件，逐文件扫描符号引用（跳过注释）
  QDirIterator it(searchRoot(), QDir::Files,
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
      Match m;
      m.filePath = filePath;
      m.line = h.line;
      m.column = h.column;
      m.length = symbolName.size();
      m.lineText = h.lineText;
      m_matches.append(m);
    }
  }

  buildResultTree(m_matches);
  updateSummary();
}

void ReferencePanel::clear() {
  m_symbolName.clear();
  m_symbolLabel->clear();
  m_matches.clear();
  clearResults();
  updateSummary();
}

// ══════════════════════════════════════════════════════════════════════════════
//  样式（面板背景 / 头部标签；结果树由基类统一处理）
// ══════════════════════════════════════════════════════════════════════════════

void ReferencePanel::refreshStyle() {
  // 面板背景用样式表：setStyleSheet 会触发重新抛光，主题切换后无需重启立即生效；
  // 选择器限定本面板类型，不影响子控件（结果树背景由 VscResultTree 自行处理）
  setStyleSheet(QStringLiteral("ReferencePanel { background-color: %1; }")
                    .arg(AuiStyle::panelBackground().name()));

  // 符号名：中等字重正文色
  m_symbolLabel->setStyleSheet(QStringLiteral("font-weight: 600; color: %1;")
                                   .arg(AuiStyle::textColor().name()));

  // 汇总标签 / 结果树由基类统一刷新
  VscResultPanel::refreshStyle();

  // 强制重新抛光 + 重绘：主题切换后 QStyleSheetStyle 下仅靠 update() 不会
  // 重算背景，必须 unpolish/polish 让面板重新读取新调色板
  style()->unpolish(this);
  style()->polish(this);
  update();
}

// ══════════════════════════════════════════════════════════════════════════════
//  汇总
// ══════════════════════════════════════════════════════════════════════════════

void ReferencePanel::updateSummary() {
  // 统计文件数（按文件路径去重）
  QSet<QString> files;
  for (const Match &m : m_matches) files.insert(m.filePath);
  const int fileCount = files.size();
  const int resultCount = m_matches.size();
  setSummaryText(QStringLiteral("%1 个文件中的 %2 个结果").arg(fileCount).arg(resultCount));
}
