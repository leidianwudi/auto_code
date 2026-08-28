/**
 * @file search_panel.h
 * @brief 跨文件搜索面板（查找）
 *
 * 类似 VSCode 左侧搜索面板：
 * - 输入框 + 区分大小写(Aa) / 全词匹配(\b) 复选框（指示器由 AuiStyle 统一绘制）
 * - 顶部汇总"X 个文件有 M 个结果" + 全部折叠按钮
 * - VSCode 风格结果树（VscResultTree）：文件分组节点 → 匹配行节点
 * - 单击结果行发出 openRequested 信号，由外部打开文件、定位并选中匹配词
 *
 * 骨架（结果树 / 汇总 / 折叠按钮 / 文件过滤 / 结果构建）继承自 VscResultPanel，
 * 本类仅负责搜索特有的头部行（输入框 + 选项）与搜索逻辑。
 */

#pragma once

#include <QString>
#include <QVector>
#include <QWidget>

#include "src/util/ui/code/vsc_result_panel.h"

class QCheckBox;
class QLineEdit;
class QTimer;

class SearchPanel : public VscResultPanel {
  Q_OBJECT

public:
  /// 单次匹配结果（复用基类统一 Match 结构）
  using Match = VscResultPanel::Match;

  explicit SearchPanel(QWidget *parent = nullptr);

  /// 触发一次搜索（供外部如编辑器选中词调用）
  void startSearch(const QString &text);
  /// 当前搜索关键词（空串表示未在搜索）
  const QString &currentText() const;

  /// 主题 / 字体切换后刷新（面板背景 + 汇总 + 结果树），供外部显式调用
  void refreshStyle() override;

signals:
  /// 一次搜索完成（实时搜索每输入一次都会触发，供外部同步编辑器高亮）
  void searchPerformed(const QString &text);

protected:
  void showEvent(QShowEvent *event) override;

private slots:
  /// 输入变化实时搜索（防抖后执行）
  void onSearchTextChanged();
  /// 选项变化（大小写/全词）重新搜索
  void onOptionsChanged();

private:
  void setupUI();
  /// 应用面板统一样式（背景随主题 + 复选框/输入框文字样式）
  void applyPanelStyle();
  /// 执行跨文件搜索并构建结果树
  void performSearch();
  /// 更新顶部汇总标签
  void updateSummary();

  QLineEdit *m_searchEdit = nullptr;
  QCheckBox *m_caseCheck = nullptr;
  QCheckBox *m_wordCheck = nullptr;
  QTimer *m_searchDebounceTimer = nullptr;  ///< 搜索防抖定时器（合并连续输入）
  QVector<Match> m_matches;  ///< 最近一次搜索结果（供汇总计数）
};
