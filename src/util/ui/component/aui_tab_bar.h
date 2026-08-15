/**
 * @file aui_tab_bar.h
 * @brief 标签栏通用绘制工具 — VSCode 风格标签绘制与关闭按钮
 *
 * 集中封装标签栏相关的 UI 绘制逻辑，供编辑区标签栏（DraggableTabBar）、
 * 调试面板页签等场景复用：
 *  - VSCode 风格：选中标签顶部蓝色指示条 + 亮色文字，背景与内容区同色
 *  - 关闭按钮：自绘 X 字形，悬停时正方形高亮 + 变色，提示为中文「关闭标签」
 */

#pragma once

#include <QColor>
#include <QString>

class QPainter;
class QRect;
class QStyleOptionTab;
class QTabBar;
class QWidget;

// ════════════════════════════════════════════════════════════
//  关闭按钮 / 修改圆点 绘制位置微调常量（正值向下，方便统一调整视觉居中）
// ════════════════════════════════════════════════════════════
constexpr int kTabCloseBtnDy = 1;     // X 字形整体下移像素
constexpr int kTabModifiedDotDy = 2;  // 修改实心圆点相对按钮中心下移像素

// ════════════════════════════════════════════════════════════
//  AuiTabBar — 标签栏通用绘制工具
// ════════════════════════════════════════════════════════════

class AuiTabBar {
public:
  /// 标签栏主题色（随主题即时读取，深色模式下保证对比度）
  struct Style {
    QColor stripBg;         ///< 整条 tab 栏背景
    QColor selectedBg;      ///< 选中 tab 背景（与内容区同色，融入正文）
    QColor hoverBg;         ///< hover 背景
    QColor activeText;      ///< 选中 tab 亮色文字
    QColor hoverText;       ///< hover 文字
    QColor dimText;         ///< 未选中灰色文字
    QColor modifiedDot;     ///< 修改实心圆点颜色（深色主题白色，浅色主题灰色）
    QColor border;          ///< 底部分隔线
    QColor accent;          ///< 顶部蓝色指示条 #0e7afe
    QColor errorText;       ///< 错误标签文字红色
    QColor errorUnderline;  ///< 错误标签波浪线红色
  };

  /// 读取当前主题色
  static Style currentStyle();

  /// 绘制整条标签栏背景与底部分隔线
  static void paintBarBackground(QPainter &p, const QRect &rect, const Style &st);

  /// 绘制单个标签的背景与顶部指示条
  /// @param selected 是否为当前标签
  /// @param activePanel 所在面板是否为聚焦面板
  /// @param hovered 鼠标是否悬停
  static void paintTabBackground(QPainter &p, const QRect &r, bool selected, bool activePanel,
                                 bool hovered, const Style &st);

  /// 计算标签关闭按钮区域（Qt 6 中关闭按钮为 tab 右侧子元素）
  static QRect closeButtonRect(const QTabBar *bar, const QStyleOptionTab &opt);

  /// 绘制关闭按钮
  /// @param modified 标签是否已修改（已修改且未悬停时显示实心圆点，悬停后显示 X）
  static void paintCloseButton(QPainter &p, const QRect &rect, bool hovered, bool selected,
                               bool modified, const Style &st);

  /// 绘制标签文字下方的红色波浪线（VSCode 风格错误指示）
  static void paintErrorUnderline(QPainter &p, const QRect &textRect, const Style &st);

  /// 关闭按钮的中文提示
  static QString closeButtonTip();

  /// 将关闭按钮真实子控件（若存在）的 tooltip / 无障碍名称本地化为中文，
  /// 避免 Qt 在部分场景用真实子控件作关闭按钮时显示英文 "Close Tab"
  static void localizeCloseButton(QTabBar *bar, int index);
};
