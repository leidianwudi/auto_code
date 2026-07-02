/**
 * @file aui_button.h
 * @brief UI 公共按钮工具类
 *
 * 提供预定义样式的按钮工厂方法，统一按钮外观和交互行为。
 */

#pragma once

#include <QIcon>
#include <QPushButton>
#include <QString>

class AuiButton {
public:
  // ════════════════════════════════════════════════════════════
  //  通用按钮样式
  // ════════════════════════════════════════════════════════════

  /// 对按钮应用标题栏按钮通用样式
  static void applyCommonStyle(QPushButton *btn);

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
  static QPushButton *createSaveAllButton(int size = 18);

  // ════════════════════════════════════════════════════════════
  //  图标更新
  // ════════════════════════════════════════════════════════════
  /// 更新最大化 / 还原按钮图标
  static void updateMaximizeIcon(QPushButton *btn, bool isMaximized);

private:
  AuiButton() = delete;
};