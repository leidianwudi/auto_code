/**
 * @file reference_panel.cpp
 * @brief 引用面板实现
 */

#include "reference_panel.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QRegularExpression>
#include <QSet>
#include <QStyle>
#include <QStyleOption>
#include <QStyledItemDelegate>
#include <QTextStream>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "comment_scan.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/setting_store.h"

/// 单个命中位置（行内匹配）
struct RefHit {
  int line = 0;     // 1-based 行号
  int column = 0;   // 0-based 匹配起始列
  QString lineText; // 整行文本
};

/// 在整段文本中查找符号的所有引用位置（跳过注释中的出现，与 VSCode 规则一致）
static QVector<RefHit> findReferencesInText(const QString &text, const QString &name) {
  QVector<RefHit> hits;
  if (name.isEmpty()) return hits;

  const QVector<QPair<int, int>> comments = collectCommentRanges(text);
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
      if (posInComments(comments, absPos)) continue;
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
//  条目绘制代理（VSCode 引用视图）
//  - 文件节点：正文色 + 加粗（相对路径 + 数量）
//  - 引用行：弱化色行号 + 正文内容
//  - 整行悬停 / 选中高亮；背景覆盖后自绘展开/收起分支
// ══════════════════════════════════════════════════════════════════════════════

class ReferenceItemDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter *p, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    const int line = index.data(Qt::UserRole + 1).toInt();
    const bool isFile = (line <= 0);
    const auto *item = static_cast<const QTreeWidgetItem *>(index.internalPointer());

    const auto *panel = qobject_cast<const ReferencePanel *>(parent());
    const bool hovered = panel && item && (panel->hoverItem() == item);

    QStyle *st = option.widget ? option.widget->style() : QApplication::style();
    const auto *tree = qobject_cast<const QTreeWidget *>(option.widget);

    // 1) 整行背景（覆盖到视口右缘，VSCode 列表交互）
    QRect rowRect = option.rect;
    if (tree && tree->viewport()) {
      rowRect.setLeft(0);
      rowRect.setRight(tree->viewport()->width() - 1);
    }
    QColor bg;
    if (option.state & QStyle::State_Selected)
      bg = AuiStyle::listSelectionBackground();
    else if (hovered)
      bg = AuiStyle::listHoverBackground();
    else
      bg = AuiStyle::panelBackground();
    p->fillRect(rowRect, bg);

    // 2) 展开/收起分支（背景覆盖了 QTreeView 的分支，需在此自绘）
    if (item && item->childCount() > 0 && tree) {
      const int indent = tree->indentation();
      QStyleOptionViewItem branchOpt = option;
      branchOpt.rect =
          QRect(option.rect.x() - indent + 4, option.rect.y(), indent, option.rect.height());
      branchOpt.state |= QStyle::State_Item | QStyle::State_Children | QStyle::State_Enabled;
      if (item->isExpanded()) branchOpt.state |= QStyle::State_Open;
      st->drawPrimitive(QStyle::PE_IndicatorBranch, &branchOpt, p, option.widget);
    }

    // 3) 文字（垂直居中）
    QFont textFont = option.font;
    if (isFile) textFont.setBold(true);
    QFontMetrics fm(textFont);
    const int baseline =
        option.rect.top() + (option.rect.height() - fm.height()) / 2 + fm.ascent();
    p->setFont(textFont);

    if (isFile) {
      const QString text = index.data(Qt::DisplayRole).toString();
      const QRect r = option.rect.adjusted(6, 0, -4, 0);
      p->setPen(AuiStyle::textColor());
      p->drawText(r.left(), baseline, fm.elidedText(text, Qt::ElideRight, r.width()));
    } else {
      // 行号（弱化色）+ 内容（正文色）
      const QString lineStr = QString::number(line);
      const QString content = index.data(Qt::UserRole + 4).toString();
      const int lineW = fm.horizontalAdvance(lineStr) + 6;
      p->setPen(AuiStyle::mutedTextColor());
      p->drawText(option.rect.left() + 6, baseline, lineStr);
      p->setPen(AuiStyle::textColor());
      const QRect r = option.rect.adjusted(6 + lineW, 0, -4, 0);
      p->drawText(r.left(), baseline, fm.elidedText(content, Qt::ElideRight, r.width()));
    }
  }

  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setHeight(qMax(20, option.fontMetrics.height() + 6));
    return s;
  }
};

// ══════════════════════════════════════════════════════════════════════════════
//  构造 / UI
// ══════════════════════════════════════════════════════════════════════════════

ReferencePanel::ReferencePanel(QWidget *parent) : QWidget(parent) {
  setupUI();
  // 主题 / 代码字体变化时刷新样式
  connect(&SettingStore::ins(), &SettingStore::fontsChanged, this,
          &ReferencePanel::reloadStyle);
  connect(&SettingStore::ins(), &SettingStore::themeChanged, this,
          &ReferencePanel::reloadStyle);
  reloadStyle();
}

void ReferencePanel::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(6, 6, 6, 6);
  layout->setSpacing(4);

  // ── 第一行：符号名 + 刷新按钮（VSCode 引用视图头部）──
  auto *headerRow = new QHBoxLayout;
  headerRow->setContentsMargins(0, 0, 0, 0);
  headerRow->setSpacing(4);

  m_symbolLabel = new QLabel;
  m_symbolLabel->setTextFormat(Qt::PlainText);
  headerRow->addWidget(m_symbolLabel, 1);

  m_refreshBtn = new QToolButton;
  m_refreshBtn->setText(QStringLiteral("\u21BB"));  // ↻ 刷新
  m_refreshBtn->setToolTip(QStringLiteral("刷新"));
  m_refreshBtn->setFixedSize(22, 22);
  m_refreshBtn->setAutoRaise(true);
  m_refreshBtn->setCursor(Qt::PointingHandCursor);
  connect(m_refreshBtn, &QToolButton::clicked, this, &ReferencePanel::onRefresh);
  headerRow->addWidget(m_refreshBtn);

  // ── 第二行：汇总标签 ──
  m_summaryLabel = new QLabel;

  // ── 结果树：单列，条目由 ReferenceItemDelegate 自绘（VSCode 引用视图）──
  m_resultTree = new QTreeWidget;
  m_resultTree->setColumnCount(1);
  m_resultTree->setHeaderHidden(true);
  m_resultTree->setRootIsDecorated(true);
  m_resultTree->setAlternatingRowColors(false);
  m_resultTree->setUniformRowHeights(true);
  m_resultTree->setTextElideMode(Qt::ElideRight);
  m_resultTree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_resultTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_resultTree->setItemDelegate(new ReferenceItemDelegate(this));
  // 纯代码 delegate 不自带 State_MouseOver，这里手动追踪悬停并整行高亮
  m_resultTree->viewport()->setMouseTracking(true);
  m_resultTree->viewport()->installEventFilter(this);
  connect(m_resultTree, &QTreeWidget::itemClicked, this, &ReferencePanel::onItemClicked);

  layout->addLayout(headerRow);
  layout->addWidget(m_summaryLabel);
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
  m_hoverItem = nullptr;
  updateSummary();
}

// ══════════════════════════════════════════════════════════════════════════════
//  样式（VSCode 引用视图：主题背景 / 弱化行号 / 悬停选中高亮）
// ══════════════════════════════════════════════════════════════════════════════

void ReferencePanel::reloadStyle() {
  // 面板整体背景（VSCode 浅色引用视图为白色，跟随主题面板背景）
  QPalette pal = palette();
  pal.setColor(QPalette::Window, AuiStyle::panelBackground());
  setAutoFillBackground(true);
  setPalette(pal);

  // 字体跟随「代码字体」设置（与问题面板一致）
  QFont f = font();
  const QString fam = SettingStore::ins().fontFamily(QStringLiteral("font.code"));
  if (!fam.isEmpty()) f.setFamily(fam);
  f.setPointSize(SettingStore::ins().fontSize(QStringLiteral("font.code")));
  setFont(f);

  // 符号名：中等字重正文色
  m_symbolLabel->setStyleSheet(QStringLiteral("font-weight: 600; color: %1;")
                                   .arg(AuiStyle::textColor().name()));

  // 汇总：弱化色
  m_summaryLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                    .arg(AuiStyle::mutedTextColor().name()));

  // 刷新按钮：扁平 + 主题 hover（对齐 AuiStyle 菜单按钮风格）
  m_refreshBtn->setStyleSheet(
      QStringLiteral("QToolButton { color: %1; background: transparent; "
                     "border: 1px solid transparent; border-radius: 3px; }"
                     "QToolButton:hover { background: %2; border: 1px solid %3; }")
          .arg(AuiStyle::textColor().name(), AuiStyle::hoverBackground().name(),
               AuiStyle::borderColor().name()));

  // 结果树：主题背景 / 文字（悬停与选中由 ReferenceItemDelegate 绘制）
  m_resultTree->setStyleSheet(QStringLiteral("QTreeWidget { background: %1; color: %2; "
                                             "border: none; }")
                                  .arg(AuiStyle::panelBackground().name(),
                                       AuiStyle::textColor().name()));

  // 重建结果树以应用新主题 / 新字体下的颜色
  if (!m_symbolName.isEmpty()) buildTree();
  update();
}

// ══════════════════════════════════════════════════════════════════════════════
//  结果构建
// ══════════════════════════════════════════════════════════════════════════════

void ReferencePanel::buildTree() {
  m_resultTree->clear();
  m_hoverItem = nullptr;

  // 文件节点显示相对工作区的路径（同目录下更直观，VSCode 风格）
  QMap<QString, QTreeWidgetItem *> fileItems;
  for (const ReferenceMatch &m : m_matches) {
    QTreeWidgetItem *fileItem = fileItems.value(m.filePath);
    if (!fileItem) {
      fileItem = new QTreeWidgetItem(m_resultTree);
      QString rel = QDir(m_searchRoot).relativeFilePath(m.filePath);
      fileItem->setText(0, rel.isEmpty() ? m.filePath : rel);
      fileItem->setData(0, Qt::UserRole, m.filePath);
      fileItem->setData(0, Qt::UserRole + 1, 0);  // 文件节点：line=0
      fileItem->setToolTip(0, m.filePath);
      fileItems.insert(m.filePath, fileItem);
    }
    QTreeWidgetItem *lineItem = new QTreeWidgetItem(fileItem);
    lineItem->setText(0, QStringLiteral("%1: %2").arg(m.line).arg(m.lineText.trimmed()));
    lineItem->setData(0, Qt::UserRole, m.filePath);
    lineItem->setData(0, Qt::UserRole + 1, m.line);
    lineItem->setData(0, Qt::UserRole + 2, m.column);
    lineItem->setData(0, Qt::UserRole + 3, m.length);
    lineItem->setData(0, Qt::UserRole + 4, m.lineText.trimmed());  // 供代理绘制内容
  }
  // 文件节点显示引用数
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

bool ReferencePanel::eventFilter(QObject *obj, QEvent *event) {
  // 追踪鼠标悬停的条目，供条目绘制代理做整行高亮
  if (obj == m_resultTree->viewport()) {
    if (event->type() == QEvent::MouseMove) {
      auto *me = static_cast<QMouseEvent *>(event);
      QTreeWidgetItem *item = m_resultTree->itemAt(me->position().toPoint());
      if (item != m_hoverItem) {
        m_hoverItem = item;
        m_resultTree->viewport()->update();
      }
      return false;
    }
    if (event->type() == QEvent::Leave) {
      if (m_hoverItem) {
        m_hoverItem = nullptr;
        m_resultTree->viewport()->update();
      }
      return false;
    }
  }
  return QWidget::eventFilter(obj, event);
}

void ReferencePanel::onRefresh() {
  findReferences(m_symbolName);
}

void ReferencePanel::onItemClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  if (!item) return;
  // 仅结果行（有行号）触发跳转；文件节点不跳转
  const int line = item->data(0, Qt::UserRole + 1).toInt();
  if (line <= 0) return;
  const QString filePath = item->data(0, Qt::UserRole).toString();
  const int col = item->data(0, Qt::UserRole + 2).toInt();
  const int len = item->data(0, Qt::UserRole + 3).toInt();
  emit openRequested(filePath, line, col, len);
}
