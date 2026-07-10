/**
 * @file aui_icon.h
 * @brief 图标绘制工具类
 *
 * 提供统一的图标绘制静态方法，供按钮、树节点等控件复用。
 */

#pragma once

#include <QIcon>
#include <QString>

class AuiIcon {
public:
  /// 创建构建按钮的三角形箭头图标
  /// @param size  三角区域边长（像素）
  static QIcon createBuildIcon(int size = 18);

  /// 在图标右下角叠加绿色三角形（启动项标记）
  /// @param baseIcon 基础图标
  /// @param size     图标像素尺寸
  /// @param triSize  三角形边长
  static QIcon createStartupOverlayIcon(const QIcon &baseIcon, int size = 16, int triSize = 9);

  /// 创建下拉框向下箭头图标（用于 QComboBox::down-arrow）
  /// @param size 图标像素尺寸
  static QIcon createComboBoxDownArrow(int size = 16);

private:
  AuiIcon() = delete;
};