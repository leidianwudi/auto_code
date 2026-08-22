/**
 * @file aui_input_dialog.h
 * @brief 自绘样式的单行输入对话框工具类
 *
 * 用于替代原生 QInputDialog，应用统一的无边框窗口样式和按钮样式。
 */

#pragma once

#include <QString>

class QWidget;

class AuiInputDialog {
public:
  /// 弹出自绘样式的单行输入对话框
  /// @param parent 父窗口
  /// @param title  对话框标题
  /// @param label  输入框上方的提示文字
  /// @param init   输入框初始文本
  /// @return 用户点击确定且输入非空时返回输入文本；否则返回空字符串
  static QString getText(QWidget *parent, const QString &title, const QString &label,
                         const QString &init = QString());

private:
  AuiInputDialog() = delete;
};