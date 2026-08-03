/**
 * @file tree_dir.cpp
 * @brief 可打勾的文件树控件实现
 */

#include "tree_dir.h"

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
#include "src/util/common/util_file.h"
#include "src/util/ui/component/aui_icon.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/rename_dialog.h"

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

/// 绘制错误态：文件名 + 错误数量，红色加粗
static void paintErrorState(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index, int errorCount) {
  QStyleOptionViewItem opt = option;
  // 合并显示文本：文件名 + 错误数量
  QString originalText = index.data(Qt::DisplayRole).toString();
  opt.text = originalText + QStringLiteral("  (%1)").arg(errorCount);

  // 修改 palette 文字颜色为红色
  QPalette redPalette = opt.palette;
  redPalette.setColor(QPalette::Text, AuiStyle::errorTextColor());
  redPalette.setColor(QPalette::WindowText, AuiStyle::errorTextColor());
  redPalette.setColor(QPalette::HighlightedText, AuiStyle::errorTextColor());
  opt.palette = redPalette;

  QFont boldFont = opt.font;
  boldFont.setBold(true);
  opt.font = boldFont;

  QStyledItemDelegate delegate;
  delegate.paint(painter, opt, index);
}

/// 绘制修改态：默认内容 + 文件名右上角红色 "*"
static void paintModifiedState(QPainter *painter, const QStyleOptionViewItem &option,
                               const QModelIndex &index, const QTreeWidget *tree) {
  // 先绘制默认内容
  QStyledItemDelegate delegate;
  delegate.paint(painter, option, index);

  painter->save();
  painter->setPen(AuiStyle::modifiedColor());

  QFont treeFont = tree ? tree->font() : option.font;
  QFontMetrics fm(treeFont);
  QString text = index.data(Qt::DisplayRole).toString();

  int decoWidth = option.decorationSize.width();
  if (decoWidth == 0) decoWidth = option.icon.actualSize(QSize(16, 16)).width();

  int checkWidth = 0;
  if (index.data(Qt::CheckStateRole).isValid())
    checkWidth = tree ? tree->style()->pixelMetric(QStyle::PM_IndicatorWidth) + 4 : 20;

  int textStartX = option.rect.left() + checkWidth + decoWidth + 4;
  int starX = textStartX + fm.horizontalAdvance(text) + 6;
  int starY = option.rect.top() + fm.ascent();

  QFont boldFont = treeFont;
  boldFont.setBold(true);
  painter->setFont(boldFont);
  painter->drawText(starX, starY, QStringLiteral("*"));
  painter->restore();
}

void ModifiedFileDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const {
  int errorCount = index.data(Qt::UserRole + 3).toInt();
  bool modified = index.data(Qt::UserRole + 2).toBool();

  if (errorCount > 0) {
    paintErrorState(painter, option, index, errorCount);
  } else if (modified) {
    paintModifiedState(painter, option, index, qobject_cast<const QTreeWidget *>(parent()));
  } else {
    // 无特殊状态，正常绘制
    QStyledItemDelegate::paint(painter, option, index);
  }
}

// ============================================================================
// 构造
// ============================================================================

TreeDir::TreeDir(QWidget *parent) : QTreeWidget(parent) {
  setHeaderLabel(QStringLiteral("文件"));
  setMinimumWidth(kMinWidth);
  setMaximumWidth(kMaxWidth);
  setAnimated(true);
  setIndentation(16);
  setSortingEnabled(false);

  setItemDelegate(new ModifiedFileDelegate(this));

  // 保存防抖定时器：复选框频繁变化时合并写入，避免每次勾选都落盘
  m_saveTimer = new QTimer(this);
  m_saveTimer->setSingleShot(true);
  m_saveTimer->setInterval(300);
  connect(m_saveTimer, &QTimer::timeout, this, &TreeDir::saveState);

  connect(this, &QTreeWidget::itemClicked, this, &TreeDir::onItemClicked);
  connect(this, &QTreeWidget::itemDoubleClicked, this, &TreeDir::onItemDoubleClicked);
  connect(this, &QTreeWidget::itemChanged, this, &TreeDir::onItemChanged);
}

// ============================================================================
// mouseReleaseEvent — 区分复选框点击 vs 文本/图标点击
// ============================================================================

void TreeDir::mouseReleaseEvent(QMouseEvent *event) {
  QTreeWidgetItem *item = itemAt(event->pos());
  m_lastClickOnCheckbox = false;

  if (item && (item->flags() & Qt::ItemIsUserCheckable)) {
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
  m_configPath = m_rootPath + QStringLiteral("/tree.config");

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
    // 目录节点：只有点击复选框区域才切换子节点复选框
    if (m_lastClickOnCheckbox) {
      // Qt 已自动切换了文件夹自身的复选框，此处直接读取当前状态同步给子节点
      Qt::CheckState state = item->checkState(0);
      m_bulkUpdating = true;
      setJsonChildrenCheckState(item, state);
      m_bulkUpdating = false;
      updateParentCheckState(item);
      scheduleSave();
    }
    return;
  }

  // 文件节点
  if (isJsonLike(filePath)) {
    if (m_lastClickOnCheckbox) {
      // 单击复选框 → Qt 已自动切换复选框，刷新状态
      updateParentCheckState(item);
      scheduleSave();
    } else {
      // 单击文本/图标区域 → 打开文件
      emit fileActivated(filePath);
    }
  } else {
    // .ac / .tpl 文件：直接打开
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
    dirItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
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
    fileItem->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
    fileItem->setData(0, Qt::UserRole + 1, info.absoluteFilePath());

    if (isJsonLike(info.absoluteFilePath())) {
      fileItem->setFlags(fileItem->flags() | Qt::ItemIsUserCheckable);
      fileItem->setCheckState(0, Qt::Unchecked);
    }
  }

  // 为目录添加复选框（仅当目录下（含子目录）有 .json 文件）
  for (const auto &sd : subDirs) {
    bool hasJson = false;
    for (int i = 0; i < sd.item->childCount(); ++i) {
      if (sd.item->child(i)->flags() & Qt::ItemIsUserCheckable) {
        hasJson = true;
        break;
      }
    }
    if (hasJson) {
      sd.item->setFlags(sd.item->flags() | Qt::ItemIsUserCheckable);
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

    if (!filePath.isEmpty() && isJsonLike(filePath)) child->setCheckState(0, state);

    setJsonChildrenCheckState(child, state);
  }
}

void TreeDir::updateParentCheckState(QTreeWidgetItem *item) {
  QTreeWidgetItem *parent = item->parent();
  if (!parent) return;

  int checkedCount = 0;
  int totalCheckable = 0;
  for (int i = 0; i < parent->childCount(); ++i) {
    QTreeWidgetItem *child = parent->child(i);
    if (child->flags() & Qt::ItemIsUserCheckable) {
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

  // 无配置时默认全部展开
  if (!m_store.load(m_configPath)) {
    expandAll();
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

  // 有不存在的文件被过滤掉时，更新配置文件
  if (startupPruned) saveState();

  // 刷新启动项图标
  refreshStartupIcons();

  // 通知外部启动项已恢复
  emit startupItemsChanged();

  // ── 恢复可视化编辑按钮状态 ──
  m_visualToggle = m_store.visualToggle;

  // ── 恢复展开状态（无记录时默认全部展开）──
  applyExpandedToTree(m_store.expandedRelPaths);

  m_bulkUpdating = false;
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
}

void TreeDir::applyExpandedToTree(const QStringList &expandedRelPaths) {
  if (expandedRelPaths.isEmpty()) {
    // 无展开记录 → 默认全部展开
    expandAll();
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

/// @brief 创建带三角标记的启动项图标 — 委托 AuiButton 叠加绿色三角
QIcon TreeDir::makeStartupIcon() const {
  return AuiIcon::createStartupOverlayIcon(style()->standardIcon(QStyle::SP_FileIcon));
}

/// @brief 根据 m_startupFiles 集合更新所有 .ac 文件节点的图标
void TreeDir::refreshStartupIcons() {
  QIcon startupIcon = makeStartupIcon();
  QIcon normalIcon = style()->standardIcon(QStyle::SP_FileIcon);

  // 遍历所有顶层节点
  QList<QTreeWidgetItem *> stack;
  for (int i = 0; i < topLevelItemCount(); ++i) stack.append(topLevelItem(i));

  while (!stack.isEmpty()) {
    QTreeWidgetItem *item = stack.takeLast();
    for (int i = 0; i < item->childCount(); ++i) stack.append(item->child(i));

    QString filePath = item->data(0, Qt::UserRole + 1).toString();
    if (!filePath.isEmpty() && filePath.endsWith(AcFileSuffix::kAc, Qt::CaseInsensitive)) {
      if (m_startupFiles.contains(filePath))
        item->setIcon(0, startupIcon);
      else
        item->setIcon(0, normalIcon);
    }
  }
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
    QTreeWidget::contextMenuEvent(event);
    return;
  }

  QString filePath = item->data(0, Qt::UserRole + 1).toString();

  if (filePath.isEmpty()) {
    // 文件夹节点：[在文件资源管理器中显示] [新建] [重命名] [删除] [刷新]
    QMenu menu(this);
    QAction *showInExplorerAct = menu.addAction(QStringLiteral("在文件资源管理器中显示"));
    menu.addSeparator();
    QAction *newAct = menu.addAction(QStringLiteral("新建"));
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

void TreeDir::setFileModified(const QString &filePath, bool modified) {
  QTreeWidgetItem *item = findItemByPath(filePath);
  if (item) {
    // 通过自定义数据角色存储修改状态，由 ModifiedFileDelegate 绘制红色 "*"
    item->setData(0, Qt::UserRole + 2, modified);
  }
}

// ════════════════════════════════════════════════════════════
//  setFileError / clearFileError — 设置/清除文件错误状态
// ════════════════════════════════════════════════════════════

void TreeDir::setFileError(const QString &filePath, int errorCount) {
  QTreeWidgetItem *item = findItemByPath(filePath);
  if (item) {
    // 存储错误数量，由 ModifiedFileDelegate 绘制红色文件名和错误数量徽章
    item->setData(0, Qt::UserRole + 3, errorCount);
  }
}

void TreeDir::clearFileError(const QString &filePath) {
  QTreeWidgetItem *item = findItemByPath(filePath);
  if (item) {
    // 清除错误数量
    item->setData(0, Qt::UserRole + 3, 0);
  }
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
  // 选中并滚动到可见
  setCurrentItem(item);
  scrollToItem(item, QAbstractItemView::PositionAtCenter);
}