/**
 * @file aui_code_tab_bar.h
 * @brief VSCode 风格代码编辑标签栏（通用控件）
 *
 * 集中封装代码编辑框标签栏的全部自绘与交互逻辑，任何窗口直接实例化即可复用：
 *  - 自绘标签背景 / 文字颜色 / 顶部蓝色指示条 / 错误红色波浪线
 *  - 关闭按钮自绘（X 字形 + 悬停正方形背景），已修改时未悬停显示实心圆点
 *  - 不使用 Qt 原生关闭按钮（tabsClosable(false)），彻底避免原生 X 覆盖自绘内容
 *  - 关闭点击、悬停变色、中文提示「关闭标签」均由本控件接管
 *  - tabSizeHint 保证宽度至少容纳完整文件名；minimumTabSizeHint 使标签不压缩到
 *    自然宽度以下，空间不足时横向滚动（VSCode 风格）而非提前省略
 */

#pragma once

#include <QSet>
#include <QTabBar>

class QMouseEvent;
class QPaintEvent;

class AuiCodeTabBar : public QTabBar {
  Q_OBJECT

public:
  explicit AuiCodeTabBar(QWidget *parent = nullptr);

  /// 设置标签页的修改状态（已修改则关闭按钮显示实心圆点，悬停后显示 X）
  void setTabModified(int index, bool modified);
  bool isTabModified(int index) const;

  /// 设置标签页的错误状态（有错误则文字变红并加红色波浪线，VSCode 风格）
  void setTabError(int index, bool hasError);
  bool isTabError(int index) const;

protected:
  void paintEvent(QPaintEvent *event) override;
  bool event(QEvent *event) override;                  // ToolTip → 中文「关闭标签」
  void mousePressEvent(QMouseEvent *event) override;   // 关闭按钮点击
  void mouseMoveEvent(QMouseEvent *event) override;    // 悬停背景/圆点↔X 即时刷新
  void leaveEvent(QEvent *event) override;             // 离开时刷新悬停状态
  QSize tabSizeHint(int index) const override;         // 保证宽度至少容纳完整文件名
  QSize minimumTabSizeHint(int index) const override;  // 标签不压缩到自然宽度以下
  void tabRemoved(int index) override;                 // 同步修正状态索引

  /// 供子类复用：命中关闭按钮区域则发出 tabCloseRequested 并返回 true
  bool handleClosePress(const QPoint &pos);

private:
  /// 手动计算关闭按钮区域（tab 右侧垂直居中的正方形，不依赖样式/原生按钮）
  QRect closeButtonRectForIndex(int index) const;
  /// 手动计算文字区域（在样式文本矩形基础上裁剪右侧关闭区）
  QRect textRectForIndex(int index) const;
  /// 计算能完整显示文件名所需的最小标签宽度（文字宽 + tab 空白 + 关闭按钮区）
  int fullTabWidth(int index) const;

  QSet<int> m_modifiedTabs;  ///< 已修改标签页索引集合
  QSet<int> m_errorTabs;     ///< 有错误标签页索引集合
};
