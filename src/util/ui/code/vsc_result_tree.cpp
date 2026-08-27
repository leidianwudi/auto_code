/**
 * @file vsc_result_tree.cpp
 * @brief VSCode 风格结果树控件实现
 */

#include "vsc_result_tree.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QStyle>
#include <QStyleOption>
#include <QStyledItemDelegate>
#include <QtMath>

#include "src/util/ui/component/aui_icon.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/setting_store.h"

// ══════════════════════════════════════════════════════════════════════════════
//  结果树布局间距（统一在此调整，方便自定义）
// ══════════════════════════════════════════════════════════════════════════════
namespace {
/// 内容（文件图标 / 行号）相对行左边缘的缩进（px）
constexpr int kContentIndent = 0;
/// 折叠箭头右缘与文件小图标左缘的间隔（px）
constexpr int kArrowIconGap = 0;
/// 文件小图标与文件名的间隔（px）
constexpr int kIconTextGap = 0;
/// 匹配行行号与内容的间隔（px）
constexpr int kLineTextGap = 2;
/// 文本右侧留白（px）
constexpr int kTextRightMargin = 4;
/// 折叠箭头开口张角（与代码编辑框一致）
constexpr qreal kFoldArrowAngle = 100.0;
/// 折叠箭头左侧与边框的外间距（px）
constexpr int kArrowLeftMargin = 4;
/// 期望箭头宽度（px；实际受可用空间约束，kTreeIndent 需 ≥ 左间距+箭头宽+图标间距 才显示满宽）
constexpr int kArrowGutterWidth = 6;
/// 子节点（匹配行/行号所在行）缩进（px，比默认 20 紧凑）
constexpr int kTreeIndent = 12;
}  // namespace

/// 结果树图标尺寸：随字体高度缩放（文字变大，折叠箭头 / 文件小图标随之变大）
static int scaledIconSize(int fontHeight) {
  return qMax(12, fontHeight - 2);
}

/// 折叠箭头宽度：默认字体下 = kArrowGutterWidth（直接生效）；
/// 随字体变大与文件小图标同步增大（scaledIconSize 基准尺寸 12px）
static int scaledArrowWidth(int fontHeight) {
  return qMax(2, kArrowGutterWidth + (scaledIconSize(fontHeight) - 12));
}

// ══════════════════════════════════════════════════════════════════════════════
//  条目绘制代理（VSCode 结果视图）
//  - 文件分组节点：文件类型图标 + 加粗正文色（相对路径 + 数量）
//  - 匹配行：弱化色行号 + 正文内容
//  - 整行悬停 / 选中高亮；背景覆盖后自绘居中 VSCode 折叠箭头
// ══════════════════════════════════════════════════════════════════════════════

class VscResultItemDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter *p, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    const int line = index.data(VscTreeRole::Line).toInt();
    const bool isFile = (line <= 0);
    const auto *item = static_cast<const QTreeWidgetItem *>(index.internalPointer());

    const auto *tree = qobject_cast<const VscResultTree *>(option.widget);
    const bool hovered = tree && item && (tree->hoverItem() == item);

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
      bg = option.palette.color(QPalette::Base);  // 与目录树一致：跟随全局调色板，主题自动切换
    p->fillRect(rowRect, bg);

    // 文件小图标尺寸随字体缩放（文字变大，图标随之变大）
    const int iconSize = scaledIconSize(option.fontMetrics.height());

    // 2) 文字基线（垂直居中）
    QFont textFont = option.font;
    if (isFile) textFont.setBold(true);
    QFontMetrics fm(textFont);
    const int baseline =
        option.rect.top() + (option.rect.height() - fm.height()) / 2 + fm.ascent();
    p->setFont(textFont);

    // 内容起点统一为内容缩进（文件图标紧贴折叠箭头、行号紧贴行左，整体更紧凑）
    int textX = option.rect.left() + kContentIndent;
    if (isFile) {
      // 文件类型小图标（尺寸随字体缩放，与文字垂直居中）
      const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
      if (!icon.isNull()) {
        const QIcon::Mode mode =
            (option.state & QStyle::State_Selected) ? QIcon::Selected : QIcon::Normal;
        icon.paint(p, QRect(textX, option.rect.center().y() - iconSize / 2, iconSize, iconSize),
                   Qt::AlignCenter, mode);
        textX += iconSize + kIconTextGap;  // 图标与文件名间隔
      }
      const QString text = index.data(Qt::DisplayRole).toString();
      const QRect r(textX, option.rect.top(), option.rect.right() - textX - kTextRightMargin,
                    option.rect.height());
      p->setPen(AuiStyle::textColor());
      p->drawText(r, Qt::AlignVCenter | Qt::AlignLeft,
                  fm.elidedText(text, Qt::ElideRight, r.width()));
    } else {
      // 行号（弱化色）+ 内容（正文色），行号与内容间隔
      const QString lineStr = QString::number(line);
      const QString content = index.data(VscTreeRole::Content).toString();
      const int lineNumW = fm.horizontalAdvance(lineStr);
      p->setPen(AuiStyle::mutedTextColor());
      p->drawText(textX, baseline, lineStr);
      p->setPen(AuiStyle::textColor());
      const int contentLeft = textX + lineNumW + kLineTextGap;
      const QRect r(contentLeft, option.rect.top(),
                    option.rect.right() - contentLeft - kTextRightMargin, option.rect.height());
      p->drawText(r, Qt::AlignVCenter | Qt::AlignLeft,
                  fm.elidedText(content, Qt::ElideRight, r.width()));
    }

    // 3) 折叠箭头（与代码编辑框收起/展开箭头同款 100° 张角）。
    //    箭头铺满可用空间 [kArrowLeftMargin, 图标左缘 - kArrowIconGap]：
    //    kArrowIconGap / kArrowLeftMargin 分别控制右/左边界，均即时生效；
    //    kArrowGutterWidth 为期望箭头宽度（受可用空间约束，避免截断）。
    if (item && item->childCount() > 0) {
      const QColor chevron =
          (hovered || (option.state & QStyle::State_Selected)) ? AuiStyle::textColor()
                                                               : AuiStyle::mutedTextColor();
      const qreal half = qDegreesToRadians(kFoldArrowAngle / 2.0);
      const qreal iconLeft = option.rect.left() + kContentIndent;
      // 可用空间 = 图标左缘 - 左外间距 - 图标间距；箭头期望宽度随字体缩放
      const qreal avail = iconLeft - kArrowLeftMargin - kArrowIconGap;
      const qreal arrowWidth =
          qMin<qreal>(scaledArrowWidth(option.fontMetrics.height()), qMax<qreal>(2.0, avail));
      const qreal len = arrowWidth / (2.0 * std::cos(half));
      const qreal lx = len * std::cos(half);
      const qreal ly = len * std::sin(half);
      const qreal left = kArrowLeftMargin;
      const qreal rightEdge = left + arrowWidth;
      const qreal tipX = item->isExpanded() ? left + lx : rightEdge - lx / 2.0;
      const QPointF tip(tipX, option.rect.center().y() + (item->isExpanded() ? ly / 2.0 : 0.0));
      AuiStyle::drawFoldArrow(*p, tip, item->isExpanded(), chevron, len, kFoldArrowAngle);
    }
  }

  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setHeight(qMax(20, option.fontMetrics.height() + 6));
    return s;
  }
};

// ══════════════════════════════════════════════════════════════════════════════
//  VscResultTree
// ══════════════════════════════════════════════════════════════════════════════

VscResultTree::VscResultTree(QWidget *parent) : QTreeWidget(parent) {
  setColumnCount(1);
  setHeaderHidden(true);
  setFrameShape(QFrame::NoFrame);  // 无边框，避免深色主题下残留浅色边框
  setFrameShadow(QFrame::Plain);
  setIndentation(kTreeIndent);  // 子节点缩进（统一变量，便于调整）
  setRootIsDecorated(true);
  setAlternatingRowColors(false);
  setUniformRowHeights(true);
  setTextElideMode(Qt::ElideRight);
  setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setItemDelegate(new VscResultItemDelegate(this));
  // 纯代码 delegate 不自带 State_MouseOver，开启鼠标跟踪 + viewportEvent 手动追踪悬停
  viewport()->setMouseTracking(true);

  // 主题 / 代码字体变化时刷新样式
  connect(&SettingStore::ins(), &SettingStore::fontsChanged, this, &VscResultTree::reloadStyle);
  connect(&SettingStore::ins(), &SettingStore::themeChanged, this, &VscResultTree::reloadStyle);
  reloadStyle();
}

void VscResultTree::reloadStyle() {
  m_hoverItem = nullptr;

  // 字体跟随「目录树字体」设置（与文件面板目录树一致，大小可随设置调整）
  QFont f = font();
  const QString fam = SettingStore::ins().fontFamily(QStringLiteral("font.tree"));
  if (!fam.isEmpty()) f.setFamily(fam);
  f.setPointSize(SettingStore::ins().fontSize(QStringLiteral("font.tree")));
  setFont(f);

  // 子节点缩进随字体缩放（文字变大，箭头/图标随之变大且缩进槽同步加宽以容纳箭头）
  const QFontMetrics fm(f);
  const int indent = qMax(kTreeIndent, scaledArrowWidth(fm.height()) + kArrowLeftMargin + kArrowIconGap);
  setIndentation(indent);

  // 背景 / 滚动条与目录树（TreeDir）保持一致：
  // 不在此设置 per-widget 样式表 / 调色板，行背景由绘制代理读取
  // option.palette.color(QPalette::Base)（跟随全局调色板），滚动条走全局样式表，
  // 主题切换时由 applyGlobalStyle 的全局重抛/重绘自动刷新。
  viewport()->update();
  update();

  // 文件类型图标颜色随主题刷新
  refreshFileIcons();
}

void VscResultTree::refreshFileIcons() {
  // 为所有文件分组节点（line==0）按后缀重建类型图标（颜色随当前主题）
  QList<QTreeWidgetItem *> stack;
  for (int i = 0; i < topLevelItemCount(); ++i) stack.append(topLevelItem(i));
  while (!stack.isEmpty()) {
    QTreeWidgetItem *item = stack.takeLast();
    for (int i = 0; i < item->childCount(); ++i) stack.append(item->child(i));
    if (item->data(0, VscTreeRole::Line).toInt() <= 0) {
      const QString path = item->data(0, VscTreeRole::FilePath).toString();
      if (!path.isEmpty())
        item->setIcon(0, AuiIcon::createFileTypeIcon(QFileInfo(path).suffix(),
                                                     scaledIconSize(fontMetrics().height())));
    }
  }
}

QTreeWidgetItem *VscResultTree::addFileNode(const QString &filePath, const QString &displayText) {
  auto *item = new QTreeWidgetItem(this);
  item->setText(0, displayText);
  item->setData(0, VscTreeRole::FilePath, filePath);
  item->setData(0, VscTreeRole::Line, 0);  // 文件分组节点：line=0
  item->setIcon(0, AuiIcon::createFileTypeIcon(QFileInfo(filePath).suffix(),
                                               scaledIconSize(fontMetrics().height())));
  item->setToolTip(0, filePath);
  return item;
}

QTreeWidgetItem *VscResultTree::addMatchNode(QTreeWidgetItem *fileNode, const QString &filePath,
                                             int line, int column, int length,
                                             const QString &content) {
  auto *item = new QTreeWidgetItem(fileNode);
  item->setData(0, VscTreeRole::FilePath, filePath);
  item->setData(0, VscTreeRole::Line, line);
  item->setData(0, VscTreeRole::Column, column);
  item->setData(0, VscTreeRole::Length, length);
  item->setData(0, VscTreeRole::Content, content);
  return item;
}

bool VscResultTree::viewportEvent(QEvent *event) {
  // 手动追踪悬停条目：Qt 纯代码 delegate 不会自动携带 State_MouseOver
  if (event->type() == QEvent::MouseMove) {
    auto *me = static_cast<QMouseEvent *>(event);
    QTreeWidgetItem *item = itemAt(me->position().toPoint());
    if (item != m_hoverItem) {
      m_hoverItem = item;
      viewport()->update();
    }
  } else if (event->type() == QEvent::Leave) {
    if (m_hoverItem) {
      m_hoverItem = nullptr;
      viewport()->update();
    }
  }
  return QTreeWidget::viewportEvent(event);
}
