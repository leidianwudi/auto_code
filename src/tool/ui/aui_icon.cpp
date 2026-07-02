/**
 * @file aui_icon.cpp
 * @brief 图标绘制工具类实现
 */

#include "aui_icon.h"
#include "aui_style.h"

#include <QPainter>
#include <QPixmap>
#include <QPolygonF>

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
  p.setBrush(AuiStyle::textColor());
  const double half = size * 0.5;
  QPolygonF tri;
  tri << QPointF(1.0, 1.0) << QPointF(size - 1.0, half)
      << QPointF(1.0, size - 1.0);
  p.drawPolygon(tri);
  p.end();
  return QIcon(px);
}

// ════════════════════════════════════════════════════════════
//  启动项叠加图标
// ════════════════════════════════════════════════════════════

/// @brief 在图标右下角叠加绿色三角形（启动项标记）
/// @param baseIcon 基础图标
/// @param size     图标像素尺寸
/// @param triSize  三角形边长
QIcon AuiIcon::createStartupOverlayIcon(const QIcon &baseIcon, int size,
                                        int triSize) {
  // 将基础图标绘制到指定尺寸的像素图上
  QPixmap base = baseIcon.pixmap(size, size);
  QPixmap overlay(base.size());
  overlay.fill(Qt::transparent);

  // 先绘制基础图标，再叠加三角形
  QPainter painter(&overlay);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.drawPixmap(0, 0, base);

  // 在图标右下角绘制绿色三角形，箭头方向指向右
  int w = base.width();
  int h = base.height();
  QPolygon tri;
  tri << QPoint(w - triSize, 0)     // 左上角（三角形左侧上顶点）
      << QPoint(w - triSize, h - 1) // 左下角（三角形左侧下顶点）
      << QPoint(w - 1, h / 2);      // 右顶点（箭头尖端朝右）
  painter.setBrush(QColor(QStringLiteral("#4ec9b0"))); // 绿色填充
  painter.setPen(Qt::NoPen);                           // 无边框
  painter.drawPolygon(tri);
  painter.end();

  return QIcon(overlay);
}