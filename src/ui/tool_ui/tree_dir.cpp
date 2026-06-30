/**
 * @file tree_dir.cpp
 * @brief 可打勾的文件树控件实现
 */

#include "tree_dir.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
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
  m_rootPath = dirPath;
  m_configPath = dirPath + QStringLiteral("/tree.config");

  addDirectoryToTree(nullptr, dirPath);
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

  QStringList checkedFiles;
  for (int i = 0; i < topLevelItemCount(); ++i)
    collectJsonFiles(topLevelItem(i), checkedFiles);

  QJsonArray arr;
  for (const QString &absPath : checkedFiles) {
    if (absPath.startsWith(m_rootPath)) {
      QString rel = absPath.mid(m_rootPath.length() + 1);
      arr.append(rel);
    }
  }

  QJsonObject root;
  root[QStringLiteral("checked")] = arr;

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

  QJsonArray arr = doc.object()[QStringLiteral("checked")].toArray();
  QStringList checkedFiles;
  for (const QJsonValue &v : arr) {
    if (v.isString())
      checkedFiles.append(m_rootPath + QStringLiteral("/") + v.toString());
  }

  for (int i = 0; i < topLevelItemCount(); ++i)
    applyStateToTree(topLevelItem(i), checkedFiles, m_rootPath);
}

// ============================================================================
// 辅助：收集 / 应用勾选状态
// ============================================================================

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

void TreeDir::applyStateToTree(QTreeWidgetItem *item,
                               const QStringList &checkedFiles,
                               const QString &rootPath) {
  // 检查当前节点本身（处理根目录下的 .json 文件）
  QString selfPath = item->data(0, Qt::UserRole + 1).toString();
  if (!selfPath.isEmpty() &&
      selfPath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
    if (checkedFiles.contains(selfPath))
      item->setCheckState(0, Qt::Checked);
    return; // 文件节点无子节点，无需递归
  }

  // 目录节点：递归处理子节点
  for (int i = 0; i < item->childCount(); ++i) {
    QTreeWidgetItem *child = item->child(i);
    QString filePath = child->data(0, Qt::UserRole + 1).toString();

    if (!filePath.isEmpty() &&
        filePath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
      if (checkedFiles.contains(filePath))
        child->setCheckState(0, Qt::Checked);
    }

    applyStateToTree(child, checkedFiles, rootPath);

    if (child->childCount() > 0)
      updateParentCheckState(child);
  }
}