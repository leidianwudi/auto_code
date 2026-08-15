/**
 * @file tree_dir.h
 * @brief 可打勾的文件树控件
 *
 * 继承 QTreeWidget，提供：
 * - 扫描目录构建文件树（.ac / .json）
 * - 对 .json 文件自动添加复选框
 * - 文件夹级联选中/取消
 * - 打勾状态持久化到 file/tree.config
 * - 文件夹右键新建/刷新/重命名，文件右键重命名
 */

#pragma once

#include <QJsonArray>
#include <QSet>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeWidget>

#include "tree_state_store.h"

class QTreeWidgetItem;
class QContextMenuEvent;

/// 文件树绘制代理 — 自绘复选框、图标与文本；已修改文件在图标右上角绘制实心圆点，
/// 有错误的文件以红色加粗显示并附加错误数量
class ModifiedFileDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
};

/**
 * @class TreeDir
 * @brief 文件树控件
 */
class TreeDir : public QTreeWidget {
  Q_OBJECT

public:
  static constexpr int kMinWidth = 20;

public:
  explicit TreeDir(QWidget *parent = nullptr);

  /// 扫描 dirPath 并构建文件树（自动加载已有勾选状态）
  void buildTree(const QString &dirPath);

  /// 获取当前展示的根目录
  QString rootPath() const { return m_rootPath; }

  /// 展开所有节点
  void expandAllNodes();

  /// 从 file/tree.config 加载勾选状态
  void loadState();

  /// 将当前勾选状态保存到 file/tree.config
  void saveState();

  /// 刷新当前目录树（重新扫描文件系统）
  void refreshTree();

  /// 获取当前所有勾选的 json 文件绝对路径
  QStringList checkedJsonFiles() const;

  /// 获取所有被设为启动项的 .ac 文件绝对路径
  QStringList startupFiles() const;
  /// 获取当前选中的启动项绝对路径
  QString selectedStartup() const { return m_selectedStartup; }
  /// 设置当前选中的启动项（由外部下拉框联动）
  void setSelectedStartup(const QString &path);

  /// 获取可视化编辑按钮状态
  bool visualToggle() const { return m_visualToggle; }
  /// 设置可视化编辑按钮状态并保存
  void setVisualToggle(bool enabled);

  /// 重命名路径时更新启动项数据（m_startupFiles 和 m_selectedStartup）
  void renameStartupPath(const QString &oldPath, const QString &newPath);

  /// 批量重命名启动项路径（文件夹重命名时使用，只触发一次信号和保存）
  void renameStartupPaths(const QList<QPair<QString, QString>> &renames);

  /// 设置文件的修改状态（树节点文本追加/移除 " *"）
  void setFileModified(const QString &filePath, bool modified);

  /// 设置文件的错误状态（树节点文本显示红色，并在右侧显示错误数量）
  void setFileError(const QString &filePath, int errorCount);

  /// 清除文件的错误状态
  void clearFileError(const QString &filePath);

  /// 定位到指定文件路径的节点（选中 + 展开父节点 + 滚动到可见）
  void locateFile(const QString &filePath);

signals:
  /// 双击非 json 文件时发射，携带文件绝对路径
  void fileActivated(const QString &filePath);
  /// 启动项列表变化时发射
  void startupItemsChanged();
  /// 请求重命名，携带旧绝对路径和新文件名（仅文件名，不含目录）
  void renameRequested(const QString &oldPath, const QString &newName);
  /// 请求删除，携带待删除的绝对路径
  void deleteRequested(const QString &path);

private slots:
  /// 单击节点
  void onItemClicked(QTreeWidgetItem *item, int column);
  /// 双击节点：发射 fileActivated
  void onItemDoubleClicked(QTreeWidgetItem *item, int column);
  /// 复选框状态变化时级联更新父/子节点
  void onItemChanged(QTreeWidgetItem *item, int column);

private:
  /// 递归添加目录/文件到树
  void addDirectoryToTree(QTreeWidgetItem *parentItem, const QString &dirPath);

  /// 级联更新父节点复选框状态
  void updateParentCheckState(QTreeWidgetItem *item);

  /// 递归选中/取消某个节点下的所有 json 文件
  void setJsonChildrenCheckState(QTreeWidgetItem *item, Qt::CheckState state);

  /// 递归收集所有 json 文件路径（绝对路径）
  void collectJsonFiles(QTreeWidgetItem *item, QStringList &files) const;

  /// 递归设置节点的勾选状态
  void applyStateToTree(QTreeWidgetItem *item, const QStringList &checkedAbsPaths);

  /// 为启动项创建带三角标记的图标
  QIcon makeStartupIcon() const;

  /// 根据启动项集合更新树中所有 .ac 文件的图标
  void refreshStartupIcons();

  /// 按绝对路径查找树节点（找不到返回 nullptr）
  QTreeWidgetItem *findItemByPath(const QString &absPath) const;

  /// 收集所有已勾选 json 文件的相对路径
  QStringList collectCheckedRelPaths() const;
  /// 收集所有启动项 .ac 文件的相对路径
  QStringList collectStartupRelPaths() const;
  /// 收集所有展开目录节点的相对路径
  QStringList collectExpandedRelPaths() const;
  /// 将绝对路径转换为相对根目录的路径（根目录外返回空串）
  QString toRelPath(const QString &absPath) const;

  /// 将保存的勾选状态应用到树
  void applyCheckedToTree(const QStringList &checkedRelPaths);
  /// 将保存的展开状态应用到树（无记录时默认全部展开）
  void applyExpandedToTree(const QStringList &expandedRelPaths);

  /// 防抖保存（复选框频繁变化时合并写入）
  void scheduleSave();

  /// 拦截鼠标释放以判断点击位置是否在复选框区域
  void mouseReleaseEvent(QMouseEvent *event) override;

  /// 右键菜单：文件设为/取消启动项，文件夹新建/刷新/重命名，文件重命名
  void contextMenuEvent(QContextMenuEvent *event) override;

  bool m_lastClickOnCheckbox = false;  ///< 最近一次鼠标释放是否落在复选框区域
  bool m_bulkUpdating = false;         ///< 批量更新中，抑制 itemChanged 级联

  QString m_rootPath;    ///< 当前展示的根目录
  QString m_configPath;  ///< tree.config 完整路径

  QSet<QString> m_startupFiles;  ///< 被设为启动项的 .ac 文件绝对路径集合
  QString m_selectedStartup;     ///< 当前下拉框选中的启动项路径

  bool m_visualToggle = false;  ///< 可视化编辑按钮状态

  TreeStateStore m_store;         ///< 状态持久化数据层（相对路径）
  QTimer *m_saveTimer = nullptr;  ///< 保存防抖定时器
};