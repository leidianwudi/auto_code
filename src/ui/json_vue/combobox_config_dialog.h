/**
 * @file combobox_config_dialog.h
 * @brief 下拉框数据源配置对话框
 *
 * 用于配置 ApiSelect 组件的远程数据源参数：
 *   - 请求 URL
 *   - Value 字段名（实际值）
 *   - Label 字段名（显示文本）
 *
 * 提供"测试"按钮发送 HTTP 请求，自动提取返回数据的字段名。
 */

#pragma once

#include <QDialog>

#include "json_vue_model.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QLabel;

/**
 * @class ComboboxConfigDialog
 * @brief 下拉框数据源配置对话框
 *
 * 对话框流程：
 *   1. 输入 URL → 点击"测试"发送 HTTP 请求
 *   2. 解析返回数据的 data.list 第一行，提取所有字段名
 *   3. 字段名填充到 Value 和 Label 下拉框
 *   4. 用户选择后点击"确定"保存配置
 */
class ComboboxConfigDialog : public QDialog {
  Q_OBJECT

public:
  explicit ComboboxConfigDialog(QWidget *parent = nullptr);
  ~ComboboxConfigDialog() override = default;

  /// 设置初始配置
  void setConfig(const QString &url, const QString &valueField, const QString &labelField);

  /// 获取配置结果
  QString url() const;
  QString valueField() const;
  QString labelField() const;

  /// 设置 HTTP 请求参数（baseUrl、authHeader、postData）
  void setHttpConfig(const QString &baseUrl, const QString &authHeader, const QString &postData);

private slots:
  /// 点击"测试"按钮，发起 HTTP 请求
  void onTest();
  /// HTTP 请求完成
  void onHttpFinished(const QString &url, const class QJsonDocument &doc);
  /// HTTP 请求失败
  void onHttpError(const QString &url, const QString &errorMsg);

private:
  /// 构建界面
  void setupUI();

  // ── 控件 ──
  QLineEdit *m_urlEdit = nullptr;       ///< 请求 URL 输入框
  QPushButton *m_testBtn = nullptr;     ///< 测试按钮
  QTableWidget *m_previewTable = nullptr;  ///< 返回数据预览表格
  QComboBox *m_valueCombo = nullptr;    ///< Value 字段下拉框
  QComboBox *m_labelCombo = nullptr;    ///< Label 字段下拉框
  QLabel *m_statusLabel = nullptr;      ///< 状态提示标签

  // ── HTTP 配置 ──
  QString m_baseUrl;     ///< baseUrl
  QString m_authHeader;  ///< Authorization 请求头
  QString m_postData;    ///< POST 请求数据
};
