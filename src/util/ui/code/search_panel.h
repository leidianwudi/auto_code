/**
 * @file search_panel.h
 * @brief 跨文件搜索面板（查找）
 *
 * 类似 VSCode 左侧搜索面板：
 * - 输入框 + 区分大小写(Aa) / 全词匹配(\b) 复选框（指示器由 AuiStyle 统一绘制）
 * - 顶部汇总"X 个文件有 M 个结果"
 * - VSCode 风格结果树（VscResultTree）：文件分组节点 → 匹配行节点
 * - 单击结果行发出 openRequested 信号，由外部打开文件、定位并选中匹配词
 */

#pragma once

#include <QString>
#include <QVector>
#include <QWidget>

#include "src/util/ui/code/vsc_result_tree.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidgetItem;

/// 单次匹配结果
struct SearchMatch {
  QString filePath;  // 完整文件路径
  int line = 0;      // 1-based 行号
  int column = 0;    // 0-based 匹配起始列
  int length = 0;    // 匹配长度
  QString lineText;  // 整行文本（未 trim，用于显示）
};

class SearchPanel : public QWidget {
  Q_OBJECT

public:
  explicit SearchPanel(QWidget *parent = nullptr);

  /// 设置跨文件搜索根目录
  void setSearchRoot(const QString &rootPath);
  /// 触发一次搜索（供外部如编辑器选中词调用）
  void startSearch(const QString &text);

  /// 主题 / 字体切换后刷新（标签文字色、结果树背景与滚动条），供外部显式调用
  void refreshStyle();

signals:
  /// 单击结果项：请求打开文件并定位选中（column/length 为 0-based，用于选中匹配词）
  void openRequested(const QString &filePath, int line, int column, int length);

protected:
  void showEvent(QShowEvent *event) override;

private slots:
  /// 输入变化实时搜索
  void onSearchTextChanged();
  /// 选项变化（大小写/全词）重新搜索
  void onOptionsChanged();
  /// 单击结果项跳转
  void onItemClicked(QTreeWidgetItem *item, int column);

private:
  void setupUI();
  /// 应用面板统一样式（背景随主题 + 复选框/输入框文字样式）
  void applyPanelStyle();
  /// 执行跨文件搜索并构建结果树
  void performSearch();
  /// 清空结果树与缓存
  void clearResults();
  /// 更新顶部汇总标签
  void updateSummary();
  /// 该文件是否应参与搜索（排除 build 目录、二进制等）
  bool shouldSearchFile(const QString &filePath) const;

  QLineEdit *m_searchEdit = nullptr;
  QCheckBox *m_caseCheck = nullptr;
  QCheckBox *m_wordCheck = nullptr;
  QLabel *m_summaryLabel = nullptr;
  QPushButton *m_collapseBtn = nullptr;  ///< 全部折叠按钮
  VscResultTree *m_resultTree = nullptr;

  QString m_searchRoot;            ///< 搜索根目录
  QVector<SearchMatch> m_matches;  ///< 最近一次搜索结果（供汇总计数）
};
