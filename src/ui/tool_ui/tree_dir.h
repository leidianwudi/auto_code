/**
 * @file tree_dir.h
 * @brief 可打勾的文件树控件
 *
 * 继承 QTreeWidget，提供：
 * - 扫描目录构建文件树（.ac / .json）
 * - 对 .json 文件自动添加复选框
 * - 文件夹级联选中/取消
 * - 打勾状态持久化到 file/tree.config
 */

#pragma once

#include <QJsonArray>
#include <QTreeWidget>

class QTreeWidgetItem;

/**
 * @class TreeDir
 * @brief 文件树控件
 */
class TreeDir : public QTreeWidget {
  Q_OBJECT

public:
  explicit TreeDir(QWidget *parent = nullptr);

  /// 扫描 dirPath 并构建文件树（自动加载已有勾选状态）
  void buildTree(const QString &dirPath);

  /// 展开所有节点
  void expandAllNodes();

  /// 从 file/tree.config 加载勾选状态
  void loadState();

  /// 将当前勾选状态保存到 file/tree.config
  void saveState();

signals:
  /// 双击非 json 文件时发射，携带文件绝对路径
  void fileActivated(const QString &filePath);

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

  /// 递归设置节点的勾选状态（从已加载的路径集合匹配）
  void applyStateToTree(QTreeWidgetItem *item, const QStringList &checkedFiles,
                        const QString &rootPath);

  /// 拦截鼠标释放以判断点击位置是否在复选框区域
  void mouseReleaseEvent(QMouseEvent *event) override;

  bool m_lastClickOnCheckbox = false; ///< 最近一次鼠标释放是否落在复选框区域
  bool m_bulkUpdating = false;        ///< 批量更新中，抑制 itemChanged 级联

  QString m_rootPath;   ///< 当前展示的根目录
  QString m_configPath; ///< tree.config 完整路径
};