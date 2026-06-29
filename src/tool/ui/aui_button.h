/**
 * @file aui_button.h
 * @brief UI 公共按钮工具类
 *
 * 提供预定义样式的按钮工厂方法，统一按钮外观和交互行为。
 */

#pragma once

#include "aui_style.h"

#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>

class AuiButton {
public:
  // ════════════════════════════════════════════════════════════
  //  通用按钮样式
  // ════════════════════════════════════════════════════════════

  /// 对按钮应用标题栏按钮通用样式
  static void applyCommonStyle(QPushButton *btn) {
    btn->setFixedSize(36, 26);
    btn->setFlat(true);
    btn->setFocusPolicy(Qt::NoFocus);
  }

  // ════════════════════════════════════════════════════════════
  //  工厂方法
  // ════════════════════════════════════════════════════════════

  /// 创建「向右拆分」按钮
  static QPushButton *createSplitButton() {
    auto *btn = new QPushButton;
    {
      QPixmap px(20, 20);
      px.fill(Qt::transparent);
      QPainter p(&px);
      p.setRenderHint(QPainter::Antialiasing);
      p.setPen(QPen(AuiStyle::textColor(), 1.5));
      p.drawRect(2, 3, 16, 14);                    // 外框
      p.drawLine(QPointF(10, 3), QPointF(10, 17)); // 中分线
      p.end();
      btn->setIcon(QIcon(px));
      btn->setIconSize(QSize(20, 20));
    }
    btn->setToolTip(QStringLiteral("向右拆分编辑器 (Ctrl+\\)"));
    applyCommonStyle(btn);
    return btn;
  }

  /// 创建「最小化」按钮
  static QPushButton *createMinButton() {
    auto *btn = new QPushButton(QStringLiteral("—"));
    applyCommonStyle(btn);
    return btn;
  }

  /// 创建「最大化」按钮（初始为未最大化图标）
  static QPushButton *createMaxButton() {
    auto *btn = new QPushButton;
    updateMaximizeIcon(btn, false);
    applyCommonStyle(btn);
    return btn;
  }

  /// 创建「关闭」按钮
  static QPushButton *createCloseButton() {
    auto *btn = new QPushButton(QStringLiteral("✕"));
    applyCommonStyle(btn);
    return btn;
  }

  // ════════════════════════════════════════════════════════════
  //  图标更新
  // ════════════════════════════════════════════════════════════

  /// 更新最大化 / 还原按钮图标
  static void updateMaximizeIcon(QPushButton *btn, bool isMaximized) {
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

private:
  AuiButton() = delete;
};