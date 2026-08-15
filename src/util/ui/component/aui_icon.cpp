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

#include "aui_style.h"

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

  // 无底框，纯文字标识：ac 蓝色「A」/ json 琥珀「J」/ jsonvue 琥珀「V」/ tpl 绿色「T」，
  // 颜色随主题明暗调整，字号大、居中铺满
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
  const QColor stroke =
      dark ? QColor(0xe8, 0xe8, 0xe8) : QColor(0x2b, 0x2b, 0x2b);  // 主描边（前板）
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