/**
 * @file reference_panel.h
 * @brief 引用面板（VSCode「查找所有引用」结果视图）
 *
 * 与查找面板（SearchPanel）相互独立，仅用于展示跨文件引用结果：
 * - 顶部：符号名 + 汇总「在 N 个文件中有 M 个结果」+ 全部折叠按钮
 * - 无输入框（引用由右键菜单 / Shift+F12 触发）
 * - 结果按文件分组的树：文件节点（相对路径 + 数量）→ 引用行节点
 * - 单击引用行发出 openRequested 信号，由外部打开文件并定位
 * - 样式与字体跟随主题 / 代码字体设置（refreshStyle）
 *
 * 骨架（结果树 / 汇总 / 折叠按钮 / 文件过滤 / 结果构建）继承自 VscResultPanel，
 * 本类仅负责引用特有的头部行（符号名）与跨文件引用查找逻辑。
 */

#pragma once

#include <QString>
#include <QVector>
#include <QWidget>

#include "src/util/ui/code/vsc_result_panel.h"

class QLabel;

class ReferencePanel : public VscResultPanel {
  Q_OBJECT

public:
  /// 单个引用位置（复用基类统一 Match 结构）
  using Match = VscResultPanel::Match;

  explicit ReferencePanel(QWidget *parent = nullptr);

  /// 跨文件查找符号引用并展示（VSCode「查找所有引用」）
  void findReferences(const QString &symbolName);

  /// 清空结果与符号名
  void clear();

  /// 当前符号名（供外部在切回引用面板时恢复编辑器引用高亮）
  const QString &symbolName() const { return m_symbolName; }

  /// 主题切换后刷新（面板背景 / 头部标签 / 汇总 / 结果树），供外部显式调用
  void refreshStyle() override;

private:
  void setupUI();
  /// 更新顶部汇总标签
  void updateSummary();

  QLabel *m_symbolLabel = nullptr;       ///< 符号名标签
  QString m_symbolName;                  ///< 当前符号名
  QVector<Match> m_matches;              ///< 最近一次引用结果
};
