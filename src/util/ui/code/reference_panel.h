/**
 * @file reference_panel.h
 * @brief 引用面板（VSCode「查找所有引用」结果视图）
 *
 * 与查找面板（SearchPanel）相互独立，仅用于展示跨文件引用结果：
 * - 顶部：符号名 + 刷新按钮 + 汇总「在 N 个文件中有 M 个结果」
 * - 无输入框（引用由右键菜单 / Shift+F12 触发）
 * - 结果按文件分组的树：文件节点（相对路径 + 数量）→ 引用行节点
 *   （弱化色行号列 + 正文内容列，VSCode 引用视图风格）
 * - 单击引用行发出 openRequested 信号，由外部打开文件并定位选中
 * - 样式与字体跟随主题 / 代码字体设置（reloadStyle）
 */

#pragma once

#include <QString>
#include <QVector>
#include <QWidget>

#include "src/util/ui/code/vsc_result_tree.h"

class QLabel;
class QPushButton;
class QTreeWidgetItem;

/// 单个引用位置
struct ReferenceMatch {
  QString filePath;  // 完整文件路径
  int line = 0;      // 1-based 行号
  int column = 0;    // 0-based 匹配起始列
  int length = 0;    // 匹配长度
  QString lineText;  // 整行文本（用于显示）
};

class ReferencePanel : public QWidget {
  Q_OBJECT

public:
  explicit ReferencePanel(QWidget *parent = nullptr);

  /// 设置引用搜索根目录（工作区）
  void setSearchRoot(const QString &rootPath);

  /// 跨文件查找符号引用并展示（VSCode「查找所有引用」）
  void findReferences(const QString &symbolName);

  /// 清空结果与符号名
  void clear();

  /// 当前符号名（供外部在切回引用面板时恢复编辑器引用高亮）
  const QString &symbolName() const { return m_symbolName; }

  /// 主题切换后刷新（面板背景 / 头部标签），供外部显式调用
  void refreshStyle();

signals:
  /// 单击结果项：请求打开文件并定位选中（column/length 为 0-based，用于选中匹配词）
  void openRequested(const QString &filePath, int line, int column, int length);

private slots:
  /// 单击结果项跳转
  void onItemClicked(QTreeWidgetItem *item, int column);

private:
  void setupUI();
  /// 应用主题样式（面板背景 / 头部标签）
  void reloadStyle();
  /// 重建结果树
  void buildTree();
  /// 更新顶部汇总标签
  void updateSummary();
  /// 该文件是否应参与扫描（排除 build 目录、二进制等）
  bool shouldScanFile(const QString &filePath) const;

  QPushButton *m_collapseBtn = nullptr;  ///< 全部折叠按钮
  QLabel *m_symbolLabel = nullptr;       ///< 符号名标签
  QLabel *m_summaryLabel = nullptr;      ///< 汇总标签（N 个文件 M 个结果）
  VscResultTree *m_resultTree = nullptr;

  QString m_searchRoot;              ///< 搜索根目录
  QString m_symbolName;              ///< 当前符号名
  QVector<ReferenceMatch> m_matches; ///< 最近一次引用结果
};
