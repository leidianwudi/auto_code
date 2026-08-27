/**
 * @file search_panel.h
 * @brief 跨文件搜索面板（查找 / 引用）
 *
 * 类似 VSCode 左侧搜索面板：
 * - 输入框 + 区分大小写(Aa) / 全词匹配(\b) 复选框
 * - 顶部汇总"X 个文件有 M 个结果"
 * - QTreeWidget 结果树：文件节点 → 匹配行节点（行号 + 行文本）
 * - 单击结果行发出 openRequested 信号，由外部打开文件、定位并选中匹配词
 *
 * 查找与引用两个左侧 tab 复用本控件（仅标题不同）。
 */

#pragma once

#include <QString>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QTreeWidget;
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
  QTreeWidget *m_resultTree = nullptr;

  QString m_searchRoot;            ///< 搜索根目录
  QVector<SearchMatch> m_matches;  ///< 最近一次搜索结果（供汇总计数）
};
