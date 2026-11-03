/**
 * @file aui_input_dialog.h
 * @brief 自绘样式的单行输入对话框工具类
 *
 * 用于替代原生 QInputDialog，应用统一的无边框窗口样式和按钮样式。
 */

#pragma once

#include <QString>
#include <QStringList>

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

  /// 弹出自绘样式的下拉选择对话框（语义对齐 QInputDialog::getItem，样式统一走项目封装）
  /// @param parent  父窗口
  /// @param title   对话框标题
  /// @param label   下拉框上方的提示文字
  /// @param items   候选项
  /// @param current 初始选中索引（越界取 0）
  /// @param ok      非空时回写用户是否点击了确定
  /// @return 用户点击确定时返回当前选中项文本；取消返回空字符串
  static QString getItem(QWidget *parent, const QString &title, const QString &label,
                         const QStringList &items, int current = 0, bool *ok = nullptr);

private:
  AuiInputDialog() = delete;
};