/**
 * @file about_dialog.h
 * @brief 关于对话框 — 显示软件名称与版本号
 *
 * 点击「帮助 → 帮助文档 → 关于」弹出，展示软件信息：
 * 软件名称 Auto Code，版本号 1.0.1。
 */

#pragma once

#include <QDialog>

class QLabel;

/**
 * @class AboutDialog
 * @brief 关于对话框
 */
class AboutDialog : public QDialog {
  Q_OBJECT

public:
  explicit AboutDialog(QWidget *parent = nullptr);
  ~AboutDialog() override = default;

private:
  /// 初始化界面布局
  void setupUI();

  QWidget *m_titleBar = nullptr;  ///< 自定义标题栏（nativeEvent 拖拽）

#if defined(Q_OS_WIN)
  bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif
};
