/**
 * @file aui_message_box.h
 * @brief 消息对话框工具类
 *
 * 替代 QMessageBox，提供与项目统一风格的消息提示和确认对话框。
 * 使用无边框窗口 + 自定义标题栏，外观与 RenameDialog / CreateUi 一致。
 *
 * 使用方式：
 * @code
 *   // 纯提示
 *   AuiMessageBox::show(parent, "打开失败", "无法打开文件: xxx");
 *
 *   // 确认操作
 *   if (AuiMessageBox::confirm(parent, "确认删除", "确定要删除吗？")) {
 *     // 用户点了确定
 *   }
 * @endcode
 */

#pragma once

#include <QString>

class QWidget;

/// 消息对话框工具类 — 替代 QMessageBox，统一项目风格
class AuiMessageBox {
public:
  /// 显示消息对话框（只有"确定"按钮）
  /// @param parent  父窗口
  /// @param title   标题
  /// @param text    内容
  static void show(QWidget *parent, const QString &title, const QString &text);

  /// 显示确认对话框（"确定"和"取消"按钮）
  /// @param parent  父窗口
  /// @param title   标题
  /// @param text    内容
  /// @return true 表示用户点击了"确定"
  static bool confirm(QWidget *parent, const QString &title, const QString &text);

private:
  AuiMessageBox() = delete;
};