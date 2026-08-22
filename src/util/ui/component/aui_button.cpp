/**
 * @file aui_button.cpp
 * @brief UI 公共按钮工具类实现
 */

#include "aui_button.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <functional>

#include "aui_icon.h"
#include "aui_style.h"
#include "src/util/common/code_constants.h"

namespace {
/// 让按钮图标随主题 / 颜色设置变化自动重绘（颜色取自当前主题，切换后无需重启）
template <typename DrawFn>
void bindThemeAwareIcon(QPushButton *btn, DrawFn draw) {
  auto apply = [btn, draw]() { btn->setIcon(QIcon(draw())); };
  apply();
  QObject::connect(&SettingStore::ins(), &SettingStore::themeChanged, btn, apply);
  QObject::connect(&SettingStore::ins(), &SettingStore::colorsChanged, btn, apply);
}
}  // namespace

/// 调试单步按钮边长（唯一修改点：只需改这一处，图标和按钮尺寸会同步缩放）
static constexpr int kDebugStepSize = 23;

/// @brief 绘制调试单步按钮图标（继续/跳过/进入/跳出）
static QIcon makeDebugStepIcon(int kind) {
  const int S = kDebugStepSize;
  const qreal k = S / 24.0;  // 坐标以 24 为基准，按尺寸线性缩放
  auto pt = [k](qreal x, qreal y) { return QPointF(x * k, y * k); };
  QPixmap pm(S, S);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  const QColor c = AuiStyle::textColor();
  QPen pen(c, 2.2);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  switch (kind) {
    case 0: {  // 继续 ▶
      QPolygonF tri;
      tri << pt(8, 5) << pt(8, 19) << pt(19, 12);
      p.setBrush(c);
      p.setPen(Qt::NoPen);
      p.drawPolygon(tri);
      break;
    }
    case 1: {  // 单步执行：先向下再向右下的箭头
      p.drawLine(pt(6, 5), pt(6, 15));
      p.drawLine(pt(6, 15), pt(18, 15));
      p.drawLine(pt(18, 15), pt(15, 12));
      p.drawLine(pt(18, 15), pt(15, 18));
      break;
    }
    case 2: {  // 单步进入：向下箭头落入一条线
      p.drawLine(pt(6, 5), pt(6, 13));
      p.drawLine(pt(6, 13), pt(13, 13));
      p.drawLine(pt(13, 13), pt(10, 10));
      p.drawLine(pt(13, 13), pt(10, 16));
      p.drawLine(pt(4, 19), pt(18, 19));
      break;
    }
    case 3: {  // 单步跳出：从一条线向上跳出
      p.drawLine(pt(4, 5), pt(18, 5));
      p.drawLine(pt(6, 5), pt(6, 13));
      p.drawLine(pt(6, 13), pt(13, 13));
      p.drawLine(pt(13, 13), pt(10, 10));
      p.drawLine(pt(13, 13), pt(10, 16));
      break;
    }
  }
  p.end();
  return QIcon(pm);
}

/// @brief 调试单步图标按钮样式表（浅色背景 + 边框 + hover/pressed 反馈）
static QString debugStepButtonStyleSheet() {
  return QStringLiteral(
             "QPushButton {"
             "  background: %1;"
             "  border: 1px solid %2;"
             "  border-radius: 4px;"
             "  margin: 1px;"
             "  padding: 0px;"
             "}"
             "QPushButton:hover {"
             "  background: %3;"
             "  border: 1px solid %2;"
             "}"
             "QPushButton:pressed {"
             "  background: %4;"
             "}")
      .arg(AuiStyle::background().name(), AuiStyle::borderColor().name(),
           AuiStyle::hoverBackground().name(),
           AuiStyle::iconButtonPressedBg().name(QColor::HexArgb));
}

// ════════════════════════════════════════════════════════════
//  通用按钮样式
// ════════════════════════════════════════════════════════════

void AuiButton::applyCommonStyle(QPushButton *btn) {
  btn->setFixedSize(AuiStyle::titleBarButtonSize());
  btn->setFlat(true);
  btn->setFocusPolicy(Qt::NoFocus);
  // 标记为「标题栏控制按钮」，供主题切换时 refreshThemedButtons 重绘图标
  btn->setProperty("auiTitleBarBtn", true);
  // 不在按钮上单独 setStyleSheet：颜色统一由 app 级 mainStyleSheet 动态管理，
  // 否则切换主题时按钮文字会固化旧的 textColor（浅色主题的深灰），深色下看不清
}

void AuiButton::applyIconButtonStyle(QPushButton *btn) {
  // 颜色统一由 app 级 mainStyleSheet 动态管理（QPushButton 透明背景 + 动态文字色），
  // 不在按钮上单独 setStyleSheet，避免切换主题时文字固化旧色导致深色下看不清
  btn->setFlat(true);
}

QString AuiButton::dialogButtonStyleSheet() {
  const QString fs = QString::number(AuiStyle::dialogFontSize()) + QStringLiteral("px");
  return QStringLiteral(
             "QPushButton {"
             "  background: %1; border: 1px solid %2; border-radius: 3px;"
             "  padding: 6px 20px; font-size: %3; color: %7;"
             "}"
             "QPushButton:hover {"
             "  background: %4; border: 1px solid %2; color: %7;"
             "}"
             "QPushButton:pressed {"
             "  background: %5; color: %7;"
             "}"
             "QPushButton:disabled {"
             "  color: %6;"
             "}")
      .arg(AuiStyle::background().name(), AuiStyle::borderColor().name(), fs,
           AuiStyle::hoverBackground().name(),
           AuiStyle::iconButtonPressedBg().name(QColor::HexArgb), AuiStyle::mutedTextColor().name(),
           AuiStyle::textColor().name());
}

void AuiButton::applyDialogButtonStyle(QPushButton *btn) {
  // 标记为「对话框标准按钮」，供主题切换时 refreshThemedButtons 重建样式
  btn->setProperty("auiDialogBtn", true);
  btn->setStyleSheet(dialogButtonStyleSheet());
}

// ════════════════════════════════════════════════════════════
//  工厂方法
// ════════════════════════════════════════════════════════════
QPushButton *AuiButton::createSplitButton() {
  auto *btn = new QPushButton;
  bindThemeAwareIcon(btn, []() {
    QPixmap px(20, 20);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(AuiStyle::textColor(), 1.5));
    p.drawRect(2, 3, 16, 14);                     // 外框
    p.drawLine(QPointF(10, 3), QPointF(10, 17));  // 中分线
    p.end();
    return px;
  });
  btn->setIconSize(QSize(20, 20));
  btn->setToolTip(QStringLiteral("向右拆分编辑器 (Ctrl+\\)"));
  applyCommonStyle(btn);
  return btn;
}

QPushButton *AuiButton::createVisualToggleButton() {
  auto *btn = new QPushButton;
  btn->setCheckable(true);  // 可切换状态

  // 绘制图标：左侧方块（代码）+ 右侧网格（可视化）
  auto drawIcon = [](bool checked) {
    QPixmap px(20, 20);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    QColor c = checked ? AuiStyle::compileButtonColor() : AuiStyle::textColor();
    p.setPen(QPen(c, 1.5));
    // 左侧：代码符号 < >
    p.drawLine(QPointF(2, 6), QPointF(5, 10));
    p.drawLine(QPointF(2, 14), QPointF(5, 10));
    p.drawLine(QPointF(8, 6), QPointF(5, 10));
    p.drawLine(QPointF(8, 14), QPointF(5, 10));
    // 右侧：可视化网格 2x2
    p.drawRect(11, 5, 7, 10);
    p.drawLine(QPointF(14.5, 5), QPointF(14.5, 15));
    p.drawLine(QPointF(11, 10), QPointF(18, 10));
    p.end();
    return px;
  };

  btn->setIcon(QIcon(drawIcon(false)));
  btn->setIconSize(QSize(20, 20));
  btn->setToolTip(QStringLiteral("可视化编辑 / 代码编辑切换"));

  // 切换状态时更新图标
  QObject::connect(btn, &QPushButton::toggled, btn,
                   [btn, drawIcon](bool checked) { btn->setIcon(QIcon(drawIcon(checked))); });

  applyCommonStyle(btn);
  return btn;
}

QPushButton *AuiButton::createMinButton() {
  auto *btn = new QPushButton;
  // 自绘 "-" 图标（取代文字字形；粗细/视觉尺寸与最大化按钮一致，随主题色自动重绘）
  bindThemeAwareIcon(btn, []() {
    const int s = 14;
    const qreal m = 2.0;  // 横线端距图标边缘的内边距（与最大化/关闭按钮一致）
    QPixmap px(s, s);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(AuiStyle::textColor(), 1.2));
    p.setBrush(Qt::NoBrush);
    const qreal y = s / 2.0;  // 垂直居中
    p.drawLine(QPointF(m, y), QPointF(s - m, y));
    p.end();
    return px;
  });
  btn->setIconSize(QSize(14, 14));
  applyCommonStyle(btn);
  return btn;
}

QPushButton *AuiButton::createMaxButton() {
  auto *btn = new QPushButton;
  updateMaximizeIcon(btn, false);
  applyCommonStyle(btn);
  return btn;
}

QPushButton *AuiButton::createCloseButton() {
  auto *btn = new QPushButton;
  // 自绘 X 图标（取代原本的 "✕" 文字；粗细/视觉尺寸与最大化按钮一致，随主题色自动重绘）
  bindThemeAwareIcon(btn, []() {
    const int s = 14;
    const qreal m = 2.0;  // X 端点距图标边缘的内边距（与最大化/最小化按钮一致）
    QPixmap px(s, s);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(AuiStyle::textColor(), 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(m, m), QPointF(s - m, s - m));
    p.drawLine(QPointF(s - m, m), QPointF(m, s - m));
    p.end();
    return px;
  });
  btn->setIconSize(QSize(14, 14));
  applyCommonStyle(btn);
  return btn;
}

// ════════════════════════════════════════════════════════════
//  构建 / 生成按钮
// ════════════════════════════════════════════════════════════

QPushButton *AuiButton::createBuildButton(int size) {
  auto *btn = new QPushButton;
  btn->setIcon(AuiIcon::createBuildIcon(size));
  btn->setIconSize(QSize(size, size));
  btn->setCursor(Qt::PointingHandCursor);
  btn->setFocusPolicy(Qt::NoFocus);
  applyIconButtonStyle(btn);
  return btn;
}

// ════════════════════════════════════════════════════════════
//  停止按钮
// ════════════════════════════════════════════════════════════

QPushButton *AuiButton::createStopButton(int size) {
  auto *btn = new QPushButton;
  // 绘制方形停止图标（红色，与 VSCode 停止按钮一致；禁用时使用灰色图标）
  QColor red = AuiStyle::errorTextColor();
  QColor gray = AuiStyle::inactiveTabColor();
  const int m = 4;  // margin
  auto drawSquare = [&](const QColor &color) {
    QPixmap px(size, size);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.2));
    p.setBrush(QBrush(color));
    p.drawRect(m, m, size - 2 * m, size - 2 * m);
    p.end();
    return px;
  };
  QIcon icon;
  icon.addPixmap(drawSquare(red), QIcon::Normal, QIcon::Off);
  icon.addPixmap(drawSquare(gray), QIcon::Disabled, QIcon::Off);
  btn->setIcon(icon);
  btn->setIconSize(QSize(size, size));
  btn->setCursor(Qt::PointingHandCursor);
  btn->setFocusPolicy(Qt::NoFocus);
  btn->setToolTip(QStringLiteral("停止脚本执行"));
  applyIconButtonStyle(btn);
  return btn;
}

// ════════════════════════════════════════════════════════════
//  调试按钮
// ════════════════════════════════════════════════════════════

QPushButton *AuiButton::createDebugButton(int size) {
  auto *btn = new QPushButton;
  // 绘制红色 bug 图标（与 VSCode 调试按钮风格一致；禁用时使用灰色图标）
  QColor red = AuiStyle::errorTextColor();
  QColor gray = AuiStyle::inactiveTabColor();
  auto drawBug = [&](const QColor &color) {
    QPixmap px(size, size);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.4));
    p.setBrush(Qt::NoBrush);
    const qreal m = 4.0;
    const qreal w = size - 2 * m;
    // 两段触角
    p.drawLine(QPointF(m + 2, m), QPointF(size * 0.35, m + 3));
    p.drawLine(QPointF(size - m - 2, m), QPointF(size * 0.65, m + 3));
    // 身体（椭圆）
    p.drawEllipse(QPointF(size / 2, size * 0.55), w * 0.32, w * 0.30);
    // 中缝
    p.drawLine(QPointF(size / 2, m + 3), QPointF(size / 2, size * 0.85));
    // 六条腿
    for (int i = 0; i < 3; ++i) {
      qreal y = size * 0.45 + i * size * 0.12;
      p.drawLine(QPointF(size * 0.30, y), QPointF(size * 0.12, y - 2));
      p.drawLine(QPointF(size * 0.70, y), QPointF(size * 0.88, y - 2));
    }
    p.end();
    return px;
  };
  QIcon icon;
  icon.addPixmap(drawBug(red), QIcon::Normal, QIcon::Off);
  icon.addPixmap(drawBug(gray), QIcon::Disabled, QIcon::Off);
  btn->setIcon(icon);
  btn->setIconSize(QSize(size, size));
  btn->setCursor(Qt::PointingHandCursor);
  btn->setFocusPolicy(Qt::NoFocus);
  btn->setToolTip(QStringLiteral("开始调试 (F5)"));
  applyIconButtonStyle(btn);
  return btn;
}

QPushButton *AuiButton::createDebugStepButton(int kind) {
  auto *btn = new QPushButton;
  btn->setProperty("auiDebugStepBtn", kind);  // 标记调试单步按钮，供主题切换时重绘
  btn->setIcon(makeDebugStepIcon(kind));
  // 图标略小于按钮，留出 1px 边框内边距，避免被裁切
  btn->setIconSize(QSize(kDebugStepSize - 2, kDebugStepSize - 2));
  btn->setFixedSize(kDebugStepSize, kDebugStepSize);
  btn->setCursor(Qt::PointingHandCursor);
  btn->setFocusPolicy(Qt::NoFocus);
  btn->setStyleSheet(debugStepButtonStyleSheet());
  return btn;
}

// ════════════════════════════════════════════════════════════
//  保存按钮
// ════════════════════════════════════════════════════════════

QPushButton *AuiButton::createSaveButton(int size) {
  auto *btn = new QPushButton;
  // 绘制软盘图标（随主题 / 颜色变化自动重绘；禁用时使用灰色图标，与启用状态明显区分）
  bindThemeAwareIcon(btn, [size]() {
    auto drawDisk = [size](const QColor &color) {
      QPixmap px(size, size);
      px.fill(Qt::transparent);
      QPainter p(&px);
      p.setRenderHint(QPainter::Antialiasing);
      p.setPen(QPen(color, 1.2));
      int m = 2;  // margin
      // 软盘外框
      p.drawRect(m, m, size - 2 * m, size - 2 * m);
      // 软盘标签（上部小矩形）
      p.drawRect(m + 3, m, size - 2 * m - 6, size / 3);
      // 底部横线
      p.drawLine(m + 2, size - m - 4, size - m - 2, size - m - 4);
      p.end();
      return px;
    };
    QIcon icon;
    icon.addPixmap(drawDisk(AuiStyle::textColor()), QIcon::Normal, QIcon::Off);
    icon.addPixmap(drawDisk(AuiStyle::inactiveTabColor()), QIcon::Disabled, QIcon::Off);
    return icon;
  });
  btn->setIconSize(QSize(size, size));
  btn->setCursor(Qt::PointingHandCursor);
  btn->setFocusPolicy(Qt::NoFocus);
  btn->setToolTip(QStringLiteral("保存 (Ctrl+S)"));
  applyIconButtonStyle(btn);
  btn->setEnabled(false);
  return btn;
}

// ════════════════════════════════════════════════════════════
//  保存全部按钮
// ════════════════════════════════════════════════════════════

QPushButton *AuiButton::createSaveAllButton(int size) {
  auto *btn = new QPushButton;
  // 绘制双层软盘图标（随主题 / 颜色变化自动重绘；禁用时使用灰色图标，与启用状态明显区分）
  bindThemeAwareIcon(btn, [size]() {
    auto drawDisk = [size](QPainter &p, int ox, int oy, const QColor &color) {
      p.setPen(QPen(color, 1.2));
      int d = 2;
      int m = 1 + d;  // 留出偏移空间
      int x = m + ox;
      int y = m + oy;
      int w = size - 2 * m;
      int h = size - 2 * m;
      p.drawRect(x, y, w, h);
      p.drawRect(x + 3, y, w - 6, h / 3);
      p.drawLine(x + 2, y + h - 4, x + w - 2, y + h - 4);
    };
    auto build = [&](const QColor &fg, const QColor &bg) {
      QPixmap px(size, size);
      px.fill(Qt::transparent);
      QPainter p(&px);
      p.setRenderHint(QPainter::Antialiasing);
      int d = 2;
      // 背景层 左上偏移
      drawDisk(p, -d, -d, bg);
      // 前景层 右下偏移
      drawDisk(p, +d, +d, fg);
      p.end();
      return px;
    };
    QIcon icon;
    icon.addPixmap(build(AuiStyle::textColor(), AuiStyle::saveAllButtonBgColor()),
                   QIcon::Normal, QIcon::Off);
    icon.addPixmap(build(AuiStyle::inactiveTabColor(), AuiStyle::inactiveTabColor()),
                   QIcon::Disabled, QIcon::Off);
    return icon;
  });
  btn->setIconSize(QSize(size, size));
  btn->setCursor(Qt::PointingHandCursor);
  btn->setFocusPolicy(Qt::NoFocus);
  btn->setToolTip(QStringLiteral("保存全部"));
  applyIconButtonStyle(btn);
  return btn;
}

// ════════════════════════════════════════════════════════════
//  对话框按钮行
// ════════════════════════════════════════════════════════════

DialogButtons AuiButton::createDialogButtons(QWidget *parent, bool showCancel) {
  DialogButtons result;
  result.okBtn = nullptr;
  result.cancelBtn = nullptr;
  result.layout = new QHBoxLayout;

  result.layout->addStretch();

  result.okBtn = new QPushButton(QString::fromUtf8(CodeConstants::UiText::kConfirm), parent);
  result.okBtn->setDefault(true);
  result.okBtn->setMinimumWidth(80);
  applyDialogButtonStyle(result.okBtn);
  result.layout->addWidget(result.okBtn);

  if (showCancel) {
    result.layout->addSpacing(12);
    result.cancelBtn = new QPushButton(QString::fromUtf8(CodeConstants::UiText::kCancel), parent);
    result.cancelBtn->setMinimumWidth(80);
    applyDialogButtonStyle(result.cancelBtn);
    result.layout->addWidget(result.cancelBtn);
  }

  result.layout->addStretch();

  return result;
}

// ════════════════════════════════════════════════════════════
//  图标更新
// ════════════════════════════════════════════════════════════
void AuiButton::updateMaximizeIcon(QPushButton *btn, bool isMaximized) {
  // 记录状态到属性，供主题切换时 refreshThemedButtons 用当前文字色重绘图标
  btn->setProperty("auiMaximized", isMaximized);
  QPixmap px(14, 14);
  px.fill(Qt::transparent);
  QPainter p(&px);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(AuiStyle::textColor(), 1.2));

  if (isMaximized) {
    p.drawRect(1, 4, 9, 7);
    p.drawRect(5, 1, 9, 7);
  } else {
    p.drawRect(1, 1, 12, 12);
  }
  p.end();
  btn->setIcon(QIcon(px));
  btn->setIconSize(QSize(14, 14));
}

/// 主题切换后重建按钮的样式/图标（颜色用当前主题色）
/// 用于对话框内标准按钮（背景/边框/文字）和标题栏控制按钮（最大化图标）
void AuiButton::refreshThemedButtons(QWidget *root) {
  if (!root) return;
  const auto btns = root->findChildren<QPushButton *>();
  for (QPushButton *b : btns) {
    if (b->property("auiDialogBtn").toBool()) {
      applyDialogButtonStyle(b);
    } else if (b->property("auiDebugStepBtn").isValid()) {
      // 调试单步按钮：样式表和图标颜色在创建时固化，用当前主题色重建
      const int kind = b->property("auiDebugStepBtn").toInt();
      b->setStyleSheet(debugStepButtonStyleSheet());
      b->setIcon(makeDebugStepIcon(kind));
    } else if (b->property("auiTitleBarBtn").toBool()) {
      // 最大化按钮是自绘图标（颜色固化在绘制时），用当前文字色重绘
      if (b->property("auiMaximized").isValid()) {
        updateMaximizeIcon(b, b->property("auiMaximized").toBool());
      }
    }
  }
}