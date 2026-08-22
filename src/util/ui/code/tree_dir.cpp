/**
 * @file tree_dir.cpp
 * @brief 可打勾的文件树控件实现
 */

#include "tree_dir.h"

#include <QApplication>
#include <QAbstractItemModel>
#include <QContextMenuEvent>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QIcon>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>

#include "src/engine/ac_language.h"
#include "src/ui/create/create_mgr.h"
#include "src/util/common/code_constants.h"
#include "src/util/common/util_file.h"
#include "src/util/ui/component/aui_icon.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/rename_dialog.h"
#include "src/util/ui/setting_store.h"

/// 检查文件路径是否为 JSON 类型（.json 或 .jsonvue）
static inline bool isJsonLike(const QString &path) {
  return path.endsWith(AcFileSuffix::kJson, Qt::CaseInsensitive) ||
         path.endsWith(AcFileSuffix::kJsonvue, Qt::CaseInsensitive);
}

// 静态辅助函数声明（定义在下方，供上方成员函数使用）
static void collectCheckedRelRecursive(QTreeWidgetItem *item, const QString &rootPath,
                                       QStringList &rel);
static QString buildRelativePath(QTreeWidgetItem *item);
static void buildPathMap(QTreeWidgetItem *item, const QString &parentRel,
                         QHash<QString, QTreeWidgetItem *> &map);

// ──────────────────────────────────────────────────────────────
//  ModifiedFileDelegate 实现
// ──────────────────────────────────────────────────────────────
void ModifiedFileDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const {
  int errorCount = index.data(Qt::UserRole + 3).toInt();
  bool modified = index.data(Qt::UserRole + 2).toBool();
  const TreeDir *tree = qobject_cast<const TreeDir *>(parent());
  QStyle *st = option.widget ? option.widget->style() : QApplication::style();

  // 整行矩形：覆盖整个视口宽度（左 0 → 右 viewport 右缘），
  // 保证选中/悬停背景连左右空白区也变色（Trae IDE 风格）
  QRect rowRect = option.rect;
  if (tree && tree->viewport()) {
    rowRect.setLeft(0);
    rowRect.setRight(tree->viewport()->width() - 1);
  }

  // 1) 背景：始终填充整行（覆盖 QTreeView 自绘的分支/交替背景，避免箭头与文字重影）。
  //    选中态 → 选中背景；悬停态 → hover 高亮；否则沿用树视图默认背景。
  //    悬停节点由 TreeDir::viewportEvent 实时记录在 hoverItem() 中。
  const bool hovered =
      tree && (tree->hoverItem() == static_cast<const QTreeWidgetItem *>(index.internalPointer()));
  QColor bgColor;
  if (option.state & QStyle::State_Selected) {
    bgColor = AuiStyle::listSelectionBackground();
  } else if (hovered) {
    bgColor = AuiStyle::listHoverBackground();
  } else {
    bgColor = option.palette.color(QPalette::Base);  // 与树视图默认背景一致
  }
  painter->fillRect(rowRect, bgColor);

  // 2) 展开/收起三角（分支指示符）：背景覆盖了 QTreeView 的分支，需在此自绘。
  //    注意：option.rect 是缩进后的内容区（x = indent*(level+1)），箭头左边缘应
  //    在缩进区域的最内层（option.rect.x() - indent + 4），不能从内容区起点算起，
  //    否则箭头会偏右画到图标/文本区域造成重影。
  if (index.model() && index.model()->hasChildren(index)) {
    const int indent = tree ? tree->indentation() : 16;
    const auto *item = static_cast<const QTreeWidgetItem *>(index.internalPointer());
    QStyleOptionViewItem branchOpt = option;
    // 箭头矩形位于该行缩进区域的最内层；右移 3px 收窄与右侧复选框的间隔
    branchOpt.rect = QRect(option.rect.x() - indent + 4, option.rect.y(), indent,
                           option.rect.height());
    branchOpt.state |= QStyle::State_Item | QStyle::State_Children | QStyle::State_Enabled;
    if (item && item->isExpanded()) branchOpt.state |= QStyle::State_Open;
    st->drawPrimitive(QStyle::PE_IndicatorBranch, &branchOpt, painter, option.widget);
  }

  // 3) 复选框自绘：有勾选指示时用原生风格绘制三态复选框
  QRect checkRect;
  if (index.data(Qt::CheckStateRole).isValid()) {
    QStyleOptionViewItem checkOpt = option;
    initStyleOption(&checkOpt, index);
    checkRect =
        st->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &checkOpt, option.widget);
    QStyleOptionButton cb;
    cb.rect = checkRect;
    cb.state = QStyle::State_Enabled;
    // 文件节点（非文件夹）只应二态（勾选/不勾选），不应出现"部分选中"小方块。
    // 正常逻辑下 PartiallyChecked 只设到文件夹（聚合子文件状态），这里防御性处理，
    // 即使数据异常也强制文件节点按未勾选绘制，避免 jsonvue 等文件显示三态。
    int cs = index.data(Qt::CheckStateRole).toInt();
    const bool nodeIsFolder = index.data(Qt::UserRole + 1).toString().isEmpty();
    if (!nodeIsFolder && cs == Qt::PartiallyChecked) cs = Qt::Unchecked;
    if (cs == Qt::Checked)
      cb.state |= QStyle::State_On;
    else if (cs == Qt::PartiallyChecked)
      cb.state |= QStyle::State_NoChange;  // 部分勾选：小方块
    st->drawPrimitive(QStyle::PE_IndicatorCheckBox, &cb, painter, option.widget);
  }
  // 复选框右缘到图标左缘的间隔（px）：文件夹与 json 文件保持一致。
  // json 文件图标是居中字母（左侧有透明留白），视觉上比文件夹图形显得更远，
  // 因此 json 文件额外收紧一点，使"复选框→图标"的视觉间隔与文件夹一致
  static constexpr int kCheckToIconGap = 4;
  const int checkWidth = checkRect.isValid() ? checkRect.width() + kCheckToIconGap : 0;

  // 4) 字体（与普通节点一致，不加粗、字号不变）
  QFont textFont = tree ? tree->font() : option.font;
  QFontMetrics fm(textFont);

  // 5) 布局：复选框 → 图标 → 文本
  // 文件夹节点 UserRole+1 为空，文件节点非空（图标/文本间距、图标偏移据此区分）
  const bool isFolder = index.data(Qt::UserRole + 1).toString().isEmpty();
  QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
  int maxIconH = option.decorationSize.height();
  if (maxIconH <= 0) maxIconH = 16;
  // 按图标实际尺寸绘制（默认 16x16），高度不超过行图标高度
  QSize actual = icon.isNull() ? QSize(0, 0) : icon.actualSize(QSize(64, maxIconH));
  int decoW = actual.width() > 0 ? actual.width() : option.decorationSize.width();
  int decoH = actual.height() > 0 ? qMin(actual.height(), maxIconH) : maxIconH;
  // 文件夹图形占满图标左侧，字母图标左侧留白多 → json 等字母图标额外左移 2px 对齐视觉间隔
  int iconX = option.rect.left() + checkWidth + (isFolder ? 0 : -2);
  int iconY = option.rect.top() + (option.rect.height() - decoH) / 2;
  QRect iconRect(iconX, iconY, decoW, decoH);

  // 启动项标记：在图标左侧空隙绘制绿色右向三角（仅移动三角，不移动图标文字位置）
  const bool isStartup = index.data(kTreeStartupRole).toBool();
  if (isStartup) {
    const int triW = kTreeStartupTriWidth;
    const int cy = iconRect.center().y();
    const int triHalfH = qRound(maxIconH * 0.45);    // 高度约为图标高度的 90%
    const int x0 = qMax(1, iconRect.left() - triW);  // 防止贴住视口左缘时被裁剪
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(AuiStyle::compileButtonColor());  // 绿色填充
    QPolygon tri;
    tri << QPoint(x0, cy - triHalfH) << QPoint(x0, cy + triHalfH) << QPoint(iconRect.left(), cy);
    painter->drawPolygon(tri);
    painter->restore();
  }

  if (!icon.isNull()) {
    QIcon::Mode mode = (option.state & QStyle::State_Selected) ? QIcon::Selected : QIcon::Normal;
    icon.paint(painter, iconRect, Qt::AlignCenter, mode);
  }

  // 文本起始：文件夹与名称间距加大（更易辨认），文件与名称间距更紧凑
  int textX = iconRect.right() + (isFolder ? 3 : -1);
  int textBaseline = option.rect.top() + (option.rect.height() - fm.height()) / 2 + fm.ascent();

  painter->save();
  painter->setFont(textFont);

  QString text = index.data(Qt::DisplayRole).toString();

  if (errorCount > 0) {
    // 错误态：文件名红色（字号与普通一致），错误数量绘制在行最右侧，
    // 锚定 option.rect 右缘，随目录宽度拖动始终贴右显示
    painter->save();
    painter->setFont(textFont);
    painter->setPen(AuiStyle::errorTextColor());
    // 文件名右边界为错误数量徽章预留位置，过长时省略号裁剪避免与徽章重叠
    const QString countText = QStringLiteral("%1").arg(errorCount);
    const int countWidth = fm.horizontalAdvance(countText);
    const int textMaxX = option.rect.right() - countWidth - 14;  // 8px 右边距 + 6px 文字留白
    painter->drawText(textX, textBaseline,
                      fm.elidedText(text, Qt::ElideRight, qMax(0, textMaxX - textX)));
    painter->restore();

    // 错误数量徽章（红色，右对齐到行最右缘）
    painter->save();
    painter->setFont(textFont);
    painter->setPen(AuiStyle::errorTextColor());
    painter->drawText(option.rect.adjusted(-8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignRight,
                      countText);
    painter->restore();
  } else {
    // 正常 / 修改态文字颜色：修改的节点以琥珀色显示（VSCode 风格，替代原来的实心圆点），
    // 未修改跟随选中态；错误态优先（红色），两者并存时显示错误色
    QPalette::ColorRole role =
        (option.state & QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text;
    QColor textColor = option.palette.color(role);
    if (modified) textColor = AuiStyle::modifiedTextColor();
    painter->setPen(textColor);
    painter->drawText(textX, textBaseline, text);
    painter->restore();
  }
}

// ============================================================================
// 构造
// ============================================================================

TreeDir::TreeDir(QWidget *parent) : QTreeWidget(parent) {
  // 宿主左侧已有「文件」tab，隐藏树目录自身的「文件」表头
  setHeaderHidden(true);
  // 宿主左侧面板自带边框，文件树不再绘制默认 frame 边框/外边距
  setFrameShape(QFrame::NoFrame);
  setMinimumWidth(kMinWidth);
  // 不设最大宽度，允许用户拖动分隔条自由加宽目录树
  setAnimated(false);
  // 每层缩进 12px（原 16 偏大），顶层三角箭头左边空白与层级缩进同时收窄
  setIndentation(12);
  setSortingEnabled(false);
  // 树目录上的节点可点击展开/选中/勾选，悬停时显示手型光标
  viewport()->setCursor(Qt::PointingHandCursor);
  // 开启鼠标跟踪，使 viewportEvent 能实时收到 MouseMove/Leave 实现悬停整行高亮
  viewport()->setAttribute(Qt::WA_MouseTracking, true);

  setItemDelegate(new ModifiedFileDelegate(this));

  // 保存防抖定时器：复选框频繁变化时合并写入，避免每次勾选都落盘
  m_saveTimer = new QTimer(this);
  m_saveTimer->setSingleShot(true);
  m_saveTimer->setInterval(300);
  connect(m_saveTimer, &QTimer::timeout, this, &TreeDir::saveState);

  connect(this, &QTreeWidget::itemClicked, this, &TreeDir::onItemClicked);
  connect(this, &QTreeWidget::itemDoubleClicked, this, &TreeDir::onItemDoubleClicked);
  connect(this, &QTreeWidget::itemChanged, this, &TreeDir::onItemChanged);

  // 从设置应用目录树字体大小，并在字体设置变化时即时刷新
  applyFontFromSetting();
  connect(&SettingStore::ins(), &SettingStore::fontsChanged, this, &TreeDir::applyFontFromSetting);

  // 文件夹展开/收起时切换展开/收起样式图标（仅目录节点）
  connect(this, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem *item) {
    if (item->data(0, Qt::UserRole + 1).toString().isEmpty()) item->setIcon(0, m_folderOpenIcon);
  });
  connect(this, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem *item) {
    if (item->data(0, Qt::UserRole + 1).toString().isEmpty()) item->setIcon(0, m_folderIcon);
  });

  // 初始化文件与文件夹图标；主题/颜色变化时重新生成（图标颜色随主题）
  refreshIcons();
  connect(&SettingStore::ins(), &SettingStore::themeChanged, this, &TreeDir::refreshIcons);
  connect(&SettingStore::ins(), &SettingStore::colorsChanged, this, &TreeDir::refreshIcons);
}

// ============================================================================
// applyFontFromSetting — 从设置读取目录树字体大小并应用
// ============================================================================

void TreeDir::applyFontFromSetting() {
  QFont f = font();
  // 字体族：跟随「目录树字体」设置（未设置时沿用当前默认字体）
  const QString fam = SettingStore::ins().fontFamily(QStringLiteral("font.tree"));
  if (!fam.isEmpty()) f.setFamily(fam);
  f.setPointSize(SettingStore::ins().fontSize(QStringLiteral("font.tree")));
  setFont(f);
}

// ============================================================================
// mouseReleaseEvent — 区分复选框点击 vs 文本/图标点击
// ============================================================================

void TreeDir::mouseReleaseEvent(QMouseEvent *event) {
  QTreeWidgetItem *item = itemAt(event->pos());
  m_lastClickOnCheckbox = false;

  // 仅当节点真正可勾选（调用过 setCheckState，即 CheckStateRole 有效）时，
  // 才需要判断点击是否落在复选框区域。不能用 flags() & ItemIsUserCheckable，
  // 因为 QTreeWidgetItem 默认 flags 就含该项，所有节点都满足。
  if (item && item->data(0, Qt::CheckStateRole).isValid()) {
    // 估算复选框区域的宽度
    int cbWidth = style()->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, this);
    if (cbWidth < 10) cbWidth = 16;  // 默认值

    QRect itemRect = visualRect(indexFromItem(item));
    int clickX = static_cast<int>(event->position().x()) - itemRect.x();
    // 复选框通常绘制在 item 左侧，留 4px 边距
    if (clickX >= 0 && clickX < cbWidth + 4) m_lastClickOnCheckbox = true;
  }

  QTreeWidget::mouseReleaseEvent(event);
}

// ============================================================================
// buildTree — 扫描目录并构建文件树
// ============================================================================

void TreeDir::buildTree(const QString &dirPath) {
  clear();
  // 统一使用正斜杠，确保与 QFileInfo::absoluteFilePath() 格式一致
  m_rootPath = QDir::cleanPath(dirPath);
  m_configPath = m_rootPath + QString::fromUtf8(CodeConstants::Paths::kTreeConfigFile);

  // 树构建期间 setCheckState 会触发 itemChanged → onItemChanged → saveState，
  // 在 loadState 完成前禁止保存，避免将空的未选中状态写入 tree.config
  m_bulkUpdating = true;
  addDirectoryToTree(nullptr, dirPath);
  m_bulkUpdating = false;

  // 展开状态由 loadState 恢复（无配置时默认全部展开）
  loadState();
}

// ============================================================================
// refreshTree — 刷新当前目录树
// ============================================================================

void TreeDir::refreshTree() {
  if (m_rootPath.isEmpty()) return;
  // 保存当前展开状态（包括勾选、启动项、展开节点），重建后由 loadState 恢复
  saveState();
  buildTree(m_rootPath);
}

// ============================================================================
// 单击 / 双击
// ============================================================================

void TreeDir::onItemClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  if (!item) return;

  const QString filePath = item->data(0, Qt::UserRole + 1).toString();

  if (filePath.isEmpty()) {
    // 目录节点：点击复选框区域 → 级联切换子节点复选框；点击其他区域 → 展开/收起
    if (m_lastClickOnCheckbox) {
      // Qt 已自动切换了文件夹自身的复选框，此处直接读取当前状态同步给子节点
      Qt::CheckState state = item->checkState(0);
      m_bulkUpdating = true;
      setJsonChildrenCheckState(item, state);
      m_bulkUpdating = false;
      updateParentCheckState(item);
      scheduleSave();
    } else {
      // 单击文件夹文本/图标/空白处 → 切换展开/收起（setExpanded 会触发
      // itemExpanded/itemCollapsed，构造函数已连接切换文件夹展开图标）
      item->setExpanded(!item->isExpanded());
      scheduleSave();  // 保存展开状态，重启后可还原
    }
    return;
  }

  // 文件节点
  if (isJsonLike(filePath) && m_lastClickOnCheckbox) {
    // 单击 json/jsonvue 复选框 → Qt 已自动切换复选框，刷新父级三态并保存
    updateParentCheckState(item);
    scheduleSave();
  } else {
    // 单击文本/图标区域 → 打开文件（所有类型统一单击打开；
    // locateFile 已改为仅在节点不可见时滚动，单击打开不会引起树滚动，
    // 双击的第二次点击仍落在原节点上，不会再出现定位错乱）
    emit fileActivated(filePath);
  }
}

void TreeDir::onItemDoubleClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  if (!item) return;

  const QString filePath = item->data(0, Qt::UserRole + 1).toString();
  if (!filePath.isEmpty()) emit fileActivated(filePath);
}

/// 获取文件夹节点在目录树中的完整路径（相对于根目录）
static QString buildFolderPath(QTreeWidgetItem *item, const QString &rootPath);

// ============================================================================
// onItemChanged — 复选框状态变化时级联更新
// ============================================================================

void TreeDir::onItemChanged(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  if (!item || m_bulkUpdating) return;

  // 仅复选框（Qt::CheckStateRole）变化才需要级联与保存；
  // 悬停高亮等其他自定义角色（如 kTreeHoverRole）变化不应触发副作用。
  if (!item->data(0, Qt::CheckStateRole).isValid()) return;

  const QString filePath = item->data(0, Qt::UserRole + 1).toString();

  if (!filePath.isEmpty() && isJsonLike(filePath)) {
    // json/jsonvue 文件的复选框变化：更新父节点状态
    updateParentCheckState(item);
    scheduleSave();
  }
}

// ============================================================================
// addDirectoryToTree — 递归添加目录/文件
// ============================================================================

void TreeDir::addDirectoryToTree(QTreeWidgetItem *parentItem, const QString &dirPath) {
  QDir dir(dirPath);

  // 文件（.ac、.tpl、.json 和 .jsonvue）
  QStringList nameFilters;
  nameFilters << QStringLiteral("*.ac") << QStringLiteral("*.tpl") << QStringLiteral("*.json")
              << QStringLiteral("*.jsonvue");
  QFileInfoList files = dir.entryInfoList(nameFilters, QDir::Files);

  // 子目录
  QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

  struct SubDirInfo {
    QTreeWidgetItem *item;
    QString path;
  };
  QList<SubDirInfo> subDirs;
  for (const QFileInfo &info : dirs) {
    auto *dirItem = parentItem ? new QTreeWidgetItem(parentItem) : new QTreeWidgetItem(this);
    dirItem->setText(0, info.fileName());
    dirItem->setIcon(0, m_folderIcon);  // 收起样式，展开时由 itemExpanded 切到展开样式
    dirItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    subDirs.append({dirItem, info.absoluteFilePath()});
  }

  // 递归子目录
  for (const auto &sd : subDirs) {
    addDirectoryToTree(sd.item, sd.path);
  }

  // 添加文件节点
  for (const QFileInfo &info : files) {
    auto *fileItem = parentItem ? new QTreeWidgetItem(parentItem) : new QTreeWidgetItem(this);
    fileItem->setText(0, info.fileName());
    // 按后缀区分三种文件图标（ac 蓝 / json 琥珀 / tpl 绿）
    fileItem->setIcon(0, iconForSuffix(info.suffix()));
    fileItem->setData(0, Qt::UserRole + 1, info.absoluteFilePath());

    // 启动项标记：.ac 文件按 m_startupFiles 设置（三角由 ModifiedFileDelegate 绘制）
    if (info.suffix().compare(QStringLiteral("ac"), Qt::CaseInsensitive) == 0)
      fileItem->setData(0, kTreeStartupRole, m_startupFiles.contains(info.absoluteFilePath()));

    if (isJsonLike(info.absoluteFilePath())) {
      fileItem->setFlags(fileItem->flags() | Qt::ItemIsUserCheckable);
      fileItem->setCheckState(0, Qt::Unchecked);
    }
  }

  // 为目录添加复选框（仅当目录下（含子目录）有 .json 文件）
  for (const auto &sd : subDirs) {
    bool hasJson = false;
    for (int i = 0; i < sd.item->childCount(); ++i) {
      // 只有真正可勾选（CheckStateRole 有效）的子项才说明该目录下有 json 文件。
      // 不能用 flags() & ItemIsUserCheckable，因为默认 flags 就含该项。
      if (sd.item->child(i)->data(0, Qt::CheckStateRole).isValid()) {
        hasJson = true;
        break;
      }
    }
    if (hasJson) {
      sd.item->setCheckState(0, Qt::Unchecked);
    }
  }
}

// ============================================================================
// 级联更新
// ============================================================================

void TreeDir::setJsonChildrenCheckState(QTreeWidgetItem *item, Qt::CheckState state) {
  for (int i = 0; i < item->childCount(); ++i) {
    QTreeWidgetItem *child = item->child(i);
    QString filePath = child->data(0, Qt::UserRole + 1).toString();

    if (!filePath.isEmpty() && isJsonLike(filePath)) {
      child->setCheckState(0, state);
    } else if (child->data(0, Qt::CheckStateRole).isValid()) {
      // 中间层文件夹：其下所有 json 文件都会被设置为同一状态，
      // 因此文件夹自身的复选框也应同步为相同状态（避免漏勾中间层）。
      // 注意：不能用 flags() & ItemIsUserCheckable 判断，因为 QTreeWidgetItem
      // 的默认 flags 就含该位（所有节点都满足），必须用 CheckStateRole 是否有效
      // （即是否调用过 setCheckState）来判断节点是否真正可勾选。
      child->setCheckState(0, state);
    }

    setJsonChildrenCheckState(child, state);
  }
}

/// 后序重算文件夹复选框状态：先递归子节点，再由子节点聚合出自身三态
/// （全勾=打勾 / 部分勾=小方块 / 全不勾=空）。仅在配置恢复后调用一次，
/// 因为 applyStateToTree 只恢复 json 文件的勾选，文件夹状态需重新聚合。
void TreeDir::recomputeFolderCheckStates(QTreeWidgetItem *item) {
  for (int i = 0; i < item->childCount(); ++i) recomputeFolderCheckStates(item->child(i));

  // 仅处理文件夹节点（文件路径为空）且自身有复选框（CheckStateRole 有效）
  if (!item->data(0, Qt::UserRole + 1).toString().isEmpty()) return;
  if (!item->data(0, Qt::CheckStateRole).isValid()) return;

  int checked = 0;
  int total = 0;
  bool partial = false;
  for (int i = 0; i < item->childCount(); ++i) {
    QTreeWidgetItem *child = item->child(i);
    if (!child->data(0, Qt::CheckStateRole).isValid()) continue;
    ++total;
    const Qt::CheckState cs = child->checkState(0);
    if (cs == Qt::Checked)
      ++checked;
    else if (cs == Qt::PartiallyChecked)
      partial = true;
  }

  Qt::CheckState st = Qt::Unchecked;
  if (total > 0 && checked == total)
    st = Qt::Checked;
  else if (partial || (checked > 0 && checked < total))
    st = Qt::PartiallyChecked;
  item->setCheckState(0, st);
}

void TreeDir::updateParentCheckState(QTreeWidgetItem *item) {
  QTreeWidgetItem *parent = item->parent();
  if (!parent) return;

  int checkedCount = 0;
  int totalCheckable = 0;
  for (int i = 0; i < parent->childCount(); ++i) {
    QTreeWidgetItem *child = parent->child(i);
    // 只统计真正可勾选的子节点（CheckStateRole 有效，即调用过 setCheckState）。
    // 不能用 flags() & ItemIsUserCheckable，因为 QTreeWidgetItem 默认 flags 就含
    // 该项，会把 .ac/.tpl 等不可勾选文件也计入 totalCheckable，导致父文件夹
    // 永远无法达到全选状态。
    if (child->data(0, Qt::CheckStateRole).isValid()) {
      ++totalCheckable;
      Qt::CheckState cs = child->checkState(0);
      if (cs == Qt::Checked)
        ++checkedCount;
      else if (cs == Qt::PartiallyChecked) {
        parent->setCheckState(0, Qt::PartiallyChecked);
        updateParentCheckState(parent);
        return;
      }
    }
  }

  if (totalCheckable == 0)
    parent->setCheckState(0, Qt::Unchecked);
  else if (checkedCount == totalCheckable)
    parent->setCheckState(0, Qt::Checked);
  else if (checkedCount == 0)
    parent->setCheckState(0, Qt::Unchecked);
  else
    parent->setCheckState(0, Qt::PartiallyChecked);

  updateParentCheckState(parent);
}

// ============================================================================
// 状态持久化
// ============================================================================

void TreeDir::saveState() {
  if (m_configPath.isEmpty()) return;

  // 从树/成员状态收集数据，交给数据层持久化
  m_store.checkedRelPaths = collectCheckedRelPaths();
  m_store.startupRelPaths = collectStartupRelPaths();
  m_store.selectedStartupRel = toRelPath(m_selectedStartup);
  m_store.visualToggle = m_visualToggle;
  m_store.expandedRelPaths = collectExpandedRelPaths();

  m_store.save(m_configPath);
}

void TreeDir::loadState() {
  if (m_configPath.isEmpty()) return;

  // 无配置或配置损坏 → 不还原、也不默认全部展开，保持当前（折叠）状态
  if (!m_store.load(m_configPath)) {
    return;
  }

  m_bulkUpdating = true;

  // ── 恢复勾选状态 ──
  applyCheckedToTree(m_store.checkedRelPaths);

  // ── 恢复启动项 ──
  m_startupFiles.clear();
  bool startupPruned = false;
  for (const QString &rel : m_store.startupRelPaths) {
    QString abs = QDir::cleanPath(m_rootPath + QLatin1Char('/') + rel);
    if (QFileInfo::exists(abs)) {
      m_startupFiles.insert(abs);
    } else {
      startupPruned = true;
    }
  }

  // ── 恢复当前选中的启动项 ──
  if (!m_store.selectedStartupRel.isEmpty()) {
    QString abs = QDir::cleanPath(m_rootPath + QLatin1Char('/') + m_store.selectedStartupRel);
    if (QFileInfo::exists(abs)) {
      m_selectedStartup = abs;
    } else {
      m_selectedStartup.clear();
      startupPruned = true;
    }
  } else {
    m_selectedStartup.clear();
  }

  // 刷新启动项图标
  refreshStartupIcons();

  // 通知外部启动项已恢复
  emit startupItemsChanged();

  // ── 恢复可视化编辑按钮状态 ──
  m_visualToggle = m_store.visualToggle;

  // ── 恢复展开状态 ──
  applyExpandedToTree(m_store.expandedRelPaths);

  m_bulkUpdating = false;

  // 有不存在的启动项文件被过滤掉时，更新配置文件。
  // 须在 applyExpandedToTree 之后执行：此时树已恢复展开状态，
  // saveState 收集到的才是正确的展开路径，不会把 config 中的展开记录清空。
  if (startupPruned) saveState();
}

// ============================================================================
// 辅助：收集 / 应用勾选状态
// ============================================================================

QStringList TreeDir::checkedJsonFiles() const {
  QStringList files;
  for (int i = 0; i < topLevelItemCount(); ++i) collectJsonFiles(topLevelItem(i), files);
  return files;
}

QStringList TreeDir::collectCheckedRelPaths() const {
  QStringList rel;
  for (int i = 0; i < topLevelItemCount(); ++i)
    collectCheckedRelRecursive(topLevelItem(i), m_rootPath, rel);
  return rel;
}

QStringList TreeDir::collectStartupRelPaths() const {
  QStringList rel;
  for (const QString &abs : m_startupFiles) {
    QString r = toRelPath(abs);
    if (!r.isEmpty()) rel.append(r);
  }
  return rel;
}

QStringList TreeDir::collectExpandedRelPaths() const {
  QStringList rel;
  QTreeWidgetItemIterator it(const_cast<TreeDir *>(this));
  while (*it) {
    QTreeWidgetItem *item = *it;
    // 仅目录节点（UserRole+1 为空）参与展开记录
    if (item->isExpanded() && item->data(0, Qt::UserRole + 1).toString().isEmpty()) {
      QString r = buildRelativePath(item);
      if (!r.isEmpty()) rel.append(r);
    }
    ++it;
  }
  return rel;
}

QString TreeDir::toRelPath(const QString &absPath) const {
  if (absPath.isEmpty() || !absPath.startsWith(m_rootPath)) return QString();
  return absPath.mid(m_rootPath.length() + 1);
}

void TreeDir::applyCheckedToTree(const QStringList &checkedRelPaths) {
  // 将相对路径还原为绝对路径并统一格式
  QStringList checkedAbsPaths;
  for (const QString &rel : checkedRelPaths)
    checkedAbsPaths.append(QDir::cleanPath(m_rootPath + QLatin1Char('/') + rel));

  // 递归应用到树节点（setCheckState 会触发 onItemChanged → scheduleSave，
  // 但此时 m_startupFiles 尚未恢复，saveState 会写入空数组覆盖原数据，
  // 因此在 loadState 期间设置 m_bulkUpdating 禁止保存）
  for (int i = 0; i < topLevelItemCount(); ++i) applyStateToTree(topLevelItem(i), checkedAbsPaths);

  // json 文件勾选已恢复，后序重算所有文件夹的三态（部分勾=小方块），
  // 否则重启后子 json 已勾选而文件夹仍显示未勾选
  for (int i = 0; i < topLevelItemCount(); ++i) recomputeFolderCheckStates(topLevelItem(i));
}

void TreeDir::applyExpandedToTree(const QStringList &expandedRelPaths) {
  // 构造时已关闭动画（setAnimated(false)），setExpanded 始终即时生效，
  // 无需再临时关闭动画。

  if (expandedRelPaths.isEmpty()) {
    // 无展开记录（首次启动/配置丢失）→ 保持折叠，不默认全部展开
    return;
  }

  // 有保存的展开状态 → 先折叠所有，再按记录展开
  collapseAll();

  // 构建 相对路径 → 节点 映射，O(1) 查表（避免逐层线性查找）
  QHash<QString, QTreeWidgetItem *> pathMap;
  for (int i = 0; i < topLevelItemCount(); ++i) buildPathMap(topLevelItem(i), QString(), pathMap);

  for (const QString &rel : expandedRelPaths) {
    QTreeWidgetItem *item = pathMap.value(rel);
    if (item) item->setExpanded(true);
  }
}

void TreeDir::scheduleSave() {
  if (m_configPath.isEmpty()) return;
  m_saveTimer->start();
}

void TreeDir::collectJsonFiles(QTreeWidgetItem *item, QStringList &files) const {
  // 检查当前节点本身（处理根目录下的 .json 文件）
  QString selfPath = item->data(0, Qt::UserRole + 1).toString();
  if (!selfPath.isEmpty() && isJsonLike(selfPath)) {
    if (item->checkState(0) == Qt::Checked) files.append(selfPath);
  }

  // 递归检查子节点
  for (int i = 0; i < item->childCount(); ++i) collectJsonFiles(item->child(i), files);
}

/// 递归收集已勾选 json 文件的相对路径
static void collectCheckedRelRecursive(QTreeWidgetItem *item, const QString &rootPath,
                                       QStringList &rel) {
  QString selfPath = item->data(0, Qt::UserRole + 1).toString();
  if (!selfPath.isEmpty() && isJsonLike(selfPath) && item->checkState(0) == Qt::Checked) {
    if (selfPath.startsWith(rootPath)) rel.append(selfPath.mid(rootPath.length() + 1));
  }
  for (int i = 0; i < item->childCount(); ++i)
    collectCheckedRelRecursive(item->child(i), rootPath, rel);
}

/// 构建节点相对路径（各层级 text 用 "/" 连接）
static QString buildRelativePath(QTreeWidgetItem *item) {
  QStringList parts;
  QTreeWidgetItem *cur = item;
  while (cur) {
    QString text = cur->text(0);
    if (!text.isEmpty()) parts.prepend(text);
    cur = cur->parent();
  }
  return parts.join(QLatin1Char('/'));
}

/// 递归构建 相对路径 → 节点 映射（仅目录节点参与，用于展开恢复的 O(1) 查表）
static void buildPathMap(QTreeWidgetItem *item, const QString &parentRel,
                         QHash<QString, QTreeWidgetItem *> &map) {
  QString rel = parentRel.isEmpty() ? item->text(0) : parentRel + QLatin1Char('/') + item->text(0);
  if (item->data(0, Qt::UserRole + 1).toString().isEmpty()) map.insert(rel, item);
  for (int i = 0; i < item->childCount(); ++i) buildPathMap(item->child(i), rel, map);
}

/// 按绝对路径查找树节点（找不到返回 nullptr）
QTreeWidgetItem *TreeDir::findItemByPath(const QString &absPath) const {
  if (absPath.isEmpty()) return nullptr;
  const QString clean = QDir::cleanPath(absPath);
  QTreeWidgetItemIterator it(const_cast<TreeDir *>(this));
  while (*it) {
    QTreeWidgetItem *item = *it;
    if (QDir::cleanPath(item->data(0, Qt::UserRole + 1).toString()) == clean) return item;
    ++it;
  }
  return nullptr;
}

/// 检查绝对路径是否匹配某个已保存的绝对路径
static bool pathListContains(const QStringList &list, const QString &absPath) {
  for (const QString &p : list) {
    // 同时 cleanPath（主要处理分隔符），轻松对比
    if (QDir::cleanPath(p) == QDir::cleanPath(absPath)) return true;
  }
  return false;
}

void TreeDir::applyStateToTree(QTreeWidgetItem *item, const QStringList &checkedAbsPaths) {
  // 检查当前节点本身（处理根目录下的 .json 文件）
  QString selfPath = item->data(0, Qt::UserRole + 1).toString();
  if (!selfPath.isEmpty() && isJsonLike(selfPath)) {
    if (pathListContains(checkedAbsPaths, selfPath)) item->setCheckState(0, Qt::Checked);
    return;  // 文件节点无子节点，无需递归
  }

  // 目录节点：递归处理子节点
  for (int i = 0; i < item->childCount(); ++i) {
    QTreeWidgetItem *child = item->child(i);
    QString filePath = child->data(0, Qt::UserRole + 1).toString();

    if (!filePath.isEmpty() && isJsonLike(filePath)) {
      if (pathListContains(checkedAbsPaths, filePath)) child->setCheckState(0, Qt::Checked);
    }

    applyStateToTree(child, checkedAbsPaths);

    if (child->childCount() > 0) updateParentCheckState(child);
  }
}

// ============================================================================
// 启动项管理
// ============================================================================

/// @brief 按文件后缀返回对应的类型图标
QIcon TreeDir::iconForSuffix(const QString &suffix) const {
  const QString suf = suffix.toLower();
  if (suf == QStringLiteral("ac")) return m_acIcon;
  if (suf == QStringLiteral("json")) return m_jsonIcon;
  if (suf == QStringLiteral("jsonvue")) return m_jsonVueIcon;
  return m_tplIcon;  // tpl 及未知后缀
}

/// @brief 重新生成文件与文件夹图标，并应用到所有节点与启动项图标
void TreeDir::refreshIcons() {
  m_acIcon = AuiIcon::createFileTypeIcon(QStringLiteral("ac"));
  m_jsonIcon = AuiIcon::createFileTypeIcon(QStringLiteral("json"));
  m_jsonVueIcon = AuiIcon::createFileTypeIcon(QStringLiteral("jsonvue"));
  m_tplIcon = AuiIcon::createFileTypeIcon(QStringLiteral("tpl"));
  m_folderIcon = AuiIcon::createFolderIcon(false);
  m_folderOpenIcon = AuiIcon::createFolderIcon(true);

  // 遍历所有节点，按后缀设置文件图标、按展开状态设置文件夹图标
  QList<QTreeWidgetItem *> stack;
  for (int i = 0; i < topLevelItemCount(); ++i) stack.append(topLevelItem(i));
  while (!stack.isEmpty()) {
    QTreeWidgetItem *item = stack.takeLast();
    for (int i = 0; i < item->childCount(); ++i) stack.append(item->child(i));

    QString filePath = item->data(0, Qt::UserRole + 1).toString();
    if (filePath.isEmpty()) {
      // 目录节点：按当前展开状态用对应样式
      item->setIcon(0, item->isExpanded() ? m_folderOpenIcon : m_folderIcon);
    } else {
      item->setIcon(0, iconForSuffix(QFileInfo(filePath).suffix()));
    }
  }
  refreshStartupIcons();
}

/// @brief 根据 m_startupFiles 集合更新所有 .ac 文件节点的启动标记（kTreeStartupRole）。
///        图标始终用普通 m_acIcon（位置不变），启动标记由 ModifiedFileDelegate 在图标
///        左侧绘制绿色三角，因此不再需要替换节点图标。
void TreeDir::refreshStartupIcons() {
  // 遍历所有顶层节点
  QList<QTreeWidgetItem *> stack;
  for (int i = 0; i < topLevelItemCount(); ++i) stack.append(topLevelItem(i));

  while (!stack.isEmpty()) {
    QTreeWidgetItem *item = stack.takeLast();
    for (int i = 0; i < item->childCount(); ++i) stack.append(item->child(i));

    QString filePath = item->data(0, Qt::UserRole + 1).toString();
    if (!filePath.isEmpty() && filePath.endsWith(AcFileSuffix::kAc, Qt::CaseInsensitive)) {
      item->setIcon(0, m_acIcon);
      item->setData(0, kTreeStartupRole, m_startupFiles.contains(filePath));
    }
  }
  update();  // 触发重绘显示三角标记
}

/// @brief 获取所有启动项文件绝对路径
QStringList TreeDir::startupFiles() const {
  QStringList result;
  for (const QString &p : m_startupFiles) result.append(p);
  return result;
}

/// @brief 设置当前选中的启动项
void TreeDir::setSelectedStartup(const QString &path) {
  if (m_selectedStartup != path) {
    m_selectedStartup = path;
    saveState();
  }
}

void TreeDir::removeStartup(const QString &filePath) {
  if (!m_startupFiles.remove(filePath)) return;  // 不在启动项集合中，无需处理

  if (m_selectedStartup == filePath) m_selectedStartup.clear();

  refreshStartupIcons();
  saveState();
  emit startupItemsChanged();
}

void TreeDir::setVisualToggle(bool enabled) {
  if (m_visualToggle != enabled) {
    m_visualToggle = enabled;
    saveState();
  }
}

/// @brief 重命名路径时更新启动项数据
void TreeDir::renameStartupPath(const QString &oldPath, const QString &newPath) {
  bool changed = false;

  if (m_startupFiles.contains(oldPath)) {
    m_startupFiles.remove(oldPath);
    m_startupFiles.insert(newPath);
    changed = true;
  }

  if (m_selectedStartup == oldPath) {
    m_selectedStartup = newPath;
    changed = true;
  }

  if (changed) {
    saveState();
    emit startupItemsChanged();
  }
}

/// @brief 批量重命名启动项路径
void TreeDir::renameStartupPaths(const QList<QPair<QString, QString>> &renames) {
  bool changed = false;

  for (const auto &pair : renames) {
    const QString &oldP = pair.first;
    const QString &newP = pair.second;

    if (m_startupFiles.contains(oldP)) {
      m_startupFiles.remove(oldP);
      m_startupFiles.insert(newP);
      changed = true;
    }

    if (m_selectedStartup == oldP) {
      m_selectedStartup = newP;
      changed = true;
    }
  }

  if (changed) {
    saveState();
    emit startupItemsChanged();
  }
}

/// @brief 获取文件夹节点在目录树中的完整路径（相对于根目录）
static QString buildFolderPath(QTreeWidgetItem *item, const QString &rootPath) {
  return QDir::cleanPath(rootPath + QLatin1Char('/') + buildRelativePath(item));
}

// ============================================================================
// 右键菜单 — 文件设为/取消启动项，文件夹新建/刷新
// ============================================================================

void TreeDir::contextMenuEvent(QContextMenuEvent *event) {
  QTreeWidgetItem *item = itemAt(event->pos());
  if (!item) {
    // 空白区域：[新建] [刷新] — 在根目录下新建文件/文件夹
    if (!m_rootPath.isEmpty()) {
      QMenu menu(this);
      QAction *newAct = menu.addAction(QString::fromUtf8(CodeConstants::UiText::kNew));
      QAction *refreshAct = menu.addAction(QStringLiteral("刷新"));
      QAction *chosen = menu.exec(event->globalPos());
      if (chosen == newAct) {
        CreateMgr::createNew(m_rootPath, this);
        refreshTree();
      } else if (chosen == refreshAct) {
        refreshTree();
      }
      return;
    }
    QTreeWidget::contextMenuEvent(event);
    return;
  }

  QString filePath = item->data(0, Qt::UserRole + 1).toString();

  if (filePath.isEmpty()) {
    // 文件夹节点：[在文件资源管理器中显示] [新建] [重命名] [删除] [刷新]
    QMenu menu(this);
    QAction *showInExplorerAct = menu.addAction(QStringLiteral("在文件资源管理器中显示"));
    menu.addSeparator();
    QAction *newAct = menu.addAction(QString::fromUtf8(CodeConstants::UiText::kNew));
    QAction *renameAct = menu.addAction(QStringLiteral("重命名"));
    QAction *deleteAct = menu.addAction(QStringLiteral("删除"));
    menu.addSeparator();
    QAction *refreshAct = menu.addAction(QStringLiteral("刷新"));
    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == showInExplorerAct) {
      UtilFile::showInExplorer(buildFolderPath(item, m_rootPath));
    } else if (chosen == newAct) {
      QString dirPath = buildFolderPath(item, m_rootPath);
      CreateMgr::createNew(dirPath, this);
      refreshTree();
    } else if (chosen == renameAct) {
      bool ok;
      QString newName = RenameDialog::getNewName(this, item->text(0), &ok);
      if (ok) {
        QString oldPath = buildFolderPath(item, m_rootPath);
        emit renameRequested(oldPath, newName);
      }
    } else if (chosen == deleteAct) {
      QString dirPath = buildFolderPath(item, m_rootPath);
      emit deleteRequested(dirPath);
    } else if (chosen == refreshAct) {
      refreshTree();
    }
    return;
  }

  // 文件节点：[在文件资源管理器中显示] [重命名] [删除]，.ac 文件额外显示 [设为/取消启动项]
  QMenu menu(this);
  QAction *showInExplorerAct = menu.addAction(QStringLiteral("在文件资源管理器中显示"));
  menu.addSeparator();
  QAction *renameAct = menu.addAction(QStringLiteral("重命名"));
  QAction *deleteAct = menu.addAction(QStringLiteral("删除"));
  if (filePath.endsWith(AcFileSuffix::kAc, Qt::CaseInsensitive)) {
    menu.addSeparator();
    if (m_startupFiles.contains(filePath))
      menu.addAction(QStringLiteral("取消启动项"));
    else
      menu.addAction(QStringLiteral("设为启动项"));
  }
  QAction *chosen = menu.exec(event->globalPos());
  if (chosen == showInExplorerAct) {
    UtilFile::showInExplorer(filePath);
  } else if (chosen == renameAct) {
    bool ok;
    QString newName = RenameDialog::getNewName(this, item->text(0), &ok);
    if (ok) {
      QString oldPath = item->data(0, Qt::UserRole + 1).toString();
      emit renameRequested(oldPath, newName);
    }
  } else if (chosen == deleteAct) {
    emit deleteRequested(filePath);
  } else if (chosen && !filePath.isEmpty() &&
             filePath.endsWith(AcFileSuffix::kAc, Qt::CaseInsensitive)) {
    if (m_startupFiles.contains(filePath))
      m_startupFiles.remove(filePath);
    else
      m_startupFiles.insert(filePath);
    refreshStartupIcons();
    saveState();
    emit startupItemsChanged();
  }
}

// ════════════════════════════════════════════════════════════
//  setFileModified — 设置文件修改状态
// ════════════════════════════════════════════════════════════

/// 判断某节点子树（含自身）内是否存在已修改的文件节点。
/// 只信任真实文件节点（UserRole+1 非空）自身携带的修改标记，
/// 文件夹的传播标记不参与计算，避免旧的传播值导致父节点误判。
static bool hasModifiedFileInSubtree(QTreeWidgetItem *item) {
  if (!item->data(0, Qt::UserRole + 1).toString().isEmpty() &&
      item->data(0, Qt::UserRole + 2).toBool())
    return true;  // 文件节点自身已修改
  for (int i = 0; i < item->childCount(); ++i)
    if (hasModifiedFileInSubtree(item->child(i))) return true;
  return false;
}

void TreeDir::setFileModified(const QString &filePath, bool modified) {
  QTreeWidgetItem *item = findItemByPath(filePath);
  if (!item) return;
  // 通过自定义数据角色存储修改状态，由 ModifiedFileDelegate 绘制实心圆点
  item->setData(0, Qt::UserRole + 2, modified);
  // 子文件被修改时，父文件夹也要显示修改圆点；全部清除后父文件夹同步消失
  for (QTreeWidgetItem *p = item->parent(); p; p = p->parent())
    p->setData(0, Qt::UserRole + 2, hasModifiedFileInSubtree(p));
  update();  // 触发重绘
}

// ════════════════════════════════════════════════════════════
//  setFileError / clearFileError — 设置/清除文件错误状态
// ════════════════════════════════════════════════════════════

/// 计算节点子树（含自身）内所有真实文件节点（UserRole+1 非空）的错误数量总和。
/// 文件夹自身的传播值不参与计算，只信任文件节点携带的错误数量，避免旧值污染父节点。
static int subtreeErrorCount(QTreeWidgetItem *item) {
  int sum = 0;
  if (!item->data(0, Qt::UserRole + 1).toString().isEmpty())
    sum += item->data(0, Qt::UserRole + 3).toInt();  // 文件节点自身错误数
  for (int i = 0; i < item->childCount(); ++i) sum += subtreeErrorCount(item->child(i));
  return sum;
}

void TreeDir::setFileError(const QString &filePath, int errorCount) {
  QTreeWidgetItem *item = findItemByPath(filePath);
  if (!item) return;
  // 存储错误数量，由 ModifiedFileDelegate 绘制红色文件名和最右侧错误数量徽章
  item->setData(0, Qt::UserRole + 3, errorCount);
  // 父文件夹同步显示子文件错误次数总和
  for (QTreeWidgetItem *p = item->parent(); p; p = p->parent())
    p->setData(0, Qt::UserRole + 3, subtreeErrorCount(p));
  update();  // 触发重绘
}

void TreeDir::clearFileError(const QString &filePath) {
  QTreeWidgetItem *item = findItemByPath(filePath);
  if (!item) return;
  // 清除错误数量
  item->setData(0, Qt::UserRole + 3, 0);
  // 父文件夹同步重算错误次数总和
  for (QTreeWidgetItem *p = item->parent(); p; p = p->parent())
    p->setData(0, Qt::UserRole + 3, subtreeErrorCount(p));
  update();  // 触发重绘
}

// ════════════════════════════════════════════════════════════
//  locateFile — 定位到指定文件路径的节点
// ════════════════════════════════════════════════════════════

void TreeDir::locateFile(const QString &filePath) {
  QTreeWidgetItem *item = findItemByPath(filePath);
  if (!item) return;

  // 展开所有父节点
  QTreeWidgetItem *parent = item->parent();
  while (parent) {
    parent->setExpanded(true);
    parent = parent->parent();
  }
  setCurrentItem(item);

  // 仅当节点不在视口内时才滚动定位。树上单击打开文件会经标签切换/焦点变化
  // 触发本函数，若无条件居中滚动会导致滚动条跳动（用户正在点击的节点必然
  // 可见，无需滚动）；外部触发（如编辑器跳转）且节点滚出视口时才需要定位。
  const QRect vr = visualItemRect(item);
  if (!vr.isValid() || !vr.intersects(viewport()->rect()))
    scrollToItem(item, QAbstractItemView::PositionAtCenter);
}

bool TreeDir::viewportEvent(QEvent *event) {
  // 手动追踪悬停节点：Qt 纯代码 delegate 不会自动携带 State_MouseOver。
  // QAbstractScrollArea 通过 viewport 事件过滤器链把鼠标事件转给本虚函数，
  // 鼠标移动/离开都会到达这里（事件类型是 MouseMove/Leave），据此实时更新
  // m_hoverItem，供 ModifiedFileDelegate 对整行（含左右空白区）做悬停高亮。
  if (event->type() == QEvent::MouseMove) {
    auto *me = static_cast<QMouseEvent *>(event);
    QTreeWidgetItem *hover = itemAt(me->pos());
    if (hover != m_hoverItem) {
      if (m_hoverItem) repaintRow(m_hoverItem);
      m_hoverItem = hover;
      if (m_hoverItem) repaintRow(m_hoverItem);
    }
    // 光标随悬停区域联动：有可交互节点用手型；节点下方的空白区点击无效果，
    // 应恢复为正常箭头，避免手型停留误导（只有当形状变化时才设置，减少无谓更新）
    const Qt::CursorShape wanted =
        hover ? Qt::PointingHandCursor : Qt::ArrowCursor;
    if (viewport()->cursor().shape() != wanted) viewport()->setCursor(wanted);
  } else if (event->type() == QEvent::Leave) {
    if (m_hoverItem) {
      repaintRow(m_hoverItem);
      m_hoverItem = nullptr;
    }
    if (viewport()->cursor().shape() != Qt::ArrowCursor) viewport()->setCursor(Qt::ArrowCursor);
  }
  return QTreeWidget::viewportEvent(event);
}

/// 重绘某节点所在行的整行区域（左 0 → 视口右缘）。
/// 只调用 update(visualItemRect) 只刷新内容窄条，左右空白区不会变色；
/// 必须把重绘区域扩展到视口全宽，悬停/取消悬停时整行（含左右空白）才同步变色。
void TreeDir::repaintRow(const QTreeWidgetItem *item) {
  if (!item) return;
  QRect r = visualItemRect(const_cast<QTreeWidgetItem *>(item));
  if (!r.isValid()) return;
  r.setLeft(0);
  r.setRight(viewport()->width() - 1);
  viewport()->update(r);
}