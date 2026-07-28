/**
 * @file icon_picker_dialog.h
 * @brief 图标选择对话框
 *
 * 支持 3 套图标库（Element Plus / Ant Design / TDesign），标签切换，翻页浏览。
 * 通过 IconLoader 单例异步加载真实 SVG 图标并渲染。
 * 图标格式：vi-{prefix}:{name}，如 vi-ep:edit
 */

#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTabWidget;

class IconPickerDialog : public QDialog {
  Q_OBJECT

public:
  explicit IconPickerDialog(const QString &currentIcon = QString(), QWidget *parent = nullptr);
  ~IconPickerDialog() override = default;

  QString selectedIcon() const;

private:
  void setupUI();
  /// 切换到指定标签页并填充图标
  void switchTab(int tabIndex);
  /// 填充当前页的图标网格
  void fillPage();
  /// 重建页码按钮
  void rebuildPageButtons();

  /// 获取各图标库的图标列表
  static QStringList getEpIcons();
  static QStringList getAntIcons();
  static QStringList getTdIcons();

  QTabWidget *m_tabWidget = nullptr;
  QLineEdit *m_searchEdit = nullptr;
  QTableWidget *m_iconTable = nullptr;
  QLabel *m_selectedLabel = nullptr;
  QPushButton *m_prevBtn = nullptr;
  QPushButton *m_nextBtn = nullptr;
  QHBoxLayout *m_pageBtnLayout = nullptr;  ///< 页码按钮布局
  QList<QPushButton *> m_pageButtons;      ///< 页码按钮列表

  QString m_selectedIcon;
  int m_currentTab = 0;         ///< 当前标签页索引
  int m_currentPage = 0;        ///< 当前页码
  int m_totalPages = 0;         ///< 总页数
  QStringList m_filteredIcons;  ///< 当前筛选后的图标列表

  static constexpr int kCols = 12;                 ///< 网格列数
  static constexpr int kRows = 10;                 ///< 网格行数
  static constexpr int kPageSize = kCols * kRows;  ///< 每页图标数
  static constexpr int kIconSize = 32;             ///< 图标显示尺寸
  static constexpr int kCellSize = 48;             ///< 单元格尺寸
};