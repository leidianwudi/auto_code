/**
 * @file aui_code_tab_bar.cpp
 * @brief VSCode 风格代码编辑标签栏（通用控件）实现
 */

#include "aui_code_tab_bar.h"

#include <QCursor>
#include <QEvent>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QStyle>
#include <QStyleOptionTab>
#include <QToolTip>

#include "aui_tab_bar.h"

namespace {
// 关闭按钮与文字布局常量（供 closeButtonRectForIndex / textRectForIndex / tabSizeHint 共用）
constexpr int kCloseSize = 16;        // 关闭按钮边长
constexpr int kCloseRightMargin = 2;  // 关闭按钮距标签右边距
constexpr int kCloseLeftSpacing = 2;  // 文字与关闭按钮之间留白
}  // namespace

AuiCodeTabBar::AuiCodeTabBar(QWidget *parent) : QTabBar(parent) {
  // 不创建 Qt 原生关闭按钮（真实子控件），全部自绘接管，
  // 彻底避免原生 X 与自绘圆点/X 重叠、且不依赖 hide/remove 子控件。
  setTabsClosable(false);
  // 标签不压缩时依赖滚动按钮承载溢出内容（见 minimumTabSizeHint），显式启用
  setUsesScrollButtons(true);
  // 开启鼠标追踪，使关闭按钮悬停正方形背景随鼠标移动即时刷新
  setMouseTracking(true);
}

void AuiCodeTabBar::setTabModified(int index, bool modified) {
  if (modified)
    m_modifiedTabs.insert(index);
  else
    m_modifiedTabs.remove(index);
  update();  // 触发重绘
}

bool AuiCodeTabBar::isTabModified(int index) const { return m_modifiedTabs.contains(index); }

void AuiCodeTabBar::setTabError(int index, bool hasError) {
  if (hasError)
    m_errorTabs.insert(index);
  else
    m_errorTabs.remove(index);
  update();  // 触发重绘
}

bool AuiCodeTabBar::isTabError(int index) const { return m_errorTabs.contains(index); }

void AuiCodeTabBar::setShowCloseButton(bool show) {
  m_showCloseButton = show;
  update();
}

void AuiCodeTabBar::setTabHeight(int height) {
  m_tabHeight = height;
  updateGeometry();  // 触发 sizeHint 重新计算，让整条 tab 栏高度随标签高度收紧
  update();
}

QRect AuiCodeTabBar::closeButtonRectForIndex(int index) const {
  if (!m_showCloseButton) return QRect();
  const QRect r = tabRect(index);
  if (r.isNull()) return QRect();
  return QRect(r.right() - kCloseSize - kCloseRightMargin + 1,
               r.top() + (r.height() - kCloseSize) / 2, kCloseSize, kCloseSize);
}

QRect AuiCodeTabBar::textRectForIndex(int index) const {
  // 先取样式文本矩形（含 applyTabBarPadding 的四边空白），再裁剪右侧关闭按钮区域
  QStyleOptionTab opt;
  initStyleOption(&opt, index);
  QRect tr = style()->subElementRect(QStyle::SE_TabBarTabText, &opt, this);
  if (tr.isNull()) tr = tabRect(index);
  if (m_showCloseButton) {
    const int closeLeft = closeButtonRectForIndex(index).left() - kCloseLeftSpacing;
    if (tr.right() > closeLeft) tr.setRight(closeLeft);
  }
  return tr;
}

int AuiCodeTabBar::fullTabWidth(int index) const {
  // 完整显示文件名所需的最小宽度：文字宽 + tab 左右空白 + 文字与关闭按钮留白 + 关闭按钮区
  const QFontMetrics fm(font());
  const int textWidth = fm.horizontalAdvance(tabText(index));
  const int hspace = style()->pixelMetric(QStyle::PM_TabBarTabHSpace, nullptr, this);
  int width = textWidth + hspace;
  if (m_showCloseButton) width += kCloseLeftSpacing + kCloseSize + kCloseRightMargin;
  return width;
}

QSize AuiCodeTabBar::tabSizeHint(int index) const {
  QSize s = QTabBar::tabSizeHint(index);
  // 保证标签宽度至少容纳完整文件名，避免样式/布局在空间足够时提前省略文字
  s.setWidth(qMax(s.width(), fullTabWidth(index)));
  // 固定标签高度（底部面板标签栏压缩用）
  if (m_tabHeight > 0) s.setHeight(m_tabHeight);
  return s;
}

QSize AuiCodeTabBar::minimumTabSizeHint(int index) const {
  // 标签不压缩到自然宽度以下：空间不足时由 QTabBar 横向滚动（VSCode 风格），
  // 而不是把所有标签压缩省略，从而在空间足够时尽量完整显示文件名
  return tabSizeHint(index);
}

void AuiCodeTabBar::paintEvent(QPaintEvent *) {
  // 直接自绘所有标签，完全绕过 QTabBar/style 的文字与背景渲染
  // （Windows 原生风格用 DrawThemeText 画 tab 文字，忽略 setTabTextColor；
  //   Fusion 的 CE_TabBarTab 背景取自调色板，深色模式下易与文字对比不足）
  // VSCode 风格（与调试面板 调用栈/变量/断点 页签一致）：
  //   选中 tab：顶部蓝色指示条 + 亮色文字，背景与编辑器内容同色（融入正文区）；
  //   未选中 tab：灰色文字 + 透明背景，hover 时文字微亮、背景加深。
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const bool active = property("aui_focus_tab").toBool();
  const int cur = currentIndex();
  const AuiTabBar::Style st = AuiTabBar::currentStyle();
  const QPoint mousePos = mapFromGlobal(QCursor::pos());

  // 整条 tab 栏背景 + 底部分隔线（选中 tab 底部不画线，与内容区无缝衔接）
  AuiTabBar::paintBarBackground(painter, rect(), st, cur >= 0 ? tabRect(cur) : QRect());

  QFontMetrics fm(font());
  for (int i = 0; i < count(); ++i) {
    const QRect r = tabRect(i);
    const bool isSelected = (i == cur);
    const bool hovered = r.contains(mousePos);

    // 标签背景与顶部指示条
    AuiTabBar::paintTabBackground(painter, r, isSelected, active, hovered, st);

    // 文字颜色：聚焦面板的当前标签用亮色，其余灰；hover 微亮；有错误则红色（VSCode 风格）
    const bool hasError = m_errorTabs.contains(i);
    QColor c = st.dimText;
    if (hasError) {
      c = st.errorText;
    } else if (isSelected && active) {
      c = st.activeText;
    } else if (hovered) {
      c = st.hoverText;
    }

    const QRect textRect = textRectForIndex(i);
    painter.setPen(c);
    painter.setFont(font());
    painter.drawText(textRect, Qt::AlignCenter,
                     fm.elidedText(tabText(i), Qt::ElideRight, textRect.width()));

    // 有错误的标签：文字下方绘制红色波浪线（VSCode 风格）
    if (hasError) {
      AuiTabBar::paintErrorUnderline(painter, textRect, st);
    }

    // 关闭按钮：已修改且未悬停时显示实心圆点，悬停/未修改时显示 X（纯标签页可关闭）
    if (m_showCloseButton) {
      const QRect closeRect = closeButtonRectForIndex(i);
      if (!closeRect.isNull()) {
        const bool closeHovered = closeRect.contains(mousePos);
        AuiTabBar::paintCloseButton(painter, closeRect, closeHovered, isSelected,
                                    m_modifiedTabs.contains(i), st);
      }
    }
  }
}

bool AuiCodeTabBar::event(QEvent *event) {
  // 悬停在关闭按钮上时显示中文提示"关闭标签"
  if (m_showCloseButton && event->type() == QEvent::ToolTip) {
    auto *he = static_cast<QHelpEvent *>(event);
    const int index = tabAt(he->pos());
    if (index >= 0 && closeButtonRectForIndex(index).contains(he->pos())) {
      QToolTip::showText(he->globalPos(), AuiTabBar::closeButtonTip(), this);
      return true;
    }
  }
  return QTabBar::event(event);
}

bool AuiCodeTabBar::handleClosePress(const QPoint &pos) {
  if (!m_showCloseButton) return false;
  const int index = tabAt(pos);
  if (index < 0) return false;
  if (!closeButtonRectForIndex(index).contains(pos)) return false;
  emit tabCloseRequested(index);
  return true;
}

void AuiCodeTabBar::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    if (handleClosePress(event->pos())) return;
  }
  QTabBar::mousePressEvent(event);
}

void AuiCodeTabBar::mouseMoveEvent(QMouseEvent *event) {
  update();  // 悬停正方形背景 / 圆点↔X 切换即时刷新
  QTabBar::mouseMoveEvent(event);
}

void AuiCodeTabBar::leaveEvent(QEvent *) { update(); }

void AuiCodeTabBar::tabRemoved(int index) {
  // QTabBar::removeTab 会自动下移后续标签索引，需要同步修正 m_modifiedTabs / m_errorTabs
  m_modifiedTabs.remove(index);
  QSet<int> newSet;
  for (int idx : m_modifiedTabs) newSet.insert(idx > index ? idx - 1 : idx);
  m_modifiedTabs = newSet;
  m_errorTabs.remove(index);
  QSet<int> newErrSet;
  for (int idx : m_errorTabs) newErrSet.insert(idx > index ? idx - 1 : idx);
  m_errorTabs = newErrSet;
  QTabBar::tabRemoved(index);
}
