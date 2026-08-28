/**
 * @file vsc_result_panel.cpp
 * @brief VSCode 风格结果面板基类实现
 */

#include "vsc_result_panel.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QVBoxLayout>

#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/setting_store.h"

// ══════════════════════════════════════════════════════════════════════════════
//  构造 / 布局骨架
// ══════════════════════════════════════════════════════════════════════════════

VscResultPanel::VscResultPanel(QWidget *parent) : QWidget(parent) {
  m_resultTree = new VscResultTree(this);
  m_summaryLabel = new QLabel(this);
  m_summaryLabel->setTextFormat(Qt::PlainText);
}

void VscResultPanel::setupSkeleton() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  // ── 头部行：子类填充内容（搜索输入行 / 符号名行），保留合适外边距 ──
  m_headerLayout = new QHBoxLayout;
  m_headerLayout->setContentsMargins(6, 6, 6, 0);
  m_headerLayout->setSpacing(4);
  layout->addLayout(m_headerLayout);

  // ── 汇总行：汇总标签 + 弹性空间（子类可追加「全部折叠」按钮）──
  m_summaryRow = new QHBoxLayout;
  m_summaryRow->setContentsMargins(6, 0, 6, 0);
  m_summaryRow->setSpacing(4);
  m_summaryRow->addWidget(m_summaryLabel);
  m_summaryRow->addStretch(1);
  layout->addLayout(m_summaryRow);

  // ── 结果树：VSCode 风格（文件分组 → 匹配行），占满剩余空间 ──
  layout->addWidget(m_resultTree, 1);
}

// ══════════════════════════════════════════════════════════════════════════════
//  对外接口
// ══════════════════════════════════════════════════════════════════════════════

void VscResultPanel::setSearchRoot(const QString &rootPath) {
  m_searchRoot = rootPath;
}

void VscResultPanel::clearResults() {
  m_resultTree->clear();
}

void VscResultPanel::setSummaryText(const QString &text) {
  m_summaryLabel->setText(text);
}

QPushButton *VscResultPanel::collapseButton() {
  if (!m_collapseBtn) {
    m_collapseBtn = AuiButton::createCollapseAllButton();
    connect(m_collapseBtn, &QPushButton::clicked, this,
            [this]() { m_resultTree->collapseAll(); });
  }
  return m_collapseBtn;
}

// ══════════════════════════════════════════════════════════════════════════════
//  样式（主题切换后刷新汇总标签 / 结果树；面板自身背景由子类补充）
// ══════════════════════════════════════════════════════════════════════════════

void VscResultPanel::refreshStyle() {
  // 汇总标签文字色随当前主题重建（避免固化旧主题颜色）
  m_summaryLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                    .arg(AuiStyle::mutedTextColor().name()));
  // 结果树背景 / 滚动条 / 字体 / 图标随主题刷新
  m_resultTree->reloadStyle();
  update();
}

// ══════════════════════════════════════════════════════════════════════════════
//  文件扫描过滤（查找 / 引用共用）
// ══════════════════════════════════════════════════════════════════════════════

bool VscResultPanel::shouldScanFile(const QString &filePath) const {
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
//  结果构建 / 点击跳转
// ══════════════════════════════════════════════════════════════════════════════

void VscResultPanel::buildResultTree(const QVector<Match> &matches) {
  m_resultTree->clear();

  // 文件分组节点显示相对工作区的路径（同目录下更直观，VSCode 风格）
  QMap<QString, QTreeWidgetItem *> fileItems;
  for (const Match &m : matches) {
    QTreeWidgetItem *fileItem = fileItems.value(m.filePath);
    if (!fileItem) {
      QString rel = QDir(m_searchRoot).relativeFilePath(m.filePath);
      fileItem = m_resultTree->addFileNode(m.filePath, rel.isEmpty() ? m.filePath : rel);
      fileItems.insert(m.filePath, fileItem);
    }
    m_resultTree->addMatchNode(fileItem, m.filePath, m.line, m.column, m.length,
                               m.lineText.trimmed());
  }
  // 文件分组节点显示匹配数
  for (auto it = fileItems.begin(); it != fileItems.end(); ++it) {
    it.value()->setText(0, QStringLiteral("%1 (%2)")
                               .arg(it.value()->text(0))
                               .arg(it.value()->childCount()));
  }
  m_resultTree->expandAll();
}

void VscResultPanel::onResultClicked(QTreeWidgetItem *item, int column) {
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
