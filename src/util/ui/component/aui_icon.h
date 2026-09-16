/**
 * @file aui_icon.h
 * @brief 图标绘制工具类
 *
 * 提供统一的图标绘制静态方法，供按钮、树节点等控件复用。
 */

#pragma once

#include <QColor>
#include <QIcon>
#include <QPointF>
#include <QString>

class QPainter;

class AuiIcon {
public:
  /// 创建构建按钮的三角形箭头图标
  /// @param size  三角区域边长（像素）
  static QIcon createBuildIcon(int size = 18);

  /// 创建下拉框向下箭头图标（用于 QComboBox::down-arrow）
  /// @param size 图标像素尺寸
  static QIcon createComboBoxDownArrow(int size = 16);

  /// 创建「全部折叠」图标（VSCode 风格：顶部横线 + 向上箭头，颜色随主题）
  /// @param size 图标像素尺寸
  static QIcon createCollapseAllIcon(int size = 16);

  /// 创建「帮助」图标（圆圈 + 问号，颜色随主题）
  /// @param size 图标像素尺寸
  static QIcon createHelpIcon(int size = 16);

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

  /// 直接在 painter 上绘制项目齿轮（VSCode 设置齿轮风格：6 齿圆润、空心描边、中心孔）。
  /// 用于项目根文件夹整体替代文件夹图标，颜色与文件夹图标主描边一致（主题自适应）。
  /// @param painter 目标画笔（抗锯齿由本函数内部开启）
  /// @param center  齿轮中心（图标像素坐标）
  static void paintProjectGear(QPainter *painter, const QPointF &center);

private:
  AuiIcon() = delete;
};