/**
 * @file aui_button.h
 * @brief UI 公共按钮工具类
 *
 * 提供预定义样式的按钮工厂方法，统一按钮外观和交互行为。
 */

#pragma once

#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QString>

/// 对话框按钮行创建结果
struct DialogButtons {
  QPushButton *okBtn;      ///< "确定"按钮
  QPushButton *cancelBtn;  ///< "取消"按钮（可能为 nullptr）
  QHBoxLayout *layout;     ///< 已排好的水平布局（居中，可直接加入 contentLayout）
};

class AuiButton {
public:
  // ════════════════════════════════════════════════════════════
  //  通用按钮样式
  // ════════════════════════════════════════════════════════════

  /// 对按钮应用标题栏按钮通用样式
  static void applyCommonStyle(QPushButton *btn);

  /// 对按钮应用图标按钮通用样式（透明背景 + hover/pressed 半透明效果）
  static void applyIconButtonStyle(QPushButton *btn);

  /// 对话框按钮样式表（QPushButton 边框 + hover/pressed/disabled，供 dialogStyleSheet 组合）
  static QString dialogButtonStyleSheet();

  /// 对单个对话框按钮应用样式
  static void applyDialogButtonStyle(QPushButton *btn);

  // ════════════════════════════════════════════════════════════
  //  工厂方法
  // ════════════════════════════════════════════════════════════
  /// 创建「向右拆分」按钮
  static QPushButton *createSplitButton();

  /// 创建「最小化」按钮
  static QPushButton *createMinButton();

  /// 创建「最大化」按钮（初始为未最大化图标）
  static QPushButton *createMaxButton();

  /// 创建「关闭」按钮
  static QPushButton *createCloseButton();

  /// 创建「构建 / 生成」按钮（QPushButton，三角形图标）
  static QPushButton *createBuildButton(int size = 18);

  /// 创建「保存」按钮（QPushButton，软盘图标）
  static QPushButton *createSaveButton(int size = 18);

  /// 创建「保存全部」按钮（QPushButton，软盘图标 + 全部标记）
  static QPushButton *createSaveAllButton(int size = 20);

  /// 创建对话框按钮行（居中布局，确定 + 可选取消）
  /// @param parent  按钮的父控件
  /// @param showCancel  是否显示"取消"按钮
  /// @return 包含按钮指针和布局的结构体
  static DialogButtons createDialogButtons(QWidget *parent, bool showCancel = true);

  // ════════════════════════════════════════════════════════════
  //  图标更新
  // ════════════════════════════════════════════════════════════
  /// 更新最大化 / 还原按钮图标
  static void updateMaximizeIcon(QPushButton *btn, bool isMaximized);

private:
  AuiButton() = delete;
};