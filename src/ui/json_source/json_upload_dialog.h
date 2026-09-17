/**
 * @file json_upload_dialog.h
 * @brief .jsonupload 单条上传预设编辑对话框
 *
 * 编辑一条上传预设：说明 / 上传地址 / 请求方式 / 文件字段名 /
 * 响应提取路径 / 最多张数 / 提交值形态 / 附加 form 参数表格。
 * 通过 setUpload() 预填，upload() 取回编辑结果（getter 模式，无信号回写）。
 */

#pragma once

#include <QDialog>

#include "json_upload_model.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;

/**
 * @class JsonUploadDialog
 * @brief 单条上传预设编辑对话框
 */
class JsonUploadDialog : public QDialog {
  Q_OBJECT

public:
  explicit JsonUploadDialog(QWidget *parent = nullptr);

  /// 预填编辑内容
  void setUpload(const JsonUpload &u);
  /// 取回编辑结果
  JsonUpload upload() const;

private:
  /// 构建界面
  void setupUI();
  /// 从表格收集附加参数
  QVector<JsonUploadParam> collectParams() const;
  /// 向表格填充附加参数
  void fillParams(const QVector<JsonUploadParam> &params);

  QLineEdit *m_remarkEdit = nullptr;        ///< 说明
  QLineEdit *m_urlEdit = nullptr;           ///< 上传地址
  QComboBox *m_methodCombo = nullptr;       ///< 请求方式
  QLineEdit *m_fileFieldEdit = nullptr;     ///< form-data 文件字段名
  QLineEdit *m_responsePathEdit = nullptr;  ///< 响应提取路径
  QComboBox *m_maxCountCombo = nullptr;     ///< 最多张数（可编辑）
  QComboBox *m_valueTypeCombo = nullptr;    ///< 提交值形态
  QTableWidget *m_paramsTable = nullptr;    ///< 附加 form 参数表格
  QPushButton *m_addParamBtn = nullptr;     ///< 添加参数按钮
};
