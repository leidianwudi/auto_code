/**
 * @file reference_panel.cpp
 * @brief 引用面板实现
 */

#include "reference_panel.h"

#include <QDir>
#include <QFile>
#include <QFuture>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QVBoxLayout>

#include <QtConcurrent/QtConcurrent>

#include "comment_scan.h"
#include "src/util/common/workspace_iter.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/setting_store.h"

/// 读取文件文本（UTF-8）
static QString readFileText(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
  QTextStream in(&f);
  return in.readAll();
}

/// 后台线程执行：语义收集引用（作用域 + 类型推断）+ 转为 Match（含行文本）。
/// 纯计算，不访问 UI；供 QtConcurrent::run 调用。
/// liveContents 由主线程构建并拷贝进工作线程（已打开编辑器 + 缓冲文件内容）。
struct ReferenceResult {
  QVector<RenameRef> refs;                  ///< 语义引用位置
  QVector<ReferencePanel::Match> matches;   ///< 结果树展示用
};
static ReferenceResult collectReferenceMatches(const QString &root, const QString &filePath,
                                               int line, int column, const QString &name,
                                               QHash<QString, QString> liveContents) {
  ReferenceResult res;
  res.refs = collectSymbolReferencesLive(root, filePath, line, column, name, liveContents);
  QHash<QString, QStringList> lineCache;  // 文件 → 行列表（缓存避免重复读）
  for (const RenameRef &r : res.refs) {
    if (!lineCache.contains(r.filePath)) {
      // 行文本同样优先缓冲（与引用收集同一份内容，避免磁盘旧内容）
      QString src = liveContents.value(r.filePath);
      if (src.isEmpty()) src = readFileText(r.filePath);
      lineCache.insert(r.filePath, src.split(QLatin1Char('\n')));
    }
    const QStringList &lines = lineCache.value(r.filePath);
    ReferencePanel::Match m;
    m.filePath = r.filePath;
    m.line = r.line;
    m.column = r.column;
    m.length = r.length;
    if (r.line >= 1 && r.line <= lines.size()) m.lineText = lines[r.line - 1];
    res.matches.append(m);
  }
  return res;
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

void ReferencePanel::findReferences(const QString &filePath, int line, int column,
                                    const QString &symbolName) {
  m_symbolName = symbolName;
  m_matches.clear();
  m_semanticRefs.clear();
  m_symbolLabel->setText(symbolName);

  // 新一轮扫描：递增请求序号，过期的后台扫描结果将被丢弃
  const int requestId = ++m_scanRequestId;

  if (symbolName.isEmpty() || searchRoot().isEmpty()) {
    buildResultTree(m_matches);
    updateSummary();
    return;
  }

  // 清空旧结果并提示扫描中（结果由后台线程完成后回填）
  clearResults();
  setSummaryText(QStringLiteral("正在查找 %1 的引用...").arg(symbolName));

  // 后台线程语义收集（作用域 + 类型推断），完成后回主线程建树。
  // 每次新建 watcher：QFutureWatcher 不能在旧 future 未完成时 setFuture 复用
  // 主线程先构建实时内容快照（已打开编辑器 + 缓冲文件），拷贝进工作线程按值使用
  const QHash<QString, QString> liveContents =
      m_liveContentProvider ? m_liveContentProvider() : QHash<QString, QString>();
  auto *watcher = new QFutureWatcher<ReferenceResult>(this);
  connect(watcher, &QFutureWatcher<ReferenceResult>::finished, this,
          [this, watcher, requestId]() {
            watcher->deleteLater();
            if (requestId != m_scanRequestId) return;  // 过期结果丢弃
            const ReferenceResult res = watcher->result();
            m_semanticRefs = res.refs;
            m_matches = res.matches;
            buildResultTree(m_matches);
            updateSummary();
            emit referencesReady(res.refs);
          });
  watcher->setFuture(QtConcurrent::run(collectReferenceMatches, searchRoot(), filePath, line,
                                       column, symbolName, liveContents));
}

void ReferencePanel::clear() {
  ++m_scanRequestId;  // 作废进行中的后台扫描，防止结果回填
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
