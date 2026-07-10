/**
 * @file aui_combo_box.h
 * @brief 样式化下拉框工具类
 *
 * 提供统一风格的 QComboBox 工厂方法，包含自定义向下三角箭头图标。
 * 样式通过 AuiStyle 颜色常量统一管理。
 */

#pragma once

#include <QComboBox>

/**
 * @class AuiComboBox
 * @brief 样式化下拉框工厂类
 *
 * 用法示例：
 * @code
 * QComboBox *combo = AuiComboBox::create(this);
 * combo->addItem("选项 1");
 * combo->addItem("选项 2");
 * @endcode
 */
class AuiComboBox {
public:
  /// 创建带有向下三角箭头图标的样式化下拉框
  static QComboBox *create(QWidget *parent = nullptr);

  /// 对已有下拉框应用样式（含自定义三角箭头）
  static void applyStyle(QComboBox *combo);

private:
  AuiComboBox() = delete;
};