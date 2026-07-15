/**
 * @file aui_icon.cpp
 * @brief 图标绘制工具类实现
 */

#include "aui_icon.h"

#include <QPainter>
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
//  启动项叠加图标
// ════════════════════════════════════════════════════════════

/// @brief 在图标右下角叠加绿色三角形（启动项标记）
/// @param baseIcon 基础图标
/// @param size     图标像素尺寸
/// @param triSize  三角形边长
QIcon AuiIcon::createStartupOverlayIcon(const QIcon &baseIcon, int size, int triSize) {
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
  int leftSp = 3;  // 左间隔
  QPolygon tri;
  tri << QPoint(leftSp, leftSp)                      // 左上角（三角形左侧上顶点）
      << QPoint(leftSp, h - leftSp)                  // 左下角（三角形左侧下顶点）
      << QPoint((w / 2) + leftSp, h / 2);            // 右顶点（箭头尖端朝右）
  painter.setBrush(AuiStyle::compileButtonColor());  // 绿色填充
  painter.setPen(Qt::NoPen);                         // 无边框
  painter.drawPolygon(tri);
  painter.end();

  return QIcon(overlay);
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