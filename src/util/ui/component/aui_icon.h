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

  /// 创建下拉框向下箭头图标（用于 QComboBox::down-arrow）
  /// @param size 图标像素尺寸
  static QIcon createComboBoxDownArrow(int size = 16);

  /// 创建文件类型图标（纯文字样式，无底框）：
  /// ac 蓝色「A」/ json 琥珀「J」/ jsonvue 琥珀「V」/ tpl 绿色「T」，
  /// 字号大、居中铺满，颜色随深色 / 浅色主题明暗调整。
  /// @param suffix 文件后缀（ac / json / jsonvue / tpl，大小写不敏感，未知后缀用 tpl 配色）
  /// @param size   图标像素尺寸
  static QIcon createFileTypeIcon(const QString &suffix, int size = 16);

  /// 创建文件夹图标（空心描边，主题感知配色）
  /// @param open  true 为展开样式（外框 + 内部平行四边形前板），false
  /// 为收起样式（带顶标签的矩形外框）
  /// @param size  图标像素尺寸
  static QIcon createFolderIcon(bool open, int size = 16);

private:
  AuiIcon() = delete;
};