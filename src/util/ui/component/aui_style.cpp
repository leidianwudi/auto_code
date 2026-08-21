/**
 * @file aui_style.cpp
 * @brief UI 公共样式工具类实现
 */

#include "aui_style.h"

#include <QtMath>
#include <cmath>

#include <QApplication>
#include <QFontDatabase>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QProxyStyle>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOption>
#include <QTabBar>
#include <QToolButton>
#include <QWidget>

/// 标记 tab bar 已基于 Fusion（含自定义空白），供 ensureFusionTabBar 跳过覆盖
static const char *kFusionTabProperty = "aui_fusion_tab";

/// 标记编辑器标签栏的聚焦状态（true=所在面板有焦点），供代理样式绘制文字颜色
static const char *kFocusTabProperty = "aui_focus_tab";

// ──────────────────────────────────────────────────────────────
//  错误波浪线 — 编辑器与标签栏共用（VSCode 风格，样式/粗细统一）
// ──────────────────────────────────────────────────────────────

void AuiStyle::drawErrorUnderline(QPainter &p, int x1, int x2, int y, const QColor &color) {
  if (x2 < x1) x2 = x1;
  // VSCode 风格：细线 + 上下交替的半椭圆弧，波浪中心线为基线 y
  const float w = 5;      // 单个半波宽度
  const float amp = 1.5;  // 振幅（上凸最高到 y-amp，下凸最低到 y+amp）
  QPen pen(color, 1.2);
  pen.setCapStyle(Qt::FlatCap);  // 平头端点，避免端点形成多余圆点
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  // 上凸/下凸弧共用同一矩形（椭圆中心都在基线 y 上），仅弧的角度相反，
  // 保证相邻弧的端点都落在 y 上连续衔接，波浪平滑不跳变、方向正确
  bool up = true;
  int x = x1;
  while (x <= x2 - w + 1) {
    const QRect arcRect(x, y - amp, w, 2 * amp);
    // 上凸：从左侧中点经上顶点到右侧中点；下凸：从右侧中点经下顶点到左侧中点
    p.drawArc(arcRect, up ? 180 * 16 : 0, 180 * 16);
    x += w;
    up = !up;
  }
}

void AuiStyle::drawFoldArrow(QPainter &p, const QPointF &tip, bool downward,
                             const QColor &color, qreal length, qreal openAngleDeg) {
  // 半张角：两臂关于对称轴的夹角一半。两方向共用同一张角，保证开口角度一致。
  const qreal half = qDegreesToRadians(qBound<qreal>(20.0, openAngleDeg, 170.0) / 2.0);
  const qreal lx = length * std::cos(half);
  const qreal ly = length * std::sin(half);
  QPen pen(color, 1.6);
  pen.setCapStyle(Qt::FlatCap);   // 平头：避免尖端两条线起点圆帽重叠成凸点
  pen.setJoinStyle(Qt::RoundJoin);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  QPointF a, b;
  if (downward) {
    // 向下「v」：尖端在最低点，两臂朝左上/右上张开（开口朝上）
    a = QPointF(tip.x() - lx, tip.y() - ly);
    b = QPointF(tip.x() + lx, tip.y() - ly);
  } else {
    // 向右「▸」：尖端在右侧，两臂向左张开（对称轴水平，指向右）。
    // 两臂以 tip 为中心左右对称（尖端相应右移 lx/2），
    // 使 ▸ 与 v 的视觉中心对齐（否则两臂全在左侧，图标显得偏左）
    const QPointF base(tip.x() + lx / 2.0, tip.y());
    a = QPointF(base.x() - lx, base.y() - ly);
    b = QPointF(base.x() - lx, base.y() + ly);
    p.drawLine(base, a);
    p.drawLine(base, b);
    return;
  }
  p.drawLine(tip, a);
  p.drawLine(tip, b);
}

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
             "QPushButton { color: %2; background: transparent; border: 1px solid transparent; }"
             "QPushButton:hover { background: %3; border: 1px solid %5; }"
             "QPushButton:pressed { background: %4; }"
             // 下拉框：面板底 + 细边框 + 圆角；隐藏原生箭头（QSS 的 down-arrow 画不出可靠图标，
             // 统一由 AuiComboBoxWidget 在 paintEvent 里用 QPainter 绘制居中三角箭头）
             "QComboBox { color: %2; background: %6; border: 1px solid %5; border-radius: 3px; "
             "padding: 2px 8px; }"
             "QComboBox:hover { border: 1px solid %4; }"
             "QComboBox::drop-down { border: none; width: 20px; }"
             "QComboBox::down-arrow { image: none; border: none; width: 0; height: 0; }"
             "QComboBox QAbstractItemView { background: %6; color: %2; border: 1px solid %5; "
             "outline: 0; selection-background-color: %3; selection-color: %2; }"
             "QComboBox QAbstractItemView::item { padding: 4px 8px; }"
             // 其余文字控件统一用当前主题文字色，保证深色主题下文字与背景有足够对比度。
             // 这些规则只设 color，不设 background，避免覆盖各控件的自定义背景；控件自身的
             // 样式表（per-widget）优先级更高，可覆盖这里的默认文字色。
             "QLabel { color: %2; }"
             "QRadioButton { color: %2; }"
             "QCheckBox { color: %2; }"
             "QGroupBox { color: %2; }"
             "QSpinBox, QDoubleSpinBox, QDateTimeEdit { color: %2; }"
             "QLineEdit { color: %2; }"
             "QTableWidget, QTreeWidget, QListWidget, QListView, QTreeView, QTableView { color: "
             "%2; }"
             "QHeaderView::section { color: %2; }"
             "QMenuBar { color: %2; }"
             "QMenuBar::item { color: %2; }"
             // QToolTip 文字色交由调色板 ToolTipText 控制（深色主题为白底深字），
             // 不在全局样式表里固定成文字色，避免覆盖调色板的高对比配色
             "QStatusBar { background: %1; color: %2; }"
             "QStatusBar::item { border: none; }"
             "QMenu { background: %1; border: 1px solid %5; padding: 4px 0px; }"
             "QMenu::item { padding: 6px 24px; color: %2; }"
             "QMenu::item:selected { background: %3; color: %2; }"
             "#WindowFrame { background: %1; border: 1px solid %4; }"
             "#AuiTitleBar { background: %7; }"
             // 细窄滚动条：透明背景 + 半透明灰色滑块（深浅主题通用，类似 VS Code），无箭头
             "QScrollBar:vertical { background: transparent; width: 10px; margin: 0; border: none; "
             "}"
             "QScrollBar::handle:vertical {"
             "  background: rgba(120, 120, 120, 100); border-radius: 5px;"
             "  min-height: 20px; margin: 2px;"
             "}"
             "QScrollBar::handle:vertical:hover { background: rgba(120, 120, 120, 150); }"
             "QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; border: "
             "none; }"
             "QScrollBar::handle:horizontal {"
             "  background: rgba(120, 120, 120, 100); border-radius: 5px;"
             "  min-width: 20px; margin: 2px;"
             "}"
             "QScrollBar::handle:horizontal:hover { background: rgba(120, 120, 120, 150); }"
             "QScrollBar::add-line, QScrollBar::sub-line,"
             "QScrollBar::add-page, QScrollBar::sub-page {"
             "  background: transparent; width: 0; height: 0;"
             "}")
      .arg(background().name(), textColor().name(), hoverBackground().name(),
           borderDarkColor().name(), borderColor().name(), panelBackground().name(),
           titleBarBackground().name());
}

QString AuiStyle::dialogStyleSheet() {
  const QString fs = QString::number(dialogFontSize()) + QStringLiteral("px");
  return QStringLiteral(
             "QDialog { background: %1; }"
             "QLabel { color: %2; font-size: %3; }"
             "QRadioButton { color: %2; font-size: %3; }"
             "QCheckBox { color: %2; font-size: %3; }"
             "QGroupBox { color: %2; font-size: %3; }"
             "QLineEdit {"
             "  border: 1px solid %4; border-radius: 3px;"
             "  padding: 4px 6px; font-size: %3;"
             "}")
      .arg(background().name(), textColor().name(), fs, borderColor().name());
}

QString AuiStyle::menuButtonStyleSheet() {
  // 直接设置到 QToolButton 上，优先级高于全局/窗口级样式表，
  // 确保深色主题下按钮文字色不被动继承的默认值覆盖
  return QStringLiteral(
             "QToolButton { color: %1; background: transparent; "
             "border: 1px solid transparent; padding: 2px 6px; }"
             "QToolButton:hover { background: %2; border: 1px solid %3; }")
      .arg(textColor().name(), hoverBackground().name(), borderColor().name());
}

QString AuiStyle::menuStyleSheet() {
  // 直接设置到 QMenu 上，QMenu 是弹出式顶层窗口，不继承主窗口样式表
  return QStringLiteral(
             "QMenu { background: %1; border: 1px solid %2; padding: 4px 0px; }"
             "QMenu::item { padding: 6px 24px; color: %3; }"
             "QMenu::item:selected { background: %4; color: %3; }"
             "QMenu::separator { height: 1px; background: %2; margin: 4px 8px; }")
      .arg(background().name(), borderColor().name(), textColor().name(), hoverBackground().name());
}

void AuiStyle::applyMenuButtonStyle(QToolButton *btn) {
  if (!btn) return;
  // 样式表 + 调色板双保险：Fusion 绘制 QToolButton 文字优先读调色板 ButtonText/WindowText
  btn->setStyleSheet(menuButtonStyleSheet());
  QPalette p = btn->palette();
  const QColor tc = textColor();
  p.setColor(QPalette::ButtonText, tc);
  p.setColor(QPalette::WindowText, tc);
  p.setColor(QPalette::Text, tc);
  p.setColor(QPalette::HighlightedText, tc);
  btn->setPalette(p);
}

// ════════════════════════════════════════════════════════════
//  applyAppFont — 应用窗口字体到 qApp 及所有已创建窗口
// ════════════════════════════════════════════════════════════

QStringList AuiStyle::monoFontCandidates() {
  // 先保存到命名局部变量，避免「临时对象 + 悬垂迭代器」导致的崩溃
  const QStringList installed = QFontDatabase::families();
  // VSCode 常用的等宽编程字体（按优先级排列），仅保留本机已安装者
  const QStringList preferred = {
      QStringLiteral("Consolas"),        QStringLiteral("Courier New"),
      QStringLiteral("Cascadia Mono"),   QStringLiteral("Cascadia Code"),
      QStringLiteral("Fira Code"),       QStringLiteral("JetBrains Mono"),
      QStringLiteral("Source Code Pro"), QStringLiteral("Menlo"),
      QStringLiteral("Monaco"),          QStringLiteral("DejaVu Sans Mono"),
      QStringLiteral("Ubuntu Mono"),     QStringLiteral("微软雅黑"),
      QStringLiteral("Microsoft YaHei")};
  QStringList result;
  result.reserve(preferred.size());
  for (const QString &fam : preferred) {
    if (installed.contains(fam)) result.append(fam);
  }
  return result;
}

void AuiStyle::applyAppFont() {
  if (!qApp) return;
  QFont f = qApp->font();
  // 窗口字体族：跟随「窗口字体」设置（未设置时沿用系统字体）
  const QString fam = SettingStore::ins().fontFamily(QStringLiteral("font.ui"));
  if (!fam.isEmpty()) f.setFamily(fam);
  f.setPointSize(windowFontSize());
  qApp->setFont(f);

  // qApp->setFont 只影响之后创建的控件，不会即时传播到已创建的窗口。
  // 因此：给每个顶层窗口显式设字体（未显式设置字体的子控件在绘制时会动态继承它），
  // 再强制所有子控件重新布局 + 重绘，使工具栏等既有控件立即用新字号渲染，无需重启。
  // 注意这里不给子控件显式 setFont（否则会打上 WA_SetFont 标记，导致下次修改失效），
  // 代码编辑器/目录树/表格等显式设置字体的控件也会因各自字号设置保持不变。
  const QList<QWidget *> tops = QApplication::topLevelWidgets();
  for (QWidget *w : tops) {
    if (!w->isWindow()) continue;
    w->setFont(f);
    const QList<QWidget *> children = w->findChildren<QWidget *>();
    for (QWidget *c : children) {
      c->updateGeometry();
      c->update();
    }
  }
}

void AuiStyle::applyMenuStyle(QMenu *menu) {
  if (!menu) return;
  // 弹出菜单是独立顶层窗口，不继承主窗口样式；显式设样式表 + 调色板
  menu->setStyleSheet(menuStyleSheet());
  QPalette p = menu->palette();
  const QColor tc = textColor();
  p.setColor(QPalette::Window, background());
  p.setColor(QPalette::WindowText, tc);
  p.setColor(QPalette::Text, tc);
  p.setColor(QPalette::HighlightedText, tc);
  menu->setPalette(p);
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

// 可聚焦标签栏代理：继承空白代理，额外按「面板聚焦状态」直接修改绘制选项的调色板。
// 不依赖 setTabTextColor（它在某些样式/代理组合下不生效），而是在 CE_TabBarTab
// 层面修改 QStyleOptionTab 的调色板，Fusion 内部绘制标签文字时会读到修改后的颜色。
class TabBarFocusProxyStyle : public TabBarPaddingProxyStyle {
public:
  explicit TabBarFocusProxyStyle(const QMargins &m, QStyle *base)
      : TabBarPaddingProxyStyle(m, base) {}

  void drawControl(ControlElement element, const QStyleOption *opt, QPainter *painter,
                   const QWidget *widget) const override {
    if (element == CE_TabBarTab) {
      const auto *tabOpt = static_cast<const QStyleOptionTab *>(opt);
      QStyleOptionTab o = *tabOpt;
      const QTabBar *bar = qobject_cast<const QTabBar *>(widget);
      const bool active = bar ? bar->property(kFocusTabProperty).toBool() : true;
      const bool selected = (o.state & QStyle::State_Selected);
      // 聚焦面板的当前标签用醒目的正文色，其余（含非聚焦面板）一律灰色
      const QColor c =
          (active && selected) ? AuiStyle::activeTabTextColor() : AuiStyle::inactiveTabColor();
      // 覆盖调色板中所有可能用于绘制标签文字的颜色角色
      o.palette.setColor(QPalette::Text, c);
      o.palette.setColor(QPalette::WindowText, c);
      o.palette.setColor(QPalette::ButtonText, c);
      o.palette.setColor(QPalette::HighlightedText, c);
      QProxyStyle::drawControl(element, &o, painter, widget);
      return;
    }
    QProxyStyle::drawControl(element, opt, painter, widget);
  }
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

void AuiStyle::applyFocusTabBar(QTabBar *bar, int left, int top, int right, int bottom) {
  ensureFusionTabBar(bar);
  QStyle *base = bar->style();
  auto *proxy = new TabBarFocusProxyStyle(QMargins(left, top, right, bottom), base);
  proxy->setParent(bar);
  bar->setStyle(proxy);
  bar->setProperty(kFusionTabProperty, true);
  bar->setProperty(kFocusTabProperty, true);
}

void AuiStyle::setTabFocusState(QTabBar *bar, bool active) {
  if (!bar) return;
  if (bar->property(kFocusTabProperty).toBool() != active) {
    bar->setProperty(kFocusTabProperty, active);
    bar->update();
  }
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
  // 标记对象名，供主题切换时快速找到标题文字并重建颜色
  label->setObjectName(QStringLiteral("AuiTitleLabel"));
  label->setStyleSheet(QStringLiteral("color: %1; font-size: %2px; background: transparent;"
                                      "padding: 0px 2px;")
                           .arg(textColor().name(), QString::number(titleFontSize())));
  // 调色板双保险：Fusion 绘制文字优先读 WindowText/Text 角色
  QPalette p = label->palette();
  const QColor tc = textColor();
  p.setColor(QPalette::WindowText, tc);
  p.setColor(QPalette::Text, tc);
  p.setColor(QPalette::ButtonText, tc);
  p.setColor(QPalette::HighlightedText, tc);
  label->setPalette(p);
  label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void AuiStyle::applyTitleBarStyle(QWidget *titleBar) {
  // 用对象名 + 全局样式表控制标题栏背景，避免在标题栏上单独 setStyleSheet
  // （单独 setStyleSheet 会让 Qt 对标题栏及子控件启用 QStyleSheetStyle，
  //   可能干扰 QToolButton/QMenu 的文字颜色渲染）
  titleBar->setObjectName(QStringLiteral("AuiTitleBar"));
  titleBar->setAutoFillBackground(true);
  QPalette p = titleBar->palette();
  p.setColor(QPalette::Window, titleBarBackground());
  titleBar->setPalette(p);
  // 强制重解析样式表并重绘，保证主题切换时标题栏背景即时更新（无需重启）
  titleBar->style()->unpolish(titleBar);
  titleBar->style()->polish(titleBar);
  titleBar->update();
}

// ════════════════════════════════════════════════════════════
//  程序化复选框 / 单选框指示器（代理风格）
// ════════════════════════════════════════════════════════════
//  Qt QSS 的 image 属性对图片（尤其 SVG / data URI）支持不稳定，导致打勾/圆点经常
//  显示不出来。这里改用 QProxyStyle 直接拦截 PE_IndicatorCheckBox / PE_IndicatorRadioButton
//  / PE_IndicatorItemViewItemCheck，用 QPainter 程序化绘制，颜色取当前主题色，随主题即时更新。
namespace {

class IndicatorProxyStyle : public QProxyStyle {
public:
  explicit IndicatorProxyStyle(QStyle *base) : QProxyStyle(base) {}

  void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget) const override {
    switch (element) {
      case PE_IndicatorCheckBox:
        drawCheckBox(option, painter);
        return;
      case PE_IndicatorRadioButton:
        drawRadio(option, painter);
        return;
      case PE_IndicatorItemViewItemCheck:
        drawItemCheck(option, painter);
        return;
      default:
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
  }

private:
  static bool isChecked(const QStyleOption *o) { return o->state & QStyle::State_On; }

  /// 部分勾选（三态 PartialiallyChecked 对应 State_NoChange 标志）
  static bool isPartial(const QStyleOption *o) { return o->state & QStyle::State_NoChange; }

  static QRectF innerRect(const QStyleOption *o) {
    return QRectF(o->rect).adjusted(1.0, 1.0, -1.0, -1.0);
  }

  static QColor accent() { return QColor(0x0e, 0x7a, 0xfe); }
  static QColor boxFill() { return AuiStyle::panelBackground(); }
  static QColor boxBorder() { return AuiStyle::textColor(); }

  /// 绘制白色打勾（勾在填充色上方，用于复选框 / 树复选框选中态）
  static void drawCheckMark(QPainter *p, const QRectF &r, qreal w) {
    QPen pen(Qt::white, w);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    QPainterPath path;
    path.moveTo(r.left() + r.width() * 0.24, r.top() + r.height() * 0.52);
    path.lineTo(r.left() + r.width() * 0.44, r.top() + r.height() * 0.72);
    path.lineTo(r.left() + r.width() * 0.78, r.top() + r.height() * 0.30);
    p->drawPath(path);
  }

  /// 绘制部分勾选标记：填充色方块内嵌一个白色小方块（居中，约 36% 尺寸）
  static void drawPartialMark(QPainter *p, const QRectF &r) {
    QRectF inner(0.0, 0.0, r.width() * 0.36, r.height() * 0.36);
    inner.moveCenter(r.center());
    p->setPen(Qt::NoPen);
    p->setBrush(Qt::white);
    p->drawRect(inner);
  }

  void drawCheckBox(const QStyleOption *o, QPainter *p) const {
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = innerRect(o);
    if (isChecked(o)) {
      p->setPen(Qt::NoPen);
      p->setBrush(accent());
      p->drawRoundedRect(r, 2.0, 2.0);
      drawCheckMark(p, r, 2.0);
    } else if (isPartial(o)) {
      p->setPen(Qt::NoPen);
      p->setBrush(accent());
      p->drawRoundedRect(r, 2.0, 2.0);
      drawPartialMark(p, r);
    } else {
      p->setPen(QPen(boxBorder(), 1.0));
      p->setBrush(boxFill());
      p->drawRoundedRect(r, 2.0, 2.0);
    }
    p->restore();
  }

  void drawRadio(const QStyleOption *o, QPainter *p) const {
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = innerRect(o);
    const QPointF c = r.center();
    const qreal radius = qMin(r.width(), r.height()) / 2.0;
    if (isChecked(o)) {
      // 蓝色外环 + 中心实心圆点（经典单选样式）
      p->setPen(QPen(accent(), 2.0));
      p->setBrush(boxFill());
      p->drawEllipse(c, radius - 1.0, radius - 1.0);
      p->setPen(Qt::NoPen);
      p->setBrush(accent());
      p->drawEllipse(c, radius * 0.42, radius * 0.42);
    } else {
      p->setPen(QPen(boxBorder(), 1.0));
      p->setBrush(boxFill());
      p->drawEllipse(c, radius, radius);
    }
    p->restore();
  }

  void drawItemCheck(const QStyleOption *o, QPainter *p) const {
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = innerRect(o);
    if (isChecked(o)) {
      p->setPen(Qt::NoPen);
      p->setBrush(accent());
      p->drawRect(r);
      drawCheckMark(p, r, 1.6);
    } else if (isPartial(o)) {
      // 部分勾选：填充色方块 + 中心白色小方块（树形目录文件夹三态）
      p->setPen(Qt::NoPen);
      p->setBrush(accent());
      p->drawRect(r);
      drawPartialMark(p, r);
    } else {
      p->setPen(QPen(boxBorder(), 1.0));
      p->setBrush(boxFill());
      p->drawRect(r);
    }
    p->restore();
  }
};

}  // namespace

QStyle *AuiStyle::createAppStyle() {
  // 基础风格：Fusion（使用 QPalette 渲染，彻底脱离 Windows 系统主题色）
  QStyle *base = QStyleFactory::create(QStringLiteral("Fusion"));
  if (!base) return nullptr;
  // 包一层代理，程序化绘制复选框 / 单选框指示器；base 归代理所有，避免泄漏
  auto *proxy = new IndicatorProxyStyle(base);
  base->setParent(proxy);
  return proxy;
}