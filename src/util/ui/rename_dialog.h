/**
 * @file rename_dialog.h
 * @brief 重命名对话框
 *
 * 模态对话框，提供名称输入和校验。
 * 使用 AuiWindow 统一样式，与 CreateUi 保持一致的无边框外观。
 */

#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QString>

class QWidget;

/**
 * @class RenameDialog
 * @brief 重命名模态对话框
 *
 * 提供旧名称预填充、输入校验和确认/取消交互。
 * 支持静态便捷方法 getNewName()。
 * 弹出时自动为父窗口安装半透明遮罩层，关闭时自动移除。
 */
class RenameDialog : public QDialog {
  Q_OBJECT

public:
  explicit RenameDialog(const QString &oldName, QWidget *parent = nullptr);
  ~RenameDialog() override = default;

  /// 获取用户输入的新名称
  QString newName() const;

  /// 静态便捷方法：弹出重命名对话框，返回新名称（取消时返回空字符串）
  /// @param parent  父窗口
  /// @param oldName 旧名称（预填充）
  /// @param ok      输出参数，true 表示用户点击了确认
  static QString getNewName(QWidget *parent, const QString &oldName, bool *ok = nullptr);

protected:
  void showEvent(QShowEvent *event) override;
  void done(int r) override;

private slots:
  void onAccept();

private:
  void setupUI();

  QLineEdit *m_nameEdit = nullptr;  ///< 名称输入框
  QLabel *m_errorLabel = nullptr;   ///< 错误提示标签
  QPushButton *m_okBtn = nullptr;   ///< 确定按钮
  QString m_oldName;                ///< 旧名称
  QWidget *m_overlay = nullptr;     ///< 父窗口遮罩层
};