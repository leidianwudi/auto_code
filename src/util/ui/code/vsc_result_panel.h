/**
 * @file vsc_result_panel.h
 * @brief VSCode 风格结果面板基类（查找 / 引用共用骨架）
 *
 * 查找面板（SearchPanel）与引用面板（ReferencePanel）共用的骨架：
 * - 统一主布局（头部行 + 汇总行 + 结果树，外边距 / 间距统一定义）
 * - 汇总标签 + 「全部折叠」按钮（懒创建）
 * - 文件扫描过滤（shouldScanFile，排除 build/二进制等）
 * - 结果树分组构建（buildResultTree，文件节点 → 匹配行节点）
 * - 结果行点击跳转（openRequested 信号）
 *
 * 子类各自负责头部行的具体内容：
 * - 查找面板：搜索输入框 + Aa/\b 复选框
 * - 引用面板：符号名标签
 * 两者仍保持相互独立文件，仅共享本基类骨架。
 */

#pragma once

#include <QString>
#include <QVector>
#include <QWidget>

#include "src/util/ui/code/vsc_result_tree.h"

class QHBoxLayout;
class QLabel;
class QPushButton;
class QTreeWidgetItem;

class VscResultPanel : public QWidget {
  Q_OBJECT

public:
  /// 单条结果（查找 / 引用统一结构：文件 + 行号 + 列 + 长度 + 行文本）
  struct Match {
    QString filePath;  // 完整文件路径
    int line = 0;      // 1-based 行号
    int column = 0;    // 0-based 匹配起始列
    int length = 0;    // 匹配长度
    QString lineText;  // 整行文本（未 trim，用于显示）
  };

  explicit VscResultPanel(QWidget *parent = nullptr);

  /// 设置结果搜索根目录（工作区）
  void setSearchRoot(const QString &rootPath);
  const QString &searchRoot() const { return m_searchRoot; }

  /// 该文件是否应参与扫描（排除 build 目录、二进制、tree.config 等）。
  /// 纯函数（不依赖实例状态），供后台线程扫描直接调用
  static bool shouldScanFile(const QString &filePath);

  /// 清空结果树
  void clearResults();
  /// 设置顶部汇总标签文字（如「N 个文件有 M 个结果」）
  void setSummaryText(const QString &text);
  /// 构建结果树：按文件分组 → 匹配行（文件节点带数量后缀，自动展开）
  void buildResultTree(const QVector<Match> &matches);

  /// 结果树控件
  VscResultTree *resultTree() const { return m_resultTree; }
  /// 汇总标签
  QLabel *summaryLabel() const { return m_summaryLabel; }
  /// 「全部折叠」按钮（懒创建，与结果树折叠联动）
  QPushButton *collapseButton();

  /// 主题切换后刷新（汇总标签文字色 + 结果树背景/滚动条/字体/图标）
  virtual void refreshStyle();

signals:
  /// 单击结果项：请求打开文件并定位选中（column/length 为 0-based，用于选中匹配词）
  void openRequested(const QString &filePath, int line, int column, int length);

protected:
  /// 创建主布局骨架（头部行 + 汇总行 + 结果树），子类在构造时调用后填充头部行
  void setupSkeleton();
  /// 头部行布局（子类填充搜索输入行 / 符号名行内容）
  QHBoxLayout *headerLayout() const { return m_headerLayout; }
  /// 汇总行布局（子类可继续追加控件，如查找面板的「全部折叠」按钮）
  QHBoxLayout *summaryLayout() const { return m_summaryRow; }
  /// 结果行点击 → 跳转（子类在 itemClicked 时调用，仅结果行触发）
  void onResultClicked(QTreeWidgetItem *item, int column);

private:
  QHBoxLayout *m_headerLayout = nullptr;  ///< 头部行
  QHBoxLayout *m_summaryRow = nullptr;    ///< 汇总行（标签 + 弹性空间）
  VscResultTree *m_resultTree = nullptr;  ///< VSCode 风格结果树
  QPushButton *m_collapseBtn = nullptr;   ///< 全部折叠按钮（懒创建）
  QLabel *m_summaryLabel = nullptr;       ///< 汇总标签
  QString m_searchRoot;                   ///< 搜索根目录
};
