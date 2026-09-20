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

  /// 隐藏下拉框右侧三角箭头，并移除其预留的 dropdown 区域，使文字占满整个宽度。
  /// 适用于仅作方法/状态选择的小下拉框（如 GET/POST），避免文字被预留的箭头区域截断。
  /// 内部通过动态属性结合全局样式表（AuiStyle 中的 QComboBox[auiNoArrow="true"] 规则）实现。
  static void hideArrow(QComboBox *combo);

  /// 弹层高度保证（通用机制，对所有 QComboBox 生效）：
  /// ① 统一条目高度口径——安装与渲染严格一致的委托（hint = 字体行高 + 全局样式表
  ///    ::item padding 4px×2）；默认委托的 hint 在样式表字体刷新前会给出过期值，
  ///    弹层按过期 hint 计算高度会导致条目截断并出现滚动条。定制委托可预先给视图
  ///    设置 auiComboDelegateCustom 属性声明豁免（如 AuiComboDelete）。
  /// ② 视图高度按内容精确固定（min=max，任何后续布局 pass 都无法改变视图高度）。
  /// ComboPopDownFilter 在弹层显示时自动调用；填完条目后手动调用可赶在 Qt 原生
  /// 几何计算之前生效。
  static void ensurePopupFit(QComboBox *combo);

  /// 安装全局事件过滤器：所有 QComboBox 的弹出列表一律向下展开（不上弹）。
  /// 程序启动时调用一次即可（幂等）。
  static void ensureGlobalPopDown();

  /// 安装全局事件过滤器：禁止鼠标滚轮悬停在下拉框上时改动选中值（防止悬停误改数据）。
  /// 弹出列表展开时不拦截，仍可正常滚动列表项。程序启动时调用一次即可（幂等）。
  static void ensureGlobalWheelSafe();

private:
  AuiComboBox() = delete;
};