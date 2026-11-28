/**
 * @file icon_picker_dialog.h
 * @brief 图标选择对话框
 *
 * 图标集与 shop IconSelect 对齐（Element / Remix / Ant Design / Material），
 * 数据来自 IconLoader 的本地图标集，不依赖网络；标签切换，翻页浏览。
 * 图标格式：{prefix}:{name}，如 ep:edit（兼容旧 vi-ep:edit）。
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

  /// 获取标签页对应的本地图标列表（prefix:name）
  static QStringList iconsForTab(int tabIndex);

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

  static constexpr int kCols = 14;                 ///< 网格列数
  static constexpr int kRows = 10;                 ///< 网格行数
  static constexpr int kPageSize = kCols * kRows;  ///< 每页图标数
  static constexpr int kIconSize = 32;             ///< 图标显示尺寸
  static constexpr int kCellSize = 48;             ///< 单元格尺寸
};