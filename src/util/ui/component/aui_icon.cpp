/**
 * @file aui_icon.cpp
 * @brief 图标绘制工具类实现
 */

#include "aui_icon.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QtMath>

#include "aui_style.h"

/// 极坐标取点（角度制；齿轮轮廓顶点计算用）
static QPointF gearPolarPt(const QPointF &c, qreal deg, qreal r) {
  const qreal rad = qDegreesToRadians(deg);
  return QPointF(c.x() + r * qCos(rad), c.y() + r * qSin(rad));
}

/// 文件夹图标主描边色（createFolderIcon 与项目齿轮共用，保证颜色一致）
static QColor folderStrokeColor() {
  const bool dark = (SettingStore::ins().theme() == SettingStore::ThemeDark);
  return dark ? QColor(0xe8, 0xe8, 0xe8) : QColor(0x2b, 0x2b, 0x2b);
}

// ════════════════════════════════════════════════════════════
//  项目齿轮（VSCode 设置齿轮风格）
// ════════════════════════════════════════════════════════════

void AuiIcon::paintProjectGear(QPainter *painter, const QPointF &center) {
  constexpr int kTeeth = 6;            // 齿数
  constexpr qreal kTipR = 6;           // 齿顶半径（整图标尺寸，替代文件夹图标）
  constexpr qreal kRootR = 4.1;        // 齿根半径（齿高 1.5，圆润厚重）
  constexpr qreal kTipHalfDeg = 12.0;  // 齿顶半角（齿顶弧占 24°，齿谷 36°）
  constexpr qreal kHoleR = 2.0;        // 中心孔半径
  constexpr qreal kGearWidth = 1.0;    // 齿轮笔宽

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  QPen pen(folderStrokeColor(), kGearWidth);  // 笔宽
  pen.setJoinStyle(Qt::RoundJoin);            // 齿尖/齿根圆角过渡，接近 VSCode 观感
  painter->setPen(pen);
  painter->setBrush(Qt::NoBrush);

  // 齿轮轮廓：齿顶圆弧 + 径向齿侧 + 齿谷圆弧交替（弧线按 4° 细分成短线段）
  QPolygonF gear;
  const qreal step = 360.0 / kTeeth;
  for (int i = 0; i < kTeeth; ++i) {
    const qreal a = i * step;
    for (qreal t = -kTipHalfDeg; t <= kTipHalfDeg; t += 4.0)
      gear << gearPolarPt(center, a + t, kTipR);  // 齿顶弧
    for (qreal t = kTipHalfDeg; t <= step - kTipHalfDeg; t += 4.0)
      gear << gearPolarPt(center, a + t, kRootR);  // 齿谷弧（齿侧由相邻点连线形成）
  }
  painter->drawPolygon(gear);                    // NoBrush → 只描边 = 空心
  painter->drawEllipse(center, kHoleR, kHoleR);  // 中心孔（空心）
  painter->restore();
}

// ════════════════════════════════════════════════════════════
//  构建按钮图标
// ════════════════════════════════════════════════════════════

QIcon AuiIcon::createBuildIcon(int size) {
  QPixmap px(size, size);
  px.fill(Qt::transparent);
  QPainter p(&px);
  p.setRenderHint(QPainter::Antialiasing);

  // 绘制三角形（接近铺满，仅留 1px 边距）
  p.setPen(Qt::NoPen);
  p.setBrush(AuiStyle::compileButtonColor());
  const double half = size * 0.5;
  QPolygonF tri;
  tri << QPointF(1.0, 1.0) << QPointF(size - 1.0, half) << QPointF(1.0, size - 1.0);
  p.drawPolygon(tri);
  p.end();
  return QIcon(px);
}

// ════════════════════════════════════════════════════════════
//  下拉框向下箭头图标
// ════════════════════════════════════════════════════════════

QIcon AuiIcon::createComboBoxDownArrow(int size) {
  // 使用 16x16 尺寸，内部绘制 10x6 的三角形
  QPixmap px(size, size);
  px.fill(Qt::transparent);
  QPainter p(&px);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(Qt::NoPen);
  p.setBrush(AuiStyle::textColor());

  // 居中绘制倒三角，宽 10px、高 6px
  const double cx = size * 0.5;
  const double triW = 10.0;
  const double triH = 6.0;
  QPolygonF tri;
  tri << QPointF(cx - triW / 2.0, (size - triH) / 2.0)
      << QPointF(cx + triW / 2.0, (size - triH) / 2.0) << QPointF(cx, (size + triH) / 2.0);
  p.drawPolygon(tri);
  p.end();
  return QIcon(px);
}

// ════════════════════════════════════════════════════════════
//  全部折叠图标（VSCode 风格：顶部横线 + 向上箭头）
// ════════════════════════════════════════════════════════════

QIcon AuiIcon::createCollapseAllIcon(int size) {
  // 2x 超采样绘制再缩放，边缘更清晰
  const int s = qMax(8, size * 2);
  QPixmap pm(s, s);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  QPen pen(AuiStyle::textColor(), 1.6 * 2.0);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);

  const double u = 2.0;  // 缩放因子（s/size）
  // 顶部横线
  p.drawLine(QPointF(3.0 * u, 3.5 * u), QPointF(13.0 * u, 3.5 * u));
  // 向上箭头（折叠 → 全部收起）
  QPainterPath path;
  path.moveTo(4.0 * u, 11.5 * u);
  path.lineTo(8.0 * u, 7.5 * u);
  path.lineTo(12.0 * u, 11.5 * u);
  p.drawPath(path);
  p.end();
  return QIcon(pm);
}

// ════════════════════════════════════════════════════════════
//  帮助图标（圆圈 + 问号）
// ════════════════════════════════════════════════════════════

QIcon AuiIcon::createHelpIcon(int size) {
  // 2x 超采样绘制再缩放，边缘更清晰
  const int s = qMax(8, size * 2);
  QPixmap pm(s, s);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);

  const double u = 2.0;  // 缩放因子（s/size），16px 逻辑坐标系
  const QColor color = AuiStyle::textColor();

  // 圆圈（描边空心，与文件夹图标风格一致）
  p.setPen(QPen(color, 1.3 * u));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(QPointF(8.0 * u, 8.0 * u), 6.1 * u, 6.1 * u);

  // 问号（加粗文字，居中于圆心）
  QFont f = p.font();
  f.setBold(true);
  f.setPixelSize(qMax(6, qRound(9.5 * u)));
  p.setFont(f);
  p.setPen(QPen(color, 1.0));
  p.drawText(QRectF(0, 0, 16.0 * u, 16.0 * u), Qt::AlignCenter, QStringLiteral("?"));
  p.end();
  return QIcon(pm);
}

// ════════════════════════════════════════════════════════════
//  文件类型图标（ac / json / tpl）
// ════════════════════════════════════════════════════════════

QIcon AuiIcon::createFileTypeIcon(const QString &suffix, int size) {
  // 2x 超采样绘制再缩放，边缘更清晰
  const int s = qMax(8, size * 2);
  QPixmap pm(s, s);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.scale(2.0, 2.0);  // 以 size 为逻辑坐标绘制

  const bool dark = (SettingStore::ins().theme() == SettingStore::ThemeDark);
  const qreal w = size;

  // 无底框，纯文字标识：ac 蓝色「A」/ json 琥珀「J」/ jsonvue 琥珀「V」/
  // jsonsource 琥珀「S」/ jsonupload 紫色「U」/ tpl 绿色「T」，颜色随主题明暗调整
  QColor accent;
  QString glyph;
  int pixelSize;
  const QString suf = suffix.toLower();
  if (suf == QStringLiteral("ac")) {
    accent = dark ? QColor(0x4f, 0x9c, 0xf9) : QColor(0x2b, 0x6d, 0xe0);
    glyph = QStringLiteral("A");
    pixelSize = qMax(8, qRound(size * 0.78));
  } else if (suf == QStringLiteral("json")) {
    accent = dark ? QColor(0xe3, 0xa5, 0x18) : QColor(0xb5, 0x7e, 0x00);
    glyph = QStringLiteral("J");
    pixelSize = qMax(8, qRound(size * 0.78));
  } else if (suf == QStringLiteral("jsonvue")) {
    accent = dark ? QColor(0xe3, 0xa5, 0x18) : QColor(0xb5, 0x7e, 0x00);
    glyph = QStringLiteral("V");
    pixelSize = qMax(8, qRound(size * 0.78));
  } else if (suf == QStringLiteral("jsonsource")) {
    accent = dark ? QColor(0xe3, 0xa5, 0x18) : QColor(0xb5, 0x7e, 0x00);
    glyph = QStringLiteral("S");
    pixelSize = qMax(8, qRound(size * 0.78));
  } else if (suf == QStringLiteral("jsonupload")) {
    accent = dark ? QColor(0xb0, 0x7f, 0xe8) : QColor(0x7a, 0x4f, 0xc0);
    glyph = QStringLiteral("U");
    pixelSize = qMax(8, qRound(size * 0.78));
  } else {  // tpl 及未知后缀
    accent = dark ? QColor(0x4c, 0xb0, 0x5e) : QColor(0x2f, 0x8a, 0x44);
    glyph = QStringLiteral("T");
    pixelSize = qMax(8, qRound(size * 0.78));
  }

  QFont f;
  f.setFamily(QStringLiteral("Segoe UI"));
  f.setBold(true);
  f.setPixelSize(pixelSize);
  p.setFont(f);
  p.setPen(accent);
  p.drawText(QRectF(0.0, 0.0, w, w), Qt::AlignCenter, glyph);

  p.end();
  return QIcon(pm);
}

// ════════════════════════════════════════════════════════════
//  文件夹图标（展开 / 收起）
// ════════════════════════════════════════════════════════════

QIcon AuiIcon::createFolderIcon(bool open, int size) {
  // 2x 超采样绘制再缩放，边缘更清晰
  const int s = qMax(8, size * 2);
  QPixmap pm(s, s);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.scale(2.0, 2.0);

  const bool dark = (SettingStore::ins().theme() == SettingStore::ThemeDark);

  // 空心描边文件夹：仅画轮廓不填充（内部透出背景），主题感知配色。
  // 收起 = 带顶标签的矩形外框；展开 = 同款外框 + 内部梯形前板（开口文件夹）
  const QColor stroke = folderStrokeColor();  // 主描边（前板）
  const QColor strokeDim =
      dark ? QColor(0xa8, 0xa8, 0xa8) : QColor(0x6e, 0x6e, 0x6e);  // 次要描边（背板）
  const qreal pw = 1.2;
  p.setBrush(Qt::NoBrush);

  // 外框：带顶标签的矩形（标签与左壁齐平，右上收窄后接主体顶边），直角无圆角
  QPainterPath frame;
  frame.moveTo(2.2, 3.4);    // 标签左上角
  frame.lineTo(6.8, 3.4);    // 标签顶边
  frame.lineTo(8.4, 5.2);    // 标签右侧斜边
  frame.lineTo(13.6, 5.2);   // 主体顶边
  frame.lineTo(13.6, 12.9);  // 右壁
  frame.lineTo(2.2, 12.9);   // 底边
  frame.lineTo(2.2, 3.4);    // 左壁（含标签左侧，齐平）
  frame.closeSubpath();

  if (!open) {
    // 收起：仅外框，空心矩形 + 顶标签
    p.setPen(QPen(stroke, pw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(frame);
  } else {
    // 展开：外框（背板）+ 内部平行四边形前板。
    // 背板先画，但以前板区域为裁剪排除被前板遮挡的部分（只画可见处）；
    // 前板最后画，其主色描边覆盖裁剪边界的残边，形成正确的前后遮挡关系。
    QPainterPath front;      // 前板：平行四边形，底边与后板底边等宽，两侧边由底部向上向右倾斜
    front.moveTo(3, 12.9);   // 左下角（与后板底边左端对齐）
    front.lineTo(13, 12.9);  // 右下角（与后板底边右端对齐，底边等宽）
    front.lineTo(16, 8.2);   // 右上角（右斜边，向上向右倾斜）
    front.lineTo(6, 8.2);    // 左上角（顶边）
    front.closeSubpath();

    QPainterPath backVisible;  // 背板可见区域 = 整幅画布 - 前板区域
    backVisible.addRect(QRectF(-1.0, -1.0, 18.0, 18.0));
    backVisible = backVisible.subtracted(front);

    p.save();
    p.setClipPath(backVisible);
    p.setPen(QPen(strokeDim, pw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(frame);
    p.restore();

    p.setPen(QPen(stroke, pw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(front);
  }

  p.end();
  return QIcon(pm);
}