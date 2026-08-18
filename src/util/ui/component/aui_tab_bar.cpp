/**
 * @file aui_tab_bar.cpp
 * @brief 标签栏通用绘制工具实现
 */

#include "aui_tab_bar.h"

#include <QPainter>
#include <QPen>
#include <QStyle>
#include <QStyleOptionTab>
#include <QTabBar>
#include <QtGlobal>

#include "aui_style.h"

AuiTabBar::Style AuiTabBar::currentStyle() {
  Style st;
  st.stripBg = AuiStyle::tabUnselectedBackground();     // tab 栏背景
  st.selectedBg = AuiStyle::editorBackground();         // 选中 tab 与内容区同色
  st.hoverBg = AuiStyle::tabHoverBackground();          // hover 背景
  st.activeText = AuiStyle::activeTabTextColor();       // 选中亮色文字
  st.hoverText = AuiStyle::secondaryTextColor();        // hover 文字
  st.dimText = AuiStyle::inactiveTabColor();            // 未选中灰
  st.modifiedDot = AuiStyle::modifiedDotColor();        // 修改实心圆点（深色主题白色）
  st.border = AuiStyle::borderColor();                  // 底部分隔线
  st.accent = QColor(0x0e, 0x7a, 0xfe);                 // 顶部蓝色指示条
  st.errorText = AuiStyle::errorTextColor();            // 错误标签文字
  st.errorUnderline = AuiStyle::errorUnderlineColor();  // 错误波浪线
  return st;
}

void AuiTabBar::paintBarBackground(QPainter &p, const QRect &rect, const Style &st,
                                   const QRect &selectedTabRect) {
  // 整条 tab 栏背景
  p.fillRect(rect, st.stripBg);
  // 底部分隔线：默认整条；若指定选中 tab 区域则跳过它（选中 tab 与内容区同色无缝衔接）
  const int lineY = rect.bottom() - 1;
  const int left = rect.left();
  const int right = rect.right();
  if (selectedTabRect.isNull()) {
    p.fillRect(QRect(left, lineY, rect.width(), 1), st.border);
    return;
  }
  // 分隔线左侧段
  if (selectedTabRect.left() > left) {
    p.fillRect(QRect(left, lineY, selectedTabRect.left() - left, 1), st.border);
  }
  // 分隔线右侧段
  if (selectedTabRect.right() < right) {
    p.fillRect(QRect(selectedTabRect.right() + 1, lineY, right - selectedTabRect.right(), 1),
               st.border);
  }
}

void AuiTabBar::paintTabBackground(QPainter &p, const QRect &r, bool selected, bool activePanel,
                                   bool hovered, const Style &st) {
  if (selected) {
    // 选中与编辑器同色，聚焦面板顶部加蓝色指示条
    p.fillRect(r, st.selectedBg);
    if (activePanel) {
      p.fillRect(QRect(r.left(), r.top(), r.width(), 2), st.accent);
    }
  } else if (hovered) {
    // 未选中 hover 时背景加深
    p.fillRect(r, st.hoverBg);
  }
}

QRect AuiTabBar::closeButtonRect(const QTabBar *bar, const QStyleOptionTab &opt) {
  return bar->style()->subElementRect(QStyle::SE_TabBarTabRightButton, &opt, bar);
}

void AuiTabBar::paintCloseButton(QPainter &p, const QRect &rect, bool hovered, bool selected,
                                 bool modified, const Style &st) {
  if (rect.isNull()) return;

  if (modified && !hovered) {
    // 已修改且未悬停 → 实心圆点（深色主题白色，浅色主题灰色；VSCode 风格，非红色），
    // 位于 X 号中心；鼠标移入后隐藏圆点、显示 X 号与悬停背景
    p.setPen(Qt::NoPen);
    p.setBrush(st.modifiedDot);
    const int r = qMax(3, qMin(rect.width(), rect.height()) / 3);
    // 垂直偏移由 kTabModifiedDotDy 控制（相对按钮中心，正值向下）
    p.drawEllipse(QPoint(rect.center().x(), rect.center().y() + kTabModifiedDotDy), r, r);
    return;
  }

  // 悬停：正方形高亮背景（非圆形）
  if (hovered) {
    p.setPen(Qt::NoPen);
    p.setBrush(st.hoverBg);
    p.drawRect(rect);
  }

  // X 字形颜色：悬停用最亮色，选中用中亮色，其余灰色
  QColor xColor = hovered ? st.activeText : (selected ? st.hoverText : st.dimText);
  p.setPen(QPen(xColor, 1.2));
  const int m = qMax(3, rect.width() / 4);
  const int x1 = rect.left() + m;
  // X 字形整体垂直偏移由 kTabCloseBtnDy 控制（正值向下）
  const int y1 = rect.top() + m + kTabCloseBtnDy;
  const int x2 = rect.right() - m + 1;
  const int y2 = rect.bottom() - m + 1 + kTabCloseBtnDy;
  p.drawLine(x1, y1, x2, y2);
  p.drawLine(x2, y1, x1, y2);
}

QString AuiTabBar::closeButtonTip() { return QStringLiteral("关闭标签"); }

void AuiTabBar::paintErrorUnderline(QPainter &p, const QRect &textRect, const Style &st) {
  // 与代码编辑器共用 VSCode 风格红色波浪线（样式/粗细统一）
  const int y = textRect.bottom() + 2;  // 基线（相对文字下移 2px，避免与文件名重合）
  AuiStyle::drawErrorUnderline(p, textRect.left(), textRect.right(), y, st.errorUnderline);
}

void AuiTabBar::localizeCloseButton(QTabBar *bar, int index) {
  if (!bar) return;
  // Qt 6.12 在部分场景用真实子控件作为关闭按钮，其自带英文 "Close Tab" 提示，
  // 这里把该子控件的 tooltip 与无障碍名称统一改为中文
  const auto pos = static_cast<QTabBar::ButtonPosition>(
      bar->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, bar));
  if (QWidget *btn = bar->tabButton(index, pos)) {
    btn->setToolTip(closeButtonTip());
    btn->setAccessibleName(closeButtonTip());
  }
}
