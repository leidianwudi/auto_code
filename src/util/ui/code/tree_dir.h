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

/// 启动项标记（绿色三角）宽度（px），绘制在文件图标左侧空隙处
constexpr int kTreeStartupTriWidth = 7;
/// 启动项标记数据角色：节点是否为启动项（.ac 文件）→ bool
constexpr int kTreeStartupRole = Qt::UserRole + 4;
/// 拖拽可放置目标标记数据角色：节点是否为当前拖拽目标 → bool
/// （setData 触发 Qt 自动重绘该行，自绘 delegate 据此整行变色提示目的地）
constexpr int kTreeDropTargetRole = Qt::UserRole + 6;

/// 文件树绘制代理 — 自绘复选框、图标与文本；
/// - 已修改文件/文件夹名称以琥珀色显示（VSCode 风格，替代原来的实心圆点）
/// - 启动项 .ac 文件在图标左侧绘制绿色右向三角标记（不移动图标文字位置）
/// - 有错误的节点以红色显示并在行最右侧绘制错误数量徽章（父文件夹显示子文件错误次数总和）
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

  /// 删除文件/文件夹后调用：从持久化勾选列表（tree.config checked）中移除该路径，
  /// 包含该文件自身及该目录下的所有文件，并立即保存配置。
  /// 否则 getCheckedFiles() 仍会返回已删除文件（如脚本执行时的"文件重复"误报）。
  void pruneCheckedByPath(const QString &path);

  /// 重命名/移动后调用：同步更新持久化勾选列表（tree.config checked）中的路径，
  /// 使勾选记录跟随文件/文件夹的新位置（isDir=true 时级联更新目录下所有文件），
  /// 避免 getCheckedFiles() 返回失效路径
  void renameCheckedByPath(const QString &oldPath, const QString &newPath, bool isDir);

  /// 获取所有被设为启动项的 .ac 文件绝对路径
  QStringList startupFiles() const;
  /// 获取当前选中的启动项绝对路径
  QString selectedStartup() const { return m_selectedStartup; }
  /// 设置当前选中的启动项（由外部下拉框联动）
  void setSelectedStartup(const QString &path);

  /// 移除某个启动项（下拉框删除按钮调用）；若为当前选中项则同步清除选中
  void removeStartup(const QString &filePath);

  /// 获取可视化编辑按钮状态
  bool visualToggle() const { return m_visualToggle; }
  /// 设置可视化编辑按钮状态并保存
  void setVisualToggle(bool enabled);

  /// 当前悬停的节点（由 viewportEvent 实时更新，供 delegate 整行高亮）
  const QTreeWidgetItem *hoverItem() const { return m_hoverItem; }

  /// 重命名路径时更新启动项数据（m_startupFiles 和 m_selectedStartup）
  void renameStartupPath(const QString &oldPath, const QString &newPath);

  /// 批量重命名启动项路径（文件夹重命名时使用，只触发一次信号和保存）
  void renameStartupPaths(const QList<QPair<QString, QString>> &renames);

  /// 设置文件的修改状态（树节点文本追加/移除 " *"）
  void setFileModified(const QString &filePath, bool modified);

  /// 设置文件的错误状态（文件名显示红色，错误数量徽章右对齐到行最右缘，
  /// 父文件夹同步显示子文件错误次数总和）
  void setFileError(const QString &filePath, int errorCount);

  /// 清除文件的错误状态
  void clearFileError(const QString &filePath);

  /// 定位到指定文件路径的节点（选中 + 展开父节点 + 滚动到可见）
  void locateFile(const QString &filePath);

  /// 按文件名过滤：仅显示名称包含 text（不区分大小写）的文件及其父目录链，
  /// 空串清除过滤并恢复完整树（不影响勾选/展开状态持久化）
  void filterByText(const QString &text);

signals:
  /// 双击非 json 文件时发射，携带文件绝对路径
  void fileActivated(const QString &filePath);
  /// 启动项列表变化时发射
  void startupItemsChanged();
  /// 请求重命名，携带旧绝对路径和新文件名（仅文件名，不含目录）
  void renameRequested(const QString &oldPath, const QString &newName);
  /// 请求移动（目录树拖拽），携带待移动的绝对路径和目标文件夹绝对路径
  void moveRequested(const QString &oldPath, const QString &targetDir);
  /// 请求删除，携带待删除的绝对路径
  void deleteRequested(const QString &path);

protected:
  /// 重写 viewportEvent 手动追踪鼠标悬停节点（QEvent::MouseMove/Leave），
  /// 供 ModifiedFileDelegate 整行高亮（见 tree_dir.cpp 实现）
  bool viewportEvent(QEvent *event) override;

  /// 记录按下节点（拖拽源）与起始位置
  void mousePressEvent(QMouseEvent *event) override;
  /// 拖动超过阈值时启动文件拖拽
  void mouseMoveEvent(QMouseEvent *event) override;
  /// 拖拽进入：仅接受本控件发起的移动拖拽
  void dragEnterEvent(QDragEnterEvent *event) override;
  /// 拖拽移动：根据目标合法性接受/忽略，并更新目标节点高亮
  void dragMoveEvent(QDragMoveEvent *event) override;
  /// 拖放：校验后发射 moveRequested
  void dropEvent(QDropEvent *event) override;
  /// 拖拽离开控件：清除目标高亮
  void dragLeaveEvent(QDragLeaveEvent *event) override;

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

  /// 后序重算文件夹复选框状态（三态聚合：全勾/全不勾/部分勾小方块）
  /// 配置恢复后调用——json 文件勾选已就位，文件夹状态需由子节点聚合得出
  void recomputeFolderCheckStates(QTreeWidgetItem *item);

  /// 递归收集所有 json 文件路径（绝对路径）
  void collectJsonFiles(QTreeWidgetItem *item, QStringList &files) const;

  /// 递归设置节点的勾选状态
  void applyStateToTree(QTreeWidgetItem *item, const QStringList &checkedAbsPaths);

  /// 按文件后缀返回对应的类型图标（ac / json(jsonvue) / tpl）
  QIcon iconForSuffix(const QString &suffix) const;

  /// 重新生成文件与文件夹图标并应用到所有节点（主题/颜色变化时调用）
  void refreshIcons();

  /// 根据启动项集合更新树中所有 .ac 文件的启动标记（kTreeStartupRole）
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

  /// 启动文件/文件夹拖拽（携带源绝对路径）
  void startFileDrag(QTreeWidgetItem *item);
  /// 计算拖放目标文件夹并校验合法性（合法返回 true 并填 targetDir）
  bool resolveDropTarget(const QString &srcPath, QTreeWidgetItem *target,
                         QString &targetDir) const;

  /// 右键菜单：文件设为/取消启动项，文件夹新建/刷新/重命名，文件重命名
  void contextMenuEvent(QContextMenuEvent *event) override;

  /// 从设置读取目录树字体大小并应用（构造与字体设置变化时调用）
  void applyFontFromSetting();

  /// 重绘某节点所在行的整行区域（左 0 → 视口右缘），悬停/取消悬停时整行变色
  void repaintRow(const QTreeWidgetItem *item);

  bool m_lastClickOnCheckbox = false;  ///< 最近一次鼠标释放是否落在复选框区域
  bool m_bulkUpdating = false;         ///< 批量更新中，抑制 itemChanged 级联

  // ── 拖拽移动状态 ──
  QTreeWidgetItem *m_pressItem = nullptr;  ///< 按下时命中的节点（潜在拖拽源）
  QPoint m_pressPos;                       ///< 按下位置（判定拖动阈值）
  QString m_dragSourcePath;                ///< 正在拖拽的源绝对路径（拖拽期间有效）

  QString m_rootPath;    ///< 当前展示的根目录
  QString m_configPath;  ///< tree.config 完整路径

  QSet<QString> m_startupFiles;  ///< 被设为启动项的 .ac 文件绝对路径集合
  QString m_selectedStartup;     ///< 当前下拉框选中的启动项路径

  class QTreeWidgetItem *m_hoverItem = nullptr;  ///< 当前鼠标悬停的节点（用于整行高亮）
  QTreeWidgetItem *m_dropRoleItem = nullptr;  ///< 当前设置了拖拽目标角色（kTreeDropTargetRole）的节点

  bool m_visualToggle = false;  ///< 可视化编辑按钮状态

  QIcon m_acIcon;          ///< .ac 文件图标（蓝色「A」）
  QIcon m_jsonIcon;        ///< .json 文件图标（琥珀「J」）
  QIcon m_jsonVueIcon;     ///< .jsonvue 文件图标（琥珀「V」）
  QIcon m_jsonSourceIcon;  ///< .jsonsource 文件图标（琥珀「S」）
  QIcon m_tplIcon;         ///< .tpl 文件图标（绿色「T」）
  QIcon m_folderIcon;      ///< 文件夹收起图标
  QIcon m_folderOpenIcon;  ///< 文件夹展开图标

  TreeStateStore m_store;         ///< 状态持久化数据层（相对路径）
  QTimer *m_saveTimer = nullptr;  ///< 保存防抖定时器
};