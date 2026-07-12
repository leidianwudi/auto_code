/**
 * @file create_ui.h
 * @brief 新建文件对话框 — 视图层
 *
 * 模态对话框，提供文件类型选择、名称输入和验证反馈。
 * 由 CreateMgr 创建和管理。
 */

#pragma once

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>

class QWidget;

/**
 * @class CreateUi
 * @brief 新建文件/文件夹的模态对话框
 *
 * MVC 中的视图层，负责界面布局和用户交互。
 * 用户确认后通过 CreateMgr 执行创建逻辑。
 * 弹出时自动为父窗口安装半透明遮罩层，关闭时自动移除。
 */
class CreateUi : public QDialog {
  Q_OBJECT

public:
  explicit CreateUi(QWidget *parent = nullptr);
  ~CreateUi() override = default;

  /// 获取用户输入的文件类型索引（对应 CreateModel::FileType）
  int fileTypeIndex() const;
  /// 获取用户输入的文件/文件夹名称
  QString fileName() const;

  /// 设置错误提示文本
  void setError(const QString &msg);
  /// 清除错误提示
  void clearError();

protected:
  void showEvent(QShowEvent *event) override;
  void done(int r) override;

private slots:
  /// 文件类型切换时自动更新名称后缀提示
  void onFileTypeChanged(int index);
  /// 确认按钮点击
  void onAccept();

private:
  void setupUI();

  /// 获取当前选中类型对应的后缀
  QString currentSuffix() const;

  QComboBox *m_typeCombo = nullptr;    ///< 文件类型下拉框
  QLineEdit *m_nameEdit = nullptr;     ///< 名称输入框
  QLabel *m_errorLabel = nullptr;      ///< 错误提示标签
  QPushButton *m_okBtn = nullptr;      ///< 确定按钮
  QPushButton *m_cancelBtn = nullptr;  ///< 取消按钮
  QWidget *m_overlay = nullptr;        ///< 父窗口遮罩层
};