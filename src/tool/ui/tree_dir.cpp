/**
 * @file tree_dir.cpp
 * @brief 可打勾的文件树控件实现
 */

#include "tree_dir.h"

#include "src/tool/ui/aui_button.h"

#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTreeWidgetItem>

// ============================================================================
// 构造
// ============================================================================

TreeDir::TreeDir(QWidget *parent) : QTreeWidget(parent) {
  setHeaderLabel(QStringLiteral("文件"));
  setMinimumWidth(200);
  setMaximumWidth(400);
  setAnimated(true);
  setIndentation(16);
  setSortingEnabled(false);

  connect(this, &QTreeWidget::itemClicked, this, &TreeDir::onItemClicked);
  connect(this, &QTreeWidget::itemDoubleClicked, this,
          &TreeDir::onItemDoubleClicked);
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
    int cbWidth =
        style()->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, this);
    if (cbWidth < 10)
      cbWidth = 16; // 默认值

    QRect itemRect = visualRect(indexFromItem(item));
    int clickX = static_cast<int>(event->position().x()) - itemRect.x();
    // 复选框通常绘制在 item 左侧，留 4px 边距
    if (clickX >= 0 && clickX < cbWidth + 4)
      m_lastClickOnCheckbox = true;
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

  expandAll();
  loadState();
}

// ============================================================================
// 单击 / 双击
// ============================================================================

void TreeDir::onItemClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  if (!item)
    return;

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
      saveState();
    }
    return;
  }

  // 文件节点
  if (filePath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
    if (m_lastClickOnCheckbox) {
      // 单击复选框 → Qt 已自动切换复选框，刷新状态
      updateParentCheckState(item);
      saveState();
    } else {
      // 单击文本/图标区域 → 打开文件
      emit fileActivated(filePath);
    }
  } else {
    // .ac 文件：直接打开
    emit fileActivated(filePath);
  }
}

void TreeDir::onItemDoubleClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  if (!item)
    return;

  const QString filePath = item->data(0, Qt::UserRole + 1).toString();
  if (!filePath.isEmpty())
    emit fileActivated(filePath);
}

// ============================================================================
// onItemChanged — 复选框状态变化时级联更新
// ============================================================================

void TreeDir::onItemChanged(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  if (!item || m_bulkUpdating)
    return;

  const QString filePath = item->data(0, Qt::UserRole + 1).toString();

  if (!filePath.isEmpty() &&
      filePath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
    // json 文件的复选框变化：更新父节点状态
    updateParentCheckState(item);
    saveState();
  }
}

// ============================================================================
// addDirectoryToTree — 递归添加目录/文件
// ============================================================================

void TreeDir::addDirectoryToTree(QTreeWidgetItem *parentItem,
                                 const QString &dirPath) {
  QDir dir(dirPath);

  // 文件（.ac 和 .json）
  QStringList nameFilters;
  nameFilters << QStringLiteral("*.ac") << QStringLiteral("*.json");
  QFileInfoList files = dir.entryInfoList(nameFilters, QDir::Files);

  int jsonCount = 0;
  for (const QFileInfo &info : files) {
    if (info.fileName().endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
      ++jsonCount;
  }

  // 子目录
  QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
  int childDirJsonCount = 0;

  struct SubDirInfo {
    QTreeWidgetItem *item;
    QString path;
  };
  QList<SubDirInfo> subDirs;
  for (const QFileInfo &info : dirs) {
    auto *dirItem = parentItem ? new QTreeWidgetItem(parentItem)
                               : new QTreeWidgetItem(this);
    dirItem->setText(0, info.fileName());
    dirItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    dirItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    subDirs.append({dirItem, info.absoluteFilePath()});
  }

  // 递归子目录并统计 .json 子项数
  for (const auto &sd : subDirs) {
    addDirectoryToTree(sd.item, sd.path);
    for (int i = 0; i < sd.item->childCount(); ++i) {
      QTreeWidgetItem *child = sd.item->child(i);
      if (child->flags() & Qt::ItemIsUserCheckable)
        ++childDirJsonCount;
    }
  }

  // 添加文件节点
  for (const QFileInfo &info : files) {
    auto *fileItem = parentItem ? new QTreeWidgetItem(parentItem)
                                : new QTreeWidgetItem(this);
    fileItem->setText(0, info.fileName());
    fileItem->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
    fileItem->setData(0, Qt::UserRole + 1, info.absoluteFilePath());

    if (info.fileName().endsWith(QStringLiteral(".json"),
                                 Qt::CaseInsensitive)) {
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

void TreeDir::setJsonChildrenCheckState(QTreeWidgetItem *item,
                                        Qt::CheckState state) {
  for (int i = 0; i < item->childCount(); ++i) {
    QTreeWidgetItem *child = item->child(i);
    QString filePath = child->data(0, Qt::UserRole + 1).toString();

    if (!filePath.isEmpty() &&
        filePath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
      child->setCheckState(0, state);

    setJsonChildrenCheckState(child, state);
  }
}

void TreeDir::updateParentCheckState(QTreeWidgetItem *item) {
  QTreeWidgetItem *parent = item->parent();
  if (!parent)
    return;

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
  if (m_configPath.isEmpty())
    return;

  // 确保父目录存在
  QFileInfo fi(m_configPath);
  QDir().mkpath(fi.absolutePath());

  // ── 勾选的 json 文件 ──
  QStringList checkedFiles;
  for (int i = 0; i < topLevelItemCount(); ++i)
    collectJsonFiles(topLevelItem(i), checkedFiles);

  QJsonArray checkedArr;
  for (const QString &absPath : checkedFiles) {
    if (absPath.startsWith(m_rootPath)) {
      QString rel = absPath.mid(m_rootPath.length() + 1);
      checkedArr.append(rel);
    }
  }

  // ── 启动项 .ac 文件 ──
  QJsonArray startupArr;
  for (const QString &absPath : m_startupFiles) {
    if (absPath.startsWith(m_rootPath)) {
      QString rel = absPath.mid(m_rootPath.length() + 1);
      startupArr.append(rel);
    }
  }

  // ── 当前选中的启动项 ──
  QString selectedRel;
  if (!m_selectedStartup.isEmpty() && m_selectedStartup.startsWith(m_rootPath))
    selectedRel = m_selectedStartup.mid(m_rootPath.length() + 1);

  QJsonObject root;
  root[QStringLiteral("checked")] = checkedArr;
  root[QStringLiteral("startup")] = startupArr;
  if (!selectedRel.isEmpty())
    root[QStringLiteral("startupSelected")] = selectedRel;

  QFile file(m_configPath);
  if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void TreeDir::loadState() {
  if (m_configPath.isEmpty())
    return;

  QFile file(m_configPath);
  if (!file.open(QIODevice::ReadOnly))
    return;

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (doc.isNull() || !doc.isObject())
    return;

  QJsonObject obj = doc.object();

  // ── 恢复勾选状态 ──
  QJsonArray arr = obj[QStringLiteral("checked")].toArray();

  // 将相对路径还原为绝对路径并统一格式
  QStringList checkedAbsPaths;
  for (const QJsonValue &v : arr) {
    if (v.isString()) {
      // 用 "/" 拼接相对路径，再统一 cleanPath
      QString abs =
          QDir::cleanPath(m_rootPath + QStringLiteral("/") + v.toString());
      checkedAbsPaths.append(abs);
    }
  }

  // 递归应用到树节点
  for (int i = 0; i < topLevelItemCount(); ++i)
    applyStateToTree(topLevelItem(i), checkedAbsPaths);

  // ── 恢复启动项 ──
  QJsonArray startupArr = obj[QStringLiteral("startup")].toArray();
  m_startupFiles.clear();
  for (const QJsonValue &v : startupArr) {
    if (v.isString()) {
      QString abs =
          QDir::cleanPath(m_rootPath + QStringLiteral("/") + v.toString());
      m_startupFiles.insert(abs);
    }
  }

  // ── 恢复当前选中的启动项 ──
  QString selRel = obj[QStringLiteral("startupSelected")].toString();
  if (!selRel.isEmpty())
    m_selectedStartup =
        QDir::cleanPath(m_rootPath + QStringLiteral("/") + selRel);
  else
    m_selectedStartup.clear();

  // 刷新启动项图标
  refreshStartupIcons();

  // 通知外部启动项已恢复
  emit startupItemsChanged();
}

// ============================================================================
// 辅助：收集 / 应用勾选状态
// ============================================================================

QStringList TreeDir::checkedJsonFiles() const {
  QStringList files;
  for (int i = 0; i < topLevelItemCount(); ++i)
    collectJsonFiles(topLevelItem(i), files);
  return files;
}

void TreeDir::collectJsonFiles(QTreeWidgetItem *item,
                               QStringList &files) const {
  // 检查当前节点本身（处理根目录下的 .json 文件）
  QString selfPath = item->data(0, Qt::UserRole + 1).toString();
  if (!selfPath.isEmpty() &&
      selfPath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
    if (item->checkState(0) == Qt::Checked)
      files.append(selfPath);
  }

  // 递归检查子节点
  for (int i = 0; i < item->childCount(); ++i)
    collectJsonFiles(item->child(i), files);
}

/// 检查绝对路径是否匹配某个已保存的绝对路径
static bool pathListContains(const QStringList &list, const QString &absPath) {
  for (const QString &p : list) {
    // 同时 cleanPath（主要处理分隔符），轻松对比
    if (QDir::cleanPath(p) == QDir::cleanPath(absPath))
      return true;
  }
  return false;
}

void TreeDir::applyStateToTree(QTreeWidgetItem *item,
                               const QStringList &checkedAbsPaths) {
  // 检查当前节点本身（处理根目录下的 .json 文件）
  QString selfPath = item->data(0, Qt::UserRole + 1).toString();
  if (!selfPath.isEmpty() &&
      selfPath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
    if (pathListContains(checkedAbsPaths, selfPath))
      item->setCheckState(0, Qt::Checked);
    return; // 文件节点无子节点，无需递归
  }

  // 目录节点：递归处理子节点
  for (int i = 0; i < item->childCount(); ++i) {
    QTreeWidgetItem *child = item->child(i);
    QString filePath = child->data(0, Qt::UserRole + 1).toString();

    if (!filePath.isEmpty() &&
        filePath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
      if (pathListContains(checkedAbsPaths, filePath))
        child->setCheckState(0, Qt::Checked);
    }

    applyStateToTree(child, checkedAbsPaths);

    if (child->childCount() > 0)
      updateParentCheckState(child);
  }
}

// ============================================================================
// 启动项管理
// ============================================================================

/// @brief 创建带三角标记的启动项图标 — 委托 AuiButton 叠加绿色三角
QIcon TreeDir::makeStartupIcon() const {
  return AuiButton::createStartupOverlayIcon(
      style()->standardIcon(QStyle::SP_FileIcon));
}

/// @brief 根据 m_startupFiles 集合更新所有 .ac 文件节点的图标
void TreeDir::refreshStartupIcons() {
  QIcon startupIcon = makeStartupIcon();
  QIcon normalIcon = style()->standardIcon(QStyle::SP_FileIcon);

  // 遍历所有顶层节点
  QList<QTreeWidgetItem *> stack;
  for (int i = 0; i < topLevelItemCount(); ++i)
    stack.append(topLevelItem(i));

  while (!stack.isEmpty()) {
    QTreeWidgetItem *item = stack.takeLast();
    for (int i = 0; i < item->childCount(); ++i)
      stack.append(item->child(i));

    QString filePath = item->data(0, Qt::UserRole + 1).toString();
    if (!filePath.isEmpty() &&
        filePath.endsWith(QStringLiteral(".ac"), Qt::CaseInsensitive)) {
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
  for (const QString &p : m_startupFiles)
    result.append(p);
  return result;
}

/// @brief 设置当前选中的启动项
void TreeDir::setSelectedStartup(const QString &path) {
  if (m_selectedStartup != path) {
    m_selectedStartup = path;
    saveState();
  }
}

// ============================================================================
// 右键菜单 — .ac 文件设为/取消启动项
// ============================================================================

void TreeDir::contextMenuEvent(QContextMenuEvent *event) {
  QTreeWidgetItem *item = itemAt(event->pos());
  if (!item) {
    QTreeWidget::contextMenuEvent(event);
    return;
  }

  QString filePath = item->data(0, Qt::UserRole + 1).toString();
  if (filePath.isEmpty() ||
      !filePath.endsWith(QStringLiteral(".ac"), Qt::CaseInsensitive)) {
    QTreeWidget::contextMenuEvent(event);
    return;
  }

  bool isStartup = m_startupFiles.contains(filePath);

  QMenu menu(this);
  if (isStartup) {
    QAction *act = menu.addAction(QStringLiteral("取消启动项"));
    if (menu.exec(event->globalPos()) == act) {
      m_startupFiles.remove(filePath);
      if (m_selectedStartup == filePath)
        m_selectedStartup.clear();
      refreshStartupIcons();
      saveState();
      emit startupItemsChanged();
    }
  } else {
    QAction *act = menu.addAction(QStringLiteral("设为启动项"));
    if (menu.exec(event->globalPos()) == act) {
      m_startupFiles.insert(filePath);
      // 如果此前没有选中的启动项，自动选中第一个
      if (m_selectedStartup.isEmpty())
        m_selectedStartup = filePath;
      refreshStartupIcons();
      saveState();
      emit startupItemsChanged();
    }
  }
}