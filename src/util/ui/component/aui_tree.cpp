/**
 * @file aui_tree.cpp
 * @brief UI 公共列表/树控件工厂类实现
 */

#include "aui_tree.h"

#include <QHeaderView>
#include <QModelIndex>
#include <QPainter>
#include <QPen>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTreeWidget>

#include "aui_style.h"

/// @brief 列表单元格委托：在列与列之间绘制竖线（最后一列不画，避免与右边界线重叠）
class AuiTreeGridDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyledItemDelegate::paint(painter, option, index);
    if (index.column() >= index.model()->columnCount() - 1) return;
    painter->save();
    painter->setPen(QPen(AuiStyle::borderColor()));
    const QRect r = option.rect;
    painter->drawLine(r.right(), r.top(), r.right(), r.bottom());
    painter->restore();
  }
};

/// @brief 列表样式表：交替行、选中/悬停态、行内边距、表头
QString AuiTree::listStyleSheet() {
  return QStringLiteral(
             "QTreeWidget {"
             "  background: %1;"
             "  border: 1px solid %2;"
             "  outline: 0;"
             "  margin: 1px;"
             "}"
             "QTreeWidget::item {"
             "  padding: 2px 3px;"
             "}"
             "QTreeWidget::item:alternate {"
             "  background: %3;"
             "}"
             "QTreeWidget::item:hover {"
             "  background: %4;"
             "}"
             "QTreeWidget::item:selected {"
             "  background: %5;"
             "  color: %6;"
             "}"
             "QTreeWidget::item:selected:!active {"
             "  background: %4;"
             "}"
             "QHeaderView::section {"
             "  background: %1;"
             "  border: none;"
             "  border-right: 1px solid %2;"
             "  border-bottom: 1px solid %2;"
             "  padding: 3px 6px;"
             "  font-weight: bold;"
             "}")
      .arg(AuiStyle::panelBackground().name(), AuiStyle::borderColor().name(),
           AuiStyle::listAlternateBackground().name(), AuiStyle::listHoverBackground().name(),
           AuiStyle::listSelectionBackground().name(), AuiStyle::textColor().name(),
           AuiStyle::borderColor().name());
}

/// @brief 创建风格统一的列表树控件（交替行 + 可拖动列宽 + 应用列表样式）
QTreeWidget *AuiTree::createListTree() {
  auto *tree = new QTreeWidget;
  tree->setAlternatingRowColors(true);
  tree->setStyleSheet(listStyleSheet());
  // 所有列均可拖动列边线调整宽度；最后一列自动拉伸填满剩余宽度，
  // 使列宽总和小于父控件宽度时右边界线也能贴合控件右缘
  tree->header()->setStretchLastSection(true);
  tree->header()->setSectionResizeMode(QHeaderView::Interactive);
  // 列与列之间的竖线（表头用样式表的 border-right，行内容用自绘委托）
  tree->setItemDelegate(new AuiTreeGridDelegate(tree));
  return tree;
}