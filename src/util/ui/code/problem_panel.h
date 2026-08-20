/**
 * @file problem_panel.h
 * @brief 问题列表控件 — 可跳转的错误/警告列表（VSCode 风格）
 *
 * 基于 QTreeWidget 的只读问题列表，聚合展示整个工作区已打开文件的验证结果：
 * - 两列：问题描述、位置（文件名:行）
 * - 错误红色、警告橙色，按文件分组、文件内按行号排序
 * - 双击条目发出 issueActivated 信号，由外部打开对应文件并定位到行
 */

#pragma once

#include <QTreeWidget>
#include <QVector>

#include "src/engine/validation_result.h"

class QContextMenuEvent;
class QKeyEvent;
class QMouseEvent;

/// 问题面板中的单个问题项（含完整文件路径，支持跨文件跳转）
struct IssueItem {
  QString filePath;  // 完整文件路径
  QString fileName;  // 显示用文件名
  int line = 0;      // 1-based；0 表示无法定位
  QString message;
  ValidationResult::Severity severity = ValidationResult::kError;
};

class ProblemPanel : public QTreeWidget {
  Q_OBJECT

public:
  explicit ProblemPanel(QWidget *parent = nullptr);

  /// 设置整个工作区的问题列表（多文件聚合，issues 为空则清空）
  void setIssues(const QVector<IssueItem> &issues);
  /// 清空问题列表
  void clearIssues();
  /// 主题 / 字体变化后刷新（背景/文字/条目颜色随当前主题重建）
  void reloadStyle();

signals:
  /// 双击问题项：跳转到对应文件的指定行（line 为 1-based，0 表示无法定位）
  void issueActivated(const QString &filePath, int line);

protected:
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  QVector<IssueItem> m_lastIssues;  ///< 最近一次问题列表（主题刷新时重绘使用）

  /// 将问题条目格式化为可复制文本（每行一条："消息 (文件名:行)"）
  QString formatIssuesForCopy(const QList<QTreeWidgetItem *> &items) const;
};
