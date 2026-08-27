/**
 * @file vsc_result_tree.h
 * @brief VSCode 风格结果树控件（查找 / 引用共用）
 *
 * 统一样式位置：查找面板与引用面板的结果区复用本控件，
 * 保证两者与 VSCode 的搜索/引用结果视图外观一致：
 * - 文件分组节点：加粗正文色 + 相对路径 + 数量
 * - 匹配行节点：弱化色行号 + 正文内容（两段配色）
 * - 整行悬停 / 选中高亮（覆盖到视口右缘），VSCode 风格折叠箭头
 * - 样式与字体跟随主题 / 代码字体设置（reloadStyle）
 */

#pragma once

#include <QTreeWidget>

class QEvent;
class QTreeWidgetItem;

/// 结果树条目数据角色（文件分组节点与匹配行共用）
namespace VscTreeRole {
  enum DataRole {
    FilePath = Qt::UserRole,     ///< 文件完整路径
    Line = Qt::UserRole + 1,     ///< 1-based 行号（0 = 文件分组节点）
    Column = Qt::UserRole + 2,   ///< 0-based 匹配起始列
    Length = Qt::UserRole + 3,   ///< 匹配长度
    Content = Qt::UserRole + 4,  ///< 行内容（用于绘制，已 trim）
  };
}

class VscResultTree : public QTreeWidget {
  Q_OBJECT

public:
  explicit VscResultTree(QWidget *parent = nullptr);

  /// 应用主题 / 字体样式（主题或代码字体变化时自动调用）
  void reloadStyle();
  /// 当前鼠标悬停的条目（供绘制代理整行高亮）
  QTreeWidgetItem *hoverItem() const { return m_hoverItem; }

  /// 新建文件分组节点（顶层，显示相对路径 + 数量由调用方随后补全）
  QTreeWidgetItem *addFileNode(const QString &filePath, const QString &displayText);
  /// 在文件分组节点下添加匹配行节点
  QTreeWidgetItem *addMatchNode(QTreeWidgetItem *fileNode, const QString &filePath, int line,
                                int column, int length, const QString &content);

protected:
  /// 追踪鼠标悬停条目（纯代码 delegate 不自带 State_MouseOver，需手动高亮）
  bool viewportEvent(QEvent *event) override;

private:
  /// 为所有文件分组节点按后缀重建类型图标（主题切换时刷新图标颜色）
  void refreshFileIcons();
  QTreeWidgetItem *m_hoverItem = nullptr;  ///< 鼠标悬停条目
};
