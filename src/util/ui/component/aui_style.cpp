/**
 * @file aui_style.cpp
 * @brief UI 公共样式工具类实现
 */

#include "aui_style.h"

#include <QLabel>
#include <QProxyStyle>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOption>
#include <QTabBar>

/// 标记 tab bar 已基于 Fusion（含自定义空白），供 ensureFusionTabBar 跳过覆盖
static const char *kFusionTabProperty = "aui_fusion_tab";

// ════════════════════════════════════════════════════════════
//  全局样式表 — 所有窗口共用
// ════════════════════════════════════════════════════════════

/// 返回项目中所有窗口公用的全局样式表，统一各窗口（MainDevUi、DemoUi、CreateUi）
/// 的视觉风格，包括标题栏、按钮、状态栏和窗口边框。
///
/// 选择的颜色值：
/// - #e0e0e0 — 浅灰背景（窗口背景、状态栏、窗口框架）
/// - #cccccc — 中灰背景（自定义标题栏，比窗口背景深，形成层次感）
/// - #333    — 深灰文字，保证可读性
/// - #d0d0d0 — hover 高亮底色
/// - #999999 — 窗口外边框（1px 实线）
///
/// 各选择器说明：
/// - QToolButton     : 标题栏菜单按钮（文件/视图等）
/// - QToolButton:hover : 菜单按钮鼠标悬停
/// - QPushButton     : 通用按钮（窗口控制、生成按钮等）
/// - QPushButton:hover : 按钮鼠标悬停
/// - QStatusBar      : 底部状态栏
/// - QStatusBar::item : 状态栏内部子控件之间的分隔线（去掉默认边框）
/// - #WindowFrame    : applyWindowFrame() 创建的窗口外层框架
QString AuiStyle::mainStyleSheet() {
  return QStringLiteral(
             "QToolButton { color: %2; border: 1px solid transparent; padding: 2px 6px; }"
             "QToolButton:hover { background: %3; border: 1px solid %5; }"
             "QPushButton { color: %2; border: 1px solid transparent; }"
             "QPushButton:hover { background: %3; border: 1px solid %5; }"
             "QStatusBar { background: %1; color: %2; }"
             "QStatusBar::item { border: none; }"
             "QMenu { background: %1; border: 1px solid %5; padding: 4px 0px; }"
             "QMenu::item { padding: 6px 24px; }"
             "QMenu::item:selected { background: %3; color: %2; }"
             "#WindowFrame { background: %1; border: 1px solid %4; }")
      .arg(background().name(), textColor().name(), hoverBackground().name(),
           borderDarkColor().name(), borderColor().name());
}

QString AuiStyle::dialogStyleSheet() {
  const QString fs = QString::number(dialogFontSize()) + QStringLiteral("px");
  return QStringLiteral(
             "QDialog { background: %1; }"
             "QLabel { color: %2; font-size: %3; }"
             "QLineEdit {"
             "  border: 1px solid %4; border-radius: 3px;"
             "  padding: 4px 6px; font-size: %3;"
             "}")
      .arg(background().name(), textColor().name(), fs, borderColor().name());
}

QString AuiStyle::tabBarStyleSheet() {
  // 简洁版 tab 样式：仅区分文字颜色（未选中灰 / 选中深），不加背景与边框
  return QStringLiteral(
             "QTabBar::tab { color: %1; }"
             "QTabBar::tab:selected { color: %2; }")
      .arg(inactiveTabColor().name(), textColor().name());
}

/// 覆盖 tab 上下左右空白的内边距代理样式
/// 通过像素度量控制 tab 整体尺寸，并重写文本矩形实现四边不对称空白
class TabBarPaddingProxyStyle : public QProxyStyle {
public:
  explicit TabBarPaddingProxyStyle(const QMargins &m, QStyle *base) : QProxyStyle(base), m_m(m) {}

  int pixelMetric(PixelMetric metric, const QStyleOption *opt,
                  const QWidget *widget) const override {
    if (metric == PM_TabBarTabHSpace) return m_m.left() + m_m.right();
    if (metric == PM_TabBarTabVSpace) return m_m.top() + m_m.bottom();
    return QProxyStyle::pixelMetric(metric, opt, widget);
  }

  QRect subElementRect(SubElement element, const QStyleOption *opt,
                       const QWidget *widget) const override {
    QRect r = QProxyStyle::subElementRect(element, opt, widget);
    if (element == SE_TabBarTabText) {
      // 基础文本矩形按对称的 (left+right)/2、(top+bottom)/2 内缩，这里修正为四边各自的值
      const int hpad = (m_m.left() + m_m.right()) / 2;
      const int vpad = (m_m.top() + m_m.bottom()) / 2;
      r.adjust(m_m.left() - hpad, m_m.top() - vpad, -(m_m.right() - hpad), -(m_m.bottom() - vpad));
    }
    return r;
  }

private:
  QMargins m_m;
};

void AuiStyle::applyTabBarPadding(QTabBar *bar, int left, int top, int right, int bottom) {
  ensureFusionTabBar(bar);  // 先确保基于 Fusion（便于统一渲染；文字颜色由 setTabTextColor 控制）
  QStyle *base = bar->style();
  auto *proxy = new TabBarPaddingProxyStyle(QMargins(left, top, right, bottom), base);
  proxy->setParent(bar);
  bar->setStyle(proxy);
  // 标记该 tab bar 已基于 Fusion（含自定义空白），供 ensureFusionTabBar 跳过覆盖
  bar->setProperty(kFusionTabProperty, true);
  // 注意：不要在此设置样式表，否则 Qt 会用 QStyleSheetStyle 包裹代理样式，
  // 使上面对 SE_TabBarTabText 的覆盖失效。文字颜色由 applyTabDimming 的 setTabTextColor 负责。
}

QString AuiStyle::popupListStyleSheet() {
  return QStringLiteral(
             "QListView {"
             "  border: 1px solid %1;"
             "  border-radius: 0px;"
             "  padding: 0px;"
             "  margin: 0px;"
             "  background: %2;"
             "}"
             "QListView::item {"
             "  padding: 1px 4px;"
             "  min-height: 18px;"
             "}")
      .arg(borderDarkColor().name(), panelBackground().name());
}

QString AuiStyle::errorToolTipStyleSheet() {
  return QStringLiteral(
             "#AuiErrorToolTip {"
             "  background: %1;"
             "  border: 1px solid %2;"
             "  border-radius: 3px;"
             "  padding: 4px 8px;"
             "}")
      .arg(errorToolTipBackground().name(), borderDarkColor().name());
}

// ════════════════════════════════════════════════════════════
//  风格适配（跨平台兼容）
// ════════════════════════════════════════════════════════════
void AuiStyle::ensureFusionTabBar(QTabBar *bar) {
#ifdef Q_OS_WIN
  // 已由 applyTabBarPadding 标记为基于 Fusion（含自定义空白），直接跳过，避免覆盖
  if (bar->property(kFusionTabProperty).toBool()) return;
  auto isFusionStyle = [](QStyle *s) {
    return s &&
           QString::fromLatin1(s->metaObject()->className()).contains(QStringLiteral("Fusion"));
  };
  // 当前已基于 Fusion（含我们的代理样式包裹 Fusion）则跳过，避免覆盖自定义 tab 空白
  bool isFusion = isFusionStyle(bar->style());
  if (!isFusion) {
    if (auto *proxy = qobject_cast<QProxyStyle *>(bar->style())) {
      isFusion = isFusionStyle(proxy->baseStyle());
    }
  }
  if (!isFusion) {
    QStyle *fs = QStyleFactory::create(QStringLiteral("Fusion"));
    if (fs) {
      fs->setParent(bar);
      bar->setStyle(fs);
    }
  }
#else
  Q_UNUSED(bar);
#endif
}

void AuiStyle::applyTitleLabelStyle(QLabel *label) {
  label->setStyleSheet(QStringLiteral("color: %1; font-size: %2px; background: transparent;"
                                      "padding: 0px 2px;")
                           .arg(textColor().name(), QString::number(titleFontSize())));
  label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void AuiStyle::applyTitleBarStyle(QWidget *titleBar) {
  titleBar->setStyleSheet(QStringLiteral("background: %1; margin-left: -2px; margin-right: -2px;")
                              .arg(titleBarBackground().name()));
}